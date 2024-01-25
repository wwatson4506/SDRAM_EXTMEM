#ifndef _SDRAM_H_
#define _SDRAM_H_

#include "Arduino.h"

#define SEMC_CLOCK_133  (CCM_CBCDR_SEMC_CLK_SEL | CCM_CBCDR_SEMC_ALT_CLK_SEL | CCM_CBCDR_SEMC_PODF(4))
#define SEMC_CLOCK_166  (CCM_CBCDR_SEMC_CLK_SEL | CCM_CBCDR_SEMC_ALT_CLK_SEL | CCM_CBCDR_SEMC_PODF(3))
#define SEMC_CLOCK_221  (CCM_CBCDR_SEMC_CLK_SEL | CCM_CBCDR_SEMC_ALT_CLK_SEL | CCM_CBCDR_SEMC_PODF(2))
#define SEMC_CLOCK_198  (CCM_CBCDR_SEMC_CLK_SEL | CCM_CBCDR_SEMC_PODF(1))
#define SEMC_CLOCK_CPU_DIV_4 (CCM_CBCDR_SEMC_PODF(3))
#define SEMC_CLOCK_CPU_DIV_3 (CCM_CBCDR_SEMC_PODF(2))


#ifdef __cplusplus
extern "C" {
#endif

extern void* extmem_base;
extern size_t extmem_size;
extern float extmem_freq();
// this is a weak symbol, can be overridden to one of the speeds above
extern uint32_t semc_clk;

#ifdef __cplusplus
}
#endif

#endif