#include "SDRAM.h"
#include "smalloc.h"

extern uint8_t external_psram_size;
void* extmem_base;
size_t extmem_size;

// default = 166MHz
uint32_t semc_clk __attribute((weak)) = SEMC_CLOCK_166;

#ifndef ARDUINO_TEENSY41
struct smalloc_pool extmem_smalloc_pool;
#endif

// default base address
#define SDRAM_BASE 0x80000000
// default SDRAM size (in MBs)
#define SDRAM_SIZE 32
// CAS latency 3 will be used if frequency is above this, else use CL=2
#define SDRAM_CAS2_MAX 1665e5f

#define PROBE_DATA 0x5A698421

#define PSRAM_BASE 0x70000000

FLASHMEM static float PSRAM_freq(uint32_t cbcmr)
{
	float clk = 0.0f;
	switch (cbcmr & CCM_CBCMR_FLEXSPI2_CLK_SEL_MASK)
	{
		case CCM_CBCMR_FLEXSPI2_CLK_SEL(3):
			clk = 528e6f; // PLL2
			break;
		case CCM_CBCMR_FLEXSPI2_CLK_SEL(2):
			clk = 664.62e6f; // PLL3 PFD1
			break;
		case CCM_CBCMR_FLEXSPI2_CLK_SEL(1):
			clk = 720e6f; // PLL3 PFD0
			break;
		case CCM_CBCMR_FLEXSPI2_CLK_SEL(0):
			clk = 396e6f; // PLL2 PFD2
			break;
	}
	switch (cbcmr & CCM_CBCMR_FLEXSPI2_PODF_MASK)
	{
		case CCM_CBCMR_FLEXSPI2_PODF(7):
			clk /= 8;
			break;
		case CCM_CBCMR_FLEXSPI2_PODF(6):
			clk /= 7;
			break;
		case CCM_CBCMR_FLEXSPI2_PODF(5):
			clk /= 6;
			break;
		case CCM_CBCMR_FLEXSPI2_PODF(4):
			clk /= 5;
			break;
		case CCM_CBCMR_FLEXSPI2_PODF(3):
			clk /= 4;
			break;
		case CCM_CBCMR_FLEXSPI2_PODF(2):
			clk /= 3;
			break;
		case CCM_CBCMR_FLEXSPI2_PODF(1):
			clk /= 2;
			break;
	}
	return clk;
}

FLASHMEM static float SEMC_freq(uint32_t cbcdr)
{
	float clk;
	if (cbcdr & CCM_CBCDR_SEMC_CLK_SEL)
	{
		if (cbcdr & CCM_CBCDR_SEMC_ALT_CLK_SEL)
			clk = 664.62e6f; // PLL3 PFD1
		else
			clk = 396e6f; // PLL2 PFD2
	}
	else
	{
		clk = F_CPU_ACTUAL; // peripheral clock (CPU clk before AHB prescaler)
		switch (CCM_CBCDR & (CCM_CBCDR_AHB_PODF(7)))
		{
			case CCM_CBCDR_AHB_PODF(7):
				clk *= 8.0f;
				break;
			case CCM_CBCDR_AHB_PODF(6):
				clk *= 7.0f;
				break;
			case CCM_CBCDR_AHB_PODF(5):
				clk *= 6.0f;
				break;
			case CCM_CBCDR_AHB_PODF(4):
				clk *= 5.0f;
				break;
			case CCM_CBCDR_AHB_PODF(3):
				clk *= 4.0f;
				break;
			case CCM_CBCDR_AHB_PODF(2):
				clk *= 3.0f;
				break;
			case CCM_CBCDR_AHB_PODF(1):
				clk *= 2.0f;
				break;
		}
	}
	switch (cbcdr & (CCM_CBCDR_SEMC_PODF(7)))
	{
		case CCM_CBCDR_SEMC_PODF(7):
			clk /= 8.0f;
			break;
		case CCM_CBCDR_SEMC_PODF(6):
			clk /= 7.0f;
			break;
		case CCM_CBCDR_SEMC_PODF(5):
			clk /= 6.0f;
			break;
		case CCM_CBCDR_SEMC_PODF(4):
			clk /= 5.0f;
			break;
		case CCM_CBCDR_SEMC_PODF(3):
			clk /= 4.0f;
			break;
		case CCM_CBCDR_SEMC_PODF(2):
			clk /= 3.0f;
			break;
		case CCM_CBCDR_SEMC_PODF(1):
			clk /= 2.0f;
			break;
	}
	return clk;
}

