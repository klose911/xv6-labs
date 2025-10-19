#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"


char *exec_argv[MAXARG];
int exec_argc = 0;

static void do_find(int, char *, char *);
static void do_exec(char *argv[]);
static void run_exec(char *file);
static int do_fork();

void find(char *path, char *name) {
  int fd;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    fprintf(2, "find: not a dir %s\n", path);
    close(fd);
    return;
  }

  do_find(fd, path, name);
  close(fd);
}

static void do_find(int fd, char *path, char *name) {
  char buf[512], *p;
  struct dirent de;
  struct stat st;

  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
    fprintf(2, "find: path too long\n");
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    if (stat(buf, &st) < 0) {
      printf("find: cannot stat %s\n", buf);
      continue;
    }

    switch (st.type) {
    case T_DEVICE:
    case T_FILE:
      if (!strcmp(name, de.name)) {
        if (exec_argc == 0) {
          printf("%s\n", buf);
        } else {
          run_exec(buf);
        }
      }
      break;
    case T_DIR:
      if (strcmp(".", de.name) && strcmp("..", de.name)) {
        find(buf, name);
      }
      break;
    default:
      break;
    }
  }
}

static int do_fork() {
  int pid = fork();
  if (pid == -1) {
    fprintf(2, "fork failed");
    exit(1);
  }
  return pid;
}

static void run_exec(char *file) {  
  if (file == 0) {
    fprintf(2, "no file to exec");
    exit(1);
  }

  char *cmd_argv[MAXARG];
  if (do_fork() == 0) {
    int i;    
    for (i = 0; i < exec_argc; i++) {
      cmd_argv[i] = exec_argv[i];
    }
    cmd_argv[i] = file;
    do_exec(cmd_argv);
  }
  wait(0);
}

static void do_exec(char *argv[]) {
  if (argv == 0)
    exit(1);

  if (argv[0] == 0)
    exit(1);

  exec(argv[0], argv);
  fprintf(2, "exec %s failed\n", argv[0]);

  exit(0);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "usage: find dir file\n");
    exit(1);
  }

  if (argc > 3 && !strcmp("-exec", argv[4])) {
    exec_argc = argc - 4;
    if (exec_argc > 0) {
      for (int i = 0; i < exec_argc; i++) {
        exec_argv[i] = argv[i + 5];
      }
    }
  }

  find(argv[1], argv[2]);
  exit(0);
}
