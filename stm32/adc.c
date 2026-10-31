#include "jdstm.h"

#ifndef STM32L4

static bool adc_calibrated;

#ifdef STM32WL
#define ADC1 ADC
#endif

#if defined(STM32G0) || defined(STM32C0) || defined(STM32L)
#define NEW_ADC 1
#else
#define NEW_ADC 0
#endif

#if NEW_ADC
static bool configured_fixed = false;
#endif

#if defined(PY32F0)
// PY32's ADC is the F0 design (CHSELR sequencer, so NEW_ADC == 0) minus two
// features: there is no ADRDY flag, and no ADDIS (disable takes effect at once).
// Puya's HAL enables the ADC and then waits ADC_STAB_DELAY_US (1 us) instead of
// polling ADRDY; the shim below does the same, so the shared code is unchanged.
// Calibration (ADCAL with the ADC disabled) is the same as F0's.
#define LL_ADC_IsDisableOngoing(adc) 0
static inline uint32_t py32_adc_wait_ready(void) {
    target_wait_us(1); // ADC_STAB_DELAY_US in Puya's HAL
    return 1;
}
#define LL_ADC_IsActiveFlag_ADRDY(adc) py32_adc_wait_ready()
// PY32's shortest sampling time is 3.5 cycles (ST's is 1.5). Only used to seed the
// RNG from temperature-sensor noise, where "shortest available" is what matters.
#define LL_ADC_SAMPLINGTIME_1CYCLE_5 LL_ADC_SAMPLINGTIME_3CYCLES_5
// ADC clock: PCLK/4 on every PY32 part.
//  - F030 (48 MHz): 12 MHz, the datasheet MAXIMUM for VCC 2.3-5.5 V (V2.8,
//    table 5-25; 6 MHz below 2.3 V). The ST default of PCLK/2 would be 24 MHz,
//    twice the limit.
//  - F003/F002A (24 MHz): 6 MHz. PCLK/2 would be within the limit, but the F003
//    needs >= 20 us of sampling for the temperature sensor (V2.7, table 5-24; the
//    F030 needs 15 us), and the longest sampling time, 239.5 cycles, is only
//    19.96 us at 12 MHz. At 6 MHz it's 40 us.
#define ADC_CLOCK_CFG LL_ADC_CLOCK_SYNC_PCLK_DIV4
// Temperature sensor start-up: 70 us typical, 120 us max (datasheet table 5-27).
// Puya's LL constant is only 10 us, so readings could be taken before the sensor
// settles. Use the datasheet figure.
#undef LL_ADC_DELAY_TEMPSENSOR_STAB_US
#define LL_ADC_DELAY_TEMPSENSOR_STAB_US 120U
// Puya's LL_ADC_Disable() does a dummy start/stop conversion inside a bare
// __disable_irq()/__enable_irq(). PRIMASK is one bit, not a counter, so a caller
// that had interrupts off gets them turned back on mid-critical-section (ST's
// version never touches PRIMASK). This is the same sequence, copied from
// py32f0xx_ll_adc.h (SDK v1.5.0, BSD-3), with the caller's state restored instead.
static inline ErrorStatus py32_adc_disable(ADC_TypeDef *ADCx) {
    ErrorStatus status = SUCCESS;
    uint32_t tmpCFGR1 = ADCx->CFGR1;
    __IO uint32_t timeout_cpu_cycles = 0U;

    if (LL_ADC_IsEnabled(ADCx) == 1U) {
        LL_ADC_REG_SetTriggerSource(ADCx, LL_ADC_REG_TRIG_SOFTWARE);
        if (LL_ADC_REG_IsConversionOngoing(ADCx) != 0U) {
            if (LL_ADC_REG_IsStopConversionOngoing(ADCx) == 0U)
                LL_ADC_REG_StopConversion(ADCx);
        } else {
            ADCx->CFGR1 |= (0x7 << 6);
            ADCx->CFGR1 |= (0x3 << 10);
            uint32_t primask = __get_PRIMASK();
            __disable_irq();
            LL_ADC_REG_StartConversion(ADCx);
            LL_ADC_REG_StopConversion(ADCx);
            __set_PRIMASK(primask);
        }
        timeout_cpu_cycles = LL_ADC_TIMEOUT_STOP_CONVERSION_CPU_CYCLES;
        while (LL_ADC_REG_IsStopConversionOngoing(ADCx) == 1U)
            if (timeout_cpu_cycles-- == 0U)
                status = ERROR;
        timeout_cpu_cycles = LL_ADC_TIMEOUT_DISABLE_CPU_CYCLES;
        while (LL_ADC_IsEnabled(ADCx) == 1U)
            if (timeout_cpu_cycles-- == 0U)
                status = ERROR;
        ADCx->CFGR1 = tmpCFGR1;
    }
    return status;
}
#define LL_ADC_Disable(adc) py32_adc_disable(adc)
// PY32 CLEARS ADEN BY HARDWARE at the end of every software-triggered single
// conversion, and ignores ADSTART while ADEN is 0, even when both are written
// together. ST's ADEN stays set until ADDIS. Found on silicon (PY32F030K28U6,
// 2026-10-04): the bootloader's 1000-conversion seed loop got one conversion and
// then waited for EOC forever, with ADC_CR reading 0. Writing CR by hand over SWD
// confirmed it (docs/py32-porting.md). So every conversion re-enables first.
static inline void py32_adc_ensure_enabled(void) {
    if (!LL_ADC_IsEnabled(ADC1)) {
        LL_ADC_Enable(ADC1);
        while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0) // the 1 us stabilisation delay
            ;
    }
}
#else
#define ADC_CLOCK_CFG LL_ADC_CLOCK_SYNC_PCLK_DIV2
#endif

