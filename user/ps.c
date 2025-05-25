/**
 * @file ps.c
 * @brief Утилита для вывода информации о запущенных процессах
 * 
 * Эта программа использует системный вызов ps_listinfo для получения
 * информации о всех запущенных процессах и выводит ее в удобочитаемом формате.
 */

#include "kernel/types.h"    // Базовые типы данных
#include "kernel/stat.h"     // Структуры статистики
#include "kernel/procinfo.h" // Структура procinfo
#include "user.h"            // Пользовательские функции


/**
 * @brief Преобразует числовое состояние процесса в строковое представление
 * 
 * @param state Числовое состояние процесса
 * @return Строковое представление состояния процесса
 */
const char* state_to_str(int state) {
    switch(state) {
        case 0: return "UNUSED ";  // Процесс не используется
        case 1: return "USED   ";  // Процесс используется, но не активен
        case 2: return "SLEEP  ";  // Процесс в состоянии сна
        case 3: return "RUNNABL";  // Процесс готов к выполнению
        case 4: return "RUNNING";  // Процесс выполняется
        case 5: return "ZOMBIE ";  // Процесс завершен, но не освобожден
        default: return "UNKNOWN"; // Неизвестное состояние
    }
}

/**
 * @brief Находит имя родительского процесса по его идентификатору
 * 
 * @param procs Массив структур procinfo с информацией о процессах
 * @param n Количество процессов в массиве
 * @param ppid Идентификатор родительского процесса
 * @return Имя родительского процесса или "-", если родительский процесс не найден
 */
const char* get_parent_name(struct procinfo *procs, int n, int ppid) {
    // Ищем процесс с указанным идентификатором
    for(int i = 0; i < n; i++) {
        if(procs[i].pid == ppid)
            return procs[i].name;  // Возвращаем имя найденного процесса
    }
    return "-";  // Если процесс не найден, возвращаем "-"
}

/**
 * @brief Основная функция программы
 * 
 * Получает информацию о всех запущенных процессах и выводит ее
 * в удобочитаемом формате.
 * 
 * @return Код завершения программы
 */
int main() {
    // Получаем общее количество процессов
    int cnt = ps_listinfo(0, 0);
    
    // Выделяем память для буфера нужного размера
    struct procinfo *procs = malloc(cnt * sizeof(struct procinfo));
    
    // Получаем информацию о процессах
    int ret = ps_listinfo(procs, cnt);
    
    // Проверяем, что получили информацию о всех процессах
    if(ret != cnt) {
        printf("ps error: %d/%d processes fetched\n", ret, cnt);
        exit(1);  // Завершаем программу с кодом ошибки
    }
    
    // Выводим информацию о каждом процессе
    for(int i = 0; i < cnt; i++) {
        printf("pid: %d;\tname: %s;\tstate: %s;\tppid: %d;\tpname: %s\n", 
            procs[i].pid,                                  // Идентификатор процесса
            procs[i].name,                                 // Имя процесса
            state_to_str(procs[i].state),                  // Состояние процесса
            procs[i].ppid,                                 // Идентификатор родительского процесса
            get_parent_name(procs, cnt, procs[i].ppid)     // Имя родительского процесса
        );
    }
    
    // Освобождаем выделенную память
    free(procs);
    exit(0);  // Завершаем программу с кодом успешного выполнения
}
