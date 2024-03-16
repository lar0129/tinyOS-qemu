#include <stdio.h>
#include<unistd.h>

int main(int argc,char *argv[]){
    char *filename = argv[1];
    char *argv2 [argc];
    char* envp[]={"PATH=/bin", NULL};
    for(int i = 1; i < argc; i++){
        argv2[i-1] = argv[i];
    }

    execve(filename,argv2,envp);
    return 0;
}