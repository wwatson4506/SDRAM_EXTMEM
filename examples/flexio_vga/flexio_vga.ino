#include <DMAChannel.h>
#include <SDRAM.h>

/* R2R ladder:
 *
 * GROUND <------------- 536R ----*---- 270R ---*-----------> VGA PIN: R=1/G=2/B=3
 *                                |             |
 * INTENSITY (13) <---536R -------/             |
 *                                              |
 * COLOR: R=11/G=12/B=10  <-----536R------------/
 *
 * (duplicate above three times each for R, G and B)
 *
 * VSYNC (34) <---------------68R---------------------------> VGA PIN 14
 *
 * HSYNC (35) <---------------68R---------------------------> VGA PIN 13
 */

//=====================================
// Physical pin and FlexIO pin Defines.
// Defined for T41, MicroMod and SDRAM
// dev board V4.0.
//=====================================
#define RED_PIN 11
#define GRN_PIN 12
#define BLU_PIN 10
#define INTENSITY_PIN 13

#if defined(ARDUINO_TEENSY41)
#define VSYNC_PIN 34
#define HSYNC_PIN 35
#define FLEX_VSYNC_PIN 29
#define FLEX_HSYNC_PIN 28
#else // MicroMod and Dev Board V4.0
#define VSYNC_PIN 8
#define HSYNC_PIN 7
#define FLEX_VSYNC_PIN 16
#define FLEX_HSYNC_PIN 17
#endif
//=====================================

// horizontal values must be divisible by 8 for correct operation
typedef struct {
  uint32_t height;
  uint32_t vfp;
  uint32_t vsw;
  uint32_t vbp;
  uint32_t width;
  uint32_t hfp;
  uint32_t hsw;
  uint32_t hbp;
  uint32_t clk_num;
  uint32_t clk_den;
  // sync polarities: 0 = active high, 1 = active low
  uint32_t vsync_pol;
  uint32_t hsync_pol;
} vga_timing;

class FlexIO2VGA {
public:
  FlexIO2VGA(const vga_timing& mode, bool half_height=false, bool half_width=false, unsigned int bpp=4);
  void stop(void);

  // wait parameter:
  // TRUE =  wait until previous frame ends and source is "current"
  // FALSE = queue it as the next framebuffer to draw, return immediately
  void set_next_buffer(const void* source, size_t pitch, bool wait);

  void wait_for_frame(void) {
    unsigned int count = frameCount;
    while (count == frameCount)
      yield();
  }
private:
  void set_clk(int num, int den);
  static void ISR(void);
  void TimerInterrupt(void);

  __attribute__((aligned(2))) uint8_t dma_chans[2];
  DMAChannel dma1,dma2,dmaswitcher;
  DMASetting dma_params;

  bool double_height;
  int32_t widthxbpp;
  
  volatile unsigned int frameCount;
};

