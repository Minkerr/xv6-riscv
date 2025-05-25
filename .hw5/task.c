/**
 * @file task.c
 * @brief Эхо-сервер с использованием именованных каналов (FIFO)
 *
 * Программа создает именованный канал (FIFO) и читает из него данные,
 * выводя их в лог-файл или на экран. Поддерживает работу в режиме демона
 * или в режиме foreground-процесса. Обрабатывает различные сигналы для
 * корректного завершения работы и вывода статистики.
 */

#include <stdio.h>      // Стандартный ввод-вывод
#include <stdlib.h>     // Стандартная библиотека
#include <unistd.h>     // POSIX API
#include <fcntl.h>      // Управление файловыми дескрипторами
#include <sys/stat.h>   // Информация о файлах
#include <signal.h>     // Обработка сигналов
#include <errno.h>      // Коды ошибок
#include <string.h>     // Функции для работы со строками
#include <time.h>       // Функции для работы со временем
#include <stdatomic.h>  // Атомарные операции
#include <stdbool.h>    // Булевы типы
#include <getopt.h>     // Обработка аргументов командной строки

/**
 * @brief Путь к именованному каналу по умолчанию
 */
#define DEFAULT_FIFO "/tmp/echo_server.fifo"

/**
 * @brief Путь к лог-файлу по умолчанию
 */
#define DEFAULT_LOG "/tmp/echo_server.log"

/**
 * @brief Интервал срабатывания будильника по умолчанию (в секундах)
 */
#define DEFAULT_ALARM_INTERVAL 5

/**
 * @brief Размер буфера для чтения данных из канала
 */
#define BUFFER_SIZE 4096

/**
 * @brief Макрос для проверки ошибок системных вызовов
 *
 * Проверяет результат выражения expr. Если результат равен -1,
 * выводит сообщение об ошибке msg и завершает программу.
 *
 * @param expr Выражение для проверки
 * @param msg Сообщение, выводимое в случае ошибки
 */
