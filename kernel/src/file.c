#include "klib.h"
#include "file.h"

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
  // TODO: Lab3-2, determine type according to mode
  // iopen in Lab3-2: if file exist, open and return it
  //       if file not exist and type==TYPE_NONE, return NULL
  //       if file not exist and type!=TYPE_NONE, create the file as type
  // you can ignore this in Lab3-1
  int open_type = 114514;
  ip = iopen(path, open_type);
  if (!ip) goto bad;
  int type = itype(ip);
  if (type == TYPE_FILE || type == TYPE_DIR) {
    // TODO: Lab3-2, if type is not DIR, go bad if mode&O_DIR

    // TODO: Lab3-2, if type is DIR, go bad if mode WRITE or TRUNC

    // TODO: Lab3-2, if mode&O_TRUNC, trunc the file
    // 将inode设置进file_t里，然后把偏移量初始化为0
    fp->type = TYPE_FILE; // file_t don't and needn't distingush between file and dir
    fp->inode = ip;
    fp->offset = 0;
  } else if (type == TYPE_DEV) {
    // 根据其设备号取dev_t，然后将其设置进file_t里
    fp->type = TYPE_DEV;
    fp->dev_op = dev_get(idevid(ip));
    iclose(ip);
    ip = NULL;
  } else assert(0);
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
  if(file->type == TYPE_FILE) {
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
  if(file->type == TYPE_FILE) {
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
  return file;
}

void fclose(file_t *file) {
  // Lab3-1, dec file's ref, if ref==0 and it's a file, call iclose
  // // TODO();
  --file->ref; // 当前进程的关闭文件
  if(file->ref == 0 && file->type == TYPE_FILE) {
    iclose(file->inode); // 磁盘的关闭文件
  }
}
