#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

char * ReadLineFrom(int fd, char *line, int size) {
	char *p = line;
	int n = 0;
	while ((n = read(fd, p, 1)) == 1) {
		if (*p == '\n') {
			*p = 0;
			break;
		}
		p++;
	}
	if (n == 0) {
		return 0;
	}
	return line;
}

int GetArgsFromLine(char *line, char **argvs, uint size) {
	if (size == 0) {
		return 0;
	}
	char *p = line;
	const char *end = line + strlen(line);
	int i = 0;

	while (*p == ' ') {
		++p;
	}
  
	while (p < end && i < size) {
		argvs[i++] = p;
		p = strchr(p, ' ');
		if (p == 0) {
			break;
		}
		*p++ = 0;
		while (p < end && *p == ' ') {
			p++;
		}
	}
  return i + 1;
}

int main(int argc, char **argv) {
	// echo "1\n2" | xargs echo line
	if (argc < 2) {
		fprintf(2, "usage:xargs order [param]");
		exit(1);
	}
  
	int argNum = 0;
	char *argvs[MAXARG];
  for (int i = 1; i < argc; ++i) {
		argvs[i - 1] = argv[i];
		++argNum;
	}

  int pid;
  char line[512];
	while (ReadLineFrom(0, line, sizeof(line)) != 0) {
		argNum += GetArgsFromLine(line, &argvs[argNum], MAXARG - argNum);
		argvs[argNum] = 0;

		pid = fork();
		if (pid == 0) {
			exec(argvs[0], argvs);
			exit(-1);
		}

		if (wait(&pid) < 0) {
			fprintf(2, "xargs: wait failed\n");
			exit(-1);
		}
	}

	exit(0);
}