FLASHMEM float extmem_freq() {
	if (extmem_size == 0)
		return -1.0f;

	if (extmem_base == (void*)SDRAM_BASE)
		return SEMC_freq(CCM_CBCDR);

	if (extmem_base == (void*)PSRAM_BASE)
		return PSRAM_freq(CCM_CBCMR);

	// unknown
	return 0.0f;
}


FLASHMEM static unsigned int ns_to_clocks(float ns, float freq)
{
	float clocks = ceilf(ns * 1.0e-9f * freq);
	if (clocks < 1.0f) return 1;
	return (unsigned int)clocks;
}

FLASHMEM static bool IPCommand(uint16_t command)
{
	/* Reset status and error */
	SEMC_INTR = SEMC_INTR_IPCMDDONE | SEMC_INTR_IPCMDERR;
	/* Set address */
	SEMC_IPCR0 = SDRAM_BASE;
	/* Send command code */
	SEMC_IPCMD = command | 0xA55A0000;

	/* Poll status bit till command is done */
	while (!(SEMC_INTR & SEMC_INTR_IPCMDDONE));
	/* Check for error */
	if (SEMC_INTR & SEMC_INTR_IPCMDERR)
	{
		return false;
	}

	return true;
}

FLASHMEM static bool IPCommandWrite(uint16_t command, uint32_t data)
{
	SEMC_IPTXDAT = data;
	return IPCommand(command);
}

FLASHMEM static bool IPCommandRead(uint16_t command, uint32_t *data)
{
	if (IPCommand(command) == false)
		return false;

	*data = SEMC_IPRXDAT;
	return true;
}

