#include "main.h"
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * BQ27545-G1 mock I2C slave for JBL Xtreme 2
 *
 * Presents address 0x55 (7-bit) on I2C1 (SCL=PB6, SDA=PB7).
 * Returns "healthy battery" register values so the JBL MCU will boot
 * and run normally when the real battery/fuel gauge is absent.
 *
 * Protocol: two separate transactions per register read
 *   1. S 0x55 W [A] <reg> [A] P   — master writes register address
 *   2. S 0x55 R [A] <lo> [A] <hi> [N] P — master reads 2-byte value
 *
 * Quick Commands (S 0x55 R [A] P) are handled by the error/listen
 * callback — we ACK the address and the master sends STOP; this is
 * just a presence-check and requires no data.
 *
 * USB CDC prints every transaction and a keep-alive every 5 s.
 * ----------------------------------------------------------------------- */

USBD_HandleTypeDef hUsbDeviceFS;

/* Last register address written by the master */
static volatile uint8_t  s_reg    = 0x00;
/* Transaction counters (for USB diagnostics) */
static volatile uint32_t s_wr_cnt = 0;
static volatile uint32_t s_rd_cnt = 0;

/* I2C slave state machine (register-level, no HAL) */
typedef enum { I2CS_IDLE, I2CS_RX_REG, I2CS_TX_DATA } i2cs_t;
static volatile i2cs_t   s_i2cs   = I2CS_IDLE;
static uint8_t s_txbuf[2];
static uint8_t s_txpos;

/* -----------------------------------------------------------------------
 * Register map — returns the 16-bit little-endian value for a given
 * BQ27545 standard command address.  All values are fixed "healthy"
 * readings appropriate for a 2S 3000 mAh pack that is fully charged
 * and being maintained by the wall charger.
 * ----------------------------------------------------------------------- */
static uint16_t fg_reg_read(uint8_t reg)
{
    switch (reg) {
    case 0x00: return 0x0541;   /* Control() — device type present        */
    case 0x02: return 0x0000;   /* AtRate                                  */
    case 0x04: return 0x0000;   /* AtRateTimeToEmpty                       */
    case 0x06: return 0x0BB3;   /* Temperature: 2995 × 0.1 K = 26.35 °C  */
    case 0x08: return 0x1CE8;   /* Voltage: 7400 mV (2S mid-charge)       */
    case 0x0A: return 0x0000;   /* Flags: no fault bits set                */
    case 0x0C: return 0x0BB8;   /* NominalAvailableCapacity: 3000 mAh     */
    case 0x0E: return 0x0BB8;   /* FullAvailableCapacity: 3000 mAh        */
    case 0x10: return 0x0000;   /* RemainingCapacity: not used             */
    case 0x12: return 0x0BB8;   /* FullChargeCapacity: 3000 mAh           */
    case 0x14: return 0x0000;   /* AverageCurrent: 0 mA (plugged-in)      */
    case 0x16: return 0xFFFF;   /* TimeToEmpty: 65535 (infinite — on AC)  */
    case 0x18: return 0xFFFF;   /* TimeToFull:  65535 (already full)      */
    case 0x1A: return 0x0000;   /* StandbyCurrent                         */
    case 0x1C: return 0x0BB8;   /* StandbyTimeToEmpty: 3000 (large)       */
    case 0x1E: return 0x0000;   /* MaxLoadCurrent                         */
    case 0x20: return 0x0000;   /* MaxLoadTimeToEmpty                      */
    case 0x22: return 0x0000;   /* RawCoulombCount                        */
    case 0x28: return 0x0064;   /* AveragePower (positive = charging OK)  */
    case 0x2A: return 0x0005;   /* CycleCount: 5 (low = healthy)          */
    case 0x2C: return 0x0064;   /* StateOfCharge: 100 %                   */
    case 0x2E: return 0x0064;   /* StateOfHealth: 100 %  ← key register   */
    default:   return 0x0000;
    }
}

