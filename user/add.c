#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[512];

int is_digit(char c) {
    return (c >= '0' && c <= '9');
}

int is_valid_number(const char *str, int max_digits) {
    if (*str == '-') {
        str++; 
    }
    int digit_count = 0;
    while (*str) {
        if (!is_digit(*str) && *str != '\0') {
            return 0; 
        }
        digit_count++;
        if (digit_count > max_digits) {
            return 0;
        }
        str++;
    }
    return 1;  
}


int
main(int argc, char *argv[])
{
    int n;
    int num1, num2;
    int sign1 = 1, sign2 = 1;

    printf("Enter two numbers: ");

    int t = 0;
    int newline = 0;

    while (t < sizeof(buf) - 1) {
        n = read(0, buf + t, sizeof(buf) - 1 - t);
        
        if (n < 0) {
            printf("Read error. Code: %d\n", n);
            exit(-2);
        }
        
        if (n == 0) {
            break;
        }
        
        for (int i = 0; i < n; i++) {
            if (buf[t + i] == '\n') {
                newline = 1;
                t += i + 1;
                break;
            }
        }
        
        if (newline) {
            break;
        }
        t += n;
    }

    if (newline) {
        buf[t - 1] = '\0'; 
    } else {
        buf[t] = '\0'; 
    }

    char *space_ptr = buf;
    while (*space_ptr != ' ' && *space_ptr != '\0') {
        space_ptr++;
    }

    if (*space_ptr != ' ') {
        printf("Incorrect Input. Enter two numbers separated by a space.\n");
        exit(-1);
    }

    *space_ptr = '\0';  
    char *first_num = buf;
    char *second_num = space_ptr + 1;

    if (!is_valid_number(first_num, 10) && !is_valid_number(second_num, 10)) {
        printf("Incorrect Input. Enter two int numbers separated by a space.\n");
        exit(-1);
    }

    if (*first_num == '-') {
        sign1 = -1;
        first_num++;
    }
    if (*second_num == '-') {
        sign2 = -1;
        second_num++;
    }

    num1 = sign1 * atoi(first_num);
    num2 = sign2 * atoi(second_num);

    int result = add(num1, num2);

    printf("%d\n", result);

    exit(0);
}