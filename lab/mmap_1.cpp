#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

int main(){
    char *shared_memory1 = (char *)mmap();
    // 这里父进程申请了一页共享内存shared_memory1, 本质上它可以看作是一个数组char shard_memory1[4096]
    memset(shared_memory1, 0, 4096);
    strcpy(shared_memory1, "Hello, world!");
    printf("%s\n", shared_memory1);
    // shell输出Hello, world!

    char *shared_memory2 = (char *)mmap();
    // 这里父进程申请了一页共享内存shared_memory1, 本质上它可以看作是一个数组char shard_memory1[4096]
    memset(shared_memory2, 0, 4096);
    strcpy(shared_memory2, "munmap test");
    printf("%s\n", shared_memory2);
    // shell输出munmap test

    if(fork() == 0){
        // 子进程
        strcpy(shared_memory1, "Man, what can I say? Mamba Out!");
        munmap(shared_memory2); //子进程把shared_memory2释放了，但是父进程还持有，所以父进程还能访问它。
        exit(0);
    }
    else{
        wait(); // 父进程先等子进程
        printf("%s\n", shared_memory1);
        // shell输出Man, what can I say? Mamba Out!
        printf("%s\n", shared_memory2);
        // shell输出munmap test
    }

    return 0;

}
