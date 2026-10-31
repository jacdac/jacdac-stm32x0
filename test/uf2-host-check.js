#!/usr/bin/env node
// Check that a UF2 parses the way the Jacdac host flasher parses it.
//
// A faithful port of the page-building in parseUF2() from jacdac-ts,
// src/jdom/flashing.ts (MIT, (c) Microsoft Corporation), read 2026-09-23. The
// host copies each UF2 block into ONE flash page; a block that spans two pages
// (e.g. 256 B payloads with PY32's 128 B pages) throws a RangeError there,
// and the device can't be updated over the bus. Also checks the boot-block
// trick: page 0 must be sent twice, last with the real app reset vector.
//
// Usage: node test/uf2-host-check.js file.uf2   (exit 1 on failure)
const fs = require("fs")
const UF2_MAGIC_START0 = 0x0a324655, UF2_MAGIC_START1 = 0x9e5d5157, UF2_MAGIC_END = 0x0ab16f30
const uf2ExtTags = { version: -0x9fc7bc, name: -0x650d9d, pageSize: 0x0be9f7, productIdentifier: 0xc8a729 }
function parseUF2(uf2) {
    const blobs = []; let currBlob
    for (let off = 0; off < uf2.length; off += 512) {
        const h = (i) => uf2.readUInt32LE(off + i)
        const [magic0, magic1, flags, trgaddr, payloadSize, blkNo, numBlocks, familyID] = [0,4,8,12,16,20,24,28].map(h)
        if (magic0 != UF2_MAGIC_START0 || magic1 != UF2_MAGIC_START1 || uf2.readUInt32LE(off + 508) != UF2_MAGIC_END) throw new Error("invalid UF2")
        if (blkNo == 0) { if (currBlob) blobs.push(currBlob); currBlob = { pages: [], productIdentifier: familyID, pageSize: 1024 } }
        if (flags & 0x8000) parseExtTags(uf2.slice(off + 32 + payloadSize, off + 512))
        const pageSize = currBlob.pageSize || 1024
        let currPage = currBlob.pages[currBlob.pages.length - 1]
        if (!currPage || !(currPage.targetAddress <= trgaddr && trgaddr < currPage.targetAddress + pageSize)) {
            currPage = { targetAddress: trgaddr & ~(pageSize - 1), data: new Uint8Array(pageSize) }
            currPage.data.fill(0xff); currBlob.pages.push(currPage)
        }
        currPage.data.set(uf2.slice(off + 32, off + 32 + payloadSize), trgaddr - currPage.targetAddress)
    }
    if (currBlob) blobs.push(currBlob)
    return blobs
    function parseExtTags(buf) {
        let sz = 0
        for (let i = 0; i < buf.length; i += sz) {
            sz = buf[i]; if (sz == 0) break
            const desig = buf.readUInt32LE(i) >>> 8
            for (const key of Object.keys(uf2ExtTags)) {
                const tg = uf2ExtTags[key]
                if (desig == Math.abs(tg)) { currBlob[key] = tg < 0 ? buf.slice(i + 4, i + sz).toString() : buf.readUInt32LE(i + 4); break }
            }
            sz = (sz + 3) & ~3
        }
    }
}
const fn = process.argv[2], buf = fs.readFileSync(fn)
try {
    const blobs = parseUF2(buf), b = blobs[0]
    const addrs = b.pages.map(p => (p.targetAddress - 0x8000000).toString(16))
    console.log(`OK: ${blobs.length} blob, pageSize=${b.pageSize}, ${b.pages.length} pages, every page ${[...new Set(b.pages.map(p => p.data.length))]} B`)
    console.log(`  first pages: ${addrs.slice(0, 3).join(" ")} ... last page: ${addrs[addrs.length - 1]}`)
    console.log(`  page 0 appears ${addrs.filter(a => a == "0").length}x; app_reset (off 52) in first copy = 0x${b.pages[0].data.slice(52,56).reduce((a,v,i)=>a+v*2**(8*i),0).toString(16)}, in last = 0x${b.pages[b.pages.length-1].data.slice(52,56).reduce((a,v,i)=>a+v*2**(8*i),0).toString(16)}`)
    const first = b.pages[0], last = b.pages[b.pages.length - 1]
    const rv = (p) => p.data[52] | (p.data[53] << 8) | (p.data[54] << 16) | (p.data[55] << 24)
    if (first.targetAddress != 0x8000000 || last.targetAddress != 0x8000000 || rv(first) != 0 || rv(last) == 0) {
        console.log("BOOT BLOCK CHECK FAILS: page 0 must come first with app reset 0, and last with the real one")
        process.exit(1)
    }
} catch (e) { console.log("HOST PARSE FAILS: " + e.constructor.name + ": " + e.message); process.exit(1) }
