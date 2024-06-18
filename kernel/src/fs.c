#include "klib.h"
#include "fs.h"
#include "disk.h"
#include "proc.h"

#ifdef EASY_FS

#define MAX_FILE  (SECTSIZE / sizeof(dinode_t))
#define MAX_DEV   16
#define MAX_INODE (MAX_FILE + MAX_DEV)

// On disk inode
typedef struct dinode {
  uint32_t start_sect;
  uint32_t length;
  char name[MAX_NAME + 1];
} dinode_t;

// On OS inode, dinode with special info
struct inode {
  int valid;
  int type;
  int dev; // dev_id if type==TYPE_DEV
  dinode_t dinode;
};

static inode_t inodes[MAX_INODE];

void init_fs() {
  dinode_t buf[MAX_FILE];
  read_disk(buf, 256);
  for (int i = 0; i < MAX_FILE; ++i) {
    inodes[i].valid = 1;
    inodes[i].type = TYPE_FILE;
    inodes[i].dinode = buf[i];
  }
}

inode_t *iopen(const char *path, int type) {
  for (int i = 0; i < MAX_INODE; ++i) {
    if (!inodes[i].valid) continue;
    if (strcmp(path, inodes[i].dinode.name) == 0) {
      return &inodes[i];
    }
  }
  return NULL;
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len) {
  assert(inode);
  char *cbuf = buf;
  char dbuf[SECTSIZE];
  uint32_t curr = -1;
  uint32_t total_len = inode->dinode.length;
  uint32_t st_sect = inode->dinode.start_sect;
  int i;
  for (i = 0; i < len && off < total_len; ++i, ++off) {
    if (curr != off / SECTSIZE) {
      read_disk(dbuf, st_sect + off / SECTSIZE);
      curr = off / SECTSIZE;
    }
    *cbuf++ = dbuf[off % SECTSIZE];
  }
  return i;
}

void iadddev(const char *name, int id) {
  assert(id < MAX_DEV);
  inode_t *inode = &inodes[MAX_FILE + id];
  inode->valid = 1;
  inode->type = TYPE_DEV;
  inode->dev = id;
  strcpy(inode->dinode.name, name);
}

uint32_t isize(inode_t *inode) {
  return inode->dinode.length;
}

int itype(inode_t *inode) {
  return inode->type;
}

uint32_t ino(inode_t *inode) {
  return inode - inodes;
}

int idevid(inode_t *inode) {
  return inode->type == TYPE_DEV ? inode->dev : -1;
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len) {
  panic("write doesn't support");
}

void itrunc(inode_t *inode) {
  panic("trunc doesn't support");
}

inode_t *idup(inode_t *inode) {
  return inode;
}

void iclose(inode_t *inode) { /* do nothing */ }

int iremove(const char *path) {
  panic("remove doesn't support");
}

#else

#define DISK_SIZE (128 * 1024 * 1024)
#define BLK_NUM   (DISK_SIZE / BLK_SIZE)

#define NDIRECT   11
#define NINDIRECT (BLK_SIZE / sizeof(uint32_t))
#define NININDIRECT (BLK_SIZE / sizeof(uint32_t)) * (BLK_SIZE / sizeof(uint32_t))

#define IPERBLK   (BLK_SIZE / sizeof(dinode_t)) // inode num per blk

// super block
typedef struct super_block {
  uint32_t bitmap; // block num of bitmap
  uint32_t istart; // start block no of inode blocks
  uint32_t inum;   // total inode num
  uint32_t root;   // inode no of root dir
} sb_t;

// On disk inode
typedef struct dinode {
  uint16_t type;   // file type
  uint16_t nlink;    // reference count
  uint32_t device; // if it is a dev, its dev_id
  uint32_t size;   // file size
  uint32_t addrs[NDIRECT + 2]; // data block addresses, 11 direct and 1 indirect and 1 inindirect
} dinode_t;

struct inode { // 操作系统中的 inode 管理的是打开的文件的信息
  int no; // inode 的编号
  int ref; //  inode 的引用计数
  int del; // 删除记号
  dinode_t dinode;
};

#define SUPER_BLOCK 32
static sb_t sb;

void init_fs() {
  bread(&sb, sizeof(sb), SUPER_BLOCK, 0);
}

