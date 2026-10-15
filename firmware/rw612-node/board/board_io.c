/**
 * @file board_io.c
 * @brief SDK wrappers for MP1. The only translation unit that includes fsl_*.h.
 *
 * Each block cites the SDK example it follows (paths relative to
 * C:\nxp\SDK_RW612\boards\frdmrw612\) and the API header in
 * C:\nxp\SDK_RW612\devices\RW612\drivers\.
 */
#include "board_io.h"

#include <stdarg.h>
#include <stddef.h>

#include "fsl_adc.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_ctimer.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_io_mux.h"
#include "fsl_power.h"
#include "fsl_reset.h"

#include "app_config.h" /* BUZZER_KIND, APP_ENABLE_SHIFT_LIGHT */
#include "board_pins.h"

/* From the imported hello_world project (demo_apps/hello_world/hardware_init.c):
 * BOARD_InitBootPins() + BOARD_InitBootClocks() + BOARD_InitDebugConsole(). */
void BOARD_InitHardware(void);

#define BOARD_ADC_TIMEOUT_US 5000u /* single conversion with 16x averaging; generous bound */
#define BOARD_US_PER_S       1000000u

typedef struct
{
    uint32_t port;
    uint32_t pin;
} gpio_line_t;

static gpio_line_t s_gpio[BOARD_IO_COUNT];
static uint8_t s_shift_count;

/* Software extension of the 32-bit CTIMER counter (see time_us64()). */
static uint32_t s_last_tc;
static uint32_t s_tc_wraps;

/* ------------------------------------------------------------------------- */
/* GPIO — driver_examples/gpio/led_output/gpio_led_output.c:61-79            */
/* API: fsl_gpio.h -> GPIO_PortInit, GPIO_PinInit, GPIO_PinWrite, GPIO_PinRead */
/* ------------------------------------------------------------------------- */

static bool io_valid(board_io_t io)
{
    if ((unsigned)io >= (unsigned)BOARD_IO_COUNT)
    {
        return false;
    }
    if ((unsigned)io >= (unsigned)BOARD_IO_SHIFT_0)
    {
        return ((unsigned)io - (unsigned)BOARD_IO_SHIFT_0) < s_shift_count;
    }
    return true;
}

void board_gpio_set_output(board_io_t io, bool level)
{
    if (!io_valid(io))
    {
        return;
    }
    /* GPIO_PinInit() sets the output level before switching the direction
     * (fsl_gpio.c, GPIO_PinInit), so the line never glitches. */
    gpio_pin_config_t cfg = {kGPIO_DigitalOutput, level ? 1U : 0U};
    GPIO_PinInit(GPIO, s_gpio[io].port, s_gpio[io].pin, &cfg);
}

void board_gpio_set_input(board_io_t io)
{
    if (!io_valid(io))
    {
        return;
    }
    gpio_pin_config_t cfg = {kGPIO_DigitalInput, 0U};
    GPIO_PinInit(GPIO, s_gpio[io].port, s_gpio[io].pin, &cfg);
}

void board_gpio_write(board_io_t io, bool level)
{
    if (!io_valid(io))
    {
        return;
    }
    GPIO_PinWrite(GPIO, s_gpio[io].port, s_gpio[io].pin, level ? 1U : 0U);
}

bool board_gpio_read(board_io_t io)
{
    if (!io_valid(io))
    {
        return false;
    }
    return GPIO_PinRead(GPIO, s_gpio[io].port, s_gpio[io].pin) != 0U;
}

bool board_led_active_high(void)
{
    return BOARD_LED_ACTIVE_HIGH != 0U;
}

uint8_t board_shift_led_count(void)
{
    return s_shift_count;
}