uint16_t adc_convert(void) {
#if defined(PY32F0)
    py32_adc_ensure_enabled();
#endif
    if ((LL_ADC_IsEnabled(ADC1) == 1) && (LL_ADC_IsDisableOngoing(ADC1) == 0) &&
        (LL_ADC_REG_IsConversionOngoing(ADC1) == 0)) {
        LL_ADC_REG_StartConversion(ADC1);
    } else
        JD_PANIC();

#if STM32L
    while (LL_ADC_IsActiveFlag_EOS(ADC1) == 0)
        ;
    LL_ADC_ClearFlag_EOS(ADC1);
#else
    while (LL_ADC_IsActiveFlag_EOC(ADC1) == 0)
        ;
    LL_ADC_ClearFlag_EOC(ADC1);
#endif

    return LL_ADC_REG_ReadConversionData12(ADC1) << 4;
}

static void set_channel(uint32_t chan) {
    // might be from previous disable
    while (LL_ADC_IsDisableOngoing(ADC1))
        ;

#if NEW_ADC
    if (chan == LL_ADC_CHANNEL_15 || chan == LL_ADC_CHANNEL_16 || chan == LL_ADC_CHANNEL_17) {
        if (!configured_fixed) {
            LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_FIXED);
            configured_fixed = true;
        }
        // one rank (DISABLE is equivalent to one rank)
        LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
        LL_ADC_REG_SetSequencerScanDirection(ADC1, LL_ADC_REG_SEQ_SCAN_DIR_BACKWARD);
        LL_ADC_REG_SetSequencerChannels(ADC1, chan);
        LL_ADC_REG_SetSequencerDiscont(ADC1, LL_ADC_REG_SEQ_DISCONT_1RANK);
        while (LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0)
            ;
    } else {
        if (configured_fixed) {
            LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
            while (LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0)
                ;
            configured_fixed = false;
        }
        LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, chan);
    }

    LL_ADC_SetChannelSamplingTime(ADC1, chan, LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_EnableInternalRegulator(ADC1);
    target_wait_us(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
#else
    LL_ADC_REG_SetSequencerChannels(ADC1, chan);
#endif

    if (!adc_calibrated) {
        adc_calibrated = 1;
        LL_ADC_StartCalibration(ADC1);

        while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0)
            ;
        target_wait_us(5); // ADC_DELAY_CALIB_ENABLE_CPU_CYCLES
        DMESG("ADC calib: %x", (unsigned)ADC1->DR);
    }

    // LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_AUTOWAIT_AUTOPOWEROFF);

    LL_ADC_Enable(ADC1);

    while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0)
        ;
}

static void set_sampling_time(uint32_t time, uint32_t chan) {
#ifdef STM32L4
    LL_ADC_SetChannelSamplingTime(ADC1, chan, time);
#elif NEW_ADC
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, time);
#else
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, time);
#endif
    set_channel(chan);
}

