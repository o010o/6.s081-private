#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  int err = pipe(p);
  if (err < 0) {
    fprintf(2, "pipe exec failed");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork exec failed");
    exit(1);
  } else if (pid == 0) {
    char byte;
    int n = read(p[0], &byte, 1);
    if (n < 0) {
      fprintf(2, "read failed");
      exit(1);
    }

    fprintf(1, "%d: received ping\n", getpid());

    n = write(p[1], &byte, 1);
    if (n < 0) {
      fprintf(2, "write data to pipe failed");
      exit(1);
    }
    exit(0);
  } 
  char byte;
  int n = write(p[1], &byte, 1);
  if (n < 0) {
    fprintf(2, "write data to pipe failed");
    exit(1);
  }
  
  n = read(p[0], &byte, 1);
  if (n < 0) {
    fprintf(2, "read data from pipe failed");
    exit(1);
  }

  fprintf(1, "%d: received pong\n", getpid());

  exit(0);
}