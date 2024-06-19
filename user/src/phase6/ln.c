#include "ulib.h"

int
main(int argc, char *argv[])
{

  if(argc < 3){
    exit(0);
  }
  // 软
  if(strcmp(argv[1], "-s") == 0){
    if (symlink(argv[2], argv[3]) < 0) {
      printf("Failed to create symlink %s\n", argv[3]);
      exit(1);
    }
    printf("symlink %s to %s\n", argv[2], argv[3]);
    exit(0);
  }
  // 硬
  if (link(argv[1], argv[2]) < 0) {
      printf("Failed to create link %s\n", argv[2]);
      exit(1);
  }
  printf("link %s to %s\n", argv[1], argv[2]);
  exit(0);
}
