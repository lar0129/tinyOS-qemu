#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>

typedef struct inode inode_t;

// #define EASY_FS // TODO: comment me at Lab3-2

void init_fs();

inode_t *iopen(const char *path, int type);
// 从inode代表的文件的off偏移量处，读取len字节到内存的buf里
int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len);
int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len);
void itrunc(inode_t *inode);
inode_t *idup(inode_t *inode);
void iclose(inode_t *inode);
uint32_t isize(inode_t *inode);
int itype(inode_t *inode);
uint32_t ino(inode_t *inode);
int idevid(inode_t *inode);
void iadddev(const char *name, int id);
int iremove(const char *path);
int ilink(const char *path,inode_t *old_node);
int ilookup_link(inode_t *parent, const char *name, int type, int old_no);
int isymlink(const char *path,const char *link_path);
int ilookup_symlink(inode_t *parent, const char *name, int type, const char *link_path);

#ifdef EASY_FS

#define MAX_NAME  (31 - 2 * sizeof(uint32_t))

#else

#define MAX_NAME  (31 - sizeof(uint32_t))

typedef struct dirent {
  uint32_t inode;
  char name[MAX_NAME + 1];
} dirent_t;

#endif

#endif
