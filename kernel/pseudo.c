/**
 * @file pseudo.c
 * @brief Реализация драйвера псевдоустройств для xv6
 *
 * Этот файл содержит реализацию драйвера, обеспечивающего поддержку
 * следующих псевдоустройств:
 * - null: при чтении возвращает EOF, при записи игнорирует данные
 * - zero: при чтении возвращает нулевые байты, запись не допускается
 * - urandom: при чтении возвращает случайные байты, при записи меняет seed
 * - nullstat: при записи считает количество байт, при чтении возвращает счетчик
 */

#include "types.h"      // Базовые типы данных
#include "riscv.h"      // Специфичные для RISC-V определения
#include "defs.h"       // Объявления функций ядра
#include "param.h"      // Параметры системы
#include "spinlock.h"   // Спин-блокировки
#include "sleeplock.h"  // Слип-блокировки
#include "fs.h"         // Файловая система
#include "file.h"       // Файловые дескрипторы
#include "memlayout.h"  // Макеты памяти
#include "proc.h"       // Структуры процессов

/**
 * @brief Старший номер устройства для псевдоустройств
 */
#define PSEUDO_MAJOR 2

/**
 * @brief Младший номер устройства для /dev/null
 */
#define PSEUDO_NULL 0

/**
 * @brief Младший номер устройства для /dev/zero
 */
#define PSEUDO_ZERO 1

/**
 * @brief Младший номер устройства для /dev/urandom
 */
#define PSEUDO_URANDOM 2

/**
 * @brief Младший номер устройства для /dev/nullstat
 */
#define PSEUDO_NULLSTAT 3

/**
 * @brief Структура для хранения состояния псевдоустройств
 *
 * Содержит блокировку для синхронизации доступа, счетчик байт для nullstat
 * и начальное значение для генератора случайных чисел.
 */
struct {
  struct spinlock lock;     // Блокировка для синхронизации доступа
  uint64 nullstat_count;    // Счетчик записанных байт для nullstat
  uint seed;                // Начальное значение для генератора случайных чисел
} pseudo;

/**
 * @brief Множитель для линейного конгруэнтного генератора
 */
#define LCG_A 1664525

/**
 * @brief Приращение для линейного конгруэнтного генератора
 */
#define LCG_C 1013904223

/**
 * @brief Генерирует псевдослучайное число по линейному конгруэнтному методу
 *
 * Использует формулу X_{n+1} = (a * X_n + c) mod m, где m = 2^32 (неявно из-за переполнения uint)
 *
 * @param seed Начальное значение
 * @return Новое псевдослучайное число
 */
static uint
lcg_random(uint seed)
{
  return seed * LCG_A + LCG_C;
}

/**
 * @brief Функция чтения из псевдоустройств
 *
 * Обрабатывает чтение из различных псевдоустройств в зависимости от младшего номера.
 *
 * @param user_dst Флаг, указывающий, находится ли dst в пользовательском пространстве
 * @param minor Младший номер устройства
 * @param dst Адрес буфера для записи данных
 * @param n Количество байт для чтения
 * @return Количество прочитанных байт или -1 в случае ошибки
 */