static void gpio_table_init(void)
{
    s_gpio[BOARD_IO_LED_GREEN]   = (gpio_line_t){BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_PIN};
    s_gpio[BOARD_IO_LED_YELLOW]  = (gpio_line_t){BOARD_LED_YELLOW_PORT, BOARD_LED_YELLOW_PIN};
    s_gpio[BOARD_IO_LED_RED]     = (gpio_line_t){BOARD_LED_RED_PORT, BOARD_LED_RED_PIN};
    s_gpio[BOARD_IO_BUZZER]      = (gpio_line_t){BOARD_BUZZER_PORT, BOARD_BUZZER_PIN};
    s_gpio[BOARD_IO_HCSR04_TRIG] = (gpio_line_t){BOARD_HCSR04_TRIG_PORT, BOARD_HCSR04_TRIG_PIN};
    s_gpio[BOARD_IO_HCSR04_ECHO] = (gpio_line_t){BOARD_HCSR04_ECHO_PORT, BOARD_HCSR04_ECHO_PIN};
    s_gpio[BOARD_IO_DHT11]       = (gpio_line_t){BOARD_DHT11_PORT, BOARD_DHT11_PIN};

    s_shift_count = 0u;
#if APP_ENABLE_SHIFT_LIGHT
    {
        static const uint32_t ports[] = BOARD_SHIFT_LED_PORTS;
        static const uint32_t pins[]  = BOARD_SHIFT_LED_PINS;
        uint32_t n                    = BOARD_SHIFT_LED_COUNT;
        if (n > BOARD_SHIFT_LED_MAX)
        {
            n = BOARD_SHIFT_LED_MAX;
        }
        for (uint32_t i = 0u; i < n; i++)
        {
            s_gpio[BOARD_IO_SHIFT_0 + i] = (gpio_line_t){ports[i], pins[i]};
        }
        s_shift_count = (uint8_t)n;
    }
#endif
}

/* ------------------------------------------------------------------------- */
/* Pin mux — driver_examples/gpio/led_output/pin_mux.c:61-78 and             */
/* driver_examples/ctimer/simple_pwm/pin_mux.c:64 (IO_MUX_CT0_MAT0_OUT)       */
/* API: fsl_io_mux.h -> IO_MUX_SetPinMux                                      */
/* ------------------------------------------------------------------------- */

static void pins_mux_init(void)
{
    IO_MUX_SetPinMux(BOARD_LED_GREEN_MUX);
    IO_MUX_SetPinMux(BOARD_LED_YELLOW_MUX);
    IO_MUX_SetPinMux(BOARD_LED_RED_MUX);
    IO_MUX_SetPinMux(BOARD_HCSR04_TRIG_MUX);
    IO_MUX_SetPinMux(BOARD_HCSR04_ECHO_MUX);
    IO_MUX_SetPinMux(BOARD_DHT11_MUX);
#if BUZZER_KIND == BUZZER_KIND_PASSIVE
    IO_MUX_SetPinMux(BOARD_BUZZER_PWM_MUX);
#else
    IO_MUX_SetPinMux(BOARD_BUZZER_GPIO_MUX);
#endif
#if APP_ENABLE_SHIFT_LIGHT
    BOARD_SHIFT_LED_INIT_MUX();
#endif
    /* TODO(tutor): do the ADC input pins (BOARD_ADC_CH_BRAKE / _TPS) need an
     * IO_MUX_SetPinMux()/IO_MUX_SetPinConfig() call to work as analog inputs?
     * driver_examples/adc/software_trigger/pin_mux.c muxes only the UART, and
     * fsl_io_mux.h defines no analog/ADC function for GPIO_46 (only IO_MUX_GPIO46,
     * a digital GPIO, and IO_MUX_GAU_TRIGGER0/1). Left untouched: GPIO_46 is not muxed. */
    /* TODO(tutor): does the DHT11 data line need IO_MUX_SetPinConfig(pin,
     * IO_MUX_PinConfigPullUp...) or is the module's on-board pull-up enough? */
}

/* ------------------------------------------------------------------------- */
/* ADC — driver_examples/adc/software_trigger/adc_software_trigger.c:55-104  */
/* and its hardware_init.c (GAU clock, reset, power).                        */
/* API: fsl_adc.h -> ADC_GetDefaultConfig, ADC_Init, ADC_DoAutoCalibration,   */
/*      ADC_SetScanChannel, ADC_DoSoftwareTrigger, ADC_GetStatusFlags,        */
/*      ADC_GetConversionResult, ADC_ClearStatusFlags, ADC_StopConversion     */
/* ------------------------------------------------------------------------- */

static bool s_adc_ok;

