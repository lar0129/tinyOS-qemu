#include "ulib.h"

// 测试pipe基本功能

int main() {
    printf("pipetest begins.\n");
    int fd[2];
    char write_msg[131];
    for(int i = 0; i < 130; i++) {
        write_msg[i] = 'a' + i % 26;
    }
    write_msg[130] = '\0';
    // char write_msg[] = "Hello, pipe!";
    char read_msg[120];
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

        // // 从管道读取数据
        nbytes = read(fd[0], read_msg, sizeof(read_msg));
        if (nbytes < 0) {
            printf("Son read from pipe failed in pipetest.\n");
            return -1;
        }

        // 打印读取的数据
        read_msg[nbytes] = '\0'; // 添加字符串终止符
        printf("Child received2: %s\n", read_msg);

        close(fd[0]); // 关闭读端
    } else {
        // 父进程
        close(fd[0]); // 关闭读端
        // close(fd[1]); // 关闭写端
        // 向管道写入数据
        
        printf("x = %d\n",write(fd[1], write_msg, strlen(write_msg)));
        printf("x = %d\n",write(fd[1], write_msg, strlen(write_msg)));
        printf("x = %d\n",write(fd[1], write_msg, strlen(write_msg)));
        printf("x = %d\n",write(fd[1], write_msg, strlen(write_msg)));
        printf("x = %d\n",write(fd[1], write_msg, strlen(write_msg)));

        printf("Parent finish writing to the pipe.\n");

        close(fd[1]); // 关闭写端

        // 等待子进程完成
        wait(NULL);
    }
    printf("pipetest ends.\n");
    return 0;
}
