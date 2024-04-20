#include <imxrt.h>

// meta-programming magic takes care of size calculations and endian conversion
#define EMC_PAD_MODE (IOMUXC_PAD_HYS|IOMUXC_PAD_PKE|IOMUXC_PAD_SPEED(3)|IOMUXC_PAD_DSE(7)|IOMUXC_PAD_SRE)
#define CLK_FREQ_MHZ   198

#define ns_to_ticks(a) (((a)*CLK_FREQ_MHZ + 999)/1000)
#define PRESCALER (CLK_FREQ_MHZ / 16)
#define REFRESH_TICKS (64000 * CLK_FREQ_MHZ / (8192 * (PRESCALER * 16 + 1)))
#define URGENT_THRESH ((REFRESH_TICKS-1) / 2)
#define REFRESH_THRESH (REFRESH_TICKS - URGENT_THRESH)

#define BE32(a) (((((uint32_t)a) << 24) & 0xFF000000) | \
                 ((((uint32_t)a) <<  8) & 0x00FF0000) | \
                 ((((uint32_t)a) >>  8) & 0x0000FF00) | \
                 ((((uint32_t)a) >> 24) & 0x000000FF))
#define BE16(a) (((((uint16_t)a) <<  8) & 0xFF00) | \
                 ((((uint16_t)a) >>  8) & 0x00FF))

template<uint8_t t, uint8_t p, typename c>
struct dcd_tag {
  const uint8_t tag = t;
  const uint16_t __attribute__((packed)) length = BE16(sizeof(c));
  const uint8_t param = p;
};

template<uint8_t tag, uint8_t param, uint32_t...cmds>
struct dcd_command : dcd_tag<tag,param,uint32_t[sizeof...(cmds)+1]> {
  const uint32_t cmd_arr[sizeof...(cmds)] = {(BE32(cmds))...};
};

