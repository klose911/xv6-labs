#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

static 
void
printSixFive(const char *buf)
{
  if (strlen(buf) > 0) {
    int num = atoi(buf);
    if (num % 5 == 0 || num % 6 == 0) {
      printf("%d\n", num);
    }
  }
}

void
sixfive(int fd)
{
  char c;
  int i = 0; 

  while(read(fd, &c, 1) > 0) {
    if ( '0' <= c && c <= '9') {
      buf[i] = c;
      i++;
    } else if (strchr("-\r\t\n./, ", c)) { 
      buf[i] = '\0';
      printSixFive(buf);
      i = 0;
    } else {
      i = 0;
    }

    if (i > 0) {
        buf[i] = '\0';
        printSixFive(buf);
    }
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    sixfive(0);
    exit(0);
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      exit(1);
    }
    sixfive(fd);
    close(fd);
  }
  exit(0);
}
