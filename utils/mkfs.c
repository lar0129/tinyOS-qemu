// 一个打包文件为磁盘文件系统的工具——我们会使用这个工具来构建给QEMU运行的磁盘中的用户文件部分，即第32~32767号逻辑块
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <libgen.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((noreturn))
void panic(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

#define TODO() panic("implement me")

// Disk layout:
//         [ boot.img | kernel.img |                      user.img                      ]
//         [   mbr    |   kernel   | super block | bit map | inode blocks | data blocks ]
// sect    0          1          256           264       272            512        262144
// block   0                      32            33        34             64         32768
// YOUR TASK: build user.img

#define DISK_SIZE (128 * 1024 * 1024) // disk is 128 MiB
#define BLK_SIZE  4096 // combine 8 sects to 1 block
#define BLK_OFF   32 // user img start from 256th sect, i.e. 32th block
#define IMG_SIZE  (DISK_SIZE - BLK_OFF * BLK_SIZE) // size of user.img
#define IMG_BLK   (IMG_SIZE / BLK_SIZE)
#define BLK_NUM   (DISK_SIZE / BLK_SIZE)

#define SUPER_BLK   BLK_OFF        // block no of super block
#define BITMAP_BLK  (BLK_OFF + 1)  // block no of bitmap
#define INODE_START (BLK_OFF + 2)  // start block no of inode blocks
#define DATA_START  (BLK_OFF + 32) // start block no of data blocks

#define IPERBLK   (BLK_SIZE / sizeof(dinode_t)) // inode num per blk
#define INODE_NUM ((DATA_START - INODE_START) * IPERBLK)

#define NDIRECT   12
#define NINDIRECT (BLK_SIZE / sizeof(uint32_t))

#define TYPE_NONE 0
#define TYPE_FILE 1
#define TYPE_DIR  2
#define TYPE_DEV  3

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

// block
typedef union {
  uint8_t  u8buf[BLK_SIZE];
  uint32_t u32buf[BLK_SIZE / 4];
} blk_t;

// super block
typedef struct {
  uint32_t bitmap; // block num of bitmap
  uint32_t istart; // start block no of inode blocks
  uint32_t inum;   // total inode num
  uint32_t root;   // inode no of root dir
} sb_t;

// on-disk inode
typedef struct {
  uint32_t type;   // file type
  uint32_t device; // if it is a dev, its dev_id
  uint32_t size;   // file size
  uint32_t addrs[NDIRECT + 1]; // data block addresses, 12 direct and 1 indirect
} dinode_t;

// directory is a file containing a sequence of dirent structures

#define MAX_NAME  (31 - sizeof(uint32_t))
typedef struct {
  uint32_t inode; // inode no of the file
  char name[MAX_NAME + 1]; // name of the file
} dirent_t;

struct {blk_t blocks[IMG_BLK];} *img; // pointor to the img mapped memory
sb_t *sb; // pointor to the super block
blk_t *bitmap; // pointor to the bitmap block
dinode_t *root; // pointor to the root dir's inode

// get the pointer to the memory of block no
static inline blk_t *bget(uint32_t no) { // 忽略前32个操作系统的块
  assert(no >= BLK_OFF);
  return &(img->blocks[no - BLK_OFF]);
}

// get the pointer to the memory of inode no
static inline dinode_t *iget(uint32_t no) {
  return (dinode_t*)&(bget(no/IPERBLK + INODE_START)->u8buf[(no%IPERBLK)*sizeof(dinode_t)]);
}

void init_disk();
uint32_t balloc();
uint32_t ialloc(int type);
blk_t *iwalk(dinode_t *file, uint32_t blk_no);
void iappend(dinode_t *file, const void *buf, uint32_t size);
void add_file(char *path);

int main(int argc, char *argv[]) {
  // argv[1] is target user.img, argv[2..argc-1] are files you need to add
  assert(argc > 2);
  static_assert(BLK_SIZE % sizeof(dinode_t) == 0, "sizeof inode should divide BLK_SIZE");
  char *target = argv[1];
  int tfd = open(target, O_RDWR | O_CREAT | O_TRUNC, 0777);
  if (tfd < 0) panic("open target error");
  if (ftruncate(tfd, IMG_SIZE) < 0) panic("truncate error"); // 把文件的大小扩展到(32768-32)×4KiB=127.875MiB（使用全0填充）
  // map the img to memory, you can edit file by edit memory
  img = mmap(NULL, IMG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0); // 把文件的内容映射到内存里,方便我们操作
  assert(img != (void*)-1);
  init_disk();
  for (int i = 2; i < argc; ++i) {
    add_file(argv[i]);
  }
  munmap(img, IMG_SIZE);
  close(tfd);
  return 0;
}

void init_disk() {
  sb = (sb_t*)bget(SUPER_BLK);
  sb->bitmap = BITMAP_BLK;
  sb->istart = INODE_START;
  sb->inum = INODE_NUM;
  bitmap = bget(BITMAP_BLK);
  // mark first 64 blocks used
  bitmap->u32buf[0] = bitmap->u32buf[1] = 0xffffffff; // data blocks 区从 64 号逻辑块开始
  // alloc and init root inode
  sb->root = ialloc(TYPE_DIR);
  root = iget(sb->root);
  // set root's . and ..
  dirent_t dirent;
  dirent.inode = sb->root;
  strcpy(dirent.name, ".");
  iappend(root, &dirent, sizeof dirent);
  strcpy(dirent.name, "..");
  iappend(root, &dirent, sizeof dirent);
}

uint32_t balloc() {
  // alloc a unused block, mark it on bitmap, then return its no
  static uint32_t next_blk = 64;
  if (next_blk >= BLK_NUM) panic("no more block");
  bitmap->u8buf[next_blk / 8] |= (1 << (next_blk % 8)); // 8位标志位
  return next_blk++;
}

uint32_t ialloc(int type) {
  // alloc a unused inode, return its no
  // first inode always unused, because dirent's inode 0 mark invalid
  static uint32_t next_inode = 1;
  if (next_inode >= INODE_NUM) panic("no more inode");
  iget(next_inode)->type = type;
  return next_inode++;
}

blk_t *iwalk(dinode_t *file, uint32_t blk_no) {
  // return the pointer to the file's data's blk_no th block, if no, alloc it
  if (blk_no < NDIRECT) {
    // direct address
    // // TODO();
    int blk = file->addrs[blk_no];
    if (blk == 0) {
      file->addrs[blk_no] = balloc();
      blk = file->addrs[blk_no];
    }
    return bget(blk);
  }
  blk_no -= NDIRECT;
  if (blk_no < NINDIRECT) {
    // indirect address
    // // TODO();
    int indirect_blk = file->addrs[NDIRECT];
    if (indirect_blk == 0) { // 间接块还没alloc
      file->addrs[NDIRECT] = balloc();
      indirect_blk = file->addrs[NDIRECT];
    }
    uint32_t *indirect = bget(indirect_blk)->u32buf; // 地址是32位的
    int blk = indirect[blk_no];
    if (blk == 0) {
      indirect[blk_no] = balloc();
      blk = indirect[blk_no];
    }
    return bget(blk);
  }
  panic("file too big");
}

void iappend(dinode_t *file, const void *buf, uint32_t size) {
  // append buf to file's data, remember to add file->size
  // you can append block by block
  // // TODO();
  // 一个逻辑块一个逻辑块的追加——当前文件大小除以BLK_SIZE（4096）可以得到要写的逻辑块在文件索引里是第几项，
  // 然后调用iwalk得到对应的逻辑块，当前文件大小对BLK_SIZE取模可以得到要写的字节的开始位于逻辑块的偏移量，
  // 因此这次最多能写size和BLK_SIZE-偏移量中的较小值，写的时候直接利用memcpy在buf和逻辑块直接复制即可，
  // 写完后将文件大小和buf向后移动本次写的字节数，size减去这么多字节——如此这么循环直至全部写完
  uint32_t offset = file->size % BLK_SIZE; // 第一次写可能不和block对齐
  uint32_t blk_no = file->size / BLK_SIZE;
  while (size > 0) {
    blk_t *blk = iwalk(file, blk_no);
    uint32_t write_size = MIN(size, BLK_SIZE - offset); // 写可能不足一个块
    memcpy(blk->u8buf + offset, buf, write_size);
    file->size += write_size;
    buf += write_size;
    size -= write_size;
    offset = 0;
    blk_no++; // 溢出12后也在iwalk里处理
  }
}

// 把文件添加到我们磁盘的根目录里
void add_file(char *path) {
  static uint8_t buf[BLK_SIZE];
  FILE *fp = fopen(path, "rb");
  if (!fp) panic("file not exist");
  // alloc a inode
  uint32_t inode_blk = ialloc(TYPE_FILE);
  dinode_t *inode = iget(inode_blk);
  // append dirent to root dir
  dirent_t dirent;
  dirent.inode = inode_blk;
  strcpy(dirent.name, basename(path));
  iappend(root, &dirent, sizeof dirent);
  // write the file's data, first read it to buf then call iappend
  // // TODO();
  // 循环调用fread和iappend，先用fread将文件内容读到buf中，再用iappend添加到磁盘中，直至读完整个文件
  while (1) {
    size_t size = fread(buf, 1, BLK_SIZE, fp); // 读取1*BLK_SIZE大小的数据到buf中
    if (size == 0) break;
    iappend(inode, buf, size); // 溢出block后也在iappend里处理
  }

  fclose(fp);
}
