#include "klib.h"
#include "file.h"
#include "sem.h"
#include "vme.h"

#define TOTAL_FILE 128

file_t files[TOTAL_FILE]; //代表整个操作系统可以使用的file_t就这么多

static file_t *falloc() {
  // Lab3-1: find a file whose ref==0, init it, inc ref and return it, return NULL if none
  // // TODO();
  for(int i = 0; i < TOTAL_FILE; i++) {
    if(files[i].ref == 0) {
      files[i].ref = 1;
      files[i].type = TYPE_NONE;
      return &files[i];
    }
  }
  return NULL;
}

file_t *fopen(const char *path, int mode) {
  file_t *fp = falloc();
  inode_t *ip = NULL;
  if (!fp) goto bad;
  // // TODO: Lab3-2, determine type according to mode
  // iopen in Lab3-2: if file exist, open and return it
  //       if file not exist and type==TYPE_NONE, return NULL
  //       if file not exist and type!=TYPE_NONE, create the file as type
  // you can ignore this in Lab3-1
  int open_type = 114514;
  // 如果mode没有O_CREATE，说明不需要创建文件，应该设为TYPE_NONE，否则应根据是否有O_DIR位设置为TYPE_FILE或TYPE_DIR
  if(mode & O_CREATE) {
    if(mode & O_DIR) {
      open_type = TYPE_DIR;
    } else {
      open_type = TYPE_FILE;
    }
  } else {
    open_type = TYPE_NONE;
  }
  ip = iopen(path, open_type);
  if (!ip) goto bad;
  int type = itype(ip);
  if (type == TYPE_FILE || type == TYPE_DIR || type == TYPE_SOFTLINK) {
    // // TODO: Lab3-2, if type is not DIR, go bad if mode&O_DIR
    // 如果mode有O_DIR，但打开的不是目录，应跳转到bad关闭文件并返回NULL；
    if (type != TYPE_DIR && mode & O_DIR) goto bad;
    // // TODO: Lab3-2, if type is DIR, go bad if mode WRITE or TRUNC
    // 如果打开的是目录，但mode中有O_WRONLY、O_RDWR或O_TRUNC（即要写或清空），也应跳转到bad关闭文件并返回NULL。
    if(type == TYPE_DIR && (mode & O_WRONLY || mode & O_RDWR || mode & O_TRUNC)) goto bad;
    // // TODO: Lab3-2, if mode&O_TRUNC, trunc the file
    //如果打开的是一个普通文件并且mode中有O_TRUNC，还需要调用itrunc清空该文件的内容。
    if(type == TYPE_FILE && mode & O_TRUNC) {
      itrunc(ip);
    }

    // 将inode设置进file_t里，然后把偏移量初始化为0
    fp->type = TYPE_FILE; // file_t don't and needn't distingush between file and dir and softlink
    fp->inode = ip;
    fp->offset = 0;
  } else if (type == TYPE_DEV) {
    // 根据其设备号取dev_t，然后将其设置进file_t里
    fp->type = TYPE_DEV;
    fp->dev_op = dev_get(idevid(ip));
    iclose(ip);
    ip = NULL;
  } else if(type == TYPE_FIFO){
    fp->type = TYPE_FIFO;
    fp->inode = ip;
    fp->offset = 0;
    fp->pipe = (pipe_t *)kalloc();
    fp->pipe->read_pos = 0;
    fp->pipe->write_pos = 0;
    fp->pipe->read_open = (mode & O_RDONLY) || (mode & O_RDWR);
    fp->pipe->write_open = (mode & O_WRONLY) || (mode & O_RDWR);
    // 初始化信号量
    sem_init(&fp->pipe->read_sem, 0);
    sem_init(&fp->pipe->write_sem, 0);
  }
  else assert(0);
  fp->readable = !(mode & O_WRONLY);
  fp->writable = (mode & O_WRONLY) || (mode & O_RDWR);
  return fp;
bad:
  if (fp) fclose(fp);
  if (ip) iclose(ip);
  return NULL;
}