static void adc_init(void)
{
    CLOCK_AttachClk(kT3PLL_MCI_256M_to_GAU_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivGauClk, 4U);
    CLOCK_EnableClock(kCLOCK_Gau);
    RESET_PeripheralReset(kGAU_RST_SHIFT_RSTn);
    POWER_PowerOnGau();

    adc_config_t cfg;
    ADC_GetDefaultConfig(&cfg);
    /* Same settings as the SDK example. Input range 0..1.2 V (example banner):
     * the potentiometer wiring must keep the pin inside it — HANDOFF-002. */
    cfg.vrefSource            = kADC_Vref1P2V;
    cfg.inputMode             = kADC_InputSingleEnded;
    cfg.conversionMode        = kADC_ConversionOneShot;
    cfg.inputGain             = kADC_InputGain1;
    cfg.resolution            = kADC_Resolution16Bit;
    cfg.fifoThreshold         = kADC_FifoThresholdData1;
    cfg.averageLength         = kADC_Average16;
    cfg.enableInputGainBuffer = false;
    cfg.enableInputBufferChop = false;
    cfg.enableADC             = true;
    ADC_Init(BOARD_ADC_BASE, &cfg);

    s_adc_ok = (ADC_DoAutoCalibration(BOARD_ADC_BASE, kADC_CalibrationVrefInternal) == kStatus_Success);
    ADC_ClearStatusFlags(BOARD_ADC_BASE, kADC_DataReadyRawFlag | kADC_DataReadyInterruptFlag);
}

#define BOARD_ADC_SOURCE_NONE 0xFFFFFFFFu /* no scan channel programmed yet */
#define BOARD_ADC_FLUSH_MAX   64u         /* bound for draining stale FIFO entries */

static uint32_t s_adc_source = BOARD_ADC_SOURCE_NONE;
static uint32_t s_adc_last_raw32;
static uint8_t s_adc_last_fifo;

/* Flow of adc_software_trigger.c:84-104: scan channel set once, then per sample
 * DoSoftwareTrigger -> wait for data -> GetConversionResult -> StopConversion. */
static bool adc_convert(adc_channel_source_t source, int32_t *raw)
{
    if (!s_adc_ok || (raw == NULL))
    {
        return false;
    }

    /* The example programs the scan channel once; do it again only when the channel changes. */
    if ((uint32_t)source != s_adc_source)
    {
        ADC_SetScanChannel(BOARD_ADC_BASE, kADC_ScanChannel0, source);
        s_adc_source = (uint32_t)source;
    }

    /* Drop stale results (calibration, previous channel) so the read below is this conversion's.
     * TODO(tutor): confirm that reading ADC_REG_RESULT pops one FIFO entry (RM00278 22.1.16). */
    for (uint32_t i = 0u; (i < BOARD_ADC_FLUSH_MAX) && (ADC_GetFifoDataCount(BOARD_ADC_BASE) > 0U); i++)
    {
        (void)ADC_GetConversionResult(BOARD_ADC_BASE);
    }
    ADC_ClearStatusFlags(BOARD_ADC_BASE, kADC_DataReadyRawFlag | kADC_DataReadyInterruptFlag);

    ADC_DoSoftwareTrigger(BOARD_ADC_BASE);

    /* Wait until the FIFO holds data (fsl_adc.h -> ADC_GetFifoDataCount), not only the raw flag. */
    uint32_t t0 = board_time_us();
    uint8_t fifo;
    while ((fifo = ADC_GetFifoDataCount(BOARD_ADC_BASE)) == 0U)
    {
        if ((board_time_us() - t0) > BOARD_ADC_TIMEOUT_US)
        {
            ADC_StopConversion(BOARD_ADC_BASE);
            s_adc_last_fifo  = 0u;
            s_adc_last_raw32 = 0u;
            return false;
        }
    }

    uint32_t raw32 = ADC_GetConversionResult(BOARD_ADC_BASE);
    ADC_ClearStatusFlags(BOARD_ADC_BASE, kADC_DataReadyRawFlag | kADC_DataReadyInterruptFlag);
    ADC_StopConversion(BOARD_ADC_BASE);

    s_adc_last_fifo  = fifo;
    s_adc_last_raw32 = raw32;

    /* TODO(tutor): format of ADC_GetConversionResult() with kADC_ResultWidth16
     * (default) and kADC_Resolution16Bit single-ended: is it a signed 16-bit value,
     * and can two samples be packed in the 32-bit word? The example readme gives
     * "Conversion Result = (Vin / Vref) * 32767". Taken as a signed 16-bit sample;
     * the log shows the full word (adc32) to settle it on the bench. */
    *raw = (int32_t)(int16_t)(raw32 & 0xFFFFU);
    return true;
}

void board_adc_last_diag(uint32_t *raw32, uint8_t *fifo_count)
{
    if (raw32 != NULL)
    {
        *raw32 = s_adc_last_raw32;
    }
    if (fifo_count != NULL)
    {
        *fifo_count = s_adc_last_fifo;
    }
}