#define I2BLKNO(no)  (sb.istart + no / IPERBLK)
#define I2BLKOFF(no) ((no % IPERBLK) * sizeof(dinode_t))

// di开头：与dinote_t（disk里的inode信息）相关的操作
static void diread(dinode_t *di, uint32_t no) {
  bread(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no)); //inode序号转换为磁盘block号和偏移量
}

static void diwrite(const dinode_t *di, uint32_t no) {
  bwrite(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static uint32_t dialloc(int type) {
  // Lab3-2: iterate all dinode, find a empty one (type==TYPE_NONE)
  // set type, clean other infos and return its no (remember to write back)
  // if no empty one, just abort
  // note that first (0th) inode always unused, because dirent's inode 0 mark invalid
  // 在操作系统中磁盘 inode 是可以被回收的，因此你需要遍历所有 inode（注意 0 号 inode 永不使用），
  // 找到一个空闲的（type 是TYPE_NONE的），然后设置其 type（其他成员我们约定在difree时已经清零），
  // 设置完后还要调用diwrite将其写回磁盘，最后返回其编号；
  dinode_t dinode;
  for (uint32_t i = 1; i < sb.inum; ++i) {
    diread(&dinode, i);
    // // TODO();
    if (dinode.type == TYPE_NONE) { // 操作系统管理的空闲文件
      dinode.type = type;
      dinode.size = 0;
      dinode.device = 0;
      dinode.nlink = 1;
      memset(dinode.addrs, 0, sizeof(dinode.addrs));
      diwrite(&dinode, i);
      return i;
    }
  }
  assert(0);
}

static void difree(uint32_t no) {
  dinode_t dinode;
  memset(&dinode, 0, sizeof dinode);
  diwrite(&dinode, no); // 释放操作系统的inode，同步写回磁盘。
}

// b开头：与block相关的操作
static uint32_t balloc() {
  // Lab3-2: iterate bitmap, find one free block
  // set the bit, clean the blk (can call bzero) and return its no
  // if no free block, just abort
  uint32_t byte;
  for (int i = 0; i < BLK_NUM / 32; ++i) {
    bread(&byte, 4, sb.bitmap, i * 4); // 读取4字节bitmap，相当于一次检查32位
    if (byte != 0xffffffff) {
      // // TODO();
      // 找出其中一个为 0 的比特，假设是在第j位，那就说明第32*i+j号逻辑块是空闲的，调用bzero将其清零，
      // 然后将这个比特置为 1 再用bwrite写回磁盘；不必考虑没有空闲逻辑块的情况
      for (int j = 0; j < 32; ++j) {
        if ((byte & (1 << j)) == 0) {
          bzero(32 * i + j);
          byte |= (1 << j); // 32 * i + j
          bwrite(&byte, 4, sb.bitmap, i * 4);
          return 32 * i + j;
        }
      }
    }
  }
  assert(0);
}

static void bfree(uint32_t blkno) {
  // Lab3-2: clean the bit of blkno in bitmap
  assert(blkno >= 64); // cannot free first 64 block
  // // TODO();
  // 回收第blkno号逻辑块
  // 首先使用bread读取这个逻辑块在bitmap上对应的比特所在的字节，再将其对应的比特清零，再用bwrite写回
  uint32_t byte;
  bread(&byte, 1, sb.bitmap, blkno / 8); // 从（blkno / 8）*8处offset，读取1字节bitmap
  byte &= ~(1 << (blkno % 8)); // 清零第blkno % 8位
  // bzero？清不清都行
  bwrite(&byte, 1, sb.bitmap, blkno / 8);
}

#define INODE_NUM 128
static inode_t inodes[INODE_NUM]; //整个操作系统我们可以打开的 inode，我们称之为[活动 inode 表]

// i开头：与inode相关的操作
// 在活动inode表中打开编号为no的inode
static inode_t *iget(uint32_t no) {
  // Lab3-2
  // if there exist one inode whose no is just no, inc its ref and return it
  // otherwise, find a empty inode slot, init it and return it
  // if no empty inode slot, just abort
  for (int i = 0; i < INODE_NUM; ++i) {
    if (inodes[i].ref > 0 && inodes[i].no == no) {
      inodes[i].ref++;
      return &inodes[i];
    }
  }
  for (int i = 0; i < INODE_NUM; ++i) {
    if (inodes[i].ref == 0) {
      inodes[i].ref = 1;
      inodes[i].no = no;
      inodes[i].del = 0;
      diread(&inodes[i].dinode, no); // 从inode起始处读取磁盘inode信息
      return &inodes[i];
    }
  }
  // // TODO();
  assert(0);
}

// 为了保证磁盘上的数据同步，未来每当你修改系统inode中的磁盘inode部分时，都需要调用这个函数
static void iupdate(inode_t *inode) {
  // Lab3-2: sync the inode->dinode to disk
  // call me EVERYTIME after you edit inode->dinode
  diwrite(&inode->dinode, inode->no);
}


// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return NULL.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = NULL
//
static const char* skipelem(const char *path, char *name) {
  const char *s;
  int len;
  while (*path == '/') path++;
  if (*path == 0) return 0;
  s = path;
  while(*path != '/' && *path != 0) path++;
  len = path - s;
  if (len >= MAX_NAME) {
    memcpy(name, s, MAX_NAME);
    name[MAX_NAME] = 0;
  } else {
    memcpy(name, s, len);
    name[len] = 0;
  }
  while (*path == '/') path++;
  return path;
}

static void idirinit(inode_t *inode, inode_t *parent) {
  // Lab3-2: init the dir inode, i.e. create . and .. dirent
  assert(inode->dinode.type == TYPE_DIR);
  assert(parent->dinode.type == TYPE_DIR); // both should be dir
  assert(inode->dinode.size == 0); // inode shoule be empty
  dirent_t dirent;
  // set .
  dirent.inode = inode->no;
  strcpy(dirent.name, ".");
  iwrite(inode, 0, &dirent, sizeof dirent);
  // set ..
  dirent.inode = parent->no;
  strcpy(dirent.name, "..");
  iwrite(inode, sizeof dirent, &dirent, sizeof dirent);
}

// 这个API用于遍历parent这个目录，找到其中名字为name的文件并打开，返回打开的inode
// 如果off不为NULL，将name对应的目录项在parent内容中的偏移量记录于此
// 当name文件不存在时，如果type为TYPE_NONE（0），返回NULL，否则在当前目录创建一个名字为name，类型为type的文件
static inode_t *ilookup(inode_t *parent, const char *name, uint32_t *off, int type) {
  // Lab3-2: iterate the parent dir, find a file whose name is name
  // if off is not NULL, store the offset of the dirent_t to it
  // if no such file and type == TYPE_NONE, return NULL
  // if no such file and type != TYPE_NONE, create the file with the type
  assert(parent->dinode.type == TYPE_DIR); // parent must be a dir
  dirent_t dirent;
  uint32_t size = parent->dinode.size, empty = size;
  for (uint32_t i = 0; i < size; i += sizeof dirent) {
    // directory is a file containing a sequence of dirent structures
    iread(parent, i, &dirent, sizeof dirent);
    if (dirent.inode == 0) {
      // a invalid dirent, record the offset (used in create file), then skip
      // 记录空目录，待会儿用于创建
      if (empty == size) empty = i;
      continue;
    }
    // a valid dirent, compare the name
    // // TODO();
    if(strcmp(dirent.name, name) == 0) {
      // found
      if (off) *off = i; //如果off不为NULL，将name对应的目录项在parent内容中的偏移量记录于off
      return iget(dirent.inode);
    }
  }
  // not found
  if (type == TYPE_NONE) return NULL;
  // need to create the file, first alloc inode, then init dirent, write it to parent
  // if you create a dir, remember to init it's . and ..
  // // TODO();
  uint32_t ino = dialloc(type);
  dirent.inode = ino;
  strcpy(dirent.name, name);
  iwrite(parent, empty, &dirent, sizeof dirent);
  // 初始化文件夹
  inode_t *inode = iget(ino);
  if (type == TYPE_DIR) idirinit(inode, parent);
  // 记录off
  if (off) *off = empty;
  return inode;
}

static inode_t *iopen_parent(const char *path, char *name) {
  // Lab3-2: open the parent dir of path, store the basename to name
  // if no such parent, return NULL
  inode_t *ip, *next;
  // set search starting inode
  if (path[0] == '/') {
    ip = iget(sb.root);
  } else {
    ip = idup(proc_curr()->cwd); // current working dir，复制对inode的引用
  }
  // 绝对路径 = ip + path
  assert(ip);
  while ((path = skipelem(path, name))) {
    // curr round: need to search name in ip
    if (ip->dinode.type != TYPE_DIR) {
      // not dir, cannot search
      iclose(ip);
      return NULL;
    }
    if (*path == 0) {
      // last search, return ip because we just need parent
      return ip;
    }
    // not last search, need to continue to find parent
    next = ilookup(ip, name, NULL, TYPE_NONE);
    if (next == NULL) {
      // name not exist
      iclose(ip);
      return NULL;
    }
    iclose(ip);
    ip = next; // ip向右扩展，path缩短
  }
  iclose(ip);
  return NULL;
}

// 打开path路径指向的文件本身，如果文件不存在且type非TYPE_NONE就以type创建该文件（要求沿途目录必须存在），否则返回NULL
inode_t *iopen(const char *path, int type) {
  // Lab3-2: if file exist, open and return it
  // if file not exist and type==TYPE_NONE, return NULL
  // if file not exist and type!=TYPE_NONE, create the file as type
  char name[MAX_NAME + 1];
  if (skipelem(path, name) == NULL) {
    // no parent dir for path, path is "" or "/" 
    // "" is an invalid path, "/" is root dir
    // 路径就一层， 特判
    return path[0] == '/' ? iget(sb.root) : NULL;
  }
  // path do have parent, use iopen_parent and ilookup to open it
  // remember to close the parent inode after you ilookup it
  // // TODO();
  inode_t *parent = iopen_parent(path, name);
  if (parent == NULL) return NULL;
  inode_t *inode = ilookup(parent, name, NULL, type); // 打开/创建均在ilookup中完成
  // printf("inode name: %s\n", name);
  // printf("inode no: %d\n", inode->no);
  // printf("itype: %d\n", itype(inode));
  if(itype(inode) == TYPE_SOFTLINK){
    // 软链接
    char link_path[MAX_NAME + 1];
    int link_path_len = iread(inode, 0, link_path, inode->dinode.size);
    link_path[link_path_len] = '\0';
    iclose(inode);
    inode = iopen(link_path, type);
  }
  iclose(parent); // ref -- 
  return inode;
}

static uint32_t iwalk(inode_t *inode, uint32_t no) {
  // return the blkno of the file's data's no th block, if no, alloc it
  dinode_t *dinode = &inode->dinode;
  if (no < NDIRECT) {
    // direct address
    // // TODO();
    uint32_t blk = dinode->addrs[no];
    if(blk == 0){
      dinode->addrs[no] = balloc();
      blk = dinode->addrs[no];
      iupdate(inode);
    }
    return blk;
  }
  no -= NDIRECT;
  if (no < NINDIRECT) {
    // indirect address
    // // TODO();
    int indirect_blk = dinode->addrs[NDIRECT];
    if (indirect_blk == 0) {
      dinode->addrs[NDIRECT] = balloc();
      indirect_blk = dinode->addrs[NDIRECT];
      iupdate(inode);
    }
    uint32_t blk;
    bread(&blk, 4, indirect_blk, no * 4); // 4*8=32
    if (blk == 0) {
      blk = balloc();
      bwrite(&blk, 4, indirect_blk, no * 4);
    }
    return blk;
  }
  no -= NINDIRECT;
  // 二级索引
  if (no < NININDIRECT) {
    int inindirect_blk = dinode->addrs[NDIRECT + 1];
    if (inindirect_blk == 0) {
      dinode->addrs[NDIRECT + 1] = balloc();
      inindirect_blk = dinode->addrs[NDIRECT + 1];
      iupdate(inode);
    }
    uint32_t indirect_blk;
    bread(&indirect_blk, 4, inindirect_blk, no / NINDIRECT * 4); // 例：2050个block在第 2050/1024=2个indirect_blk中
    if (indirect_blk == 0) {
      indirect_blk = balloc();
      bwrite(&indirect_blk, 4, inindirect_blk, no / NINDIRECT * 4);
    }
    uint32_t blk;
    bread(&blk, 4, indirect_blk, no % NINDIRECT * 4); // 例：2050个block在第2个indirect_blk的第2050%1024=2个blk中
    if (blk == 0) {
      blk = balloc();
      bwrite(&blk, 4, indirect_blk, no % NINDIRECT * 4);
    }
    return blk;
  }
  assert(0); // file too big, not need to handle this case
}

// 从inode代表的文件的off偏移量处，读取len字节到内存的buf里，返回读取的字节数（或-1如果失败）
int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len) {
  // Lab3-2: read the inode's data [off, MIN(off+len, size)) to buf
  // use iwalk to get the blkno and read blk by blk
  // 一个逻辑块一个逻辑块的读——首先计算当前要读的是这个文件数据的第几个逻辑块，然后调用iwalk获得这个逻辑块的编号，
  // 接着计算这一块最多能读多少字节（注意不能超过文件末尾），然后调用bread将这些数据读进内存，如此循环直至读完
  // // TODO();
  if(off > inode->dinode.size) return -1;
  if(off + len > inode->dinode.size) len = inode->dinode.size - off; // 读取的字节数不能超过文件大小

  int total_read = 0;
  uint32_t no = off / BLK_SIZE; // 逻辑块号
  uint32_t blkoff = off % BLK_SIZE; // 逻辑块内偏移
  while(len > 0){
    uint32_t blkno = iwalk(inode, no); // 物理块号
    uint32_t read_size = MIN(BLK_SIZE - blkoff, len); 
    // 三种限制条件：文件末尾、逻辑块末尾、读完len
    bread(buf+total_read, read_size, blkno, blkoff);
    total_read += read_size;
    len -= read_size; // 判断有没有读完len、有没有到文件末尾
    blkoff = 0;
    no++; // 找了很久的bug：分清逻辑blockno和物理no
  }
  return total_read;
}

// 从内存的buf里，写len字节到inode代表的文件的off偏移量处，返回写入的字节数（或-1如果失败）
// 允许off+len超过写之前文件的大小，此时会更新文件新大小为off+len
int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len) {
  // Lab3-2: write buf to the inode's data [off, off+len)
  // if off>size, return -1 (can not cross size before write)
  // if off+len>size, update it as new size (but can cross size after write)
  // use iwalk to get the blkno and read blk by blk
  // // TODO();
  // 一个逻辑块一个逻辑块的写——首先计算当前要写的是这个文件数据的第几个逻辑块，然后调用iwalk获得这个逻辑块的编号，
  // 接着计算这一块最多能写多少字节，然后调用bwrite将这些数据写进磁盘，如此循环直至写完
  if(off > inode->dinode.size) return -1;
  // 和iread不同，我们允许写的范围的结尾off+len超过文件的大小，此时要更新文件的大小为此新值，
  // 注意此值是记录在磁盘inode部分里的，故改完后还要调用iupdate写回磁盘；
  // 你可以不处理写的范围的开头off超过文件的大小的情况
  if(off + len > inode->dinode.size){
    inode->dinode.size = off + len;
    iupdate(inode);
  }
  int total_write = 0;
  uint32_t no = off / BLK_SIZE; // 逻辑块号
  uint32_t blkoff = off % BLK_SIZE; // 逻辑块内偏移
  while(len > 0){
    uint32_t blkno = iwalk(inode, no);
    uint32_t write_size = MIN(BLK_SIZE - blkoff, len); 
    bwrite(buf + total_write, write_size, blkno, blkoff);
    total_write += write_size;
    len -= write_size; // 判断有没有写完len
    blkoff = 0;
    // blkno++; 找了很久的bug：分清逻辑blockno和物理no
    no++;
  }
  return total_write;
}

