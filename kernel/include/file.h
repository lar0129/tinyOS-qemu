#ifndef __FILE_H__
#define __FILE_H__

#include <stdint.h>
#include "fs.h"
#include "dev.h"
#include "sem.h"

#define PIPE_SIZE 128

typedef struct pipe {
    char buffer[PIPE_SIZE];
    int read_pos;
    int write_pos;
    int read_open;
    int write_open;
    sem_t read_sem;
    sem_t write_sem;
    // struct sem_t mutex;
} pipe_t;

typedef struct file {
  int type; // TYPE_FILE代表该文件是磁盘文件，包括普通文件和文件夹，TYPE_DEV仍然代表设备文件。
  int ref; //引用计数
  int readable, writable;

  // for normal file
  inode_t *inode;
  uint32_t offset; //偏移量来记录目前文件操作的位置，每次对文件读写了多少个字节，偏移量就前进多少。

  // for dev file
  dev_t *dev_op;

  // for pipe
  pipe_t *pipe;
} file_t;

file_t *fopen(const char *path, int mode);  
int fread(file_t *file, void *buf, uint32_t size);
int fwrite(file_t *file, const void *buf, uint32_t size);
uint32_t fseek(file_t *file, uint32_t off, int whence);
file_t *fdup(file_t *file);
void fclose(file_t *file);
int fcreate_pipe(file_t **pread_file, file_t **pwrite_file);

#endif