FLASHMEM FlexIO2VGA::FlexIO2VGA(const vga_timing& mode, bool half_height, bool half_width, unsigned int bpp) {
  frameCount = 0;
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 4; // FLEXIO2_D2    RED
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 4; // FLEXIO2_D1    GREEN
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 4; // FLEXIO2_D0    BLUE
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 4; // FLEXIO2_D3    INTENSITY
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00 = 4; // FLEXIO2_D29   VSYNC
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 = 4; // FLEXIO2_D28   HSYNC

//  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_13 = 4; // FLEXIO2_D29   VSYNC
//  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_12 = 4; // FLEXIO2_D28   HSYNC

  dma_chans[0] = dma2.channel;
  dma_chans[1] = dma1.channel;

  memset(dma_params.TCD, 0, sizeof(*dma_params.TCD));
  dma_params.TCD->DOFF = 4;
  dma_params.TCD->ATTR = DMA_TCD_ATTR_DMOD(4) | DMA_TCD_ATTR_DSIZE(2);
  dma_params.TCD->NBYTES = 16;
  dma_params.TCD->DADDR = &FLEXIO2_SHIFTBUF0;
  dma1.triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);
  dma2.triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);

  dmaswitcher.TCD->SADDR = dma_chans;
  dmaswitcher.TCD->SOFF = 1;
  dmaswitcher.TCD->DADDR = &DMA_SERQ;
  dmaswitcher.TCD->DOFF = 0;
  dmaswitcher.TCD->ATTR = DMA_TCD_ATTR_SMOD(1);
  dmaswitcher.TCD->NBYTES = 1;
  dmaswitcher.TCD->BITER = dmaswitcher.TCD->CITER = 1;

  double_height = half_height;
  widthxbpp = (mode.width * bpp) / (half_width ? 2 : 1);

  set_clk(4*mode.clk_num, mode.clk_den);

  FLEXIO2_CTRL = FLEXIO_CTRL_SWRST;
  asm volatile("dsb");
  FLEXIO2_CTRL = FLEXIO_CTRL_FASTACC | FLEXIO_CTRL_FLEXEN;
  // wait for reset to clear
  while (FLEXIO2_CTRL & FLEXIO_CTRL_SWRST);

  // timer 0: divide pixel clock by 8
  FLEXIO2_TIMCFG0 = 0;
  FLEXIO2_TIMCMP0 = (4*8)-1;
  
  // timer 1: generate HSYNC
  FLEXIO2_TIMCFG1 = FLEXIO_TIMCFG_TIMDEC(1);
  // on = HSW, off = rest of line
  FLEXIO2_TIMCMP1 = ((((mode.width+mode.hbp+mode.hfp)/8)-1)<<8) | ((mode.hsw/8)-1);
  // trigger = timer0, HSYNC=D28
  FLEXIO2_TIMCTL1 = FLEXIO_TIMCTL_TRGSEL(4*0+3) | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINCFG(3) | FLEXIO_TIMCTL_PINSEL(FLEX_HSYNC_PIN) | FLEXIO_TIMCTL_TIMOD(2) | (mode.hsync_pol*FLEXIO_TIMCTL_PINPOL);

  // timer 2: frame counter
  // tick on HSYNC
  FLEXIO2_TIMCFG2 = FLEXIO_TIMCFG_TIMDEC(1);
  FLEXIO2_TIMCMP2 = ((mode.height+mode.vbp+mode.vfp+mode.vsw)*2)-1;
  // trigger = HYSNC pin
  FLEXIO2_TIMCTL2 = FLEXIO_TIMCTL_TRGSEL(2*FLEX_HSYNC_PIN) | (mode.hsync_pol * FLEXIO_TIMCTL_TRGPOL) | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_TIMOD(3);

  // timer 3: generate VSYNC
  FLEXIO2_TIMCFG3 = FLEXIO_TIMCFG_TIMDIS(2) | FLEXIO_TIMCFG_TIMENA(7);
  // active for VSW lines. 4*total horizontal pixels*vertical sync length must be <= 65536 to not overflow this timer
  FLEXIO2_TIMCMP3 = (4*mode.vsw*(mode.width+mode.hbp+mode.hsw+mode.hfp))-1;
  // trigger = frame counter, VSYNC=D29
  FLEXIO2_TIMCTL3 = FLEXIO_TIMCTL_TRGSEL(4*2+3) | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINCFG(3) | FLEXIO_TIMCTL_PINSEL(FLEX_VSYNC_PIN) | FLEXIO_TIMCTL_TIMOD(3) | (mode.vsync_pol*FLEXIO_TIMCTL_PINPOL);

  // timer4: count VSYNC and back porch
  // enable on VSYNC start, disable after (VSW+VBP)*2 edges of HSYNC
  FLEXIO2_TIMCFG4 = FLEXIO_TIMCFG_TIMDEC(2) | FLEXIO_TIMCFG_TIMDIS(2) | FLEXIO_TIMCFG_TIMENA(6);
  FLEXIO2_TIMCMP4 = ((mode.vsw+mode.vbp)*2)-1;
  // trigger = VSYNC pin, pin = HSYNC
  FLEXIO2_TIMCTL4 = FLEXIO_TIMCTL_TRGSEL(2*FLEX_VSYNC_PIN) | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINSEL(FLEX_HSYNC_PIN) | FLEXIO_TIMCTL_TIMOD(3) | (mode.vsync_pol*FLEXIO_TIMCTL_TRGPOL) | (mode.hsync_pol*FLEXIO_TIMCTL_PINPOL);

  // timer 5: vertical active region
  // enable when previous timer finishes, disable after height*2 edges of HSYNC
  FLEXIO2_TIMCFG5 = FLEXIO_TIMCFG_TIMDEC(2) | FLEXIO_TIMCFG_TIMDIS(2) | FLEXIO_TIMCFG_TIMENA(6);
  FLEXIO2_TIMCMP5 = (mode.height*2)-1;
  // trigger = timer4 negative, pin = HSYNC
  FLEXIO2_TIMCTL5 = FLEXIO_TIMCTL_TRGSEL(4*4+3) | FLEXIO_TIMCTL_TRGPOL | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINSEL(FLEX_HSYNC_PIN) | FLEXIO_TIMCTL_TIMOD(3) | (mode.vsync_pol*FLEXIO_TIMCTL_PINPOL);

  // timer 6: horizontal active region
  // configured as PWM: OFF for HSYNC+HBP, ON for active region, reset (to off state) when HSYNC occurs (off state covers HFP then resets)
  FLEXIO2_TIMCFG6 = FLEXIO_TIMCFG_TIMOUT(1) | FLEXIO_TIMCFG_TIMDEC(1) | FLEXIO_TIMCFG_TIMRST(4) | FLEXIO_TIMCFG_TIMDIS(1) | FLEXIO_TIMCFG_TIMENA(1);
  FLEXIO2_TIMCMP6 = ((((mode.hsw+mode.hbp)/8)-1)<<8) | ((mode.width/8)-1);
  // trigger = timer0, pin = HSYNC
  FLEXIO2_TIMCTL6 = FLEXIO_TIMCTL_TRGSEL(4*0+3) | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINSEL(FLEX_HSYNC_PIN) | FLEXIO_TIMCTL_TIMOD(2) | (mode.hsync_pol*FLEXIO_TIMCTL_PINPOL);

  // timer 7: output pixels from shifter, runs only when trigger is ON
  FLEXIO2_TIMCFG7 = FLEXIO_TIMCFG_TIMDIS(6) | FLEXIO_TIMCFG_TIMENA(6) | FLEXIO_TIMCFG_TSTOP(2);
  FLEXIO2_TIMCMP7 = ((((128/bpp)*2)-1)<<8) | ((half_width ? 4:2)-1);
  // trigger = timer 6
  FLEXIO2_TIMCTL7 = FLEXIO_TIMCTL_TRGSEL(4*6+3) | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_TIMOD(1);

  // start blank
  FLEXIO2_SHIFTBUF3 = FLEXIO2_SHIFTBUF2 = FLEXIO2_SHIFTBUF1 = FLEXIO2_SHIFTBUF0 = 0;
  if (bpp == 4) {
    FLEXIO2_SHIFTCFG3 = FLEXIO_SHIFTCFG_PWIDTH(3);
    FLEXIO2_SHIFTCFG2 = FLEXIO2_SHIFTCFG1 = FLEXIO_SHIFTCFG_PWIDTH(3) | FLEXIO_SHIFTCFG_INSRC;
    FLEXIO2_SHIFTCTL3 = FLEXIO2_SHIFTCTL2 = FLEXIO2_SHIFTCTL1 = FLEXIO_SHIFTCTL_TIMSEL(7) | FLEXIO_SHIFTCTL_SMOD(2);
    // output stop bit when timer disables - ensures black output/zero outside active window
    FLEXIO2_SHIFTCFG0 = FLEXIO_SHIFTCFG_PWIDTH(3) | FLEXIO_SHIFTCFG_INSRC | FLEXIO_SHIFTCFG_SSTOP(2);
    FLEXIO2_SHIFTCTL0 = FLEXIO_SHIFTCTL_TIMSEL(7) | FLEXIO_SHIFTCTL_PINCFG(3) | FLEXIO_SHIFTCTL_PINSEL(0) | FLEXIO_SHIFTCTL_SMOD(2);
    
    FLEXIO2_SHIFTSTATE = 0;
  } else { // bpp==1
    FLEXIO2_SHIFTCFG3 = FLEXIO_SHIFTCFG_PWIDTH(0);
    FLEXIO2_SHIFTCFG2 = FLEXIO2_SHIFTCFG1 = FLEXIO_SHIFTCFG_PWIDTH(0) | FLEXIO_SHIFTCFG_INSRC;
    FLEXIO2_SHIFTCTL3 = FLEXIO2_SHIFTCTL2 = FLEXIO2_SHIFTCTL1 = FLEXIO_SHIFTCTL_TIMSEL(7) | FLEXIO_SHIFTCTL_SMOD(2);
    FLEXIO2_SHIFTCFG0 = FLEXIO_SHIFTCFG_PWIDTH(0) | FLEXIO_SHIFTCFG_INSRC | FLEXIO_SHIFTCFG_SSTOP(2);
    FLEXIO2_SHIFTCTL0 = FLEXIO_SHIFTCTL_TIMSEL(7)  | FLEXIO_SHIFTCTL_PINCFG(3) | FLEXIO_SHIFTCTL_PINSEL(8) | FLEXIO_SHIFTCTL_SMOD(2);

    // D8 clear = use state 6, D8 set = use state 7
    // note that PWIDTH does not seem to mask D4-7 outputs as documented!
    FLEXIO2_SHIFTBUF6 = 0x00FBEFBE;
    FLEXIO2_SHIFTCFG6 = FLEXIO_SHIFTCFG_PWIDTH(15);
    FLEXIO2_SHIFTCTL6 = FLEXIO_SHIFTCTL_TIMSEL(7) | FLEXIO_SHIFTCTL_PINCFG(3) | FLEXIO_SHIFTCTL_PINSEL(8) | FLEXIO_SHIFTCTL_SMOD(6) | FLEXIO_SHIFTCTL_TIMPOL;
    FLEXIO2_SHIFTBUF7 = 0x0FFBEFBE;
    FLEXIO2_SHIFTCFG7 = FLEXIO_SHIFTCFG_PWIDTH(15);
    FLEXIO2_SHIFTCTL7 = FLEXIO_SHIFTCTL_TIMSEL(7) | FLEXIO_SHIFTCTL_PINCFG(3) | FLEXIO_SHIFTCTL_PINSEL(8) | FLEXIO_SHIFTCTL_SMOD(6) | FLEXIO_SHIFTCTL_TIMPOL;

    FLEXIO2_SHIFTSTATE = 6;
  }

  // clear timer 5 status
  FLEXIO2_TIMSTAT = 1<<5;
  // make sure no other FlexIO interrupts are enabled
  FLEXIO2_SHIFTSIEN = 0;
  FLEXIO2_SHIFTEIEN = 0;
  // enable timer 5 interrupt
  FLEXIO2_TIMIEN = 1<<5;

  attachInterruptVector(IRQ_FLEXIO2, ISR);
  NVIC_SET_PRIORITY(IRQ_FLEXIO2, 32);
  NVIC_ENABLE_IRQ(IRQ_FLEXIO2);

  // start everything!
  FLEXIO2_TIMCTL0 = FLEXIO_TIMCTL_TIMOD(3);
}

