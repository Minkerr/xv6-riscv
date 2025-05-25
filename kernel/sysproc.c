#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/**
 * @brief Рекурсивно выводит содержимое таблицы страниц
 * 
 * Эта функция рекурсивно обходит таблицу страниц и выводит информацию о каждой
 * действительной странице (с установленным флагом PTE_V). Для каждой страницы
 * выводится индекс в таблице, физический адрес и флаги.
 * 
 * @param pagetable Указатель на таблицу страниц
 * @param level Текущий уровень таблицы страниц (2 - корневая таблица, 1 - средний уровень, 0 - листовой уровень)
 * @param va Виртуальный адрес, соответствующий текущему уровню таблицы
 */
void
print_pte(pagetable_t pagetable, int level, uint64 va)
{
  // Перебираем все 512 записей в текущей таблице страниц
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // Проверяем, что страница действительна (установлен флаг PTE_V)
    if(pte & PTE_V) {
      // Вычисляем виртуальный адрес для текущей записи
      // Сдвигаем индекс на соответствующее количество бит в зависимости от уровня таблицы
      uint64 child_va = va | (((uint64)i) << (PGSHIFT + 9*level));
      
      // Выводим отступы в зависимости от уровня таблицы для наглядности
      for(int j = 0; j < level; j++) {
        printf("......... ");
      }
      
      // Выводим индекс записи в таблице страниц
      printf("0x");
      printf("%x", i);
      printf(" -> 0x");
      // Выводим физический адрес страницы, используя макрос PTE2PA для преобразования PTE в физический адрес
      printf("%lx", PTE2PA(pte));
      printf(" ");
      
      // Выводим флаги страницы
      // Для каждого флага выводим соответствующую букву, если флаг установлен, или '_', если не установлен
      char r = (pte & PTE_R) ? 'R' : '_';  // Флаг чтения
      char w = (pte & PTE_W) ? 'W' : '_';  // Флаг записи
      char x = (pte & PTE_X) ? 'X' : '_';  // Флаг исполнения
      char u = (pte & PTE_U) ? 'U' : '_';  // Флаг пользовательского режима
      char g = (pte & PTE_G) ? 'G' : '_';  // Флаг глобальной страницы
      char a = (pte & PTE_A) ? 'A' : '_';  // Флаг доступа
      char d = (pte & PTE_D) ? 'D' : '_';  // Флаг изменения
      
      // Выводим флаги
      consputc(r);
      consputc(w);
      consputc(x);
      consputc(u);
      consputc(g);
      consputc(a);
      consputc(d);
      consputc('\n');
      
      // Если текущий уровень не листовой (level > 0) и страница не содержит прав доступа (не является листовой),
      // то рекурсивно обрабатываем следующий уровень таблицы
      if(level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
        uint64 child = PTE2PA(pte);
        print_pte((pagetable_t)child, level-1, child_va);
      }
    }
  }
}

/**
 * @brief Проверяет, находится ли виртуальный адрес в заданном буфере
 * 
 * Эта функция проверяет, пересекается ли страница, содержащая указанный
 * виртуальный адрес, с заданным буфером.
 * 
 * @param va Виртуальный адрес для проверки
 * @param buf_start Начальный адрес буфера
 * @param buf_len Длина буфера в байтах
 * @return 1, если страница пересекается с буфером или если buf_start или buf_len равны 0, иначе 0
 */
int
is_addr_in_buffer(uint64 va, uint64 buf_start, uint64 buf_len)
{
  // Если буфер не задан (buf_start или buf_len равны 0), считаем, что адрес находится в буфере
  if (buf_start == 0 || buf_len == 0)
    return 1;
  
  // Вычисляем конец буфера
  uint64 buf_end = buf_start + buf_len;
  // Вычисляем начало страницы, содержащей виртуальный адрес
  uint64 page_start = PGROUNDDOWN(va);
  // Вычисляем конец страницы
  uint64 page_end = page_start + PGSIZE;
  
  // Проверяем, пересекается ли страница с буфером
  // Страница пересекается с буфером, если начало страницы меньше конца буфера
  // и начало буфера меньше конца страницы
  return (page_start < buf_end && buf_start < page_end);
}

/**
 * @brief Обрабатывает таблицу страниц для вывода информации или очистки флагов
 * 
 * Эта функция рекурсивно обходит таблицу страниц и обрабатывает страницы,
 * которые пересекаются с заданным буфером. В зависимости от параметра clear_flags
 * либо выводит информацию о страницах, либо очищает указанные флаги.
 * 
 * @param pagetable Указатель на таблицу страниц
 * @param level Текущий уровень таблицы страниц (2 - корневая таблица, 1 - средний уровень, 0 - листовой уровень)
 * @param va Виртуальный адрес, соответствующий текущему уровню таблицы
 * @param buf_start Начальный адрес буфера
 * @param buf_len Длина буфера в байтах
 * @param flags_mask Битовая маска флагов для обработки (бит 0 - флаг D, бит 1 - флаг A)
 * @param clear_flags Флаг, указывающий, нужно ли очищать флаги (1) или выводить информацию (0)
 */