static void set_temp_ref(int t) {
    while (LL_ADC_IsDisableOngoing(ADC1))
        ;
    LL_ADC_SetCommonPathInternalCh(
        __LL_ADC_COMMON_INSTANCE(ADC1),
        t ? (LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR)
          : LL_ADC_PATH_INTERNAL_NONE);
    if (t)
        target_wait_us(LL_ADC_DELAY_TEMPSENSOR_STAB_US);
}

// 116 bytes
uint32_t bl_adc_random_seed(void) {
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
                                   LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR);
    target_wait_us(LL_ADC_DELAY_TEMPSENSOR_STAB_US);

    ADC1->CFGR1 = LL_ADC_REG_OVR_DATA_OVERWRITTEN;

#if NEW_ADC
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    target_wait_us(5);

    LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
#endif

    ADC1->CFGR2 = ADC_CLOCK_CFG;

#if NEW_ADC
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR, LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_EnableInternalRegulator(ADC1);
    target_wait_us(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
#else
    LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_TEMPSENSOR);
#endif

    // LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_AUTOWAIT_AUTOPOWEROFF);

    LL_ADC_Enable(ADC1);
    while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0)
        ;

    uint32_t h = 231;
    for (int i = 0; i < 1000; ++i) {
#if defined(PY32F0)
        py32_adc_ensure_enabled();
#endif
        LL_ADC_REG_StartConversion(ADC1);
        while (LL_ADC_IsActiveFlag_EOC(ADC1) == 0)
            ;
        LL_ADC_ClearFlag_EOC(ADC1);
        int v = LL_ADC_REG_ReadConversionData12(ADC1);
        h = (h * 0x1000193) ^ (v & 0xff);
    }

    return h;
}

// initializes RNG from TEMP sensor
// alternative would be build an RNG, eg http://robseward.com/misc/RNG2/
void adc_init_random(void) {
    __HAL_RCC_ADC_CLK_ENABLE();

    set_temp_ref(1);
    ADC1->CFGR1 = LL_ADC_REG_OVR_DATA_OVERWRITTEN;
#ifdef STM32L
    ADC1->CFGR2 = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
#endif

#if NEW_ADC
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    target_wait_us(5);

    LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
#endif

    ADC1->CFGR2 = ADC_CLOCK_CFG;

    set_sampling_time(LL_ADC_SAMPLINGTIME_1CYCLE_5, LL_ADC_CHANNEL_TEMPSENSOR); // get maximum noise

    uint32_t h = 0x811c9dc5;
    for (int i = 0; i < 1000; ++i) {
        int v = adc_convert() >> 4;
        h = (h * 0x1000193) ^ (v & 0xff);
    }
    jd_seed_random(h);

    // ADC enabled is like 200uA in STOP
    LL_ADC_Disable(ADC1);
    while (LL_ADC_IsDisableOngoing(ADC1))
        ;

    // this brings STOP draw to 4.4uA from 46uA
    set_temp_ref(0);
}

#define NO_CHANNEL 0xffffffff

static const uint32_t channels_PA[] = {
    LL_ADC_CHANNEL_0, LL_ADC_CHANNEL_1,  LL_ADC_CHANNEL_2,  LL_ADC_CHANNEL_3, LL_ADC_CHANNEL_4,
    LL_ADC_CHANNEL_5, LL_ADC_CHANNEL_6,  LL_ADC_CHANNEL_7,  NO_CHANNEL,       NO_CHANNEL,
#if defined(PY32F0)
    // PY32 has no ADC input on PA11/PA12 (G0's ADC_IN15/16). PA0-PA7 are
    // ADC_IN0-7 and PB0/PB1 are ADC_IN8/9, as below; ADC_IN11/12 are internal
    // (temperature sensor / VREFINT).
    NO_CHANNEL,       NO_CHANNEL,        NO_CHANNEL,
#else
    NO_CHANNEL,       LL_ADC_CHANNEL_15, LL_ADC_CHANNEL_16,
#endif
};

static const uint32_t channels_PB[] = {
    LL_ADC_CHANNEL_8,
    LL_ADC_CHANNEL_9,
};

#ifdef STM32F0
#define TS_CAL1 *(uint16_t *)0x1FFFF7B8
#if defined(STM32F031x6) || defined(STM32F042x6)
// not present on F030
#define TS_CAL2 *(uint16_t *)0x1FFFF7C2
#endif
#endif

#if STM32G0
#define TS_CAL1 *(uint16_t *)0x1FFF75A8 // @30C
#if STM32G031xx
#define TS_CAL2 *(uint16_t *)0x1FFF75CA // @130C (not defined on G030)
#endif
#endif