FLASHMEM void FlexIO2VGA::stop(void) {
  NVIC_DISABLE_IRQ(IRQ_FLEXIO2);
  // FlexIO2 registers don't work if they have no clock
  if (CCM_CCGR3 & CCM_CCGR3_FLEXIO2(CCM_CCGR_ON)) {
    FLEXIO2_CTRL &= ~FLEXIO_CTRL_FLEXEN;
    FLEXIO2_TIMIEN = 0;
    FLEXIO2_SHIFTSDEN = 0;
  }
  dma1.disable();
  dma2.disable();
  asm volatile("dsb");
}

FLASHMEM void FlexIO2VGA::set_clk(int num, int den) {
  int post_divide = 0;
  while (num < 27*den) num <<= 1, ++post_divide;
  int div_select = num / den;
  num -= div_select * den;

  // valid range for div_select: 27-54

  // switch video PLL to bypass, enable, set div_select
  CCM_ANALOG_PLL_VIDEO = CCM_ANALOG_PLL_VIDEO_BYPASS | CCM_ANALOG_PLL_VIDEO_ENABLE | CCM_ANALOG_PLL_VIDEO_DIV_SELECT(div_select);
  // clear misc2 vid post-divider
  CCM_ANALOG_MISC2_CLR = CCM_ANALOG_MISC2_VIDEO_DIV(3);
  switch (post_divide) {
      case 0: // div by 1
        CCM_ANALOG_PLL_VIDEO_SET = CCM_ANALOG_PLL_VIDEO_POST_DIV_SELECT(2);
        break;
      case 1: // div by 2
        CCM_ANALOG_PLL_VIDEO_SET = CCM_ANALOG_PLL_VIDEO_POST_DIV_SELECT(1);
        break;
      // div by 4
      // case 2: PLL_VIDEO pos_div_select already set to 0
      case 3: // div by 8 (4*2)
        CCM_ANALOG_MISC2_SET = CCM_ANALOG_MISC2_VIDEO_DIV(1);
        break;
      case 4: // div by 16 (4*4)
        CCM_ANALOG_MISC2_SET = CCM_ANALOG_MISC2_VIDEO_DIV(3);
        break;
  }
  CCM_ANALOG_PLL_VIDEO_NUM = num;
  CCM_ANALOG_PLL_VIDEO_DENOM = den;
  // ensure PLL is powered
  CCM_ANALOG_PLL_VIDEO_CLR = CCM_ANALOG_PLL_VIDEO_POWERDOWN;
  // wait for lock
  while (!(CCM_ANALOG_PLL_VIDEO & CCM_ANALOG_PLL_VIDEO_LOCK));
  // deactivate bypass
  CCM_ANALOG_PLL_VIDEO_CLR = CCM_ANALOG_PLL_VIDEO_BYPASS;

  // gate clock
  CCM_CCGR3 &= ~CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
  // FlexIO2 use vid clock (PLL5)
  uint32_t t = CCM_CSCMR2;
  t &= ~CCM_CSCMR2_FLEXIO2_CLK_SEL(3);
  t |= CCM_CSCMR2_FLEXIO2_CLK_SEL(2);
  CCM_CSCMR2 = t;
  // flex gets 1:1 clock, no dividing
  CCM_CS1CDR &= ~(CCM_CS1CDR_FLEXIO2_CLK_PODF(7) | CCM_CS1CDR_FLEXIO2_CLK_PRED(7));
  asm volatile("dsb");
  // disable clock gate
  CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
}

