#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

#define NAME_MAX (255)
#define PATH_MAX (4096)

int CopyLastNameTo(const char *path, char *output, int size) {
  const char *p;
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  int writes = 0;
  for (char *o = output; p < path + strlen(path) && o < output + size; p++) {
    *o++ = *p;
    ++writes;
  }
  return writes;
}

// dfs search file!
int GetDirAndName(int argc, char **argv, char *pPath, int pathSize, char *pName, int nameSize) {
  if (argc == 2) {
    int nameLen = strlen(argv[1]);
    if (pathSize < 2 || nameLen >= nameSize) {
      fprintf(2, "find: path or name to long\n");
      return -1;
    }
    strcpy(pPath, ".");
    memmove(pName, argv[1], nameLen);
    pName[nameLen] = 0;
    return 0;
  }

  int nameLen = strlen(argv[2]);
  int pathLen = strlen(argv[1]);

  if (pathLen >= pathSize || nameLen >= nameSize) {
    fprintf(2, "find: path or name to long\n");
    return -1;
  }

  strcpy(pPath, argv[1]);
  if (pPath[pathLen - 1] == '/') {
    pPath[pathLen - 1] = 0;
  } else {
    pPath[pathLen] = 0;
  }
  strcpy(pName, argv[2]);
  pName[nameLen] = 0;
  return 0;
}

int VerifyDirAndName(const char *dirPath, const char *name) {
  if (strchr(name, '/') != 0) {
    fprintf(2, "find: name %s should not have '/'\n", name);
    return -1;
  }

  int fd = open(dirPath, 0);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", dirPath);
    return -1;
  }
  close(fd);
  return 0;
}

// ^xxxxx$ 
// xxxxxx

int IsMatch(const char *regular, int rSize, const char *text, int tSize) {
  // TODO:regular
  // regular is equal to text if there is no patten
  // printf("match %s and %s\n", regular, text);
  int reg_it = 0;
  int text_it = 0;

  while (reg_it < rSize && text_it < tSize) {
    if (regular[reg_it] == '*') {
      if (reg_it == rSize - 1) {
        // printf("no more regular, matched!\n");
        return 0;
      }
      char *next = strchr(text + text_it, regular[reg_it + 1]);
      if (next == 0) {
        // printf("seach %c, begin at text[%d] failed\n", regular[reg_it + 1], text_it);
        return -1;
      }

      reg_it = reg_it + 2;
      text_it = next + 1 - text;
    } else if (regular[reg_it] == '?') {

      ++reg_it;
      ++text_it;
    } else if (regular[reg_it] == '[') {
      char *right = strchr(regular, ']');
      if (right == 0) {
        // fprintf(2, "find: invalid regular");
        exit(1);
      }

      int end = right - regular;
      int i = reg_it + 1;
      for (; i < end; ++i) {
        if (regular[i] == text[text_it]) {
          break;
        }   
      }
      if (i == end) {
        // printf("%c not in interval [%d,%d]\n", text[text_it], reg_it + 1, end - 1);
        return -1;
      }

      reg_it = right - regular + 1;
      ++text_it;
    } else {
      if (text[text_it] != regular[reg_it]) {
        // printf("char compare %c(%d) != %c(%d)\n", text[text_it], text_it, regular[reg_it], reg_it);
        return -1;
      }

      ++reg_it;
      ++text_it;
    }
  }

  return ((reg_it == rSize) && (text_it == tSize)) ? 0 : -1;
}

int SearchFileDfs(char *dirPath, int size, int end, const char *name) {
  int fd;
  struct dirent de;
  struct stat st;

  fd = open(dirPath, 0);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", dirPath);
    return -1;
  }

  int err = fstat(fd, &st);
  if (err < 0) {
    close(fd);
    fprintf(2, "find: cannot fstat %s\n", dirPath);
    return -1;
  }

  if (st.type != T_DIR) {
    close(fd);
    fprintf(2, "why dirPath not a dir");
    exit(1);
  }

  char *p = dirPath + end;
  *p++ = '/';
  end++;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) {
      continue;
    }
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
      continue;
    }

    if (end + DIRSIZ > size) {
      fprintf(2, "find: path to long\n");
      continue;
    }

    int n = CopyLastNameTo(de.name, p, DIRSIZ);
    if (end + n < size) {
      p[n] = 0;
    }

    if (stat(dirPath, &st) < 0) {
      fprintf(2, "find: cannot stat %s(%d)\n", dirPath, strlen(dirPath));
      continue;
    }

    // FIXME:may failed even p is same as name.
    if (IsMatch(name, strlen(name), p, n) == 0) {
      fprintf(1, "%s\n", dirPath);
    }

    if (st.type == T_DIR) {
      int next = strlen(p) <= DIRSIZ ? end + strlen(p) : end + DIRSIZ;
      SearchFileDfs(dirPath, size, next, name);
    }
  }

  close(fd);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(2, "usage:find [dir] filename");
    exit(1);
  }
  // default dir is current dir
  // TODO:support regular
  char dirPath[512] = ".";
  char name[DIRSIZ] = "\0";

  if (GetDirAndName(argc, argv, dirPath, sizeof(dirPath), name, sizeof(dirPath)) < 0) {
    exit(1);
  }

  if (VerifyDirAndName(dirPath, name) < 0) {
    exit(1);
  }

  // dfs search file
  SearchFileDfs(dirPath, sizeof(dirPath), strlen(dirPath), name);
  exit(0);
}