bool board_adc_read_raw(board_adc_t ch, int32_t *raw)
{
    switch (ch)
    {
        case BOARD_ADC_BRAKE:
            return adc_convert(BOARD_ADC_CH_BRAKE, raw);
        case BOARD_ADC_TPS:
#if APP_ENABLE_SHIFT_LIGHT
            return adc_convert(BOARD_ADC_CH_TPS, raw);
#else
            return false; /* TPS channel TBD (OPEN-014); not needed without the shift light */
#endif
        default:
            return false;
    }
}

bool board_adc_read_channel_raw(uint8_t channel, int32_t *raw)
{
    /* Single-ended channels 0..7 vs VSSA map 1:1 onto adc_channel_source_t (fsl_adc.h). */
    if (channel >= BOARD_ADC_SCAN_CHANNELS)
    {
        return false;
    }
    return adc_convert((adc_channel_source_t)channel, raw);
}

/* ------------------------------------------------------------------------- */
/* Timebase — driver_examples/ctimer/simple_match/simple_match.c +           */
/* hardware_init.c ("Use 16 MHz clock for the Ctimer0": kSFRO_to_CTIMER0).   */
/* API: fsl_ctimer.h -> CTIMER_GetDefaultConfig, CTIMER_Init,                */
/*      CTIMER_StartTimer, CTIMER_GetTimerCountValue; fsl_clock.h ->          */
/*      CLOCK_AttachClk, CLOCK_GetCTimerClkFreq                               */
/* ------------------------------------------------------------------------- */

static uint32_t s_timebase_hz;
static uint32_t s_timebase_prescale;

static void timebase_init(void)
{
    /* kSFRO_to_CTIMER1 writes CLKCTL1->CT32BITFCLKSEL[1] = 1, the same register
     * CLOCK_GetCTimerClkFreq(1) decodes (fsl_clock.c, case SEL(1) -> CLOCK_GetSFroFreq()).
     * Note: CLOCK_GetSFroFreq() = CLOCK_GetT3PllMci256mClkFreq() / 16, and that function
     * returns a constant 256 MHz, so this value is not a measurement: use
     * board_timebase_measure() to check the real rate. */
    CLOCK_AttachClk(BOARD_TIMEBASE_CLK_ATTACH);
    s_timebase_hz = CLOCK_GetCTimerClkFreq(BOARD_TIMEBASE_CTIMER_ID);

    ctimer_config_t cfg;
    CTIMER_GetDefaultConfig(&cfg);
    /* TC increments every (prescale + 1) clocks (simple_pwm.c:68). Guard the
     * subtraction: a clock below 1 MHz would wrap it to 0xFFFFFFFF. */
    s_timebase_prescale = (s_timebase_hz >= BOARD_US_PER_S) ? ((s_timebase_hz / BOARD_US_PER_S) - 1U) : 0U;
    cfg.prescale        = s_timebase_prescale;
    CTIMER_Init(BOARD_TIMEBASE_CTIMER, &cfg);
    CTIMER_StartTimer(BOARD_TIMEBASE_CTIMER);

    s_last_tc  = 0u;
    s_tc_wraps = 0u;
}

void board_timebase_info(uint32_t *ctimer_hz, uint32_t *prescale)
{
    if (ctimer_hz != NULL)
    {
        *ctimer_hz = s_timebase_hz;
    }
    if (prescale != NULL)
    {
        *prescale = s_timebase_prescale;
    }
}

/* Independent reference: the Cortex-M33 DWT cycle counter (CMSIS core_cm33.h ->
 * DCB->DEMCR TRCENA, DWT->CTRL CYCCNTENA) counts core clocks at SystemCoreClock. */
bool board_timebase_measure(uint32_t us, uint32_t *ctimer_ticks, uint32_t *core_cycles, uint32_t *core_hz)
{
    if ((ctimer_ticks == NULL) || (core_cycles == NULL) || (core_hz == NULL))
    {
        return false;
    }
    DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U)
    {
        return false; /* cycle counter not implemented */
    }
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t tc0 = CTIMER_GetTimerCountValue(BOARD_TIMEBASE_CTIMER);
    uint32_t cy0 = DWT->CYCCNT;
    board_delay_us(us);
    uint32_t cy1 = DWT->CYCCNT;
    uint32_t tc1 = CTIMER_GetTimerCountValue(BOARD_TIMEBASE_CTIMER);

    *ctimer_ticks = tc1 - tc0;
    *core_cycles  = cy1 - cy0;
    *core_hz      = SystemCoreClock;
    return true;
}