void FlexIO2VGA::TimerInterrupt(void) {
  if (dma_params.TCD->SADDR) {
    dma1 = dma_params;
    if (double_height) {
      dma2 = dma_params;
      dma1.disableOnCompletion();
      dma2.disableOnCompletion();
      dmaswitcher.triggerAtCompletionOf(dma1);
      dmaswitcher.triggerAtCompletionOf(dma2);
    }
    dma1.enable();
    // push first pixels into shiftbuf registers
    dma1.triggerManual();
    FLEXIO2_SHIFTSDEN = 1<<0;
  }
  frameCount++;
}

void FlexIO2VGA::set_next_buffer(const void* source, size_t pitch, bool wait) {
  // find worst alignment combo of source and pitch
  size_t log_read;
  switch (((size_t)source | pitch) & 7) {
    case 0: // 8 byte alignment
      log_read = 3;
      break;
    case 2: // 2 byte alignment
    case 6:
      log_read = 1;
      break;
    case 4: // 4 byte alignment
      log_read = 2;
      break;
    default: // 1 byte alignment, this will be slow...
      log_read = 0;
  }
  uint16_t major = (widthxbpp+127)/128; // row length in quadwords
  dma_params.TCD->SOFF = 1 << log_read;
  dma_params.TCD->ATTR_SRC = log_read;
  dma_params.TCD->SADDR = source;
  dma_params.TCD->SLAST = pitch - (major*16);
  dma_params.TCD->CITER = dma_params.TCD->BITER = major;
  if (wait)
    wait_for_frame();
}

