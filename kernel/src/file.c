#include "klib.h"
#include "file.h"
#include "sem.h"
#include "vme.h"

#define TOTAL_FILE 128

file_t files[TOTAL_FILE]; //代表整个操作系统可以使用的file_t就这么多
pipe_t pipe; //代表整个操作系统可以使用的pipe_t就这么多

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
    iread(ip, 0, &fp->pipe, sizeof(pipe_t *));
    fp->pipe->read_open += (mode & O_RDONLY) || (mode & O_RDWR);
    fp->pipe->write_open += (mode & O_WRONLY) || (mode & O_RDWR);
    // 初始化信号量
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
    if(file->type == TYPE_FIFO ) {
      sem_v(&file->pipe->read_wait);
      sem_p(&file->pipe->write_wait);
    }
    int read_size = 0;
    pipe_t *p = file->pipe;
    if(p->write_open == 0){ // 写端全部关闭，读端不再会被阻塞（会唤醒所有被阻塞的读端），读取管道中剩余的可读字节。
      while (p->read_pos != p->write_pos && read_size < size) {
        ((char*)buf)[read_size++] = p->buffer[p->read_pos++];
        if (p->read_pos == PIPE_SIZE) p->read_pos = 0;
      }
      return read_size;
    }
    while (read_size < size) {
        sem_p(&p->read_sem);
        sem_p(&p->mutex);
        // printf("read_pos: %d\n", p->read_pos);
        // printf("write_pos: %d\n", p->write_pos);
        if (p->write_open == 0) { // 阻塞后被“无写者”唤醒，一直读到管道为空
            sem_v(&p->mutex);
            return read_size;
        }
          
        ((char*)buf)[read_size++] = p->buffer[p->read_pos++];
        if (p->read_pos == PIPE_SIZE) p->read_pos = 0;

        sem_v(&p->mutex);
        sem_v(&p->write_sem);
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
    if(file->type == TYPE_FIFO ) {
      sem_v(&file->pipe->write_wait);
      sem_p(&file->pipe->read_wait);
    }
    int write_size = 0;
    pipe_t *p = file->pipe;
    while (write_size < size) { // 写完size才返回
        sem_p(&p->write_sem); // 若管道满，阻塞，待会儿再写
        sem_p(&p->mutex);

        if (p->read_open == 0) { // 阻塞后被“无读者”唤醒，不再允许写入
            sem_v(&p->mutex);
            return write_size; // Read end closed
        }

        p->buffer[p->write_pos++] = ((char*)buf)[write_size++];
        if (p->write_pos == PIPE_SIZE) p->write_pos = 0;

        sem_v(&p->mutex);
        sem_v(&p->read_sem); 
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

file_t * fcreate_pipe(int mod) {
  // create a pipe, return 0 if success, -1 if failed
  // 读端文件描述符存在fd[0]，写端文件描述符存在fd[1]
  file_t * file = falloc();
  if(!file) return NULL;
  if(mod == 0) {
    file->type = TYPE_PIPE_READ;
    file->readable = 1;
    file->writable = 0;
    // pipe_t p = {};
    file->pipe = &pipe;
    file->pipe->read_pos = 0;
    file->pipe->write_pos = 0;
    file->pipe->read_open = 1;
    file->pipe->write_open = 1;
    // 初始化信号量
    sem_init(&file->pipe->read_sem, 0);
    sem_init(&file->pipe->write_sem, PIPE_SIZE);
    sem_init(&file->pipe->mutex, 1);
  } else {
    file->type = TYPE_PIPE_WRITE;
    file->readable = 0;
    file->writable = 1;
  }
  return file;
}

int fcreate_fifo(const char *path, int mode){
  // create a fifo, return 0 if success, -1 if failed
  // // TODO();
  inode_t *ip = NULL;
  int open_type = 114514;
  // 如果mode有O_CREATE，说明需要创建文件，应该设为TYPE_FIFO
  if(mode & O_CREATE) {
    open_type = TYPE_FIFO;
    ip = iopen(path, open_type); // 在inode中创建文件
    if(!ip){
      panic("fcreate_fifo: iopen failed\n");
      return -1;
    }
    pipe_t *p = &pipe;
    // printf("p: %d\n", p);
    p->read_open = 0;
    p->write_open = 0;
    p->read_pos = 0;
    p->write_pos = 0;
    sem_init(&p->read_sem, 0);
    sem_init(&p->write_sem, PIPE_SIZE);
    sem_init(&p->mutex, 1);
    sem_init(&p->read_wait, 0);
    sem_init(&p->write_wait, 0);
    iwrite(ip, 0, &p, sizeof(pipe_t *)); // 将pipe_t写入inode
    iclose(ip);
  }
  return 0;
}
