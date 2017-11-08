#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD_NAME "git-branch-name"

int get_git_dir(char*);
char* get_rel_git_dir(char*);
char* read_file(char*);
char* git_branch(char*);

int main(int argc, char const* argv[])
{
  char buffer[1024];
  char* path = buffer, *p;
  if (getcwd(path, sizeof(buffer)) == NULL) {
    fprintf(stderr, CMD_NAME ": failed to get current directory\n");
    return 1;
  }
  if (get_git_dir(path) < 0) {
    return 1;
  }
  p = path; while (*++p) {};
  *p++ = '/'; *p++ = 'H'; *p++ = 'E'; *p++ = 'A'; *p++ = 'D'; *p++ = '\0';
  printf("%s", git_branch(read_file(path)));
  return 0;
}

int get_git_dir(char* path)
{
  struct stat fs;
  char* p = path; while (*++p) {};
  while (path < p) {
    *p++ = '/'; *p++ = '.'; *p++ = 'g'; *p++ = 'i'; *p++ = 't'; *p++ = '\0';
    if (stat(path, &fs) < 0) {
      if (errno == ENOENT) {
        goto NEXT;
      } else {
        perror(CMD_NAME ": stat failed");
        return -1;
      }
    }
    switch (fs.st_mode & S_IFMT) {
      case S_IFDIR:
        return 0;
      case S_IFREG: {
        char* cnt = get_rel_git_dir(path);
        if (cnt == NULL) {
          return -1;
        }
        p -= 6; *p++ = '/';
        while (*cnt != '\n') { *p++ = *cnt++; };
        return 0;
      }
      default:
        fprintf(stderr, CMD_NAME ": unknown file type");
        return -1;
    }
NEXT:
    p -= 6; *p = '\0';
    while (*--p != '/' && path < p) {};
    *p = '\0';
  }
  return -1;
}

char* get_rel_git_dir(char* path)
{
  char* cnt = read_file(path);
  if (cnt == NULL) {
    fprintf(stderr, CMD_NAME ": failed to read file: %s", path);
    return NULL;
  }
  if (strcmp(cnt, "gitdir:") != 32) {
    fprintf(stderr, CMD_NAME ": invalid .git file in submodule: %s", cnt);
    return NULL;
  }
  cnt += 8;
  return cnt;
}

char* read_file(char* file)
{
  FILE* fp;
  char buffer[1024];
  fp = fopen(file, "r");
  return fgets(buffer, sizeof(buffer), (FILE*)fp);
}

char* git_branch(char* str)
{
  char* p = str, *q;
  if (strcmp(p, "ref:") == 32) {
    p += 5;
    while (*p++ != '/') {}; while (*p++ != '/') {};
    for (q = p; *++q != '\n'; ) {}; *q = '\0';
    return p;
  }
  p[7] = '\0';
  return p;
}
