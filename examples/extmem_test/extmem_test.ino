#include <SDRAM.h>

bool memory_ok = false;
uint32_t *memory_begin, *memory_end;
size_t psram_size;
float psram_sizeMB;

uint32_t lcg_step;
uint32_t lcg_next(uint32_t old) {
  return (lcg_step * old + 1) % (psram_size / sizeof(uint32_t));
}

bool check_fixed_pattern(uint32_t pattern);
bool check_lfsr_pattern(uint32_t seed);

uint32_t rTCnt;

static bool run_all_tests(void) {
  rTCnt = 0;
  elapsedMillis msec = 0;
  if (!check_fixed_pattern(0x5A698421)) return false;
  if (!check_fixed_pattern(0x55555555)) return false;
  if (!check_fixed_pattern(0x33333333)) return false;
  if (!check_fixed_pattern(0x0F0F0F0F)) return false;
  if (!check_fixed_pattern(0x00FF00FF)) return false;
  if (!check_fixed_pattern(0x0000FFFF)) return false;
  if (!check_fixed_pattern(0xAAAAAAAA)) return false;
  if (!check_fixed_pattern(0xCCCCCCCC)) return false;
  if (!check_fixed_pattern(0xF0F0F0F0)) return false;
  if (!check_fixed_pattern(0xFF00FF00)) return false;
  if (!check_fixed_pattern(0xFFFF0000)) return false;
  if (!check_fixed_pattern(0xFFFFFFFF)) return false;
  if (!check_fixed_pattern(0x00000000)) return false;
  if (!check_lfsr_pattern(2976674124ul)) return false;
  if (!check_lfsr_pattern(1438200953ul)) return false;
  if (!check_lfsr_pattern(3413783263ul)) return false;
  if (!check_lfsr_pattern(1900517911ul)) return false;
  if (!check_lfsr_pattern(1227909400ul)) return false;
  if (!check_lfsr_pattern(276562754ul)) return false;
  if (!check_lfsr_pattern(146878114ul)) return false;
  if (!check_lfsr_pattern(615545407ul)) return false;
  if (!check_lfsr_pattern(110497896ul)) return false;
  if (!check_lfsr_pattern(74539250ul)) return false;
  if (!check_lfsr_pattern(4197336575ul)) return false;
  if (!check_lfsr_pattern(2280382233ul)) return false;
  if (!check_lfsr_pattern(542894183ul)) return false;
  if (!check_lfsr_pattern(3978544245ul)) return false;
  if (!check_lfsr_pattern(2315909796ul)) return false;
  if (!check_lfsr_pattern(3736286001ul)) return false;
  if (!check_lfsr_pattern(2876690683ul)) return false;
  if (!check_lfsr_pattern(215559886ul)) return false;
  if (!check_lfsr_pattern(539179291ul)) return false;
  if (!check_lfsr_pattern(537678650ul)) return false;
  if (!check_lfsr_pattern(4001405270ul)) return false;
  if (!check_lfsr_pattern(2169216599ul)) return false;
  if (!check_lfsr_pattern(4036891097ul)) return false;
  if (!check_lfsr_pattern(1535452389ul)) return false;
  if (!check_lfsr_pattern(2959727213ul)) return false;
  if (!check_lfsr_pattern(4219363395ul)) return false;
  if (!check_lfsr_pattern(1036929753ul)) return false;
  if (!check_lfsr_pattern(2125248865ul)) return false;
  if (!check_lfsr_pattern(3177905864ul)) return false;
  if (!check_lfsr_pattern(2399307098ul)) return false;
  if (!check_lfsr_pattern(3847634607ul)) return false;
  if (!check_lfsr_pattern(27467969ul)) return false;
  if (!check_lfsr_pattern(520563506ul)) return false;
  if (!check_lfsr_pattern(381313790ul)) return false;
  if (!check_lfsr_pattern(4174769276ul)) return false;
  if (!check_lfsr_pattern(3932189449ul)) return false;
  if (!check_lfsr_pattern(4079717394ul)) return false;
  if (!check_lfsr_pattern(868357076ul)) return false;
  if (!check_lfsr_pattern(2474062993ul)) return false;
  if (!check_lfsr_pattern(1502682190ul)) return false;
  if (!check_lfsr_pattern(2471230478ul)) return false;
  if (!check_lfsr_pattern(85016565ul)) return false;
  if (!check_lfsr_pattern(1427530695ul)) return false;
  if (!check_lfsr_pattern(1100533073ul)) return false;
  float mend = msec;
  Serial.printf(" test ran for %.2f seconds\n", mend / 1000.0f);
  Serial.printf(" %d MBs test ran at %.2f MB/sec overall\n", (int)(2 * rTCnt * psram_sizeMB), 2 * rTCnt * 1e3 * psram_sizeMB / mend);
  return true;
}

