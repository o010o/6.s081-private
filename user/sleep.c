#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 2 || argc >= 3) {
    fprintf(2, "usage: sleep number");
    exit(1);
  }

  int sleep_secs = atoi(argv[1]);

  sleep(sleep_secs * 10);

  exit(0);
}