#define CHECK_ERROR(expr, msg) do { \
    if ((expr) == -1) { \
        perror(msg); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

/**
 * @brief Макрос для проверки указателей
 *
 * Проверяет, что указатель ptr не равен NULL. Если указатель равен NULL,
 * выводит сообщение об ошибке msg и завершает программу.
 *
 * @param ptr Указатель для проверки
 * @param msg Сообщение, выводимое в случае ошибки
 */
#define CHECK_PTR(ptr, msg) do { \
    if ((ptr) == NULL) { \
        perror(msg); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

/**
 * @brief Флаг завершения работы программы
 *
 * 0 - продолжать работу
 * 1 - завершить работу немедленно (SIGTERM)
 * 2 - дочитать текущие данные и завершить работу (SIGINT)
 */
static volatile sig_atomic_t shutdown_flag = 0;

/**
 * @brief Флаг срабатывания будильника
 *
 * Устанавливается в 1 при получении сигнала SIGALRM
 */
static volatile sig_atomic_t alarm_triggered = 0;

/**
 * @brief Флаг запроса статистики
 *
 * Устанавливается в 1 при получении сигнала SIGUSR1
 */
static volatile sig_atomic_t stats_requested = 0;

/**
 * @brief Флаг демонизации
 *
 * Устанавливается в 1 при получении сигнала SIGHUP в режиме foreground
 */
static volatile sig_atomic_t daemonize_flag = 0;

/**
 * @brief Счетчик обработанных сообщений
 *
 * Атомарная переменная для подсчета количества циклов открытие-чтение-закрытие
 */
static _Atomic unsigned long messages = 0;

/**
 * @brief Счетчик обработанных байт
 *
 * Атомарная переменная для подсчета общего объема прочитанных данных
 */
static _Atomic unsigned long bytes = 0;

/**
 * @brief Счетчик срабатываний будильника
 *
 * Атомарная переменная для подсчета количества срабатываний будильника
 */
static _Atomic unsigned long alarms = 0;

/**
 * @brief Имя именованного канала
 *
 * Может быть изменено через аргументы командной строки
 */
static char *fifo_name = DEFAULT_FIFO;

/**
 * @brief Имя лог-файла
 *
 * Может быть изменено через аргументы командной строки
 */
static char *log_file = DEFAULT_LOG;

/**
 * @brief Интервал срабатывания будильника (в секундах)
 *
 * Может быть изменен через аргументы командной строки
 */
static int alarm_interval = DEFAULT_ALARM_INTERVAL;

/**
 * @brief Флаг работы в режиме демона
 *
 * true - программа работает как демон
 * false - программа работает в режиме foreground
 */
static bool is_daemon = false;

/**
 * @brief Флаг работы в режиме foreground
 *
 * true - программа работает в режиме foreground
 * false - программа работает как демон
 */
static bool is_foreground = true;

/**
 * @brief Поток для записи в лог
 *
 * Может быть stdout/stderr или файл
 */
static FILE *log_stream;

// Прототипы функций
void daemonize();
void print_stats();

/**
 * @brief Функция очистки при завершении программы
 *
 * Удаляет именованный канал при завершении программы.
 * Регистрируется с помощью atexit().
 */
void cleanup() {
    unlink(fifo_name);
}

/**
 * @brief Функция для записи сообщения в лог
 *
 * Добавляет к сообщению временную метку и записывает его в лог.
 *
 * @param msg Сообщение для записи в лог
 */
void log_message(const char *msg) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';  // Удаляем символ новой строки
    fprintf(log_stream, "[%s] %s\n", time_str, msg);
    fflush(log_stream);
}

/**
 * @brief Обработчик сигнала SIGTERM
 *
 * Устанавливает флаг завершения работы в 1, что приводит к немедленному
 * завершению программы без дочитывания данных из канала.
 *
 * @param sig Номер сигнала
 */
void handle_sigterm(int sig) {
    (void)sig;  // Подавляем предупреждение о неиспользуемом параметре
    shutdown_flag = 1;
}

/**
 * @brief Обработчик сигнала SIGINT (Ctrl+C)
 *
 * Устанавливает флаг завершения работы в 2, что приводит к завершению
 * программы после дочитывания текущих данных из канала.
 *
 * @param sig Номер сигнала
 */
void handle_sigint(int sig) {
    (void)sig;  // Подавляем предупреждение о неиспользуемом параметре
    shutdown_flag = 2;
}

/**
 * @brief Обработчик сигнала SIGALRM
 *
 * Устанавливает флаг срабатывания будильника, что приводит к выводу
 * диагностического сообщения.
 *
 * @param sig Номер сигнала
 */
void handle_sigalrm(int sig) {
    (void)sig;  // Подавляем предупреждение о неиспользуемом параметре
    alarm_triggered = 1;
}

/**
 * @brief Обработчик сигнала SIGUSR1
 *
 * Устанавливает флаг запроса статистики, что приводит к выводу
 * статистики работы программы.
 *
 * @param sig Номер сигнала
 */
void handle_sigusr1(int sig) {
    (void)sig;  // Подавляем предупреждение о неиспользуемом параметре
    stats_requested = 1;
}

/**
 * @brief Обработчик сигнала SIGHUP
 *
 * Если программа работает в режиме foreground, устанавливает флаг
 * демонизации, что приводит к переходу в режим демона.
 *
 * @param sig Номер сигнала
 */
void handle_sighup(int sig) {
    (void)sig;  // Подавляем предупреждение о неиспользуемом параметре
    if (is_foreground) {
        daemonize_flag = 1;
    }
}

/**
 * @brief Настройка обработчиков сигналов
 *
 * Регистрирует обработчики для сигналов SIGTERM, SIGINT, SIGQUIT,
 * SIGALRM, SIGUSR1 и SIGHUP.
 */
void setup_signals() {
    struct sigaction sa;

    // Настройка обработчика SIGTERM
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    CHECK_ERROR(sigaction(SIGTERM, &sa, NULL), "sigaction SIGTERM");

    // Настройка обработчика SIGINT (Ctrl+C)
    sa.sa_handler = handle_sigint;
    CHECK_ERROR(sigaction(SIGINT, &sa, NULL), "sigaction SIGINT");

    // Игнорирование SIGQUIT (Ctrl+\)
    sa.sa_handler = SIG_IGN;
    CHECK_ERROR(sigaction(SIGQUIT, &sa, NULL), "sigaction SIGQUIT");

    // Настройка обработчика SIGALRM
    sa.sa_handler = handle_sigalrm;
    CHECK_ERROR(sigaction(SIGALRM, &sa, NULL), "sigaction SIGALRM");

    // Настройка обработчика SIGUSR1
    sa.sa_handler = handle_sigusr1;
    CHECK_ERROR(sigaction(SIGUSR1, &sa, NULL), "sigaction SIGUSR1");

    // Настройка обработчика SIGHUP
    sa.sa_handler = handle_sighup;
    CHECK_ERROR(sigaction(SIGHUP, &sa, NULL), "sigaction SIGHUP");
}

/**
 * @brief Проверка и обработка флагов, установленных обработчиками сигналов
 *
 * Проверяет флаги shutdown_flag, alarm_triggered, stats_requested и
 * daemonize_flag, и выполняет соответствующие действия.
 */
void check_flags() {
    // Проверка флага завершения работы
    if (shutdown_flag) {
        log_message("Shutdown signal received");
        exit(EXIT_SUCCESS);
    }
    
    // Проверка флага срабатывания будильника
    if (alarm_triggered) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Alarm: %lu seconds active", alarms);
        log_message(msg);
        alarm_triggered = 0;
        alarm(alarm_interval);  // Перезапуск будильника
    }
    
    // Проверка флага запроса статистики
    if (stats_requested) {
        print_stats();
        stats_requested = 0;
    }
    
    // Проверка флага демонизации
    if (daemonize_flag) {
        daemonize();
        daemonize_flag = 0;
    }
}

