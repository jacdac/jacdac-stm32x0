#include "jdstm.h"

#if defined(PY32F0)
// PY32 flash. See docs/py32-flash.md; the register sequences follow Puya's own
// HAL (py32f0xx_hal_flash.c, SDK v1.5.0), which is the vendor reference.
//
//  - 128-byte pages. Programming is page-at-a-time: set PG, then write all 32
//    words in order, with PGSTRT set just before the 32nd write, which starts the
//    burn. Puya's HAL disables interrupts while the words are written.
//  - Only 32-bit accesses are legal: narrower writes hard-fault.
//  - Page erase: set PER and write any 32-bit value into the page.
//  - Before every erase or program, the timing registers must be loaded with the
//    factory calibration for the current HSI setting (24 MHz; see py32f0.h).
//
// Partial-page writes are needed (the settings store appends small records, and
// the bootloader writes an 8-byte device ID into its own page). Every word not
// being written is filled with 0xFFFFFFFF, which leaves those cells unchanged,
// because programming can only clear bits.
//
// Error handling differs from the ST branch on purpose. ST's code panics unless
// EOP is set after each operation (on G0 that needs EOPIE). Puya's HAL never
// checks EOP, so we don't rely on it: we wait for BSY, check WRPERR, and then read
// every write back and verify it.

STATIC_ASSERT(JD_FLASH_PAGE_SIZE == 128);
#define PAGE_WORDS (JD_FLASH_PAGE_SIZE / 4)

static void wait_done(void) {
    while (FLASH->SR & FLASH_SR_BSY)
        ;
    if (FLASH->SR & FLASH_SR_WRPERR)
        JD_PANIC();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR; // write 1 to clear
}

static void unlock(void) {
    while (FLASH->SR & FLASH_SR_BSY)
        ;
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }
    // Factory timing calibration for HSI = 24 MHz, before every operation, as
    // Puya's HAL does (it also survives any later change to the HSI setting).
    LL_FLASH_TIMMING_SEQUENCE_CONFIG_24M();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR;
    FLASH->CR &= ~(FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_PGSTRT);
}

static void lock(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

static void program_page(uint32_t page, const uint32_t *words) {
    __IO uint32_t *dp = (__IO uint32_t *)page;

    FLASH->CR |= FLASH_CR_PG;
    uint32_t primask = __get_PRIMASK(); // save/restore: we may be called with IRQs off
    __disable_irq();
    for (unsigned i = 0; i < PAGE_WORDS; ++i) {
        if (i == PAGE_WORDS - 1)
            FLASH->CR |= FLASH_CR_PGSTRT; // the 32nd write starts the program
        dp[i] = words[i];
    }
    __set_PRIMASK(primask);
    wait_done();
    FLASH->CR &= ~FLASH_CR_PG;
}

void flash_program(void *dst, const void *src, uint32_t len) {
    // 32-bit accesses only
    if ((((uint32_t)dst) & 3) || (((uint32_t)src) & 3) || (len & 3))
        JD_PANIC();
    if (len == 0)
        return;

    uint32_t addr = (uint32_t)dst;
    const uint32_t end = addr + len;
    const uint32_t *sp = src;
    uint32_t buf[PAGE_WORDS];

    unlock();
    while (addr < end) {
        const uint32_t page = addr & ~(JD_FLASH_PAGE_SIZE - 1);
        const __IO uint32_t *cur = (const __IO uint32_t *)page;
        const unsigned first = (addr - page) / 4;
        unsigned n = (end - addr) / 4;
        if (first + n > PAGE_WORDS)
            n = PAGE_WORDS - first;

        for (unsigned i = 0; i < PAGE_WORDS; ++i)
            buf[i] = 0xffffffff; // leaves those words unchanged
        for (unsigned i = 0; i < n; ++i) {
            // Same rule as the ST branches: only program erased words, except that
            // writing zero over anything is allowed (used to invalidate records).
            if (cur[first + i] != 0xffffffff && sp[i] != 0)
                JD_PANIC();
            buf[first + i] = sp[i];
        }

        program_page(page, buf);

        for (unsigned i = 0; i < n; ++i)
            if (cur[first + i] != sp[i])
                JD_PANIC(); // read-back verify

        addr += n * 4;
        sp += n;
    }
    lock();
}

void flash_erase(void *page_addr) {
    const uint32_t page = (uint32_t)page_addr;
    if (page & (JD_FLASH_PAGE_SIZE - 1))
        JD_PANIC();

    unlock();
    FLASH->CR |= FLASH_CR_PER;
    *(__IO uint32_t *)page = 0xff; // any 32-bit write into the page starts the erase
    wait_done();
    FLASH->CR &= ~FLASH_CR_PER;
    lock();

    for (unsigned i = 0; i < PAGE_WORDS; ++i)
        if (((const __IO uint32_t *)page)[i] != 0xffffffff)
            JD_PANIC(); // verify erased
}

void flash_sync() {
    // do nothing
}

#else // !PY32F0: ST implementations

#if defined(STM32G0) || defined(STM32C0) || defined(STM32L)
#define NEW_FLASH 1
#else
#define NEW_FLASH 0
#endif

#if defined(STM32G0) || defined(STM32C0)
#define FLASH_SR_BSY FLASH_SR_BSY1
#endif

#define WAIT_BUSY()                                                                                \
    while (FLASH->SR & FLASH_SR_BSY)                                                               \
        ;

static void unlock(void) {
    WAIT_BUSY();
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }
    FLASH->SR = FLASH_SR_EOP; // write to clear
    FLASH->CR &= ~(FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_STRT);
#if NEW_FLASH
    // this is required for the EOP/error bit to be set
    FLASH->CR |= FLASH_CR_EOPIE | FLASH_CR_ERRIE;
#endif
}

