#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define START_CAPACITY 128


int write_all(int fd, const char *buffer, size_t count) {
    size_t total_written = 0;

    while (total_written < count) {
        ssize_t bytes_written = write(
            fd,
            buffer + total_written,
            count - total_written
        );

        if (bytes_written == -1) {
            return -1;
        }

        if (bytes_written == 0) {
            return -1;
        }

        total_written += bytes_written;
    }

    return 0;
}


ssize_t read_line(int fd, char **line) {
    size_t capacity = START_CAPACITY;
    ssize_t length = 0;

    char *buffer = malloc(capacity);

    if (!buffer) {
        perror("malloc");
        return -1;
    }

    while (1) {
        char ch;
        ssize_t bytes_read = read(fd, &ch, 1);

        if (bytes_read == -1) {
            perror("read");
            free(buffer);
            return -1;
        }

        if (bytes_read == 0) {
            if (length == 0) {
                free(buffer);
                return 0;
            }
            break;
        }
        
        if (ch == '\n') {
            if (length == 0) {
                free(buffer);
                return -2; // пустая строка
            }

            break;
        }

        if ((size_t)length == capacity) {
            size_t new_capacity = capacity * 2;

            char *new_buffer = realloc(buffer, new_capacity);

            if (new_buffer == NULL) {
                perror("realloc");
                free(buffer);
                return -1;
            }

            buffer = new_buffer;
            capacity = new_capacity;
        }
        buffer[length++] = ch;
    }

    *line = buffer;

    return length;
}


int main(void) {
    // Создание пайпов
    int parent_to_child1_pipe[2];
    int child1_to_child2_pipe[2];
    int child2_to_parent_pipe[2];

    if (pipe(parent_to_child1_pipe) == -1) {
        perror("parent_to_child1_pipe");
        return 1;
    }

    if (pipe(child1_to_child2_pipe) == -1) {
        perror("child1_to_child2_pipe");

        close(parent_to_child1_pipe[0]);
        close(parent_to_child1_pipe[1]);

        return 1;
    }

    if (pipe(child2_to_parent_pipe) == -1) {
        perror("child2_to_parent_pipe");

        close(parent_to_child1_pipe[0]);
        close(parent_to_child1_pipe[1]);

        close(child1_to_child2_pipe[0]);
        close(child1_to_child2_pipe[1]);

        return 1;
    }


    // Создание child1
    pid_t child1_pid = fork();

    if (child1_pid == -1) {
        perror("fork child1");

        close(parent_to_child1_pipe[0]);
        close(parent_to_child1_pipe[1]);
        close(child1_to_child2_pipe[0]);
        close(child1_to_child2_pipe[1]);
        close(child2_to_parent_pipe[0]);
        close(child2_to_parent_pipe[1]);

        return 1;
    }

    if (child1_pid == 0) {
        if (dup2(parent_to_child1_pipe[0], STDIN_FILENO) == -1) {
            perror("child1 dup2 stdin");
            _exit(1);
        }

        if (dup2(child1_to_child2_pipe[1], STDOUT_FILENO) == -1) {
            perror("child1 dup2 stdout");
            _exit(1);
        }

        // Закрываем ненужные концы пайпов
        close(parent_to_child1_pipe[0]);
        close(parent_to_child1_pipe[1]);
        close(child1_to_child2_pipe[0]);
        close(child1_to_child2_pipe[1]);
        close(child2_to_parent_pipe[0]);
        close(child2_to_parent_pipe[1]);

        execl("./inc/build/child1", "child1", NULL);

        perror("execl child1");
        _exit(1);
    }


    // Создание child2
    pid_t child2_pid = fork();

    if (child2_pid == -1) {
        perror("fork child2");

        close(parent_to_child1_pipe[0]);
        close(parent_to_child1_pipe[1]);
        close(child1_to_child2_pipe[0]);
        close(child1_to_child2_pipe[1]);
        close(child2_to_parent_pipe[0]);
        close(child2_to_parent_pipe[1]);

        waitpid(child1_pid, NULL, 0);

        return 1;
    }

    if (child2_pid == 0) {
        if (dup2(child1_to_child2_pipe[0], STDIN_FILENO) == -1) {
            perror("child2 dup2 stdin");
            _exit(1);
        }

        if (dup2(child2_to_parent_pipe[1], STDOUT_FILENO) == -1) {
            perror("child2 dup2 stdout");
            _exit(1);
        }

        // Закрытие ненужных концов пайпов
        close(parent_to_child1_pipe[0]);
        close(parent_to_child1_pipe[1]);
        close(child1_to_child2_pipe[0]);
        close(child1_to_child2_pipe[1]);
        close(child2_to_parent_pipe[0]);
        close(child2_to_parent_pipe[1]);

        execl("./inc/build/child2", "child2", NULL);

        perror("execl child2");
        _exit(1);
    }


    // Код ниже выполняется только в parent
    fprintf(stderr, "parent pid: %d\n", getpid());
    fprintf(stderr, "child1 pid: %d\n", child1_pid);
    fprintf(stderr, "child2 pid: %d\n", child2_pid);

    close(parent_to_child1_pipe[0]);
    close(child1_to_child2_pipe[0]);
    close(child1_to_child2_pipe[1]);
    close(child2_to_parent_pipe[1]);

    int stop_flag = 0;

    while (!stop_flag) {
        char *line = NULL;

        ssize_t line_len = read_line(STDIN_FILENO, &line);

        if (line_len == -1 || line_len == 0) {
            break;
        }

        if (line_len == -2) {
            continue;
        }
        
        size_t offset = 0;
        
        while (offset < (size_t)line_len) {
            size_t part_size = line_len - offset;

            if (part_size > BUFFER_SIZE) {
                part_size = BUFFER_SIZE;
            }

            // Вывод части строки в child1
            if (write_all(parent_to_child1_pipe[1], line + offset, part_size) == -1) {
                perror("parent write to child1");
                free(line);
                stop_flag = 1;
                break;
            }

            size_t total_received = 0;

            // Чтение результата из child2
            while (total_received < part_size) {
                ssize_t received = read(child2_to_parent_pipe[0], line + offset + total_received, part_size - total_received);

                if (received == -1) {
                    perror("parent read from child2");
                    free(line);
                    stop_flag = 1;
                    break;
                }
                total_received += received;
            }

            if (stop_flag) {
                break;
            }

            if (write_all(STDOUT_FILENO, line + offset, part_size) == -1) {
                perror("parent write stdout");
                free(line);
                stop_flag = 1;
                break;
            }

            offset += part_size;
        }

        // Перевод строки
        if (write_all(STDOUT_FILENO, "\n", 1) == -1) {
            perror("parent write newline");
            free(line);
            stop_flag = 1;
            break;
        }

        free(line);
    }

    close(parent_to_child1_pipe[1]);
    close(child2_to_parent_pipe[0]);

    if (waitpid(child1_pid, NULL, 0) == -1) {
        perror("waitpid child1");
    }

    if (waitpid(child2_pid, NULL, 0) == -1) {
        perror("waitpid child2");
    }

    return 0;
}
