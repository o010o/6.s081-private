#include "kernel/types.h"
#include "user/user.h"

void Progress(int parent[2], int first) {
  close(parent[1]);
  int val;
  int isChildExist = 0;
  int child[2];
  int pid = -1;
  while (1) {
    int n = read(parent[0], &val, sizeof(val));
    if (n < 0) {
      fprintf(2, "read failed\n");
      exit(1);
    } else if (n > 0) {
      if (val % first == 0) {
        continue;
      }
      
      // send candidate to child
      if (isChildExist == 0) {
        isChildExist = 1;
        fprintf(1, "prime %d\n", val);
        n = pipe(child);
        if (n < 0) {
          fprintf(2, "pipe failed\n");
          exit(1);
        }
        pid = fork();
        if (pid < 0) {
          fprintf(2, "fork failed\n");
          exit(1);
        } else if (pid == 0) {
          Progress(child, val);
          exit(1);
        }
        close(child[0]);
      } 

      n = write(child[1], &val, sizeof(val));
      if (n < 0) {
          fprintf(2, "write failed\n");
          exit(1);
      }
    } else {
      break;
    }
  }

  close(child[1]);
  close(parent[0]);

  if (pid != -1) {
    wait(&pid);
  }

  exit(0);
}

int main(int argc, char *argv[]) {
  int start = 2;
  int last = 35;
  int child[2];
  int isChildExist = 0;
  int pid = -1;
  // FIXME:i think the codes is bullshit
  fprintf(1, "prime %d\n", start);
  for (int i = start + 1; i <= last; ++i) {
    if (i % start == 0) {
      continue;
    }
    if (isChildExist == 0) {
      isChildExist = 1;
      pipe(child);
      pid = fork();
      if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
      } else if (pid == 0) {
        Progress(child, i);
        exit(1);
      }
      close(child[0]);
    } 
    write(child[1], &i, sizeof(i));
  }

  close(child[1]);

  if (pid != -1) {
    wait(&pid);
  }
  exit(0);
}