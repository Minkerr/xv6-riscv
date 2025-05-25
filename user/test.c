/**
 * @file test.c
 * @brief Тестовая программа для проверки работы псевдоустройств
 *
 * Эта программа тестирует работу четырех псевдоустройств:
 * - null: при чтении возвращает EOF, при записи игнорирует данные
 * - zero: при чтении возвращает нулевые байты, запись не допускается
 * - urandom: при чтении возвращает случайные байты, при записи меняет seed
 * - nullstat: при записи считает количество байт, при чтении возвращает счетчик
 */

#include "kernel/types.h"  // Базовые типы данных
#include "kernel/stat.h"   // Структуры статистики
#include "user/user.h"     // Пользовательские функции
#include "kernel/fcntl.h"  // Константы для открытия файлов

/**
 * @brief Тестирует работу устройства /dev/null
 *
 * Проверяет, что чтение из /dev/null возвращает 0 байт (EOF),
 * а запись в /dev/null успешно "записывает" все байты (игнорирует их).
 */
void
test_null(void)
{
  int fd;
  char buf[10];
  int n;
  
  printf("testing null device:\n");
  // Открываем устройство /dev/null для чтения и записи
  fd = open("null", O_RDWR);
  if(fd < 0) {
    printf("failed to open\n");
    return;
  }
  
  // Пытаемся прочитать данные из /dev/null
  // Должно вернуть 0 (EOF)
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  
  // Пытаемся записать данные в /dev/null
  // Должно вернуть количество "записанных" байт (4)
  n = write(fd, "test", 4);
  printf("%d\n\n", n);
  
  close(fd);
}

/**
 * @brief Тестирует работу устройства /dev/zero
 *
 * Проверяет, что чтение из /dev/zero возвращает нулевые байты,
 * а запись в /dev/zero не допускается (возвращает ошибку).
 */
void
test_zero(void)
{
  int fd;
  char buf[10];
  int n, i;
  
  printf("testing zero device:\n");
  // Открываем устройство /dev/zero для чтения и записи
  fd = open("zero", O_RDWR);
  if(fd < 0) {
    printf("failed to open\n");
    return;
  }
  
  // Пытаемся прочитать данные из /dev/zero
  // Должно вернуть количество прочитанных байт (10)
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  
  // Выводим прочитанные байты
  // Все должны быть нулевыми
  for(i = 0; i < n; i++) {
    printf("%d ", buf[i]);
  }
  printf("\n");
  
  // Пытаемся записать данные в /dev/zero
  // Должно вернуть ошибку (-1)
  n = write(fd, "test", 4);
  printf("%d\n\n", n);
  
  close(fd);
}

/**
 * @brief Тестирует работу устройства /dev/urandom
 *
 * Проверяет, что чтение из /dev/urandom возвращает случайные байты,
 * а запись в /dev/urandom меняет начальное значение (seed) для генератора.
 */
void
test_urandom(void)
{
  int fd;
  char buf[10];
  int n, i;
  uint seed = 0x12345678;  // Новое начальное значение для генератора
  
  printf("testing urandom device:\n");
  // Открываем устройство /dev/urandom для чтения и записи
  fd = open("urandom", O_RDWR);
  if(fd < 0) {
    printf("failed to open\n");
    return;
  }
  
  // Пытаемся прочитать случайные данные из /dev/urandom
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  
  // Выводим прочитанные случайные байты в шестнадцатеричном формате
  for(i = 0; i < n; i++) {
    printf("0x%x ", buf[i] & 0xff);
  }
  printf("\n");
  
  // Пытаемся изменить начальное значение (seed) для генератора
  n = write(fd, &seed, sizeof(seed));
  printf("%d\n", n);
  
  // Снова читаем случайные данные
  // Они должны отличаться от предыдущих из-за изменения seed
  n = read(fd, buf, sizeof(buf));
  printf("%d\n", n);
  
  // Выводим новые случайные байты
  for(i = 0; i < n; i++) {
    printf("0x%x ", buf[i] & 0xff);
  }
  printf("\n\n");
  
  close(fd);
}

/**
 * @brief Тестирует работу устройства /dev/nullstat
 *
 * Проверяет, что запись в /dev/nullstat увеличивает счетчик байт,
 * а чтение из /dev/nullstat возвращает текущее значение счетчика.
 */
void
test_nullstat(void)
{
  int fd;
  uint64 count;  // Переменная для хранения счетчика
  int n;
  
  printf("testing nullstat device:\n");
  // Открываем устройство /dev/nullstat для чтения и записи
  fd = open("nullstat", O_RDWR);
  if(fd < 0) {
    printf("Failed to open nullstat device\n");
    return;
  }
  
  // Читаем текущее значение счетчика
  // Должно быть 0, если это первое обращение к устройству
  n = read(fd, &count, sizeof(count));
  printf("%d , %d\n", n, (int)count);
  
  // Записываем 4 байта в /dev/nullstat
  // Счетчик должен увеличиться на 4
  n = write(fd, "test", 4);
  printf("%d\n", n);
  
  // Снова читаем значение счетчика
  // Должно быть 4
  n = read(fd, &count, sizeof(count));
  printf("%d , %d\n", n, (int)count);
  
  // Записываем еще 9 байт в /dev/nullstat
  // Счетчик должен увеличиться до 13
  n = write(fd, "more data", 9);
  printf("%d\n", n);
  
  // Снова читаем значение счетчика
  // Должно быть 13
  n = read(fd, &count, sizeof(count));
  printf("%d , %d\n", n, (int)count);
  
  // Пытаемся прочитать счетчик с неправильным размером буфера
  // Должно вернуть ошибку (-1)
  n = read(fd, &count, 1);
  printf("%d\n", n);
  
  close(fd);
}

/**
 * @brief Основная функция программы
 *
 * Последовательно вызывает функции тестирования для всех псевдоустройств.
 *
 * @return Код завершения программы
 */
int
main(void)
{
  // Тестируем все псевдоустройства
  test_null();
  test_zero();
  test_urandom();
  test_nullstat();
  
  exit(0);
}