/* -----------------------------------------------------------------------
 * I2C1 slave — direct register implementation, no HAL I2C state machine.
 *
 * The HAL LISTEN-mode state machine breaks on a busy shared bus because
 * STOPF fires for every transaction (other devices too), and repeated
 * ListenCpltCallback → EnableListen_IT round-trips drift out of READY.
 * Direct register access gives us a simple, explicit state machine.
 *
 * Protocol handled:
 *   WRITE  S+0x55W + reg_byte + P   → stores reg address
 *   READ   S+0x55R + 2 bytes + N+P  → sends reg value (little-endian)
 *   QUICK  S+0x55R/W + P            → ACK+STOP, no data (presence check)
 *
 * STOPF fires for every STOP on the bus (not just ours).  We just reset
 * our state and re-enable ACK — harmless for non-our transactions.
 * ----------------------------------------------------------------------- */

void I2C1_EV_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    /* ----- Address match ------------------------------------------------ */
    if (sr1 & I2C_SR1_ADDR) {
        uint32_t sr2 = I2C1->SR2;          /* reading SR2 clears ADDR flag  */
        if (sr2 & I2C_SR2_TRA) {
            /* Master reading from us — load first byte immediately        */
            uint16_t val = fg_reg_read(s_reg);
            s_txbuf[0]   = (uint8_t)(val & 0xFF);
            s_txbuf[1]   = (uint8_t)(val >> 8);
            s_txpos      = 0;
            s_i2cs       = I2CS_TX_DATA;
            s_rd_cnt++;
            I2C1->DR = s_txbuf[s_txpos++];
            I2C1->CR2 |= I2C_CR2_ITBUFEN;  /* enable TXE interrupt          */
        } else {
            /* Master writing to us — wait for data byte (reg address)     */
            s_i2cs = I2CS_RX_REG;
            I2C1->CR2 |= I2C_CR2_ITBUFEN;  /* enable RXNE interrupt         */
        }
        I2C1->CR1 |= I2C_CR1_ACK;          /* keep ACK enabled              */
    }

    /* ----- Receive data (register address byte) ------------------------- */
    if (sr1 & I2C_SR1_RXNE) {
        uint8_t b = (uint8_t)I2C1->DR;
        if (s_i2cs == I2CS_RX_REG) {
            s_reg  = b;
            s_wr_cnt++;
        }
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;     /* one byte expected, disable    */
    }

    /* ----- Transmit empty (load next data byte) ------------------------- */
    if (sr1 & I2C_SR1_TXE) {
        if (s_i2cs == I2CS_TX_DATA && s_txpos < 2)
            I2C1->DR = s_txbuf[s_txpos++];
        if (s_txpos >= 2)
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN; /* all bytes queued, disable     */
    }

    /* ----- STOP detected ------------------------------------------------
     * Fires for ANY stop on the bus (our transaction or another device).
     * Clear by: read SR1 (done above) then write CR1.
     * Reset state and re-enable ACK for the next START.                   */
    if (sr1 & I2C_SR1_STOPF) {
        I2C1->CR1 |= I2C_CR1_ACK;          /* write CR1 clears STOPF        */
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        s_i2cs = I2CS_IDLE;
    }
}

void I2C1_ER_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    /* AF: master NACKed our last transmitted byte — normal end of read.   */
    if (sr1 & I2C_SR1_AF) {
        I2C1->SR1 = (uint16_t)~I2C_SR1_AF;
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        s_i2cs = I2CS_IDLE;
        I2C1->CR1 |= I2C_CR1_ACK;
    }
    /* BERR: bus error.  HAL workaround would set SWRST; we don't.         */
    if (sr1 & I2C_SR1_BERR) {
        I2C1->SR1 = (uint16_t)~I2C_SR1_BERR;
        s_i2cs = I2CS_IDLE;
        I2C1->CR1 |= I2C_CR1_ACK;
    }
    if (sr1 & I2C_SR1_ARLO) {
        I2C1->SR1 = (uint16_t)~I2C_SR1_ARLO;
        s_i2cs = I2CS_IDLE;
    }
}

/* -----------------------------------------------------------------------
 * Peripheral init
 * ----------------------------------------------------------------------- */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* HSI 8 MHz — no PLL, no USB clock needed when running headless.
     * Keeps the STM32 at ~2–3 mA vs ~25 mA at 72 MHz, which matters
     * because the TLV70433 LDO dissipates (VIN−3.3V)×I as heat.
     * At 19 V input: 8 MHz → ~39 mW vs 72 MHz → ~314 mW. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState   = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;   /* 8 MHz */
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;        /* HCLK  = 8 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;          /* APB1  = 8 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;          /* APB2  = 8 MHz */
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);   /* 0 wait states ≤ 24 MHz */
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LED (PC13, active-low) */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    g.Pin   = LED_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &g);
}

