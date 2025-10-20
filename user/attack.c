#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // Your code here.
  uint64 addr = 0x0000000000000010; 
  while(1){
    char *a = sbrk(PGSIZE);
    if(a == SBRK_ERROR){
      break;
    }
    if (strcmp("This may help.", a + addr) == 0) {
      printf("%s\n", a + addr + 16);
    } 
  }
  exit(1);
}
