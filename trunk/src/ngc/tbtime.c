#include "tbtime.h"

unsigned long
tb_diff_msec (tb_t * end, tb_t * start)
{
  unsigned long upper, lower;
  upper = end->u - start->u;
  if (start->l > end->l)
    upper--;
  lower = end->l - start->l;
  return ((upper * ((unsigned long) 0x80000000 / (TB_CLOCK / 2000))) +
          (lower / (TB_CLOCK / 1000)));
}