int
pseudoread(int user_dst, short minor, uint64 dst, int n)
{
  int i;
  char c;
  uint64 size;
  
  switch(minor) {
    case PSEUDO_NULL:
      // /dev/null всегда возвращает EOF (0 байт)
      return 0;
      
    case PSEUDO_ZERO:
      // /dev/zero возвращает нулевые байты
      c = 0;
      for(i = 0; i < n; i++) {
        // Копируем нулевой байт в буфер пользователя
        if(either_copyout(user_dst, dst + i, &c, 1) == -1)
          break;
      }
      return i;  // Возвращаем количество скопированных байт
      
    case PSEUDO_URANDOM:
      // /dev/urandom возвращает случайные байты
      acquire(&pseudo.lock);  // Захватываем блокировку
      for(i = 0; i < n; i++) {
        // Генерируем новое случайное число
        pseudo.seed = lcg_random(pseudo.seed);
        // Берем младший байт случайного числа
        c = (char)(pseudo.seed & 0xFF);
        // Копируем байт в буфер пользователя
        if(either_copyout(user_dst, dst + i, &c, 1) == -1)
          break;
      }
      release(&pseudo.lock);  // Освобождаем блокировку
      return i;  // Возвращаем количество скопированных байт
      
    case PSEUDO_NULLSTAT:
      // /dev/nullstat возвращает счетчик записанных байт
      acquire(&pseudo.lock);  // Захватываем блокировку
      // Проверяем, что размер буфера соответствует размеру счетчика
      if(n != sizeof(uint64)) {
        release(&pseudo.lock);
        return -1;  // Возвращаем ошибку, если размер не соответствует
      }
      // Копируем значение счетчика
      size = pseudo.nullstat_count;
      // Копируем счетчик в буфер пользователя
      if(either_copyout(user_dst, dst, &size, sizeof(uint64)) == -1) {
        release(&pseudo.lock);
        return -1;  // Возвращаем ошибку, если копирование не удалось
      }
      release(&pseudo.lock);  // Освобождаем блокировку
      return sizeof(uint64);  // Возвращаем размер счетчика
      
    default:
      // Неизвестное устройство
      return -1;  // Возвращаем ошибку
  }
}

/**
 * @brief Функция записи в псевдоустройства
 *
 * Обрабатывает запись в различные псевдоустройства в зависимости от младшего номера.
 *
 * @param user_src Флаг, указывающий, находится ли src в пользовательском пространстве
 * @param minor Младший номер устройства
 * @param src Адрес буфера с данными для записи
 * @param n Количество байт для записи
 * @return Количество записанных байт или -1 в случае ошибки
 */
int
pseudowrite(int user_src, short minor, uint64 src, int n)
{
  uint new_seed;
  
  switch(minor) {
    case PSEUDO_NULL:
      // /dev/null игнорирует все записи
      return n;  // Возвращаем количество "записанных" байт
      
    case PSEUDO_ZERO:
      // /dev/zero не поддерживает запись
      return -1;  // Возвращаем ошибку
      
    case PSEUDO_URANDOM:
      // /dev/urandom принимает новое начальное значение для генератора
      // Проверяем, что размер данных соответствует размеру seed
      if(n != sizeof(uint)) {
        return -1;  // Возвращаем ошибку, если размер не соответствует
      }
      // Копируем новое значение seed из буфера пользователя
      if(either_copyin(&new_seed, user_src, src, sizeof(uint)) == -1) {
        return -1;  // Возвращаем ошибку, если копирование не удалось
      }
      acquire(&pseudo.lock);  // Захватываем блокировку
      pseudo.seed = new_seed;  // Устанавливаем новое значение seed
      release(&pseudo.lock);  // Освобождаем блокировку
      return sizeof(uint);  // Возвращаем размер seed
      
    case PSEUDO_NULLSTAT:
      // /dev/nullstat считает количество записанных байт
      acquire(&pseudo.lock);  // Захватываем блокировку
      pseudo.nullstat_count += n;  // Увеличиваем счетчик
      release(&pseudo.lock);  // Освобождаем блокировку
      return n;  // Возвращаем количество "записанных" байт
      
    default:
      // Неизвестное устройство
      return -1;  // Возвращаем ошибку
  }
}

/**
 * @brief Инициализация драйвера псевдоустройств
 *
 * Инициализирует блокировку, счетчик и начальное значение для генератора случайных чисел,
 * а также регистрирует функции чтения и записи в таблице устройств.
 */
void
pseudoinit(void)
{
  // Инициализация блокировки
  initlock(&pseudo.lock, "pseudo");
  // Инициализация счетчика nullstat
  pseudo.nullstat_count = 0;
  // Инициализация начального значения для генератора случайных чисел
  pseudo.seed = 0x12345678;
  
  // Регистрация функций чтения и записи в таблице устройств
  devsw[PSEUDO_MAJOR].read = pseudoread;
  devsw[PSEUDO_MAJOR].write = pseudowrite;
}