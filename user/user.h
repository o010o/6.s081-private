struct stat;
struct rtcdate;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
/**
 * @brief map file into current process.
 * @param addr local virtual memory mapping to. 0
 * @param length bytes need to map
 * @param port readable/writeable/executable. FOR now, it is PROT_READ,PROT_WRITE or both.
 * @param flags MAP_SHARED or MAP_PRIVATE.
 * @param fd the file descriptor of the file to map.
 * @param offset. 0.
 * @retval return mapped address if success, -1 if failed.
*/
void* mmap(void *addr, uint64 length, int prot, int flags, int fd, uint64 offset);
/**
 * @brief 
 * @param addr. start address of mapped virtual memory.
 * @param length. the length of mapped virtual memory.
*/
int munmap(void *addr, uint64 length);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