__attribute__ ((section(".bootdata")))
static const struct deviceconfigdata : dcd_tag<0xD2,0x41,struct deviceconfigdata> {
  struct dcd_command<0xCC,0x04, // write 32-bit value
    // enable 528MHz PLL
    0x400D8030, CCM_ANALOG_PLL_SYS_ENABLE|CCM_ANALOG_PLL_SYS_DIV_SELECT, // CCM_ANALOG_PLL_SYS = CCM_ANALOG_PLL_SYS_ENABLE|CCM_ANALOG_PLL_SYS_DIV_SELECT;
    // CCM_ANALOG_PFD_528 |= 0x00FF0000; // gate PFD2 clock
    0x400D8104, 0x00FF0000,
    // CCM_ANALOG_PFD_528 ^= 0x00E70000; // set PFD2_FRAC=0x18 (396MHz) and ungate
    0x400D810C, 0x00E70000
  > stage0;

  struct dcd_command<0xCC,0x0C, // clear 32-bit bitmask (x &= ~y)
    // CCM_CBCDR &= ~(CCM_CBCDR_SEMC_PODF(7)|CCM_CBCDR_SEMC_ALT_CLK_SEL|CCM_CBCDR_SEMC_CLK_SEL);
    0x400FC014, CCM_CBCDR_SEMC_PODF(7)|CCM_CBCDR_SEMC_ALT_CLK_SEL|CCM_CBCDR_SEMC_CLK_SEL
  > stage1;

  struct dcd_command<0xCC,0x1C, // set 32-bit bitmask (x |= y)
    // SEMC use (PLL2 PFD2 / 2) as clock source, 396 / 2 = 198MHz
    // CCM_CBCDR |= CCM_CBCDR_SEMC_CLK_SEL|CCM_CBCDR_SEMC_PODF(1)
    0x400FC014, CCM_CBCDR_SEMC_CLK_SEL|CCM_CBCDR_SEMC_PODF(1),
    // ungate SEMC clock: CCM_CCGR3 |= CCM_CCGR3_SEMC(CCM_CCGR_ON);
    0x400FC074, CCM_CCGR3_SEMC(CCM_CCGR_ON)
  > stage2;

  struct dcd_command<0xCC,0x04,
    // initialize SEMC pads
    0x401F8204, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_00 = 0x110F9;
    0x401F8208, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_01 = 0x110F9;
    0x401F820C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_02 = 0x110F9;
    0x401F8210, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_03 = 0x110F9;
    0x401F8214, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_04 = 0x110F9;
    0x401F8218, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_05 = 0x110F9;
    0x401F821c, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_06 = 0x110F9;
    0x401F8220, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_07 = 0x110F9;
    0x401F8224, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_08 = 0x110F9;
    0x401F8228, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_09 = 0x110F9;
    0x401F822C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_10 = 0x110F9;
    0x401F8230, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_11 = 0x110F9;
    0x401F8234, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_12 = 0x110F9;
    0x401F8238, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_13 = 0x110F9;
    0x401F823C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_14 = 0x110F9;
    0x401F8240, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_15 = 0x110F9;
    0x401F8244, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_16 = 0x110F9;
    0x401F8248, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_17 = 0x110F9;
    0x401F824C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_18 = 0x110F9;
    0x401F8250, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_19 = 0x110F9;
    0x401F8254, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_20 = 0x110F9;
    0x401F8258, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_21 = 0x110F9;
    0x401F825C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_22 = 0x110F9;
    0x401F8260, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_23 = 0x110F9;
    0x401F8264, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_24 = 0x110F9;
    0x401F8268, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_25 = 0x110F9;
    0x401F826C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_26 = 0x110F9;
    0x401F8270, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_27 = 0x110F9;
    0x401F8274, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_28 = 0x110F9;
    0x401F8278, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_29 = 0x110F9;
    0x401F827C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_30 = 0x110F9;
    0x401F8280, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_31 = 0x110F9;
    0x401F8284, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_32 = 0x110F9;
    0x401F8288, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_33 = 0x110F9;
    0x401F828C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_34 = 0x110F9;
    0x401F8290, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_35 = 0x110F9;
    0x401F8294, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_36 = 0x110F9;
    0x401F8298, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_37 = 0x110F9;
    0x401F829C, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_38 = 0x110F9;
    0x401F82A0, EMC_PAD_MODE, // IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_39 = 0x110F9;
    // set mux for all pins to 0 (SEMC), activate SION for DQS
    0x401F8014, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00 = 0;
    0x401F8018, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_01 = 0;
    0x401F801C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_02 = 0;
    0x401F8020, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_03 = 0;
    0x401F8024, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_04 = 0;
    0x401F8028, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_05 = 0;
    0x401F802C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_06 = 0;
    0x401F8030, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_07 = 0;
    0x401F8034, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_08 = 0;
    0x401F8038, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_09 = 0;
    0x401F803C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_10 = 0;
    0x401F8040, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_11 = 0;
    0x401F8044, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_12 = 0;
    0x401F8048, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_13 = 0;
    0x401F804C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_14 = 0;
    0x401F8050, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_15 = 0;
    0x401F8054, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_16 = 0;
    0x401F8058, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_17 = 0;
    0x401F805C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_18 = 0;
    0x401F8060, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_19 = 0;
    0x401F8064, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_20 = 0;
    0x401F8068, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_21 = 0;
    0x401F806C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_22 = 0;
    0x401F8070, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_23 = 0;
    0x401F8074, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_24 = 0;
    0x401F8078, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_25 = 0;
    0x401F807C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_26 = 0;
    0x401F8080, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_27 = 0;
    0x401F8084, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_28 = 0;
    0x401F8088, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_29 = 0;
    0x401F808C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_30 = 0;
    0x401F8090, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31 = 0;
    0x401F8094, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_32 = 0;
    0x401F8098, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_33 = 0;
    0x401F809C, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_34 = 0;
    0x401F80A0, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_35 = 0;
    0x401F80A4, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_36 = 0;
    0x401F80A8, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_37 = 0;
    0x401F80AC, 0,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_38 = 0;
    0x401F80B0, 0x10,  // IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_39 = 0x10;

    // SEMC_MCR: disable until setup is done
    0x402F0000, SEMC_MCR_MDIS,
    // SEMC_BMCR0
    0x402F0008, SEMC_BMCR0_WQOS(5) | SEMC_BMCR0_WAGE(8) | SEMC_BMCR0_WSH(64) | SEMC_BMCR0_WRWS(16),
    // SEMC_BMCR1
    0x402F000C, SEMC_BMCR1_WQOS(5) | SEMC_BMCR1_WAGE(8) | SEMC_BMCR1_WPH(96) | SEMC_BMCR1_WRWS(36) | SEMC_BMCR1_WBR(64),
    // SEMC_BR0 = base address | SEMC_BR_MS(13) /* 4096 << 13 = 32MB */ | SEMC_BR_VALID;
    0x402F0010, 0x80000000 | SEMC_BR_MS(13) | SEMC_BR_VLD,
    // SEMC_BR1 = SEMC_BR2 = SEMC_BR3 = SEMC_BR4 = SEMC_BR5 = SEMC_BR6 = SEMC_BR7 = SEMC_BR8 = 0;
    0x402F0014, 0,
    0x402F0018, 0,
    0x402F001C, 0,
    0x402F0020, 0,
    0x402F0024, 0,
    0x402F0028, 0,
    0x402F002C, 0,
    0x402F0030, 0,
    // SEMC_INTEN = 0; // disable all interrupts
    0x402F0038, 0,
    // SEMC_SDRAMCR0 = SEMC_SDRAMCR0_CL(3) | SEMC_SDRAMCR0_COL(3) | SEMC_SDRAMCR0_BL(3) | SEMC_SDRAMCR0_PS; // CAS=3, 9-bit column, 8 word burst, 16-bit words
    0x402F0040, SEMC_SDRAMCR0_CL(3) | SEMC_SDRAMCR0_COL(3) | SEMC_SDRAMCR0_BL(3) | SEMC_SDRAMCR0_PS,
    // SEMC_SDRAMCR1 
    0x402F0044, SEMC_SDRAMCR1_ACT2PRE(ns_to_ticks(42)-1) |  // tRAS: ACTIVE to PRECHARGE
                SEMC_SDRAMCR1_CKEOFF(ns_to_ticks(42)-1) |   // self refresh minimum
                SEMC_SDRAMCR1_WRC(ns_to_ticks(12)-1) |      // tWR: WRITE to PRECHARGE (tDPL)
                SEMC_SDRAMCR1_RFRC(ns_to_ticks(60)-1) |     // tRFC: REFRESH recovery (tRC)
                SEMC_SDRAMCR1_ACT2RW(ns_to_ticks(18)-1) |   // tRCD: ACTIVE to READ/WRITE
                SEMC_SDRAMCR1_PRE2ACT(ns_to_ticks(18)-1),   // tRP: PRECHARGE to ACTIVE/REFRESH
    // SEMC_SDRAMCR2
    0x402F0048, SEMC_SDRAMCR2_ACT2ACT(ns_to_ticks(60)-1) |  // tRRD: ACTIVE to ACTIVE (! datasheet list this as 12ns but that dramatically increases write time during tests...)
                SEMC_SDRAMCR2_REF2REF(ns_to_ticks(60)-1) |  // tRFC: REFRESH to REFRESH (tRC)
                SEMC_SDRAMCR2_SRRC(ns_to_ticks(66)-1),      // self refresh recovery (tXSR)
    // SEMC_SDRAMCR3
    0x402F004C, SEMC_SDRAMCR3_UT(URGENT_THRESH) | SEMC_SDRAMCR3_RT(REFRESH_THRESH-1) | SEMC_SDRAMCR3_PRESCALE(PRESCALER-1),

    // SEMC_IPCR0 = 0x80000000; // slave address = SDRAM base
    0x402F0090, 0x80000000,
    // SEMC_IPCR1 = 2; // read/write IP commands use two bytes
    0x402F0094, SEMC_IPCR1_DATSZ(2),
    // SEMC_IPCR2 = 0; // no byte masking
    0x402F0098, 0,
    // SEMC_MCR: unset MDIS bit
    0x402F0000, SEMC_MCR_BTO(16) | SEMC_MCR_DQSMD,

    0x402F003C, SEMC_INTR_IPCMDDONE, // clear IP command done
    // send PRECHARGE_ALL command
    0x402F009C, 0xA55A000F
  > stage3;
  // while ((SEMC_INTR & SEMC_INTR_IPCMDDONE)==0);
  struct dcd_command <0xCF, 0x1C, 0x402F003C, SEMC_INTR_IPCMDDONE > stage4;

  struct {
    struct dcd_command <0xCC, 0x04, // write 32-bit value
      0x402F003C, SEMC_INTR_IPCMDDONE, // clear IP command done
      // send AUTO_REFRESH command
      0x402F009C, 0xA55A000C
    > stage5;

    // while ((SEMC_INTR & SEMC_INTR_IPCMDDONE)==0);
    struct dcd_command <0xCF, 0x1C, 0x402F003C, SEMC_INTR_IPCMDDONE > stage6;
  } do_auto_refresh_twice[2];

  struct dcd_command <0xCC, 0x04, // write 32-bit value
    0x402F003C, SEMC_INTR_IPCMDDONE, // clear IP command done
    // SEMC_IPTXDAT = (3<<4)|3; // CAS=3, 8-word burst length
    0x402F00A0, (3<<4)|3,
    // send MODE_REGISTER_SET command
    0x402F009C, 0xA55A000A
  > stage7;
  // while ((SEMC_INTR & SEMC_INTR_IPCMDDONE)==0);
  struct dcd_command <0xCF, 0x1C, 0x402F003C, SEMC_INTR_IPCMDDONE > stage8;

  // SEMC_SDRAMCR3 |= SEMC_SDRAMCR3_REN;
  struct dcd_command <0xCC, 0x1C, 0x402F004C, SEMC_SDRAMCR3_REN > stage9;

} DeviceConfigurationData;

extern "C" {
extern void ResetHandler(void);
extern const uint32_t BootData[];
extern const uint32_t hab_csf[];
}

__attribute__ ((section(".ivt"), used))
static const uint32_t NewImageVectorTable[] = {
	0x432000D1,		// header
	(uint32_t)&ResetHandler,// program entry
	0,			// reserved
	(uint32_t)&DeviceConfigurationData,			// dcd
	(uint32_t)BootData,	// abs address of boot data
	(uint32_t)NewImageVectorTable, // self
	(uint32_t)hab_csf,	// command sequence file
	0			// reserved
};