/**
 * @brief Функция демонизации процесса
 *
 * Преобразует процесс в демон, отсоединяя его от терминала и
 * перенаправляя вывод в лог-файл.
 */
void daemonize() {
    if (is_daemon) return;  // Если уже демон, ничего не делаем
    
    // Первый fork для отсоединения от терминала
    pid_t pid = fork();
    CHECK_ERROR(pid, "fork");
    if (pid > 0) exit(EXIT_SUCCESS);  // Родительский процесс завершается
    
    // Создание новой сессии
    CHECK_ERROR(setsid(), "setsid");
    
    // Второй fork для предотвращения повторного получения управляющего терминала
    pid = fork();
    CHECK_ERROR(pid, "fork");
    if (pid > 0) exit(EXIT_SUCCESS);  // Родительский процесс завершается
    
    // Перенаправление стандартных потоков в лог-файл
    int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0600);
    CHECK_ERROR(fd, "open log");
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
    
    // Настройка потока для записи в лог
    log_stream = fdopen(STDOUT_FILENO, "a");
    CHECK_PTR(log_stream, "fdopen");
    setvbuf(log_stream, NULL, _IONBF, 0);  // Отключение буферизации
    
    // Установка флагов
    is_daemon = true;
    log_message("Daemon initialization complete");
}

/**
 * @brief Функция вывода статистики работы программы
 *
 * Выводит в лог количество обработанных сообщений, общий объем
 * прочитанных данных и количество срабатываний будильника.
 */
void print_stats() {
    char msg[256];
    snprintf(msg, sizeof(msg), "Messages: %lu, Bytes: %lu, Alarms: %lu",
             messages, bytes, alarms);
    log_message(msg);
}

