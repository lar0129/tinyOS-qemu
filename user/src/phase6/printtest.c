#include "ulib.h"

int
main(int argc, char *argv[])
{
  int i;
  for(i = 0; i < 5; i++){
      fprintf(1, "This is printTest %d\n", i);
  }

  exit(0);
}