void itrunc(inode_t *inode) {
  // Lab3-2: free all data block used by inode (direct and indirect)
  // mark all address of inode 0 and mark its size 0
  // // TODO();
  // 逻辑块的回收——首先遍历inode的所有直接索引，将其对应的逻辑块bfree回收，然后遍历间接索引，将其对应的逻辑块回收，
  // 最后将inode的所有索引项清零，将文件大小设为0，最后调用iupdate写回磁盘
  for(int i = 0; i < NDIRECT; i++){
    if(inode->dinode.addrs[i] != 0){
      bfree(inode->dinode.addrs[i]);
      inode->dinode.addrs[i] = 0;
    }
  }
  if(inode->dinode.addrs[NDIRECT] != 0){
    for(int i = 0; i < NINDIRECT; i++){
      uint32_t blkno;
      bread(&blkno, 4, inode->dinode.addrs[NDIRECT], i * 4);
      if(blkno != 0) bfree(blkno);
      // 不需要bzero，操作系统回收block不代表清空磁盘
    }
    bfree(inode->dinode.addrs[NDIRECT]);
    inode->dinode.addrs[NDIRECT] = 0;
  }
  // 二级索引
  if(inode->dinode.addrs[NDIRECT + 1] != 0){
    for(int i = 0; i < NINDIRECT; i++){
      uint32_t indirect_blk;
      bread(&indirect_blk, 4, inode->dinode.addrs[NDIRECT + 1], i * 4);
      if(indirect_blk != 0){
        for(int j = 0; j < NINDIRECT; j++){
          uint32_t blkno;
          bread(&blkno, 4, indirect_blk, j * 4);
          if(blkno != 0) bfree(blkno);
        }
        bfree(indirect_blk);
      }
    }
    bfree(inode->dinode.addrs[NDIRECT + 1]);
    inode->dinode.addrs[NDIRECT + 1] = 0;
  }
  inode->dinode.size = 0;
  iupdate(inode);
}