static void lock(void) {
    // FLASH->CR &= ~(FLASH_CR_PG | FLASH_CR_PER);
    FLASH->CR |= FLASH_CR_LOCK;
}

static void check_eop(void) {
    WAIT_BUSY();
    // EOP should be set when not busy anymore
    if (FLASH->SR & FLASH_SR_EOP)
        FLASH->SR = FLASH_SR_EOP; // OK, write to clear
    else
        JD_PANIC(); // otherwise, whoops!
}

void flash_program(void *dst, const void *src, uint32_t len) {
#if NEW_FLASH
    // G0/WL flash requires 64 bit writes
    if ((((uint32_t)dst) & 7) || (((uint32_t)src) & 3) || (len & 7))
        JD_PANIC();
#else
    // F0 flash can handle 16 bit writes
    if ((((uint32_t)dst) & 1) || (((uint32_t)src) & 1) || (len & 1))
        JD_PANIC();
#endif

    unlock();
    FLASH->CR |= FLASH_CR_PG; // enable programming

#if NEW_FLASH
    len >>= 3;
    __IO uint32_t *dp = dst;
    const uint32_t *sp = src;
    while (len--) {
        // check if dp[0/1] == 0xffffffff
        int erased = dp[0] + 1 == 0 && dp[1] + 1 == 0;
        if (!erased && (sp[0] || sp[1]))
            JD_PANIC();

        WAIT_BUSY();
        dp[0] = sp[0];
        dp[1] = sp[1];
        check_eop();
        dp += 2;
        sp += 2;
    }
#else
    len >>= 1;
    while (len--) {
        *(__IO uint16_t *)(dst) = *(uint16_t *)src;
        check_eop();
        dst = (uint16_t *)dst + 1;
        src = (uint16_t *)src + 1;
    }
#endif

    lock();
}

void flash_erase(void *page_addr) {
    unlock();
#if NEW_FLASH
    uint32_t addrmask = (((uint32_t)page_addr >> 11) << FLASH_CR_PNB_Pos) & FLASH_CR_PNB;
    FLASH->CR = (FLASH->CR & ~FLASH_CR_PNB) | FLASH_CR_PER | addrmask;
#else
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = (uint32_t)page_addr;
#endif
    FLASH->CR |= FLASH_CR_STRT;
    check_eop();
    lock();
}

void flash_sync() {
    // do nothing
}

#endif // !PY32F0
