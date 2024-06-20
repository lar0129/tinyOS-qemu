#include "ulib.h"

// 测试pipe基本功能

int main() {
    printf("pipetest begins.\n");
    int fd[2];
    char write_msg[] = "Hello from parent!";
    char read_msg[128];
    int nbytes;

    // 创建管道
    if (pipe(fd) == -1) {
        printf("create pipe failed in pipetest.\n");
        return -1;
    }

    int pid = fork();

    if (pid < 0) {
        // 创建子进程失败
        printf("fork fails in pipetest.");
        return -1;
    } else if (pid == 0) {
        // 子进程
        close(fd[1]); // 关闭写端

        // 从管道读取数据
        nbytes = read(fd[0], read_msg, sizeof(read_msg));
        if (nbytes < 0) {
            printf("Son read from pipe failed in pipetest.\n");
            return -1;
        }

        // 打印读取的数据
        read_msg[nbytes] = '\0'; // 添加字符串终止符
        printf("Child received: %s\n", read_msg);

        close(fd[0]); // 关闭读端
    } else {
        // 父进程
        close(fd[0]); // 关闭读端

        // 向管道写入数据
        if (write(fd[1], write_msg, strlen(write_msg)) != strlen(write_msg)) {
            printf("Parent write from pipe failed in pipetest.\n");
            return -1;
        }

        printf("Parent finish writing to the pipe.\n");

        close(fd[1]); // 关闭写端

        // 等待子进程完成
        wait(NULL);
    }
    printf("pipetest ends.\n");
    return 0;
}
