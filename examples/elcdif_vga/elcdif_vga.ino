#include <SDRAM.h>

#define LCDIF_CTRL_SFTRST   ((uint32_t)1 << 31)
#define LCDIF_CTRL_CLKGATE  ((uint32_t)1 << 30)
#define LCDIF_CTRL_BYPASS_COUNT  ((uint32_t)1 << 19)
#define LCDIF_CTRL_DOTCLK_MODE  ((uint32_t)1 << 17)
#define LCDIF_CTRL_LCD_DATABUS_WIDTH(n)  ((uint32_t)(((n) & 0x3) << 10))
#define LCDIF_CTRL_WORD_LENGTH(n)  ((uint32_t)(((n) & 0x3) << 8))
#define LCDIF_CTRL_MASTER   ((uint32_t)1 << 5)
#define LCDIF_CTRL_RUN  ((uint32_t)1 << 0)
#define LCDIF_CTRL1_IMAGE_DATA_SELECT  ((uint32_t)1 << 31)
#define LCDIF_CTRL1_CS_OUT_SELECT ((uint32_t)1 << 30)
#define LCDIF_CTRL1_BM_ERROR_IRQ_EN ((uint32_t)1 << 26)
#define LCDIF_CTRL1_BM_ERROR_IRQ ((uint32_t)1 << 25)
#define LCDIF_CTRL1_RECOVER_ON_UNDERFLOW ((uint32_t)1 << 24)
#define LCDIF_CTRL1_INTERLACE_FIELDS ((uint32_t)1 << 23)
#define LCDIF_CTRL1_START_INTERLACE_FROM_SECOND_FIELD  ((uint32_t)1 << 22)
#define LCDIF_CTRL1_FIFO_CLEAR ((uint32_t)1 << 21)
#define LCDIF_CTRL1_IRQ_ON_ALTERNATE_FIELDS ((uint32_t)1 << 20)
#define LCDIF_CTRL1_BYTE_PACKING_FORMAT(n) ((uint32_t)(((n) & 0xF) << 16))
#define LCDIF_CTRL1_OVERFLOW_IRQ_EN ((uint32_t)1 << 15)
#define LCDIF_CTRL1_UNDERFLOW_IRQ_EN  ((uint32_t)1 << 14)
#define LCDIF_CTRL1_CUR_FRAME_DONE_IRQ_EN  ((uint32_t)1 << 13)
#define LCDIF_CTRL1_VSYNC_EDGE_IRQ_EN  ((uint32_t)1 << 12)
#define LCDIF_CTRL1_OVERFLOW_IRQ  ((uint32_t)1 << 11)
#define LCDIF_CTRL1_UNDERFLOW_IRQ  ((uint32_t)1 << 10)
#define LCDIF_CTRL1_CUR_FRAME_DONE_IRQ  ((uint32_t)1 << 9)
#define LCDIF_CTRL1_VSYNC_EDGE_IRQ  ((uint32_t)1 << 8)
#define LCDIF_TRANSFER_COUNT_V_COUNT(n) ((uint32_t)(((n) & 0xFFFF) << 16))
#define LCDIF_TRANSFER_COUNT_H_COUNT(n) ((uint32_t)(((n) & 0xFFFF) << 0))
#define LCDIF_VDCTRL0_ENABLE_PRESENT ((uint32_t)1 << 28)
#define LCDIF_VDCTRL0_VSYNC_POL ((uint32_t)1 << 27)
#define LCDIF_VDCTRL0_HSYNC_POL ((uint32_t)1 << 26)
#define LCDIF_VDCTRL0_DOTCLK_POL ((uint32_t)1 << 25)
#define LCDIF_VDCTRL0_ENABLE_POL  ((uint32_t)1 << 24)
#define LCDIF_VDCTRL0_VSYNC_PERIOD_UNIT ((uint32_t)1 << 21)
#define LCDIF_VDCTRL0_VSYNC_PULSE_WIDTH_UNIT ((uint32_t)1 << 20)
#define LCDIF_VDCTRL0_HALF_LINE ((uint32_t)1 << 19)
#define LCDIF_VDCTRL0_HALF_LINE_MODE ((uint32_t)1 << 18)
#define LCDIF_VDCTRL0_VSYNC_PULSE_WIDTH(n) ((uint32_t)(((n) & 0x3FFFF) << 0))
#define LCDIF_VDCTRL2_HSYNC_PULSE_WIDTH(n) ((uint32_t)(((n) & 0x3FFF) << 18))
#define LCDIF_VDCTRL2_HSYNC_PERIOD(n)  ((uint32_t)(((n) & 0x3FFFF) << 0))
#define LCDIF_VDCTRL3_MUX_SYNC_SIGNALS ((uint32_t)1 << 29)
#define LCDIF_VDCTRL3_VSYNC_ONLY ((uint32_t)1 << 28)
#define LCDIF_VDCTRL3_HORIZONTAL_WAIT_CNT(n) ((uint32_t)(((n) & 0xFFF) << 16))
#define LCDIF_VDCTRL3_VERTICAL_WAIT_CNT(n) ((uint32_t)(((n) & 0xFFFF) << 0))
#define LCDIF_VDCTRL4_DOTCLK_DLY_SEL(n) ((uint32_t)(((n) & 0x7) << 29))
#define LCDIF_VDCTRL4_SYNC_SIGNALS_ON ((uint32_t)1 << 18)
#define LCDIF_VDCTRL4_DOTCLK_H_VALID_DATA_CNT(n)  ((uint32_t)(((n) & 0xFFFF) << 0))