extern FlexIO2VGA FLEXIOVGA;
void FlexIO2VGA::ISR(void) {
  uint32_t timStatus = FLEXIO2_TIMSTAT & 0xFF;
  FLEXIO2_TIMSTAT = timStatus;

  if (timStatus & (1<<5)) {
    FLEXIOVGA.TimerInterrupt();
  }

  asm volatile("dsb");
}

/* END VGA driver code */

static void FillFrameBuffer(uint8_t *fb, int height, int width, int bpp, size_t pitch) {
  const int radius = height/6 - 10;
  static int xoff = radius;
  static int yoff = radius;
  static int xdir = 4;
  static int ydir = 2;
  static uint8_t bg = 8;
  static uint8_t fg = 8;
  const int limit = radius*radius;

  bool hit = false;
  if (xoff >= (int)width-radius) {
    hit = true;
    xdir = -xdir;
    xoff = (int)width-radius;
  }
  if (xoff <= radius) {
    hit = true;
    xdir = -xdir;
    xoff = radius;
  }
  if (yoff >= (int)height-radius) {
    hit = true;
    ydir = -ydir;
    yoff = (int)height-radius;
  }
  if (yoff <= radius) {
    hit = true;
    ydir = -ydir;
    yoff = radius;
  }

  if (hit) {
    if (++fg == 16)
      fg = 8;
    if (++bg == 24)
      bg = 0;
  }

  for (int y=0; y < height; y++) {
    uint8_t *p = fb;
    for (int x=0; x < width;)  {
      uint8_t c=0;
      if (bpp == 1) {
        for (int i=0; i < 8; i++) {
          int xdiff = x-xoff+i;
          int ydiff = y-yoff;
          if ((xdiff*xdiff + ydiff*ydiff) <= limit)
            c |= 1 << i;
        }
        x += 8;
      } else { // bpp=4
        for (int i=1; i >= 0; i--) {
          c <<= 4;
          int xdiff = x-xoff+i;
          int ydiff = y-yoff;
          c |= ((xdiff*xdiff + ydiff*ydiff) <= limit) ? fg : (bg/3);
        }
        x += 2;
      }
      *p++ = c;
    }
    fb += pitch;
  }

  xoff += xdir;
  yoff += ydir;
}

