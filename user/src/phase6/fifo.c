#include "ulib.h"
#define FIFO_PATH "./myfifo"
#define BUFFER_SIZE 128

void writer_process() {
    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        printf("Failed to open FIFO for writing.\n");
        exit(1);
    }

    const char *message = "Hello from writer process!";
    write(fd, message, strlen(message) + 1); // +1 to include null terminator
    close(fd);
}

void reader_process() {
    int fd = open(FIFO_PATH, O_RDONLY);
    if (fd == -1) {
        printf("Failed to open FIFO for reading.\n");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    read(fd, buffer, BUFFER_SIZE);
    printf("Reader received: %s\n", buffer);
    close(fd);
}

void reader_and_writer_process() {
    int fd = open(FIFO_PATH, O_RDWR);
    if (fd == -1) {
        printf("Failed to open FIFO for reading and writing.\n");
        exit(1);
    }

    const char *message = "Hello from reader_and_writer process!";
    write(fd, message, strlen(message) + 1); // +1 to include null terminator

    char buffer[BUFFER_SIZE];
    read(fd, buffer, BUFFER_SIZE);
    printf("Reader_and_writer received: %s\n", buffer);

    close(fd);
}

int main() {
    printf("fifotest begins.\n");

    // Create FIFO
    if (mkfifo(FIFO_PATH, O_CREATE | O_RDONLY) == -1) {
        printf("Failed to create FIFO.\n");
        exit(1);
    }
    int writer_pid = fork();
    // printf("test1\n");
    if (writer_pid == -1) {
        printf("Failed to fork writer process.\n");
        exit(1);
    } else if (writer_pid == 0) {
        // printf("test2\n");
        // In writer process
        // writer_process();
        // printf("writer process ends.\n");
        exit(0);
    }
    // 两个子进程
    int reader_pid = fork();
    // printf("test1\n");
    if (reader_pid == -1) {
        printf("Failed to fork reader process.\n");
        exit(1);
    } else if (reader_pid == 0) {
        // In reader process
        printf("test3\n");
        // reader_process();
        // printf("reader process ends.\n");
        reader_and_writer_process();
        printf("reader_and_writer process ends.\n");
        exit(0);
    }

    // Wait for both child processes to finish
    wait(NULL);

    // Cleanup: remove the FIFO
    unlink(FIFO_PATH);

    printf("fifotest ends.\n");
    return 0;
}
