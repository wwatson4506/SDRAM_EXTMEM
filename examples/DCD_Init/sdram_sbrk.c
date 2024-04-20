#include <errno.h>

// override default _sbrk to use SDRAM as heap

static const unsigned long _heap_start = 0x80000000;
static const unsigned long _heap_end = 0x82000000;

static char* __brkval = (char*)_heap_start;

void* _sbrk(int incr)
{
  char *new_brk = __brkval + incr;
  if (new_brk < (char*)_heap_start || new_brk > (char*)_heap_end) {
    errno = ENOMEM;
    return (void*)-1;
  }
  char *prev = __brkval;
  __brkval = new_brk;
  return prev;
}