/**
 * @brief Основная функция программы
 *
 * Обрабатывает аргументы командной строки, создает или проверяет
 * именованный канал, настраивает обработчики сигналов и входит в
 * основной цикл работы.
 *
 * @param argc Количество аргументов командной строки
 * @param argv Массив аргументов командной строки
 * @return Код завершения программы
 */
int main(int argc, char *argv[]) {
    int opt;
    bool daemon_mode = false;
    
    // Обработка аргументов командной строки
    while ((opt = getopt(argc, argv, "df:l:i:")) != -1) {
        switch (opt) {
            case 'd':  // Режим демона
                daemon_mode = true;
                break;
            case 'f':  // Имя именованного канала
                fifo_name = optarg;
                break;
            case 'l':  // Имя лог-файла
                log_file = optarg;
                break;
            case 'i':  // Интервал будильника
                alarm_interval = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-d] [-f fifo] [-l log] [-i interval]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    // Проверка существования именованного канала
    struct stat st;
    if (stat(fifo_name, &st) == 0) {
        // Файл существует, проверяем, что это именованный канал
        if (!S_ISFIFO(st.st_mode)) {
            fprintf(stderr, "%s is not a FIFO\n", fifo_name);
            exit(EXIT_FAILURE);
        }
    } else {
        // Файл не существует, создаем именованный канал
        if (mkfifo(fifo_name, 0600) == -1) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }
    
    // Регистрация функции очистки при завершении программы
    atexit(cleanup);
    
    // Демонизация, если указан соответствующий режим
    if (daemon_mode) {
        daemonize();
    } else {
        // Открытие лог-файла в режиме foreground
        log_stream = fopen(log_file, "a");
        CHECK_PTR(log_stream, "fopen log");
        setvbuf(log_stream, NULL, _IONBF, 0);  // Отключение буферизации
    }
    
    // Настройка обработчиков сигналов
    setup_signals();
    
    // Запуск будильника
    alarm(alarm_interval);
    
    // Основной цикл работы
    while (!shutdown_flag) {
        // Проверка флагов, установленных обработчиками сигналов
        check_flags();
        
        // Открытие именованного канала для чтения
        int fifo_fd;
        do {
            fifo_fd = open(fifo_name, O_RDONLY);
            if (fifo_fd == -1) {
                if (errno == EINTR) {
                    // Прерывание по сигналу, проверяем флаги и повторяем попытку
                    check_flags();
                    continue;
                }
                CHECK_ERROR(-1, "open fifo");
            }
        } while (fifo_fd == -1);
        
        // Чтение данных из канала
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        
        while ((bytes_read = read(fifo_fd, buffer, BUFFER_SIZE-1)) != 0) {
            // Проверка флагов после каждого чтения
            check_flags();
            
            if (bytes_read == -1) {
                if (errno == EINTR) {
                    // Прерывание по сигналу, проверяем флаги и повторяем попытку
                    check_flags();
                    continue;
                }
                CHECK_ERROR(-1, "read fifo");
            }
            
            // Добавление нулевого символа в конец буфера
            buffer[bytes_read] = '\0';
            
            // Вывод прочитанных данных в лог
            fprintf(log_stream, "%s", buffer);
            fflush(log_stream);
            
            // Обновление счетчика байт
            atomic_fetch_add(&bytes, bytes_read);
        }
        
        // Закрытие канала
        close(fifo_fd);
        
        // Обновление счетчика сообщений
        atomic_fetch_add(&messages, 1);
        
        // Если получен сигнал SIGINT, завершаем работу после дочитывания данных
        if (shutdown_flag == 2) break;
        
        // Проверка флага запроса статистики
        if (stats_requested) {
            print_stats();
            stats_requested = 0;
        }
    }
    
    // Вывод сообщения о завершении работы
    log_message("Server shutdown");
    return EXIT_SUCCESS;
}