void
process_pte(pagetable_t pagetable, int level, uint64 va, uint64 buf_start, uint64 buf_len, int flags_mask, int clear_flags)
{
  // Перебираем все 512 записей в текущей таблице страниц
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // Проверяем, что страница действительна (установлен флаг PTE_V)
    if(pte & PTE_V) {
      // Вычисляем виртуальный адрес для текущей записи
      uint64 child_va = va | (((uint64)i) << (PGSHIFT + 9*level));
      
      // Если текущий уровень не листовой (level > 0) и страница не содержит прав доступа (не является листовой),
      // то рекурсивно обрабатываем следующий уровень таблицы
      if(level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
        uint64 child = PTE2PA(pte);
        process_pte((pagetable_t)child, level-1, child_va, buf_start, buf_len, flags_mask, clear_flags);
      } else if (level == 0) {
        // Если это листовой уровень, проверяем, находится ли страница в заданном буфере
        if (is_addr_in_buffer(child_va, buf_start, buf_len)) {
          if (clear_flags) {
            // Если нужно очистить флаги
            if (flags_mask & 1)
              pagetable[i] &= ~PTE_D;  // Очищаем флаг D (бит 0 в flags_mask)
            if (flags_mask & 2)
              pagetable[i] &= ~PTE_A;  // Очищаем флаг A (бит 1 в flags_mask)
          } else {
            // Если нужно вывести информацию о странице
            // Выводим информацию, если flags_mask == 0 (вывод всех страниц)
            // или если установлены соответствующие флаги, указанные в flags_mask
            if (flags_mask == 0 ||
                ((flags_mask & 1) && (pte & PTE_D)) ||  // Если нужно вывести страницы с флагом D и он установлен
                ((flags_mask & 2) && (pte & PTE_A))) {  // Если нужно вывести страницы с флагом A и он установлен
              
              // Выводим индекс записи в таблице страниц
              printf("0x");
              printf("%x", i);
              printf(" -> 0x");
              // Выводим физический адрес страницы
              printf("%lx", PTE2PA(pte));
              printf(" ");
              
              // Выводим флаги страницы
              char r = (pte & PTE_R) ? 'R' : '_';  // Флаг чтения
              char w = (pte & PTE_W) ? 'W' : '_';  // Флаг записи
              char x = (pte & PTE_X) ? 'X' : '_';  // Флаг исполнения
              char u = (pte & PTE_U) ? 'U' : '_';  // Флаг пользовательского режима
              char g = (pte & PTE_G) ? 'G' : '_';  // Флаг глобальной страницы
              char a = (pte & PTE_A) ? 'A' : '_';  // Флаг доступа
              char d = (pte & PTE_D) ? 'D' : '_';  // Флаг изменения
              
              // Выводим флаги
              consputc(r);
              consputc(w);
              consputc(x);
              consputc(u);
              consputc(g);
              consputc(a);
              consputc(d);
              consputc('\n');
            }
          }
        }
      }
    }
  }
}

/**
 * @brief Системный вызов для вывода информации о страницах процесса
 * 
 * Этот системный вызов выводит информацию о страницах процесса, которые
 * пересекаются с заданным буфером. Если буфер не задан (buf_addr или buf_len равны 0),
 * выводится информация о всех страницах процесса.
 * 
 * @return 0 в случае успеха или -1 в случае ошибки
 */
uint64
sys_pageinfo(void)
{
  uint64 buf_addr;  // Адрес буфера
  int buf_len;      // Длина буфера
  int flags_mask;   // Битовая маска флагов для вывода
  
  // Получаем аргументы системного вызова
  argaddr(0, &buf_addr);  // Адрес буфера
  argint(1, &buf_len);    // Длина буфера
  argint(2, &flags_mask); // Битовая маска флагов
  
  // Проверяем корректность flags_mask
  // flags_mask должен быть в диапазоне от 0 до 3
  // 0 - вывод всех страниц
  // 1 - вывод страниц с установленным флагом D
  // 2 - вывод страниц с установленным флагом A
  // 3 - вывод страниц с установленными флагами D или A
  if (flags_mask < 0 || flags_mask > 3) {
    return -1;  // Возвращаем ошибку, если flags_mask некорректен
  }
  
  // Получаем указатель на текущий процесс
  struct proc *p = myproc();
  
  // Выводим заголовок с адресом таблицы страниц
  printf("PAGETABLE 0x");
  printf("%lx", (uint64)p->pagetable);
  printf("\n");
  
  // Если flags_mask == 0, выводим всю таблицу страниц
  if (flags_mask == 0) {
    print_pte(p->pagetable, 2, 0);
  } else {
    // Иначе выводим только страницы с указанными флагами
    process_pte(p->pagetable, 2, 0, buf_addr, buf_len, flags_mask, 0);
  }
  
  return 0;  // Возвращаем успех
}

/**
 * @brief Системный вызов для очистки флагов страниц процесса
 * 
 * Этот системный вызов очищает флаги D и/или A у страниц процесса, которые
 * пересекаются с заданным буфером. Если буфер не задан (buf_addr или buf_len равны 0),
 * очищаются флаги у всех страниц процесса.
 * 
 * @return 0 в случае успеха или -1 в случае ошибки
 */
uint64
sys_clearflags(void)
{
  uint64 buf_addr;  // Адрес буфера
  int buf_len;      // Длина буфера
  int flags_mask;   // Битовая маска флагов для очистки
  
  // Получаем аргументы системного вызова
  argaddr(0, &buf_addr);  // Адрес буфера
  argint(1, &buf_len);    // Длина буфера
  argint(2, &flags_mask); // Битовая маска флагов
  
  // Проверяем корректность flags_mask
  // flags_mask должен быть в диапазоне от 0 до 3
  // 0 - не очищать флаги (бессмысленный вызов)
  // 1 - очищать флаг D
  // 2 - очищать флаг A
  // 3 - очищать флаги D и A
  if (flags_mask < 0 || flags_mask > 3) {
    return -1;  // Возвращаем ошибку, если flags_mask некорректен
  }
  
  // Получаем указатель на текущий процесс
  struct proc *p = myproc();
  
  // Очищаем флаги у страниц процесса
  process_pte(p->pagetable, 2, 0, buf_addr, buf_len, flags_mask, 1);
  
  return 0;  // Возвращаем успех
}
