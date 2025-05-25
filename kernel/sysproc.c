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
 * @brief Системный вызов для получения текущего времени из RTC
 *
 * Эта функция реализует системный вызов rtctime, который позволяет
 * пользовательским программам получить текущее время из часов реального
 * времени (RTC). Время возвращается в виде 64-битного значения, представляющего
 * количество наносекунд с 1 января 1970 года.
 *
 * Функция принимает один аргумент - указатель на буфер в пользовательском
 * пространстве, куда будет записано 64-битное значение времени.
 *
 * @return 0 в случае успеха, -1 в случае ошибки
 */
uint64 sys_rtctime(void) {
  uint64 time = rtc_read_time();  // Получаем текущее время из RTC
  uint64 addr;                    // Адрес буфера в пользовательском пространстве
  struct proc *p = myproc();      // Получаем указатель на текущий процесс
  
  // Получаем адрес буфера из аргумента системного вызова
  argaddr(0, &addr);

  // Проверяем корректность адреса
  if (addr < 0) {
    return -1;  // Возвращаем ошибку, если адрес некорректен
  }

  // Копируем значение времени в пользовательское пространство
  if (copyout(p->pagetable, addr, (char *)&time, sizeof(time)) < 0) {
    return -1;  // Возвращаем ошибку, если копирование не удалось
  }

  return 0;  // Возвращаем успех
}