#define IMXRT_LCDIF_LUT		(*(IMXRT_REGISTER32_t *)(IMXRT_LCDIF_ADDRESS+0xb00))
#define LCDIF_LUT_CTRL		(IMXRT_LCDIF_LUT.offset000)
#define LCDIF_LUT0_ADDR		(IMXRT_LCDIF_LUT.offset010)
#define LCDIF_LUT0_DATA		(IMXRT_LCDIF_LUT.offset020)
#define LCDIF_LUT1_ADDR		(IMXRT_LCDIF_LUT.offset030)
#define LCDIF_LUT1_DATA		(IMXRT_LCDIF_LUT.offset040)

typedef struct {
  uint32_t height;
  uint32_t vfp; // vertical front porch
  uint32_t vsw; // vertical sync width
  uint32_t vbp; // vertical back porch
  uint32_t width;
  uint32_t hfp; // horizontal front porch
  uint32_t hsw; // horizontal sync width
  uint32_t hbp; // horizontal back porch
  // clk_num * 24MHz / clk_den = pixel clock
  uint32_t clk_num; // pix_clk numerator
  uint32_t clk_den; // pix_clk denominator
  uint32_t vpolarity; // 0 (active low vsync/negative) or LCDIF_VDCTRL0_VSYNC_POL (active high/positive)
  uint32_t hpolarity; // 0 (active low hsync/negative) or LCDIF_VDCTRL0_HSYNC_POL (active high/positive)
} vga_timing;

const vga_timing t1920x1080x60 = {1080, 4, 5, 36, 1920, 88, 44, 148, 297, 48, LCDIF_VDCTRL0_VSYNC_POL, LCDIF_VDCTRL0_HSYNC_POL};
// as above but with half clock speed
const vga_timing t1920x1080x30 = {1080, 4, 5, 36, 1920, 88, 44, 148, 297, 96, LCDIF_VDCTRL0_VSYNC_POL, LCDIF_VDCTRL0_HSYNC_POL};
const vga_timing t1680x1050x60 = {1050, 1, 3, 33, 1680, 104, 184, 288, 6125, 999, LCDIF_VDCTRL0_VSYNC_POL, 0};
const vga_timing t1280x1024x60 = {1024, 1, 3, 38, 1280, 48, 112, 248, 108, 24, LCDIF_VDCTRL0_VSYNC_POL, LCDIF_VDCTRL0_HSYNC_POL};
const vga_timing t1280x720x60 = {720, 13, 5, 12, 1280, 80, 40, 248, 7425, 2400, LCDIF_VDCTRL0_VSYNC_POL, LCDIF_VDCTRL0_HSYNC_POL};
const vga_timing t1024x768x60 = {768, 3, 6, 29, 1024, 24, 136, 160, 65, 24, 0, 0};

const vga_timing t800x600x100 =  {600, 1, 3, 32, 800, 48, 88, 136, 6818, 2400, LCDIF_VDCTRL0_VSYNC_POL, 0};
const vga_timing t800x600x60 =   {600, 1, 4, 23, 800, 40, 128, 88, 40, 24, LCDIF_VDCTRL0_VSYNC_POL, LCDIF_VDCTRL0_HSYNC_POL};
const vga_timing t640x480x60 =   {480, 10, 2, 33, 640, 16, 96, 48, 150, 143, 0, 0};
const vga_timing t640x400x70 =   {400, 12, 2, 35, 640, 16, 96, 48, 150, 143, LCDIF_VDCTRL0_VSYNC_POL, 0};
const vga_timing t640x350x70 =   {350, 37, 2, 60, 640, 16, 96, 48, 150, 143, 0, LCDIF_VDCTRL0_HSYNC_POL};