inode_t *idup(inode_t *inode) {
  assert(inode);
  inode->ref += 1;
  return inode;
}

void iclose(inode_t *inode) {
  assert(inode);
  if (inode->ref == 1 && inode->del) {
    inode->dinode.nlink--;
    iupdate(inode);
    if (inode->dinode.nlink == 0) {
      itrunc(inode);
      difree(inode->no);
    }
  }
  inode->ref -= 1;
}

uint32_t isize(inode_t *inode) {
  return inode->dinode.size;
}

int itype(inode_t *inode) {
  return inode->dinode.type;
}

uint32_t ino(inode_t *inode) {
  return inode->no;
}

int idevid(inode_t *inode) {
  return itype(inode) == TYPE_DEV ? inode->dinode.device : -1;
}

void iadddev(const char *name, int id) {
  inode_t *ip = iopen(name, TYPE_DEV);
  assert(ip);
  ip->dinode.device = id;
  iupdate(ip);
  iclose(ip);
}

// 前两个目录项一定是.和..，因此目录为空等价于剩下的目录项均为无效目录项（inode为0）
static int idirempty(inode_t *inode) {
  // Lab3-2: return whether the dir of inode is empty
  // the first two dirent of dir must be . and ..
  // you just need to check whether other dirent are all invalid
  assert(inode->dinode.type == TYPE_DIR);
  // // TODO();
  dirent_t dirent;
  for (uint32_t i = 2 * sizeof(dirent_t); i < inode->dinode.size; i += sizeof(dirent_t)) {
    iread(inode, i, &dirent, sizeof(dirent_t));
    if (dirent.inode != 0) return 0;
  }
  return 1;
}

