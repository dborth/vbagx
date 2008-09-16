/****************************************************************************
* tmbinc msec timer
****************************************************************************/
#ifndef __TMBINCTIMER__
#define __TMBINCTIMER__
#ifdef HW_RVL
#define TB_CLOCK  60750000 //WII
#else
#define TB_CLOCK  40500000
#endif
#define mftb(rval) ({unsigned long u; do { \
         asm volatile ("mftbu %0" : "=r" (u)); \
         asm volatile ("mftb %0" : "=r" ((rval)->l)); \
         asm volatile ("mftbu %0" : "=r" ((rval)->u)); \
         } while(u != ((rval)->u)); })

typedef struct
  {
    unsigned long l, u;
  }
tb_t;

unsigned long tb_diff_msec (tb_t * end, tb_t * start);

#endif

