#include "kernel/types.h"
#include "user.h"
#include "date.h"

static int is_leap_year(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void format_two_digits(int num, char* buf) {
    buf[0] = (num < 10) ? '0' : ('0' + num / 10);
    buf[1] = ('0' + num % 10);
  }
  

static void format_nanoseconds(uint nanoseconds, char* buf) {
  for (int i = 8; i >= 0; i--) {
    buf[i] = '0' + (nanoseconds % 10);
    nanoseconds /= 10;
  }
  buf[9] = '\0';
}

void format_number(int num, char* buf) {
    if (num < 10) {
      buf[0] = '0';
      buf[1] = '0' + num;
    } else {
      buf[0] = '0' + (num / 10);
      buf[1] = '0' + (num % 10);
    }
    buf[2] = '\0';
  }

void convert_seconds_to_date(uint64 seconds, struct rtcdate *d) {
  uint64 days = seconds / 86400;
  uint64 secs_remaining = seconds % 86400;

  d->hour = secs_remaining / 3600;
  secs_remaining %= 3600;
  d->minute = secs_remaining / 60;
  d->second = secs_remaining % 60;

  int year = 1970;
  while (1) {
    int diy = is_leap_year(year) ? 366 : 365;
    if (days < diy) break;
    days -= diy;
    year++;
  }
  d->year = year;

  int month;
  for (month = 0; month < 12; month++) {
    int dim = days_in_month[month];
    if (month == 1 && is_leap_year(year)) dim++;
    if (days < dim) break;
    days -= dim;
  }
  d->month = month + 1;
  d->day = days + 1;
}

int main() {
    uint64 ns;
    if (rtctime(&ns) != 0) {
      printf("date: error\n");
      exit(0);
    }
  
    struct rtcdate d;
    convert_seconds_to_date(ns / 1000000000, &d);
    uint nanoseconds = ns % 1000000000;
  
    char year[5], month[3], day[3], hour[3], minute[3], second[3], nanos[10];
    
    for (int i = 3; i >= 0; i--) {
      year[i] = '0' + d.year % 10;
      d.year /= 10;
    }
    year[4] = '\0';
  
    format_two_digits(d.month, month); month[2] = '\0';
    format_two_digits(d.day, day);     day[2] = '\0';
    format_two_digits(d.hour, hour);   hour[2] = '\0';
    format_two_digits(d.minute, minute); minute[2] = '\0';
    format_two_digits(d.second, second); second[2] = '\0';
  
    format_nanoseconds(nanoseconds, nanos);
  
    printf("%s-%s-%s %s:%s:%s.%s\n", 
           year, month, day, 
           hour, minute, second, 
           nanos);
  exit(0);
}