int iremove(const char *path) {
  // Lab3-2: remove the file, return 0 on success, otherwise -1
  // first open its parent, if no parent, return -1
  // then find file in parent, if not exist, return -1
  // if the file need to remove is a dir, only remove it when it's empty
  // . and .. cannot be remove, so check name set by iopen_parent
  // remove a file just need to clean the dirent points to it and set its inode's del
  // the real remove will be done at iclose, after everyone close it
  // // TODO();
  // printf("iremove: %s\n", path);
  char name[MAX_NAME + 1];
  inode_t *parent = iopen_parent(path, name);
  if (parent == NULL) return -1;
  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0){
    iclose(parent);
    return -1;
  }
  uint32_t off;
  inode_t *inode = ilookup(parent, name, &off, TYPE_NONE);
  if (inode == NULL) {
    iclose(parent);
    return -1;
  }
  if (itype(inode) == TYPE_DIR && !idirempty(inode)) {
    iclose(inode);
    iclose(parent);
    return -1;
  }
  inode->del = 1;
  dirent_t dirent;
  memset(&dirent, 0, sizeof(dirent));
  iwrite(parent, off, &dirent, sizeof(dirent));

  iclose(inode);
  iclose(parent);
  return 0;
}

// 硬链接
int ilink(const char *path,inode_t *old_node){
  char name[MAX_NAME + 1];
  int type = old_node->dinode.type;
  int old_no = old_node->no;
  if (skipelem(path, name) == NULL) {
    // no parent dir for path, path is "" or "/" 
    // "" is an invalid path, "/" is root dir
    // 路径就一层， 特判
    return -1;
  }
  // path do have parent, use iopen_parent and ilookup to open it
  // remember to close the parent inode after you ilookup it
  // // TODO();
  inode_t *parent = iopen_parent(path, name);
  if (parent == NULL) return -1;
  int link_result = icreate_link(parent, name, type, old_no);
  if(link_result == 0) {
    old_node->dinode.nlink++;
    iupdate(old_node);
  }
  iclose(parent); // ref -- 
  return link_result;
}

