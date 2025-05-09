#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

static inline uint32 rtc_read_low() {
  return *(volatile uint32*)RTC_LOW;
}

static inline uint32 rtc_read_high() {
  return *(volatile uint32*)RTC_HIGH;
}

static struct spinlock rtclock;

void rtcinit(void) {
  initlock(&rtclock, "rtc");
}

uint64 rtc_read_time() {
  uint32 low, high;

  acquire(&rtclock);

  do {
    high = rtc_read_high();
    low = rtc_read_low();
  } while (high != rtc_read_high()); 

  release(&rtclock);

  return ((uint64)high << 32) | low;
}