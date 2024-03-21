// elf_test1.c

#include <stdio.h>

int main(int argc, char* argv[]){
        printf("The number of arguments are %d, each of them are:\n", argc-1);
        for(int i = 0; i < argc; i++){
                printf("argv[%d] = %s\n", i, argv[i]);
        }
        return 0;
}