int icreate_link(inode_t *parent, const char *name, int type, int old_no) {
  // Lab3-2: iterate the parent dir, find a file whose name is name
  // if off is not NULL, store the offset of the dirent_t to it
  // if no such file and type == TYPE_NONE, return NULL
  // if no such file and type != TYPE_NONE, create the file with the type
  assert(parent->dinode.type == TYPE_DIR); // parent must be a dir
  dirent_t dirent;
  uint32_t size = parent->dinode.size, empty = size;
  for (uint32_t i = 0; i < size; i += sizeof dirent) {
    // directory is a file containing a sequence of dirent structures
    iread(parent, i, &dirent, sizeof dirent);
    if (dirent.inode == 0) {
      // a invalid dirent, record the offset (used in create file), then skip
      // 记录空目录，待会儿用于创建
      if (empty == size) empty = i;
      continue;
    }
    // a valid dirent, compare the name
    // // TODO();
    if(strcmp(dirent.name, name) == 0) {
      // found
      return -1;
    }
  }
  // not found
  if (type == TYPE_NONE) return -1;
  // need to create the file, first alloc inode, then init dirent, write it to parent
  // if you create a dir, remember to init it's . and ..
  // // TODO();
  dirent.inode = old_no;
  strcpy(dirent.name, name);
  iwrite(parent, empty, &dirent, sizeof dirent);
  return 0;
}

