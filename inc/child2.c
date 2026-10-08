#include <ctype.h>
#include <stdio.h>
#include <unistd.h> 

#define BUFFER_SIZE 1024

int main(void) {
    char buffer[BUFFER_SIZE];

    ssize_t bytes_read;

    // Считывание ввода, пока:
    // 1) Не появится ошибка
    // 2) Все дескрипторы записи в STDIN_FILENO не закрыты или в нем остались данные
    while ((bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
        // Замена пробельных символов на _
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (isspace(buffer[i])) {
                buffer[i] = '_';
            }
        }

        ssize_t total_written = 0;

        // Печать вывода, пока: 
        // 1) Не появится ошибка
        // 2) Не все символы, полученные из STDIN_FILENO, напечатаны
        while (total_written < bytes_read) {

            ssize_t bytes_written = write(
                STDOUT_FILENO,
                buffer + total_written,
                bytes_read - total_written
            );

            if (bytes_written == -1) {
                perror("write");
                return 1;
            }

            total_written += bytes_written;
        }
    }

    if (bytes_read == -1) {
        perror("read");
        return 1;
    }

    return 0;
}