int fread(file_t *file, void *buf, uint32_t size) {
  // Lab3-1, distribute read operation by file's type
  // remember to add offset if type is FILE (check if iread return value >= 0!)
  if (!file->readable) return -1;
  // // TODO();
  if(file->type == TYPE_PIPE_WRITE) panic("fread: TYPE_PIPE_WRITE\n");

  if(file->type == TYPE_PIPE_READ || file->type == TYPE_FIFO){
    int read_size = 0;
    pipe_t *pipe = file->pipe;
    while (read_size < size) {
      if (pipe->read_pos == pipe->write_pos) { // 读空了
        if(read_size > 0){ // 读到了一些数据，直接返回
          // Log("fread: read_size > 0, sem_v(write_sem)\n");
          sem_v(&pipe->write_sem); // 通知写端
          return read_size;
        }

        if (pipe->write_open <= 0) { // 读空了且写端已关闭，直接返回（包括read_size=0)
          return read_size;
        }
        // Log("fread: read empty, sem_p(read_sem)\n");
        sem_p(&pipe->read_sem); // 读空了但未读到数据，等待写端告知
      } else {
        ((char*)buf)[read_size++] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_SIZE;
      }
    }
    if(read_size > 0){
      // Log("fread: read_size > 0, sem_v(write_sem)\n");
      sem_v(&pipe->write_sem); // 通知写端
    }
    return read_size;
  }
  else if(file->type == TYPE_FILE) {
    int read_size = iread(file->inode, file->offset, buf, size);
    if(read_size >= 0) {
      file->offset += read_size;
      return read_size;
    }
  } else if(file->type == TYPE_DEV) {
    int read_size = file->dev_op->read(buf, size);
    return read_size;
  }
  return -1;
}

int fwrite(file_t *file, const void *buf, uint32_t size) {
  // Lab3-1, distribute write operation by file's type
  // remember to add offset if type is FILE (check if iwrite return value >= 0!)
  if (!file->writable) return -1;
  // // TODO();
  if(file->type == TYPE_PIPE_READ) panic("fwrite: TYPE_PIPE_READ\n");

  if(file->type == TYPE_PIPE_WRITE || file->type == TYPE_FIFO){
    int write_size = 0;
    pipe_t *pipe = file->pipe;
    while (write_size < size) { // 写完size才返回
      if ((pipe->write_pos + 1) % PIPE_SIZE == pipe->read_pos) { // 写满了
        if(write_size > 0){
          // Log("fwrite: write_size > 0, sem_v(read_sem)\n");
          sem_v(&pipe->read_sem); // 通知读端
        }
        if (pipe->read_open <= 0) { // 写满了且读端已关闭
          return write_size;
        }
        // Log("fwrite: write full, sem_p(write_sem)\n");
        sem_p(&pipe->write_sem); // 写满了，等待读端告知
        if(pipe->read_open <= 0) return write_size; // 读端已关闭，不再允许写入
      } 
      else { // 未写满
        pipe->buffer[pipe->write_pos] = ((char*)buf)[write_size++];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_SIZE;
      }
    }
    if(write_size > 0){
      // Log("fwrite: write_size > 0, sem_v(read_sem)\n");
      // Log("read pos, write_pos: %d %d\n", pipe->read_pos, pipe->write_pos);
      sem_v(&pipe->read_sem); // 通知读端
    }
    return write_size;
  }
  
  else if(file->type == TYPE_FILE) {
    int write_size = iwrite(file->inode, file->offset, buf, size);
    if(write_size >= 0) {
      file->offset += write_size;
      return write_size;
    }
  } else if(file->type == TYPE_DEV) {
    int write_size = file->dev_op->write(buf, size);
    return write_size;
  }
  return -1;
}