// 软链接
int isymlink(const char *newpath,const char *oldpath){
  char name[MAX_NAME + 1];
  int type = TYPE_SOFTLINK;
  if (skipelem(newpath, name) == NULL) {
    // no parent dir for path, path is "" or "/" 
    // "" is an invalid path, "/" is root dir
    // 路径就一层， 特判
    return -1;
  }
  // path do have parent, use iopen_parent and ilookup to open it
  // remember to close the parent inode after you ilookup it
  // // TODO();
  inode_t *parent = iopen_parent(newpath, name);
  if (parent == NULL) return -1;
  int link_result = icreate_symlink(parent, name, type,oldpath );
  iclose(parent); // ref -- 
  return link_result;
}

int icreate_symlink(inode_t *parent, const char *name, int type, const char *oldpath) {
  // Lab3-2: iterate the parent dir, find a file whose name is name
  // if off is not NULL, store the offset of the dirent_t to it
  // if no such file and type == TYPE_NONE, return NULL
  // if no such file and type != TYPE_NONE, create the file with the type
  assert(parent->dinode.type == TYPE_DIR); // parent must be a dir
  dirent_t dirent;
  uint32_t size = parent->dinode.size, empty = size;
  for (uint32_t i = 0; i < size; i += sizeof dirent) {
    // directory is a file containing a sequence of dirent structures
    iread(parent, i, &dirent, sizeof dirent);
    if (dirent.inode == 0) {
      // a invalid dirent, record the offset (used in create file), then skip
      // 记录空目录，待会儿用于创建
      if (empty == size) empty = i;
      continue;
    }
    // a valid dirent, compare the name
    // // TODO();
    if(strcmp(dirent.name, name) == 0) {
      // found
      return -1;
    }
  }
  // not found
  if (type == TYPE_NONE) return -1;
  // need to create the file, first alloc inode, then init dirent, write it to parent
  // if you create a dir, remember to init it's . and ..
  // // TODO();
  // 创建软链接dinode
  uint32_t ino = dialloc(type);
  dirent.inode = ino;
  strcpy(dirent.name, name);
  iwrite(parent, empty, &dirent, sizeof dirent);
  // 初始化软连接inode
  inode_t *inode = iget(ino);
  int len = strlen(oldpath);
  inode->dinode.size = len;
  iwrite(inode, 0, oldpath, inode->dinode.size);
  iupdate(inode);
  iclose(inode);
  return 0;
}
#endif