FLASHMEM void startup_middle_hook(void)
{
	// check if PSRAM is already present
	if (external_psram_size != 0)
	{
		extmem_base = (void*)0x70000000;
		extmem_size = external_psram_size;
		return;
	}

	/* initialize pads to 0x110F9
	 * Slew Rate Field: Fast Slew Rate
	 * Drive Strength Field: R0/7
	 * Speed Field: max(200MHz)
	 * Open Drain Enable Field: Open Drain Disabled
	 * Pull / Keep Enable Field: Pull/Keeper Enabled
	 * Pull / Keep Select Field: Keeper
	 * Pull Up / Down Config. Field: 100K Ohm Pull Down
	 * Hyst. Enable Field: Hysteresis Enabled
	 */
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_00 = /* SEMC_DATA0  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_01 = /* SEMC_DATA1  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_02 = /* SEMC_DATA2  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_03 = /* SEMC_DATA3  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_04 = /* SEMC_DATA4  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_05 = /* SEMC_DATA5  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_06 = /* SEMC_DATA6  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_07 = /* SEMC_DATA7  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_08 = /* SEMC_DMO    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_09 = /* SEMC_ADDR0  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_10 = /* SEMC_ADDR1  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_11 = /* SEMC_ADDR2  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_12 = /* SEMC_ADDR3  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_13 = /* SEMC_ADDR4  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_14 = /* SEMC_ADDR5  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_15 = /* SEMC_ADDR6  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_16 = /* SEMC_ADDR7  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_17 = /* SEMC_ADDR8  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_18 = /* SEMC_ADDR9  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_19 = /* SEMC_ADDR11 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_20 = /* SEMC_ADDR12 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_21 = /* SEMC_BA0    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_22 = /* SEMC_BA1    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_23 = /* SEMC_A10    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_24 = /* SEMC_CAS    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_25 = /* SEMC_RAS    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_26 = /* SEMC_CLK    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_27 = /* SEMC_CKE    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_28 = /* SEMC_WE     */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_29 = /* SEMC_CS0    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_30 = /* SEMC_DATA8  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_31 = /* SEMC_DATA9  */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_32 = /* SEMC_DATA10 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_33 = /* SEMC_DATA11 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_34 = /* SEMC_DATA12 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_35 = /* SEMC_DATA13 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_36 = /* SEMC_DATA14 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_37 = /* SEMC_DATA15 */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_38 = /* SEMC_DM1    */ \
	IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_39 = /* SEMC_DQS    */ 0x0110F9;

	// initialize pin muxes: ALT 0 is SEMC
	// output-only pins:
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_08 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_09 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_10 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_11 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_12 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_13 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_14 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_15 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_16 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_17 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_18 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_19 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_20 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_21 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_22 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_23 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_24 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_25 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_26 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_27 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_28 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_29 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_38 = 0;
	// input/outpins pins (data and DQS), activate SION
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_01 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_02 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_03 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_04 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_05 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_06 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_07 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_30 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_32 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_33 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_34 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_35 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_36 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_37 = \
	IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_39 = 0x10;

	/* Configure SEMC clock */
	uint32_t cbcdr = CCM_CBCDR;
	cbcdr &= ~(CCM_CBCDR_SEMC_PODF(7) | CCM_CBCDR_SEMC_ALT_CLK_SEL | CCM_CBCDR_SEMC_CLK_SEL);
	cbcdr |= semc_clk;
	CCM_CBCDR = cbcdr;

	const float freq = SEMC_freq(semc_clk);

	delayMicroseconds(1);
	CCM_CCGR3 |= CCM_CCGR3_SEMC(CCM_CCGR_ON);

	/* software reset */
	SEMC_BR0 = 0;
	SEMC_BR1 = 0;
	SEMC_BR2 = 0;
	SEMC_BR3 = 0;
	SEMC_BR4 = 0;
	SEMC_BR5 = 0;
	SEMC_BR6 = 0;
	SEMC_BR7 = 0;
	SEMC_BR8 = 0;
	SEMC_MCR = SEMC_MCR_SWRST;
	uint32_t m = micros();
	while (SEMC_MCR & SEMC_MCR_SWRST)
	{
		if (micros() - m > 1500)
		{
			return;
		}
	}

	SEMC_MCR = SEMC_MCR_MDIS | SEMC_MCR_BTO(0x1F) | (freq > 133e6f ? SEMC_MCR_DQSMD : 0);

	SEMC_BMCR0 = SEMC_BMCR0_WQOS(5) | SEMC_BMCR0_WAGE(8) | SEMC_BMCR0_WSH(0x40) | SEMC_BMCR0_WRWS(0x10);
	SEMC_BMCR1 = SEMC_BMCR1_WQOS(5) | SEMC_BMCR1_WAGE(8) | SEMC_BMCR1_WPH(0x60) | SEMC_BMCR1_WRWS(0x24) | SEMC_BMCR1_WBR(0x40);

	/* run SEMC clock */
	SEMC_MCR &= ~SEMC_MCR_MDIS;

	uint32_t CAS = (freq > SDRAM_CAS2_MAX ? 3 : 2);

	/* configure SEMC for SDRAM: IS42S16160J-6 (32MB / 166MHz / CL3) */
	SEMC_BR0 = SDRAM_BASE | SEMC_BR_MS(13) | SEMC_BR_VLD;
	SEMC_SDRAMCR0 = \
		SEMC_SDRAMCR0_CL(CAS)  | // CAS latency = 2 or 3
		SEMC_SDRAMCR0_COL(3) | // 3 = 9 bit column
		SEMC_SDRAMCR0_BL(3)  | // 3 = 8 word burst length
		SEMC_SDRAMCR0_PS;      // 16-bit words
	SEMC_SDRAMCR1 = \
		SEMC_SDRAMCR1_ACT2PRE((ns_to_clocks(42, freq)-1)) | // tRAS: ACTIVE to PRECHARGE
		SEMC_SDRAMCR1_CKEOFF((ns_to_clocks(42, freq)-1)) |  // self refresh
		SEMC_SDRAMCR1_WRC((ns_to_clocks(12, freq)-1)) |     // tWR: WRITE recovery
		SEMC_SDRAMCR1_RFRC((ns_to_clocks(67, freq)-1)) |    // tRFC or tXSR: REFRESH recovery
		SEMC_SDRAMCR1_ACT2RW((ns_to_clocks(18, freq)-1)) |  // tRCD: ACTIVE to READ/WRITE
		SEMC_SDRAMCR1_PRE2ACT((ns_to_clocks(18, freq)-1));  // tRP: PRECHARGE to ACTIVE/REFRESH
	SEMC_SDRAMCR2 =
		SEMC_SDRAMCR2_SRRC((ns_to_clocks(67, freq)-1)) |
		SEMC_SDRAMCR2_REF2REF(ns_to_clocks(60, freq)) |  /* No minus one to keep with RM */
		SEMC_SDRAMCR2_ACT2ACT(ns_to_clocks(60, freq)) |  /* No minus one to keep with RM */
		SEMC_SDRAMCR2_ITO(0);

	uint32_t prescaleperiod = 160 * (1000000000 / freq);
	uint16_t prescale = prescaleperiod / 16 / (1000000000 / freq);

	if (prescale > 256)
	{
		//Serial.println("Invalid Timer Setting");
		while (1);
	}

	uint16_t refresh = (64 * 1000000 / 8192) / prescaleperiod;
	uint16_t urgentRef = refresh;
	//uint16_t idle = 0 / prescaleperiod

	SEMC_SDRAMCR3 = /* N * 16 * 1s / clkSrc_Hz = config->tPrescalePeriod_Ns */
		SEMC_SDRAMCR3_PRESCALE(prescale) | SEMC_SDRAMCR3_RT(refresh) | SEMC_SDRAMCR3_UT(urgentRef);

	SEMC_IPCR1 = 0; // read/write IP commands use 4 bytes (two-word burst)
	SEMC_IPCR2 = 0;

	/* Initialize SDRAM device */
	delayMicroseconds(100);
	if (!IPCommand(15)) // Precharge All
		return;
	if (!IPCommand(12) || !IPCommand(12)) // 2x AutoRefresh
		return;
	/* Set mode register: burst length=8, CAS */
	if (!IPCommandWrite(10, (CAS<<4)|3))
		return;
	/* Enable refresh */
	SEMC_SDRAMCR3 |= SEMC_SDRAMCR3_REN;

	// basic test to see if SDRAM is working
	uint32_t orig = PROBE_DATA;
	for (int i=0; i < 32; i++) {
		uint32_t r=0;
		if (!IPCommandWrite(9, orig)) // Write
			return;
		if (!IPCommandRead(8, &r)) // Read
			return;
		if (r != orig)
			return;
		orig = (orig << 31) | (orig >> 1);
	}

	extmem_base = (void*)SDRAM_BASE;
	// for "old" programs that only expect PSRAM
	external_psram_size = SDRAM_SIZE;
	extmem_size = SDRAM_SIZE;

	// initialize pool for SDRAM
	sm_set_pool(&extmem_smalloc_pool, extmem_base, SDRAM_SIZE * (1<<20), 0, NULL);
}

void *extmem_malloc(size_t size)
{
	void *ptr = sm_malloc_pool(&extmem_smalloc_pool, size);
	if (ptr) return ptr;
	return malloc(size);
}

void extmem_free(void *ptr)
{
	if (sm_alloc_valid_pool(&extmem_smalloc_pool, ptr))
	{
		sm_free_pool(&extmem_smalloc_pool, ptr);
		return;
	}
	free(ptr);
}

void *extmem_calloc(size_t nmemb, size_t size)
{
	void *ptr = sm_calloc_pool(&extmem_smalloc_pool, nmemb, size);
	if (ptr) return ptr;
	return calloc(nmemb, size);
}

void *extmem_realloc(void *ptr, size_t size)
{
	if (sm_alloc_valid_pool(&extmem_smalloc_pool, ptr))
	{
		return sm_realloc_pool(&extmem_smalloc_pool, ptr, size);
	}
	return realloc(ptr, size);
}