PROGMEM static const vga_timing t1280x1024x60 = {
  .height=1024, .vfp=1, .vsw=3, .vbp=38,
  .width=1280, .hfp=48, .hsw=112, .hbp=248,
  .clk_num=108, .clk_den=24, .vsync_pol=0, .hsync_pol=0
};

PROGMEM static const vga_timing t1280x720x60 = {
  .height=720, .vfp=13, .vsw=5, .vbp=12,
  .width=1280, .hfp=80, .hsw=40, .hbp=248,
  .clk_num=7425, .clk_den=2400, .vsync_pol=0, .hsync_pol=0
};

PROGMEM static const vga_timing t1024x768x60 = {
  .height=768, .vfp=3, .vsw=6, .vbp=29,
  .width=1024, .hfp=24, .hsw=136, .hbp=160,
  .clk_num=65, .clk_den=24, .vsync_pol=1, .hsync_pol=1
};

PROGMEM static const vga_timing t1024x600x60 = {
  .height=600, .vfp=1, .vsw=4, .vbp=23,
  .width=1024, .hfp=56, .hsw=160, .hbp=112,
  .clk_num=32, .clk_den=15, .vsync_pol=0, .hsync_pol=0
};

PROGMEM static const vga_timing t800x600x100 = {
  .height=600, .vfp=1, .vsw=3, .vbp=32,
  .width=800, .hfp=48, .hsw=88, .hbp=136,
  .clk_num=6818, .clk_den=2400, .vsync_pol=0, .hsync_pol=1
};

PROGMEM static const vga_timing t800x600x60 = {
  .height=600, .vfp=1, .vsw=4, .vbp=23,
  .width=800, .hfp=40, .hsw=128, .hbp=88,
  .clk_num=40, .clk_den=24, .vsync_pol=0, .hsync_pol=0
};

PROGMEM static const vga_timing t640x480x60 = {
  .height=480, .vfp=10, .vsw=2, .vbp=33,
  .width=640, .hfp=16, .hsw=96, .hbp=48,
  .clk_num=150, .clk_den=143, .vsync_pol=1, .hsync_pol=1
};

PROGMEM static const vga_timing t640x400x70 = {
  .height=400, .vfp=12, .vsw=2, .vbp=35,
  .width=640, .hfp=16, .hsw=96, .hbp=48,
  .clk_num=150, .clk_den=143, .vsync_pol=0, .hsync_pol=1
};

