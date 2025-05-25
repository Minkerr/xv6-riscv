/**
 * @file date.c
 * @brief Утилита для отображения текущей даты и времени
 *
 * Эта программа получает текущее время из RTC в виде 64-битного значения
 * наносекунд с 1 января 1970 года, преобразует его в человекочитаемый формат
 * и выводит на экран в виде строки "YYYY-MM-DD HH:MM:SS.NNNNNNNNN".
 */

#include "kernel/types.h"  // Базовые типы данных
#include "user.h"          // Пользовательские функции
#include "date.h"          // Определение структуры rtcdate

/**
 * @brief Проверяет, является ли год високосным
 *
 * Год является високосным, если он делится на 4 и при этом не делится на 100,
 * либо делится на 400.
 *
 * @param year Год для проверки
 * @return 1, если год високосный, 0 в противном случае
 */
static int is_leap_year(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

/**
 * @brief Массив с количеством дней в каждом месяце (для невисокосного года)
 */
static int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 * @brief Форматирует двузначное число в строку
 *
 * Преобразует число в строку из двух символов, добавляя ведущий ноль при необходимости.
 *
 * @param num Число для форматирования (0-99)
 * @param buf Буфер для записи результата (должен иметь размер не менее 2 байт)
 */
static void format_two_digits(int num, char* buf) {
    buf[0] = (num < 10) ? '0' : ('0' + num / 10);
    buf[1] = ('0' + num % 10);
  }
  

/**
 * @brief Форматирует наносекунды в строку
 *
 * Преобразует значение наносекунд в строку из 9 символов,
 * добавляя ведущие нули при необходимости.
 *
 * @param nanoseconds Значение наносекунд (0-999999999)
 * @param buf Буфер для записи результата (должен иметь размер не менее 10 байт)
 */
static void format_nanoseconds(uint nanoseconds, char* buf) {
  for (int i = 8; i >= 0; i--) {
    buf[i] = '0' + (nanoseconds % 10);
    nanoseconds /= 10;
  }
  buf[9] = '\0';  // Добавляем завершающий нулевой символ
}

/**
 * @brief Форматирует число в строку из двух символов
 *
 * Преобразует число в строку из двух символов, добавляя ведущий ноль при необходимости.
 * Функция аналогична format_two_digits, но добавляет завершающий нулевой символ.
 *
 * @param num Число для форматирования (0-99)
 * @param buf Буфер для записи результата (должен иметь размер не менее 3 байт)
 */
void format_number(int num, char* buf) {
    if (num < 10) {
      buf[0] = '0';
      buf[1] = '0' + num;
    } else {
      buf[0] = '0' + (num / 10);
      buf[1] = '0' + (num % 10);
    }
    buf[2] = '\0';  // Добавляем завершающий нулевой символ
  }

/**
 * @brief Преобразует количество секунд с начала эпохи в структуру даты и времени
 *
 * Функция преобразует количество секунд, прошедших с 1 января 1970 года,
 * в структуру rtcdate, содержащую год, месяц, день, час, минуту и секунду.
 *
 * @param seconds Количество секунд с начала эпохи
 * @param d Указатель на структуру rtcdate для записи результата
 */
void convert_seconds_to_date(uint64 seconds, struct rtcdate *d) {
  // Вычисляем количество полных дней и оставшихся секунд
  uint64 days = seconds / 86400;  // 86400 = 24 * 60 * 60 (секунд в сутках)
  uint64 secs_remaining = seconds % 86400;

  // Вычисляем часы, минуты и секунды
  d->hour = secs_remaining / 3600;
  secs_remaining %= 3600;
  d->minute = secs_remaining / 60;
  d->second = secs_remaining % 60;

  // Вычисляем год
  int year = 1970;  // Начальный год эпохи Unix
  while (1) {
    int diy = is_leap_year(year) ? 366 : 365;  // Дней в году
    if (days < diy) break;
    days -= diy;
    year++;
  }
  d->year = year;

  // Вычисляем месяц и день
  int month;
  for (month = 0; month < 12; month++) {
    int dim = days_in_month[month];  // Дней в месяце
    if (month == 1 && is_leap_year(year)) dim++;  // Февраль в високосном году
    if (days < dim) break;
    days -= dim;
  }
  d->month = month + 1;  // Месяцы нумеруются с 1
  d->day = days + 1;     // Дни нумеруются с 1
}

/**
 * @brief Основная функция программы
 *
 * Получает текущее время из RTC, преобразует его в человекочитаемый формат
 * и выводит на экран.
 *
 * @return Код завершения программы
 */
int main() {
    uint64 ns;  // Переменная для хранения времени в наносекундах
    
    // Получаем текущее время из RTC
    if (rtctime(&ns) != 0) {
      printf("date: error\n");
      exit(0);
    }
  
    struct rtcdate d;  // Структура для хранения даты и времени
    
    // Преобразуем наносекунды в секунды и затем в дату и время
    convert_seconds_to_date(ns / 1000000000, &d);
    
    // Получаем оставшиеся наносекунды
    uint nanoseconds = ns % 1000000000;
  
    // Буферы для форматирования компонентов даты и времени
    char year[5], month[3], day[3], hour[3], minute[3], second[3], nanos[10];
    
    // Форматируем год (4 цифры)
    for (int i = 3; i >= 0; i--) {
      year[i] = '0' + d.year % 10;
      d.year /= 10;
    }
    year[4] = '\0';  // Добавляем завершающий нулевой символ
  
    // Форматируем остальные компоненты даты и времени
    format_two_digits(d.month, month); month[2] = '\0';
    format_two_digits(d.day, day);     day[2] = '\0';
    format_two_digits(d.hour, hour);   hour[2] = '\0';
    format_two_digits(d.minute, minute); minute[2] = '\0';
    format_two_digits(d.second, second); second[2] = '\0';
  
    // Форматируем наносекунды
    format_nanoseconds(nanoseconds, nanos);
  
    // Выводим дату и время в формате "YYYY-MM-DD HH:MM:SS.NNNNNNNNN"
    printf("%s-%s-%s %s:%s:%s.%s\n", 
           year, month, day, 
           hour, minute, second, 
           nanos);
  exit(0);
}