void setup()
{
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWriteFast(LED_BUILTIN,LOW);

  if (CrashReport) CrashReport.printTo(Serial);

  memory_begin = (uint32_t *)extmem_base;
  psram_size = extmem_size * 1024 * 1024;
  memory_end = memory_begin + (psram_size / sizeof(uint32_t));
  psram_sizeMB = (float)extmem_size;

  while (!Serial) ; // wait
  uint8_t size = extmem_size;
  Serial.printf("\n\nEXTMEM Memory Test, %d Mbyte\n", size);
  if (size == 0) return;

  Serial.printf("EXTMEM frequency: %.2f MHz\n", extmem_freq() / 1e6f);

  lcg_step = 1;
  Serial.println("Beginning sequential access tests");
  if (run_all_tests() == false) return;

  Serial.println("Beginning random access tests");
  lcg_step = 111141;
  if (run_all_tests() == false) return;

  Serial.println("All memory tests passed :-)");
  memory_ok = true;
  digitalWriteFast(LED_BUILTIN,HIGH);
}

bool fail_message(uint32_t *location, uint32_t actual, uint32_t expected)
{
  Serial.printf(" Error at %08X, read %08X but expected %08X\n",
                (uint32_t)location, actual, expected);
  return false;
}

// fill the entire RAM with a fixed pattern, then check it
bool check_fixed_pattern(uint32_t pattern)
{
  Serial.printf("testing with fixed pattern %08X\t", pattern);
  uint32_t rTime = micros();
  uint32_t p = lcg_step;
  do {
    memory_begin[p] = pattern;
    p = lcg_next(p);
  } while (p != lcg_step);
  rTime = micros() - rTime;
  Serial.printf( "\tfill us:%d MB/s:%.2f\t", rTime, 1e6 * psram_sizeMB / rTime );
  arm_dcache_flush_delete((void *)memory_begin,(uint32_t)memory_end - (uint32_t)memory_begin);
  rTime = micros();
  do {
    uint32_t actual = memory_begin[p];
    if (actual != pattern) return fail_message(memory_begin+p, actual, pattern);
    p = lcg_next(p);
  } while (p != lcg_step);
  rTime = micros() - rTime;
  Serial.printf( "\ttest us:%d MB/s:%.2f\n", rTime, 1e6 * psram_sizeMB / rTime );
  rTCnt++;
  return true;
}

// fill the entire RAM with a pseudo-random sequence, then check it
bool check_lfsr_pattern(uint32_t seed)
{
  uint32_t p;
  uint32_t reg;

  Serial.printf("testing with pseudo-random sequence, seed=%u\t", seed);
  reg = seed;
  uint32_t rTime = micros();
  p = lcg_step;
  do {
    memory_begin[p] = reg;
    for (int i = 0; i < 3; i++) {
      if (reg & 1) {
        reg >>= 1;
        reg ^= 0x7A5BC2E3;
      } else {
        reg >>= 1;
      }
    }
    p = lcg_next(p);
  } while (p != lcg_step);
  rTime = micros() - rTime;
  Serial.printf( "\tfill us:%d MB/s:%.2f\t", rTime, 1e6 * psram_sizeMB / rTime );
  arm_dcache_flush_delete((void *)memory_begin,(uint32_t)memory_end - (uint32_t)memory_begin);
  reg = seed;
  rTime = micros();
  do {
    uint32_t actual = memory_begin[p];
    if (actual != reg) return fail_message(memory_begin+p, actual, reg);
    for (int i = 0; i < 3; i++) {
      if (reg & 1) {
        reg >>= 1;
        reg ^= 0x7A5BC2E3;
      } else {
        reg >>= 1;
      }
    }
    p = lcg_next(p);
  } while (p != lcg_step);
  rTime = micros() - rTime;
  Serial.printf( "\ttest us:%d MB/s:%.2f\n", rTime, 1e6 * psram_sizeMB / rTime );
  rTCnt++;
  return true;
}

void loop()
{
  delay(100);
  if (!memory_ok) digitalToggleFast(LED_BUILTIN); // rapid blink if any test fails
  delay(100);
}