int32_t adc_read_temp(void) {
#ifdef STM32L4
    set_sampling_time(LL_ADC_SAMPLINGTIME_247CYCLES_5, LL_ADC_CHANNEL_TEMPSENSOR);
#elif defined(PY32F0)
    // 239.5 cycles: 20 us at 12 MHz on the F030 (datasheet minimum 15 us), 40 us
    // at 6 MHz on the F003 (minimum 20 us). See ADC_CLOCK_CFG.
    set_sampling_time(LL_ADC_SAMPLINGTIME_239CYCLES_5, LL_ADC_CHANNEL_TEMPSENSOR);
#elif NEW_ADC
    set_sampling_time(LL_ADC_SAMPLINGTIME_160CYCLES_5, LL_ADC_CHANNEL_TEMPSENSOR);
#else
    set_sampling_time(LL_ADC_SAMPLINGTIME_71CYCLES_5,
                      LL_ADC_CHANNEL_TEMPSENSOR); // min. sampling time for temp is 4us
#endif
    set_temp_ref(1);

    uint16_t r = adc_convert() >> 4;

    LL_ADC_Disable(ADC1);
    set_temp_ref(0);

#if defined(STM32F030x6) || defined(STM32F030x8)
    return ((TS_CAL1 - r) * 1000) / 5336 + 30;
#elif defined(STM32F031x6) || defined(STM32F042x6)
    return ((110 - 30) * (r - TS_CAL1)) / (TS_CAL2 - TS_CAL1) + 30;
#elif defined(STM32G031xx)
    return __LL_ADC_CALC_TEMPERATURE(3300, r, LL_ADC_RESOLUTION_12B);
    // return ((130 - 30) * 33 * (r - TS_CAL1)) / (30 * TS_CAL2 - TS_CAL1) + 30;
#elif defined(STM32G0)
    // 310 = 2.5mV/C * 100C * (1<<12) / 3300mV
    // 30/33 is because the TS_CAL1 is taken at 3.0V VCC and we run at 3.3V
    return ((130 - 30) * (r - (TS_CAL1 * 30 / 33))) / 310 + 30;
#elif defined(STM32L)
    return __LL_ADC_CALC_TEMPERATURE(3300, r, LL_ADC_RESOLUTION_12B);
#elif defined(PY32F0)
    // PY32 has factory calibration (TS_CAL1 @ 30 C at 0x1FFF0F14, TS_CAL2 @ 85 C at
    // 0x1FFF0F18, both at Vref+ 3.3 V), and Puya's LL macro uses it. We run at 3.3 V.
    return __LL_ADC_CALC_TEMPERATURE(3300, r, LL_ADC_RESOLUTION_12B);
#elif defined(STM32C0)
    // ST disabled __LL_ADC_CALC_TEMPERATURE() on C0 (see stm32c0xx_ll_adc.h); a
    // real reading needs __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS() with datasheet
    // avg-slope params. Return a stub until a C0 board needs on-die temperature.
    (void)r;
    return 25;
#else
#error "no temp"
#endif
}

static uint32_t pin_channel(uint8_t pin) {
    if (pin >> 4 == 0) {
        if ((pin & 0xf) >= sizeof(channels_PA) / sizeof(channels_PA[0]))
            return NO_CHANNEL;
        return channels_PA[pin & 0xf];
    } else if (pin >> 4 == 1) {
        if ((pin & 0xf) >= sizeof(channels_PB) / sizeof(channels_PB[0]))
            return NO_CHANNEL;
        return channels_PB[pin & 0xf];
    } else {
        return NO_CHANNEL;
    }
}

bool adc_can_read_pin(uint8_t pin) {
    return pin_channel(pin) != NO_CHANNEL;
}

void adc_prep_read_pin(uint8_t pin) {
    uint32_t chan = pin_channel(pin);

    if (chan == NO_CHANNEL)
        JD_PANIC();

    pin_setup_analog_input(pin);

#if NEW_ADC
    set_sampling_time(LL_ADC_SAMPLINGTIME_39CYCLES_5, chan);
#else
    set_sampling_time(LL_ADC_SAMPLINGTIME_41CYCLES_5, chan);
#endif
}

void adc_disable() {
    LL_ADC_Disable(ADC1);
}

uint16_t adc_read_pin(uint8_t pin) {
    adc_prep_read_pin(pin);
    uint16_t r = adc_convert();
    adc_disable();
    return r;
}

#endif