uint32_t fseek(file_t *file, uint32_t off, int whence) {
  // Lab3-1, change file's offset, do not let it cross file's size
  if (file->type == TYPE_FILE) {
    int size = isize(file->inode);
    switch (whence) {
      case SEEK_SET:
        file->offset = off;
        return file->offset;
        break;
      case SEEK_CUR:
        file->offset += off;
        return file->offset;
        break;
      case SEEK_END:
        file->offset = size + off;
        return file->offset;
        break;
      default:
        assert(0);
        return -1;
    }
  }
  return -1;
}

file_t *fdup(file_t *file) {
  // Lab3-1, inc file's ref, then return itself
  // // TODO();
  ++file->ref;
  if(file->type == TYPE_PIPE_READ) {
    file->pipe->read_open++;
  }
  if(file->type == TYPE_PIPE_WRITE) {
    file->pipe->write_open++;
  }
  return file;
}

void fclose(file_t *file) {
  // Lab3-1, dec file's ref, if ref==0 and it's a file, call iclose
  // // TODO();
  --file->ref; // 当前进程的关闭文件
  if(file->ref == 0 && file->type == TYPE_FILE) {
    iclose(file->inode); // 磁盘的关闭文件
  }
  // 若读端全部被关闭，写端不再会被阻塞（会唤醒所有被阻塞的写端），也不再允许写入
  if(file->type == TYPE_PIPE_READ) {
    file->pipe->read_open--;
    if(file->pipe->read_open == 0) {
      sem_v(&file->pipe->write_sem);
    }
  }
  // 若写端全部被关闭，读端不再会被阻塞（会唤醒所有被阻塞的读端），读取管道中剩余的可读字节。
  if(file->type == TYPE_PIPE_WRITE) {
    file->pipe->write_open--;
    if(file->pipe->write_open == 0) {
      sem_v(&file->pipe->read_sem);
    }
  }
}

int fcreate_pipe(file_t **pread_file, file_t **pwrite_file) {
  // create a pipe, return 0 if success, -1 if failed
  // 读端文件描述符存在fd[0]，写端文件描述符存在fd[1]
  file_t * read_file = falloc();
  file_t * write_file = falloc();
  if(!read_file || !write_file) {
    if(read_file) fclose(read_file);
    if(write_file) fclose(write_file);
    panic("fcreate_pipe: invalid file\n");
    return -1;
  }
  *pread_file = read_file;
  *pwrite_file = write_file;
  read_file->pipe = (pipe_t *)kalloc();
  write_file->pipe = read_file->pipe;
  read_file->type = TYPE_PIPE_READ;
  write_file->type = TYPE_PIPE_WRITE;
  // read_file->pipe = (pipe_t*)kalloc(); // 为pipe_t分配内存
  // write_file->pipe = read_file->pipe;
  read_file->readable = 1;
  read_file->writable = 0;
  write_file->readable = 0;
  write_file->writable = 1;
  read_file->pipe->read_pos = 0;
  read_file->pipe->write_pos = 0;
  read_file->pipe->read_open = 1;
  read_file->pipe->write_open = 1;
  // 初始化信号量
  sem_init(&read_file->pipe->read_sem, 0);
  sem_init(&read_file->pipe->write_sem, 0);
  return 0;
}

int fcreate_fifo(const char *path, int mode){
  // create a fifo, return 0 if success, -1 if failed
  // // TODO();
  file_t *fp = falloc();
  inode_t *ip = NULL;
  if (!fp){
    panic("fcreate_fifo: falloc failed\n");
    return -1;
  }
  int open_type = 114514;
  // 如果mode有O_CREATE，说明需要创建文件，应该设为TYPE_FIFO
  if(mode & O_CREATE) {
    open_type = TYPE_FIFO;
    ip = iopen(path, open_type); // 在inode中创建文件
    if(!ip){
      panic("fcreate_fifo: iopen failed\n");
      return -1;
    }
  }
  return 0;
}