// select desired mode here
#define timing t1920x1080x60
uint32_t semc_clk = SEMC_CLOCK_CPU_DIV_3;

// PIN OUTPUTS FOR TEENSY 4.1
// red/green/blue: two-thirds of 0.7V
#define VGA_RED 6
#define VGA_BLUE 9
#define VGA_GREEN 32
// intensity: one-third of 0.7V
#define VGA_INTENSITY 7
// sync signals = TTL
#define VGA_HSYNC 11   // connect sync signals to VGA via 68R
#define VGA_VSYNC 13

/* R2R ladder:
 *
 * GROUND <------------- 536R ----*---- 270R ---*-----------> VGA PIN (1/2/3)
 *                                |             |
 * INTENSITY (7/8/7) <---536R ----/             |
 *                                              |
 * COLOR (6/32/9)  <-----536R-------------------/
 */


// defined using max dimensions due to laziness
// LCDIF framebuffers must be 64-byte aligned
typedef uint8_t framebuffer_t[1920*1080] __attribute__((aligned(64)));
static uint8_t* s_frameBuffer[2];

static volatile bool s_frameDone = false;

static void LCDIF_ISR(void) {
  uint32_t intStatus = LCDIF_CTRL1 & (LCDIF_CTRL1_BM_ERROR_IRQ | LCDIF_CTRL1_OVERFLOW_IRQ | LCDIF_CTRL1_UNDERFLOW_IRQ | LCDIF_CTRL1_CUR_FRAME_DONE_IRQ | LCDIF_CTRL1_VSYNC_EDGE_IRQ);
  // clear all pending LCD interrupts
  LCDIF_CTRL1_CLR = intStatus;

  if (intStatus & (LCDIF_CTRL1_CUR_FRAME_DONE_IRQ | LCDIF_CTRL1_VSYNC_EDGE_IRQ)) {
    s_frameDone = true;
  }

  asm volatile("dsb");
}

// num,den = desired pix_clk as a ratio of 24MHz
FLASHMEM static void set_vid_clk(int num, int den) {
  int post_divide = 0;
  while (num < 27*den) num <<= 1, ++post_divide;
  int div_select = num / den;
  num -= div_select * den;

  // div_select valid range: 27-54
  float freq = ((float)num / den + div_select) * 24.0f / (1 << post_divide);
  Serial.print("VID_PLL: ");
  Serial.print(freq);
  Serial.print("Mhz, div_select: ");
  Serial.println(div_select);

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
  Serial.print("Waiting for PLL Lock...");
  while (!(CCM_ANALOG_PLL_VIDEO & CCM_ANALOG_PLL_VIDEO_LOCK));
  // deactivate bypass
  CCM_ANALOG_PLL_VIDEO_CLR = CCM_ANALOG_PLL_VIDEO_BYPASS;
  Serial.println("done.");


  Serial.print("Configuring LCD pix_clk source...");
  // gate clocks from lcd
  CCM_CCGR2 &= ~CCM_CCGR2_LCD(CCM_CCGR_ON);
  CCM_CCGR3 &= ~CCM_CCGR3_LCDIF_PIX(CCM_CCGR_ON);
  // set LCDIF source to PLL5, pre-divide by 4
  uint32_t r = CCM_CSCDR2;
  r &= ~(CCM_CSCDR2_LCDIF_PRE_CLK_SEL(7) | CCM_CSCDR2_LCDIF_PRED(7));
  r |= CCM_CSCDR2_LCDIF_PRE_CLK_SEL(2) | CCM_CSCDR2_LCDIF_PRED(3);
  CCM_CSCDR2 = r;
  // set LCDIF post-divide to 1
  CCM_CBCMR &= ~CCM_CBCMR_LCDIF_PODF(7);
  CCM_CCGR2 |= CCM_CCGR2_LCD(CCM_CCGR_ON);
  CCM_CCGR3 |= CCM_CCGR3_LCDIF_PIX(CCM_CCGR_ON);
  Serial.println("done.");
}