PROGMEM static const vga_timing t640x350x70 = {
  .height=350, .vfp=37, .vsw=2, .vbp=60,
  .width=640, .hfp=16, .hsw=96, .hbp=48,
  .clk_num=150, .clk_den=143, .vsync_pol=1, .hsync_pol=0
};

// memory restrictions
#define MAX_WIDTH (1280/2)
#define MAX_HEIGHT 720
#define STRIDE_PADDING 16

typedef uint8_t frameBuffer_t[(MAX_HEIGHT+1)*(MAX_WIDTH+STRIDE_PADDING)];

static uint8_t* s_frameBuffer[2];

const vga_timing *timing = &t640x400x70;
FlexIO2VGA FLEXIOVGA(*timing);

void setup() {
  Serial.begin(115200);
  s_frameBuffer[0] = (uint8_t*)extmem_base;
  s_frameBuffer[1] = (uint8_t*)malloc(sizeof(frameBuffer_t));

  if (s_frameBuffer[0] == NULL || s_frameBuffer[1] == NULL)
  {
    Serial.println("Failed to allocated framebuffers");
    while(1);
  }

  Serial.print("Framebuffer0: ");
  Serial.println((uint32_t)s_frameBuffer[0], HEX);
  Serial.print("Framebuffer1: ");
  Serial.println((uint32_t)s_frameBuffer[1], HEX);
  Serial.print("EXTMEM frequency: ");
  Serial.print(extmem_freq() / 1e6f);
  Serial.println("MHz");
}

void loop() {
  static uint32_t frameBufferIndex;

  static int double_height = false;
  static int double_width = false;
  static int bpp = 4;

  int height = timing->height / (double_height ? 2:1);
  int width = timing->width / (double_width ? 2:1);
  size_t pitch = width*bpp/8 + STRIDE_PADDING;

  int c = Serial.read();
  if (c >= 0) {
    switch (c) {
      case '0':
        timing = &t1280x720x60;
        Serial.println("New mode: 1280x720x60");
        break;
      case '1':
        timing = &t1024x768x60;
        Serial.println("New mode: 1024x768x60");
        break;
      case '2':
        timing = &t1024x600x60;
        Serial.println("New mode: 1024x600x60");
        break;
      case '3':
        timing = &t800x600x100;
        Serial.println("New mode: 800x600x100");
        break;
      case '4':
        timing = &t800x600x60;
        Serial.println("New mode: 800x600x60");
        break;
      case '5':
        timing = &t640x480x60;
        Serial.println("New mode: 640x480x60");
        break;
      case '6':
        timing = &t640x400x70;
        Serial.println("New mode: 640x400x70");
        break;
      case '7':
        timing = &t640x350x70;
        Serial.println("New mode: 640x350x70");
        break;
      case 'h':
      case 'H':
        double_height = !double_height;
        Serial.print("Height doubling is ");
        Serial.println(double_height ? "ON" : "OFF");
        break;
      case 'w':
      case 'W':
        double_width = !double_width;
        Serial.print("Width doubling is ");
        Serial.println(double_width ? "ON" : "OFF");
        break;
      case 'b':
      case 'B':
        bpp = (bpp==1) ? 4:1;
        Serial.print("Using BPP = ");
        Serial.println(bpp);
        break;
      default:
        Serial.println("0: 1280x720x60");
        Serial.println("1: 1024x768x60");
        Serial.println("2: 1024x600x60");
        Serial.println("3: 800x600x100");
        Serial.println("4: 800x600x60");
        Serial.println("5: 640x480x60");
        Serial.println("6: 640x400x70");
        Serial.println("7: 640x350x70");
        Serial.println("H: Toggle height doubling");
        Serial.println("W: Toggle width doubling");
        Serial.println("B: Toggle bitdepth (4/1)");
      case '\n':
        return;
    }
    FLEXIOVGA.stop();
    FLEXIOVGA = FlexIO2VGA(*timing, double_height, double_width, bpp);
  }

  FillFrameBuffer(s_frameBuffer[frameBufferIndex], height, width, bpp, pitch);
  arm_dcache_flush(s_frameBuffer[frameBufferIndex], height*pitch);
  FLEXIOVGA.set_next_buffer(s_frameBuffer[frameBufferIndex], pitch, true);
  frameBufferIndex ^= 1;
}
