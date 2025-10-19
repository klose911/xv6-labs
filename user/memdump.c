#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

void memdump(char *fmt, char *data);

int main(int argc, char *argv[]) {
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2025};
    memdump("ii", (char *)a);

    printf("Example 2:\n");
    memdump("S", "a string");

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example);

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example);
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void memdump(char *fmt, char *data) {
  // Your code here.
  int i;
  char c;
  int x = 0;
  
  for (i = 0; fmt[i]; i++) {
    c = fmt[i] & 0xff;
    switch (c) {
    case 'i':  
      printf("%d\n", *(int *)(data + x));
      x += sizeof(int);
      break;
      
    case 'p':
      printf("%lx\n", *(uint64 *)(data + x));
      x += sizeof(uint64);
      break;
      
    case 'h': 
      printf("%d\n", *(short *)(data + x));
      x += sizeof(short);
      break;

    case 'c':      
      printf("%c\n", *(data + x));
      x += sizeof(char);
      break;

    case 's':
      uint64 p;
      memcpy(&p, data, sizeof(uint64));
      printf("%s\n", (char *)p);
      x += sizeof(uint64);
      break;
      
    case 'S':
      printf("%s\n", data + x);
      return;

    default:
      break;
    }
  }
}
