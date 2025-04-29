#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

static inline uint32 rtc_read_low() {
  return *(volatile uint32*)RTC_LOW;
}

static inline uint32 rtc_read_high() {
  return *(volatile uint32*)RTC_HIGH;
}

uint64 rtc_read_time() {
  uint32 low, high;
  do {
    high = rtc_read_high();
    low = rtc_read_low();
  } while (high != rtc_read_high()); 
  return ((uint64)high << 32) | low;
}