#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD_NAME "git-branch-name"
#define PATH_MAX 4096
#define QUIET 0
long hash_truncate_length = 12;

int get_git_dir(char* path, char* path_end);
ssize_t read_first_line_of_file(const char* file, char* out, size_t n);
ssize_t read_first_line_of_file_into_buffer(const char* file);
char* get_git_branch();

char* buffer_start;
char* buffer_end;
char buffer[PATH_MAX + 16];

int main(int argc, const char* argv[]) {
  
  // If arg1 is given, use it as the hash truncate length
  if (argc >= 2 && strncmp(argv[1], "--help", 6) == 0) {
    fprintf(stderr, "usage: " CMD_NAME " [<hash-truncate-length>] [<dir>]\n");
    return 1;
  }

  // If arg1 is given, use it as the hash truncate length
  if (argc >= 2) {
    hash_truncate_length = atoi(argv[1]);
    if (hash_truncate_length <= 0) hash_truncate_length = 12;
  }

  // If arg2 is given, use it as the starting directory
  if (argc >= 3) {
    chdir(argv[2]);
  }

  // Store current directory in buffer
  if (getcwd(buffer, PATH_MAX + 1) == NULL) {
    if (!QUIET) perror(CMD_NAME ": failed to get current directory");
    return 1;
  }

  // Search upwards for directory containing `.git` (and store it in buffer)
  buffer_start = buffer;
  buffer_end = buffer + strlen(buffer);
  if (get_git_dir(buffer_start, buffer_end) < 0) {
    return 1;
  }

  // Append `.git/HEAD`  (and store new path in buffer)
  char* p = buffer_end;
  *p++ = '/';
  *p++ = 'H';
  *p++ = 'E';
  *p++ = 'A';
  *p++ = 'D';
  *p++ = '\0';

  // Read HEAD file (and store result in buffer)
  int bytes_read = read_first_line_of_file_into_buffer(buffer_start);
  if (bytes_read < 0) {
    if (!QUIET) perror(CMD_NAME ": failed to read .git/HEAD file");
    return 1;
  }

  // Extract branch (or hash) from HEAD file (and store result in buffer)
  if (get_git_branch() == NULL) {
    if (!QUIET) fprintf(stderr, CMD_NAME ": invalid .git/HEAD file format\n");
    return 1;
  }

  // Print result
  printf("%s", buffer_start);
  return 0;
}

int get_git_dir(char* path, char* path_end) {
  struct stat fs;

  // p points to end of buffer
  char* p = buffer_end;

  // Navigate upward until a path containing a .git subdirectory is found.
  while (path < p) {

    // Append "/.git\0" to buffer
    *p++ = '/';
    *p++ = '.';
    *p++ = 'g';
    *p++ = 'i';
    *p++ = 't';
    *p++ = '\0';

    // If current directory plus ".git" doesn't exist, recurse upward
    if (stat(path, &fs) < 0) {
      if (errno == ENOENT) {
        // Unappend "/.git\0"
        p -= 6;
        *p = '\0';
        // Search backwards for a slash
        while (*--p != '/' && path < p) { };
        // Mark slash as new end of string, and recurse
        *p = '\0';
        continue;
      } else {
        if (!QUIET) perror(CMD_NAME ": stat failed");
        return -1;
      }
    }

    // We found a ".git" file or directory
    // If ".git" is a directory, we're done
    if (S_ISDIR(fs.st_mode)) {
      buffer_end = p - 1;
      return 0;
    }

    // If .git is a file, we're in a submodule, so we'll need to read the ".git"
    // file, e.g., "gitdir: ../.git/modules/git-branch-name", and the path
    // within it.
    else if (S_ISREG(fs.st_mode)) {
      int bytes_read = read_first_line_of_file_into_buffer(path);
      if (bytes_read < 0) {
        if (!QUIET) perror(CMD_NAME ": failed to read submodule .git file");
        return -1;
      }
      if (bytes_read < 9) {
        if (!QUIET)
          fprintf(stderr, CMD_NAME ": invalid submodule .git file format\n");
        return -1;
      }
      buffer_start += 8;  // "gitdir: "
      return 0;
    } else {
      if (!QUIET) fprintf(stderr, CMD_NAME ": invalid .git file type\n");
      return -1;
    }

  }
  return -1;
}

ssize_t read_first_line_of_file(const char* file, char* out, size_t n) {
  FILE* fp = fopen(file, "r");
  if (fp == NULL) return -1;
  char* r = fgets(out, n, fp);
  fclose(fp);
  if (r == NULL) return -1;
  int bytes_read = strlen(r);
  if (bytes_read == 0) return 0;
  if (out[bytes_read - 1] == '\n') {
    out[bytes_read - 1] = 0;
    bytes_read--;
  }
  return bytes_read;
}

ssize_t read_first_line_of_file_into_buffer(const char* file) {
  int bytes_read = read_first_line_of_file(file, buffer, sizeof(buffer));
  if (bytes_read <= 0) return bytes_read;
  buffer_start = buffer;
  buffer_end = buffer + bytes_read;
  *buffer_end = 0;
  return bytes_read;
}

char* get_git_branch() {
  char* p = buffer_start;
  // File contains a symbolic reference
  if (strncmp(p, "ref: ", 5) == 0) {
    p += 5;
    int moved = 0;
    // Point p to first slash; fail if we didn't find a first slash
    while (*p++ != '/') {
      if (!*p) return NULL;
      moved = 1;
    };
    if (moved == 0) return NULL;
    // Point p to second slash; fail if we didn't find a second slash
    moved = 0;
    while (*p++ != '/') {
      if (!*p) return NULL;
      moved = 1;
    };
    if (moved == 0) return NULL;
    // p now points to branch/tag/remote name
    buffer_start = p;
    return p;
  }
  // File contains a raw hash value
  if(buffer_end - buffer_start > hash_truncate_length)
    buffer_end = buffer_start + hash_truncate_length;
  *buffer_end = 0;
  return buffer_start;
}