/* TODO(tutor): confirm the CTIMER TC wraps 0xFFFFFFFF -> 0 when no match resets it
 * (RM00278 ch. 41 Standard counter/timer). The extension below relies on it and on
 * being called at least once per wrap (~71 min at 1 MHz). */
static uint64_t time_us64(void)
{
    uint32_t tc = CTIMER_GetTimerCountValue(BOARD_TIMEBASE_CTIMER);
    if (tc < s_last_tc)
    {
        s_tc_wraps++;
    }
    s_last_tc = tc;
    return ((uint64_t)s_tc_wraps << 32) | tc;
}

uint32_t board_time_us(void)
{
    return (uint32_t)time_us64();
}

uint32_t board_time_ms(void)
{
    return (uint32_t)(time_us64() / 1000u);
}

void board_delay_us(uint32_t us)
{
    uint32_t t0 = board_time_us();
    while ((board_time_us() - t0) < us)
    {
    }
}

/* ------------------------------------------------------------------------- */
/* Passive buzzer tone — driver_examples/ctimer/simple_pwm/simple_pwm.c      */
/* API: fsl_ctimer.h -> CTIMER_SetupPwm, CTIMER_StartTimer, CTIMER_StopTimer */
/* ------------------------------------------------------------------------- */

#if BUZZER_KIND == BUZZER_KIND_PASSIVE
static bool s_tone_ready;
static uint32_t s_tone_hz;
#endif

void board_tone_start(uint32_t freq_hz)
{
#if BUZZER_KIND == BUZZER_KIND_PASSIVE
    if (!s_tone_ready || (freq_hz == 0u))
    {
        return;
    }
    if (freq_hz != s_tone_hz)
    {
        (void)CTIMER_SetupPwm(BOARD_BUZZER_PWM_CTIMER, BOARD_BUZZER_PWM_PERIOD_MATCH, BOARD_BUZZER_PWM_MATCH, 50U,
                              freq_hz, CLOCK_GetCTimerClkFreq(BOARD_BUZZER_PWM_CTIMER_ID), false);
        s_tone_hz = freq_hz;
    }
    CTIMER_StartTimer(BOARD_BUZZER_PWM_CTIMER);
#else
    (void)freq_hz;
#endif
}

void board_tone_stop(void)
{
#if BUZZER_KIND == BUZZER_KIND_PASSIVE
    if (!s_tone_ready)
    {
        return;
    }
    CTIMER_StopTimer(BOARD_BUZZER_PWM_CTIMER);
    /* TODO(tutor): after CTIMER_StopTimer() the match output keeps its last level;
     * how to force it low (EMR register / CTIMER_SetupMatch outControl) so a
     * passive buzzer does not sit with DC across it? */
#endif
}

static void tone_init(void)
{
#if BUZZER_KIND == BUZZER_KIND_PASSIVE
    CLOCK_AttachClk(BOARD_BUZZER_PWM_CLK_ATTACH);
    ctimer_config_t cfg;
    CTIMER_GetDefaultConfig(&cfg);
    CTIMER_Init(BOARD_BUZZER_PWM_CTIMER, &cfg);
    s_tone_ready = true;
    s_tone_hz    = 0u;
#endif
}

/* ------------------------------------------------------------------------- */
/* Debug console — demo_apps/hello_world/hello_world.c (PRINTF)              */
/* API: utilities/debug_console/fsl_debug_console.h -> DbgConsole_Vprintf    */
/* ------------------------------------------------------------------------- */

void board_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    (void)DbgConsole_Vprintf(fmt, args);
    va_end(args);
    (void)DbgConsole_Printf("\r\n");
}

/* ------------------------------------------------------------------------- */

void board_init_all(void)
{
    BOARD_InitHardware();

    gpio_table_init();
    pins_mux_init();

    /* Enable the clock of every GPIO port in use (GPIO_PortInit ungates the clock). */
    for (unsigned i = 0u; i < (unsigned)BOARD_IO_COUNT; i++)
    {
        if (io_valid((board_io_t)i))
        {
            GPIO_PortInit(GPIO, s_gpio[i].port);
        }
    }

    timebase_init();
    adc_init();
    tone_init();
}
