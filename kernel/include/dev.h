#ifndef __DEV_H__
#define __DEV_H__

#include <stdint.h>

typedef struct dev {
  int (*read)(void *buf, uint32_t size);
  int (*write)(const void *buf, uint32_t size);
  // 函数指针，指向这个设备的读函数和写函数
} dev_t;

void init_dev();
dev_t *dev_get(int id);

#endif