FLASHMEM static void init_lcd(const vga_timing* vid) {
  // mux pins for LCD module. We don't care about ENABLE or DOTCLK for VGA.
  Serial.println("Setting pins");
  /* *(portConfigRegister(VGA_RED)) */ IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_10 = 0;
  /* *(portConfigRegister(VGA_GREEN)) */ IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_11 = 0;
  /* *(portConfigRegister(VGA_BLUE)) */ IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12 = 0;
  /* *(portConfigRegister(VGA_VSYNC)) */ IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 0;
  /* *(portConfigRegister(VGA_HSYNC)) */ IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 0;
  /* *(portConfigRegister(VGA_INTENSITY)) */ IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 = 0;

  Serial.print("Resetting LCDIF...");
  // reset LCDIF
  // ungate clock and wait for it to clear
  LCDIF_CTRL_CLR = LCDIF_CTRL_CLKGATE;
  while (LCDIF_CTRL & LCDIF_CTRL_CLKGATE);
  Serial.print("poking reset...");
  /* trigger reset, wait for clock gate to enable - this is what the manual says to do...
   * but it doesn't work; the clock gate never re-activates, at least not in the register
   * so the best we can do is to make sure the reset flag is reflected and assume it's done the job
   */
  LCDIF_CTRL_SET = LCDIF_CTRL_SFTRST;
  while (!(LCDIF_CTRL & LCDIF_CTRL_SFTRST));
  
  Serial.print("re-enabling clock...");
  // clear reset and ungate clock again
  LCDIF_CTRL_CLR = LCDIF_CTRL_SFTRST | LCDIF_CTRL_CLKGATE;
  Serial.println("done.");

  Serial.print("Initializing LCDIF registers...");
  // 8 bits in, using LUT
  LCDIF_CTRL = LCDIF_CTRL_WORD_LENGTH(1) | LCDIF_CTRL_LCD_DATABUS_WIDTH(1) | LCDIF_CTRL_DOTCLK_MODE | LCDIF_CTRL_BYPASS_COUNT | LCDIF_CTRL_MASTER;
  // recover on underflow = garbage will be displayed if memory is too slow, but at least it keeps running instead of aborting
  LCDIF_CTRL1 = LCDIF_CTRL1_RECOVER_ON_UNDERFLOW | LCDIF_CTRL1_BYTE_PACKING_FORMAT(15);
  LCDIF_TRANSFER_COUNT = LCDIF_TRANSFER_COUNT_V_COUNT(vid->height) | LCDIF_TRANSFER_COUNT_H_COUNT(vid->width);
  // set vsync and hsync signal polarity (depends on mode/resolution), vsync length
  LCDIF_VDCTRL0 = LCDIF_VDCTRL0_ENABLE_PRESENT | LCDIF_VDCTRL0_VSYNC_PERIOD_UNIT | LCDIF_VDCTRL0_VSYNC_PULSE_WIDTH_UNIT | LCDIF_VDCTRL0_VSYNC_PULSE_WIDTH(vid->vsw) | vid->vpolarity | vid->hpolarity;
  // total lines
  LCDIF_VDCTRL1 = vid->height+vid->vfp+vid->vsw+vid->vbp;
  // hsync length, line = width+HBP+HSW+HFP
  LCDIF_VDCTRL2 = LCDIF_VDCTRL2_HSYNC_PULSE_WIDTH(vid->hsw) | LCDIF_VDCTRL2_HSYNC_PERIOD(vid->width+vid->hfp+vid->hsw+vid->hbp);
  // horizontal wait = back porch + sync, vertical wait = back porch + sync
  LCDIF_VDCTRL3 = LCDIF_VDCTRL3_HORIZONTAL_WAIT_CNT(vid->hsw+vid->hbp) | LCDIF_VDCTRL3_VERTICAL_WAIT_CNT(vid->vsw+vid->vbp);
  LCDIF_VDCTRL4 = LCDIF_VDCTRL4_SYNC_SIGNALS_ON | LCDIF_VDCTRL4_DOTCLK_H_VALID_DATA_CNT(vid->width);
  Serial.println("done.");
}

