#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <getopt.h>

#define DEFAULT_FIFO "/tmp/echo_server.fifo"
#define DEFAULT_LOG "/tmp/echo_server.log"
#define DEFAULT_ALARM_INTERVAL 5
#define BUFFER_SIZE 4096
#define CHECK_ERROR(expr, msg) do { \
    if ((expr) == -1) { \
        perror(msg); \
        exit(EXIT_FAILURE); \
    } \
} while(0)
#define CHECK_PTR(ptr, msg) do { \
    if ((ptr) == NULL) { \
        perror(msg); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

static volatile sig_atomic_t shutdown_flag = 0;
static volatile sig_atomic_t alarm_triggered = 0;
static volatile sig_atomic_t stats_requested = 0;
static volatile sig_atomic_t daemonize_flag = 0;
static _Atomic unsigned long messages = 0;
static _Atomic unsigned long bytes = 0;
static _Atomic unsigned long alarms = 0;

static char *fifo_name = DEFAULT_FIFO;
static char *log_file = DEFAULT_LOG;
static int alarm_interval = DEFAULT_ALARM_INTERVAL;
static bool is_daemon = false;
static bool is_foreground = true;
static FILE *log_stream;

void daemonize();
void print_stats();

void cleanup() {
    unlink(fifo_name);
}

void log_message(const char *msg) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; 
    fprintf(log_stream, "[%s] %s\n", time_str, msg);
    fflush(log_stream);
}

void handle_sigterm(int sig) {
    (void)sig;
    shutdown_flag = 1;
}

void handle_sigint(int sig) {
    (void)sig;
    shutdown_flag = 2;
}

void handle_sigalrm(int sig) {
    (void)sig;
    alarm_triggered = 1;
}

void handle_sigusr1(int sig) {
    (void)sig;
    stats_requested = 1;
}

void handle_sighup(int sig) {
    (void)sig;
    if (is_foreground) {
        daemonize_flag = 1;
    }
}

void setup_signals() {
    struct sigaction sa;

    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    CHECK_ERROR(sigaction(SIGTERM, &sa, NULL), "sigaction SIGTERM");

    sa.sa_handler = handle_sigint;
    CHECK_ERROR(sigaction(SIGINT, &sa, NULL), "sigaction SIGTERM");

    sa.sa_handler = SIG_IGN;
    CHECK_ERROR(sigaction(SIGQUIT, &sa, NULL), "sigaction SIGTERM");

    sa.sa_handler = handle_sigalrm;
    CHECK_ERROR(sigaction(SIGALRM, &sa, NULL), "sigaction SIGTERM");

    sa.sa_handler = handle_sigusr1;
    CHECK_ERROR(sigaction(SIGUSR1, &sa, NULL), "sigaction SIGTERM");

    sa.sa_handler = handle_sighup;
    CHECK_ERROR(sigaction(SIGHUP, &sa, NULL), "sigaction SIGTERM");
}

void check_flags() {
    if (shutdown_flag) {
        log_message("Shutdown signal received");
        exit(EXIT_SUCCESS);
    }
    
    if (alarm_triggered) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Alarm: %lu seconds active", alarms);
        log_message(msg);
        alarm_triggered = 0;
        alarm(alarm_interval);
    }
    
    if (stats_requested) {
        print_stats();
        stats_requested = 0;
    }
    
    if (daemonize_flag) {
        daemonize();
        daemonize_flag = 0;
    }
}

void daemonize() {
    if (is_daemon) return;
    pid_t pid = fork();
    CHECK_ERROR(pid, "fork");
    if (pid > 0) exit(EXIT_SUCCESS); 

    CHECK_ERROR(setsid(), "setsid");

    pid = fork();
    CHECK_ERROR(pid, "fork");
    if (pid > 0) exit(EXIT_SUCCESS);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0600);
    CHECK_ERROR(fd, "open log");
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    log_stream = fdopen(STDOUT_FILENO, "a");
    CHECK_PTR(log_stream, "fdopen");

    is_daemon = true;
    log_message("Daemonized via SIGHUP");
}

void print_stats() {
    char msg[256];
    snprintf(msg, sizeof(msg), "Messages: %lu, Bytes: %lu, Alarms: %lu",
             messages, bytes, alarms);
    log_message(msg);
}

int main(int argc, char *argv[]) {
    int opt;
    bool daemon_mode = false;

    while ((opt = getopt(argc, argv, "df:l:i:")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = true;
                break;
            case 'f':
                fifo_name = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'i':
                alarm_interval = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-d] [-f fifo] [-l log] [-i interval]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    struct stat st;
    if (stat(fifo_name, &st) == 0) {
        if (!S_ISFIFO(st.st_mode)) {
            fprintf(stderr, "%s is not a FIFO\n", fifo_name);
            exit(EXIT_FAILURE);
        }
    } else {
        if (mkfifo(fifo_name, 0600) == -1) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }

    atexit(cleanup);

    if (daemon_mode) {
        daemonize();
    } else {
        log_stream = stdout;
    }

    setup_signals();
    alarm(alarm_interval);

    while (!shutdown_flag) {
        check_flags();

        int fifo_fd;
        do {
            fifo_fd = open(fifo_name, O_RDONLY);
            if (fifo_fd == -1) {
                if (errno == EINTR) {
                    check_flags();
                    continue;
                }
                CHECK_ERROR(-1, "open fifo");
            }
        } while (fifo_fd == -1);

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = read(fifo_fd, buffer, BUFFER_SIZE-1)) != 0) {
            check_flags();
            if (bytes_read == -1) {
                if (errno == EINTR) {
                    check_flags();
                    continue;
                }
                CHECK_ERROR(-1, "read fifo");
            }
            buffer[bytes_read] = '\0';
            fprintf(log_stream, "%s", buffer);
            fflush(log_stream);
            atomic_fetch_add(&bytes, bytes_read);
        }

        close(fifo_fd);
        atomic_fetch_add(&messages, 1);

        if (shutdown_flag == 2) break;

        if (stats_requested) {
            print_stats();
            stats_requested = 0;
        }
    }

    log_message("Server shutdown");
    return EXIT_SUCCESS;
}