static void MX_I2C1_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* PB6 = SCL, PB7 = SDA as AF open-drain */
    g.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode  = GPIO_MODE_AF_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pull  = GPIO_NOPULL;  /* external pull-ups on the JBL bus */
    HAL_GPIO_Init(GPIOB, &g);

    /* Software reset to release any residual state */
    I2C1->CR1 = I2C_CR1_SWRST;
    for (volatile int i = 0; i < 200; i++);
    I2C1->CR1 = 0;

    /* APB1 = 8 MHz (HSI, no PLL) */
    I2C1->CR2   = 8U;                      /* FREQ field = 8 MHz            */
    I2C1->CCR   = 40U;                     /* 100 kHz: 8e6/(2*100e3)=40     */
    I2C1->TRISE = 9U;                      /* (1000ns × 8MHz)+1 = 9         */

    /* OAR1: 7-bit address 0x55.  Bit 14 must be kept 1 per RM0008.        */
    I2C1->OAR1 = (1U << 14) | (0x55U << 1); /* = 0x40AA                    */

    /* Enable peripheral + ACK (NoStretch=0 = stretching enabled) */
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;

    /* Flush any ADDR that may have matched between PE=1 and ITEVTEN below  */
    (void)I2C1->SR1;
    (void)I2C1->SR2;

    /* Enable EV and ER interrupts */
    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;

    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /* USB not initialised: PLL is off (running on 8 MHz HSI to save power),
     * and USB requires exactly 48 MHz.  Connect USB only for debug flashing. */

    /* Configure PB6/PB7 as plain inputs while waiting for the JBL to power on.
     * When JBL is off its I2C pull-ups are unpowered — both lines sit at 0V.
     * Initialising the I2C peripheral with lines at 0V confuses it (BUSY=1,
     * clock stretch, HAL BERR→SWRST bug).  Wait until both go HIGH. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    {
        GPIO_InitTypeDef g = {0};
        g.Pin  = GPIO_PIN_6 | GPIO_PIN_7;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &g);
    }

    /* Phase 1 — wait for JBL bus: fast-blink LED at 100 ms (5 Hz).
     * Spin until SCL=1 AND SDA=1 stable for 5 ms. */
    {
        uint32_t high_since = 0;
        uint32_t led_ts     = 0;
        for (;;) {
            uint32_t now = HAL_GetTick();

            /* fast blink */
            if (now - led_ts >= 100) {
                HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
                led_ts = now;
            }

            if ((GPIOB->IDR & (GPIO_PIN_6 | GPIO_PIN_7)) ==
                              (GPIO_PIN_6 | GPIO_PIN_7)) {
                if (!high_since) high_since = now;
                if (now - high_since >= 5) break;
            } else {
                high_since = 0;
            }
        }
    }

    /* Phase 2 — bus live, I2C ready: LED off until first transaction. */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET); /* active-low OFF */
    MX_I2C1_Init();

    uint32_t last_txn = 0;   /* last seen wr+rd total          */
    uint32_t blink_ts = 0;   /* blink start timestamp (0=idle) */

    while (1) {
        /* Sleep until the next interrupt (SysTick 1 ms or I2C event).
         * I2C peripheral stays clocked in sleep mode — responses are
         * unaffected.  At 8 MHz + WFI: ~2–3 mA vs ~25 mA at 72 MHz. */
        __WFI();

        /* Phase 3 — blink LED on each I2C event (wr or rd).
         * Multiple events in quick succession extend the blink window. */
        uint32_t txn = s_wr_cnt + s_rd_cnt;
        if (txn != last_txn) {
            last_txn = txn;
            HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET); /* LED on */
            blink_ts = HAL_GetTick();
        }
        if (blink_ts && HAL_GetTick() - blink_ts >= 60) {
            HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET); /* LED off */
            blink_ts = 0;
        }
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        for (volatile int i = 0; i < 200000; i++);
    }
}