FLASHMEM static void InitLUT(void) {
  /* index 6 in the "classic" 16-color palette should be brown instead of dark yellow.
   * The values of green and green-intensity are swapped for this entry to accomplish this
   */
   
  // bits in palette entries match the LCD_DATA* signals
  static const uint32_t red = 1 << 6;                        // red = pin 6
  static const uint32_t green = 1 << 7;                      // green = pin 32
  static const uint32_t blue = 1 << 8;                       // blue = pin 9
  static const uint32_t intensity_g = 1 << 12;               // intensity_g = pin 8
  static const uint32_t intensity = (1 << 13) | intensity_g; // intensity_rb = pin 7

  /* index 0 is output during blanking period!
   * The CRT monitor uses that time to measure black levels, if color
   * lines are active their voltage levels will be sampled as the
   * new ground/black reference.
   * tl,dr: index 0 should ALWAYS be black.
   */

  PROGMEM static const uint32_t fgColorTable[16] = {0, blue, green, blue|green, red, red|blue, red|green, red|green|blue,
                                            intensity, intensity|blue, intensity|green, intensity|blue|green, intensity|red, intensity|red|blue, intensity|red|green, intensity|red|green|blue};

  // clear LUT1, just to ensure we don't accidentally end up using it
  // (LUT1 is used when the lowest bit of the framebuffer address is set)
  LCDIF_LUT0_ADDR = 0;
  LCDIF_LUT1_ADDR = 0;
  for (size_t i=0; i < (sizeof(fgColorTable)/sizeof(fgColorTable[0])); i++) {
    LCDIF_LUT0_DATA = fgColorTable[i];
    LCDIF_LUT1_DATA = 0;
  }
  // activate LUT
  LCDIF_LUT_CTRL = 0;
}

// draws scrolling colorbars
static void FillFrameBuffer(uint8_t *fb) {
  int height = timing.height;
  int width = timing.width;
  size_t pitch = width;

  const int radius = height/6 - 10;
  static int xoff = radius;
  static int yoff = radius;
  static int xdir = 4;
  static int ydir = 2;
  static uint8_t bg = 8;
  static uint8_t fg = 8;
  const int limit = radius*radius;

  bool hit = false;
  if (xoff >= width-radius) {
    hit = true;
    xdir = -xdir;
    xoff = width-radius;
  }
  if (xoff <= radius) {
    hit = true;
    xdir = -xdir;
    xoff = radius;
  }
  if (yoff >= height-radius) {
    hit = true;
    ydir = -ydir;
    yoff = height-radius;
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
      uint8_t c;
      int xdiff = x-xoff;
      int ydiff = y-yoff;
      c = ((xdiff*xdiff + ydiff*ydiff) <= limit) ? fg : (bg/3);
      x++;
      *p++ = c;
    }
    arm_dcache_flush(fb, (p-fb));
    fb += pitch;
  }

  xoff += xdir;
  yoff += ydir;
}

void setup() {
  Serial.begin(0);

  s_frameBuffer[0] = (uint8_t*)extmem_base;
  s_frameBuffer[1] = (uint8_t*)extmem_base + sizeof(framebuffer_t);

  set_vid_clk(4*timing.clk_num,timing.clk_den);
  init_lcd(&timing);

  LCDIF_CUR_BUF = (uint32_t)s_frameBuffer[1];
  LCDIF_NEXT_BUF = (uint32_t)s_frameBuffer[0];

  Serial.println("Enabling LCDIF interrupt");
  attachInterruptVector(IRQ_LCDIF, LCDIF_ISR);
  NVIC_SET_PRIORITY(IRQ_LCDIF, 32);
  NVIC_ENABLE_IRQ(IRQ_LCDIF);

  InitLUT();

  FillFrameBuffer(s_frameBuffer[0]);
  FillFrameBuffer(s_frameBuffer[1]);

  Serial.println("Unmasking frame interrupt");
  // unmask CUR_FRAME_DONE interrupt
  LCDIF_CTRL1_SET = LCDIF_CTRL1_CUR_FRAME_DONE_IRQ_EN;
  // VSYNC_EDGE interrupt also available to notify beginning of raster
  //LCDIF_CTRL1_SET = LCDIF_CTRL1_VSYNC_EDGE_IRQ_EN;
  Serial.println("Running LCD");
  // start LCD
  LCDIF_CTRL_SET = LCDIF_CTRL_RUN | LCDIF_CTRL_DOTCLK_MODE;
}

void loop() {
  static uint32_t nextBufferIndex;
  if (s_frameDone) {
    nextBufferIndex ^= 1;

    // this is the buffer that just finished, redraw it
    FillFrameBuffer(s_frameBuffer[nextBufferIndex]);
    // queue for display
    LCDIF_NEXT_BUF = (uint32_t)s_frameBuffer[nextBufferIndex];
    s_frameDone = false;
  }
}
