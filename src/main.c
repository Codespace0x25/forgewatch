#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <tchar.h>
#define access _access
#define F_OK 0
#else
#include <errno.h>
#include <ftw.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#define MAX_WATCHES 20480
#define MAX_WATCH_DIRS 32
#define DEBOUNCE_MS 1000
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#ifdef NO_FANSCY_ERROR
#define error_printf(fmt, ...) fprintf(stderr, "[ ERROR ] " fmt "\n", ##__VA_ARGS__)
#else
#define error_printf(fmt, ...) fprintf(stderr, "\033[1;41;97m[ ERROR ]\033[0m " fmt "\n", ##__VA_ARGS__)
#endif

#ifdef DEBUG
#define debug_printf(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...) (void)0
#endif

char *watch_dirs[MAX_WATCH_DIRS];
int watch_dir_count = 0;
char *build_cmd = NULL;
char *watch_exts = NULL;

#ifndef _WIN32
pid_t current_pid = -1;
int inotify_fd = -1;
int watch_fds[MAX_WATCHES];
int watch_fd_count = 0;
struct timeval last_build_time = {0};
#else
PROCESS_INFORMATION current_proc = {0};
DWORD last_build_time = 0;
#endif

char *resolve_path(const char *path) {
#ifdef _WIN32
  char *resolved = (char *)malloc(MAX_PATH);
  if (_fullpath(resolved, path, MAX_PATH)) return resolved;
  free(resolved);
  return NULL;
#else
  return realpath(path, NULL);
#endif
}

void kill_current() {
#ifndef _WIN32
  if (current_pid > 0) {
    kill(current_pid, SIGTERM);
    waitpid(current_pid, NULL, 0);
    current_pid = -1;
  }
#else
  if (current_proc.hProcess != NULL) {
    TerminateProcess(current_proc.hProcess, 0);
    CloseHandle(current_proc.hProcess);
    CloseHandle(current_proc.hThread);
    current_proc.hProcess = NULL;
  }
#endif
}

void run_build() {
#ifndef _WIN32
  struct timeval now;
  gettimeofday(&now, NULL);
  long elapsed = (now.tv_sec - last_build_time.tv_sec) * 1000 +
                 (now.tv_usec - last_build_time.tv_usec) / 1000;
  if (elapsed < DEBOUNCE_MS) return;
  last_build_time = now;

  kill_current();
  current_pid = fork();
  if (current_pid == 0) {
    execl("/bin/sh", "sh", "-c", build_cmd, NULL);
    error_printf("Exec failed");
    exit(1);
  }
#else
  DWORD now = GetTickCount();
  if (now - last_build_time < DEBOUNCE_MS) return;
  last_build_time = now;

  kill_current();
  STARTUPINFOA si = { .cb = sizeof(STARTUPINFOA) };
  PROCESS_INFORMATION pi;
  if (!CreateProcessA(NULL, build_cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    error_printf("CreateProcess failed: %lu", GetLastError());
    return;
  }
  current_proc = pi;
#endif
}

bool is_temporary_file(const char *filename) {
  if (!filename || filename[0] == '\0') return true;
  if (filename[0] == '.' || strncmp(filename, ".#", 2) == 0) return true;
  const char *suffixes[] = {".swp", ".swo", ".tmp", "~", NULL};
  for (int i = 0; suffixes[i]; ++i) {
    size_t len = strlen(filename);
    size_t suffix_len = strlen(suffixes[i]);
    if (len >= suffix_len && strcmp(filename + len - suffix_len, suffixes[i]) == 0)
      return true;
  }
  return false;
}

bool has_valid_extension(const char *filename) {
  if (is_temporary_file(filename)) return false;
  if (!watch_exts) return true;

  const char *ext = strrchr(filename, '.');
  if (!ext) return false;

  char *copy = strdup(watch_exts);
  char *token = strtok(copy, " ");
  bool found = false;
  while (token) {
    if (strcmp(token, ext) == 0) {
      found = true;
      break;
    }
    token = strtok(NULL, " ");
  }
  free(copy);
  return found;
}

void load_config() {
  FILE *fp = fopen(".forgewatchrc", "r");
  if (!fp) return;

  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = 0;
    if (strncmp(line, "ForgWatch_path=", 15) == 0) {
      char *val = line + 15;
      char *token = strtok(val, " ");
      while (token && watch_dir_count < MAX_WATCH_DIRS) {
        char *resolved = resolve_path(token);
        if (resolved) watch_dirs[watch_dir_count++] = resolved;
        token = strtok(NULL, " ");
      }
    } else if (strncmp(line, "ForgWatch_build=", 16) == 0) {
      free(build_cmd);
      build_cmd = strdup(line + 16);
    } else if (strncmp(line, "ForgWatch_Extension=", 20) == 0) {
      free(watch_exts);
      watch_exts = strdup(line + 20);
    }
  }

  fclose(fp);
}

void create_config_interactive() {
  char path[256], cmd[256], filetype[256];
  printf("Enter directory(s) to watch: ");
  fgets(path, sizeof(path), stdin);
  path[strcspn(path, "\n")] = 0;

  printf("Enter build/run command: ");
  fgets(cmd, sizeof(cmd), stdin);
  cmd[strcspn(cmd, "\n")] = 0;

  printf("Enter file extensions to watch (e.g. .c .h .txt): ");
  fgets(filetype, sizeof(filetype), stdin);
  filetype[strcspn(filetype, "\n")] = 0;

  FILE *fp = fopen(".forgewatchrc", "w");
  if (!fp) {
    error_printf("Failed to create .forgewatchrc");
    exit(1);
  }

  fprintf(fp, "ForgWatch_path=%s\n", path);
  fprintf(fp, "ForgWatch_build=%s\n", cmd);
  fprintf(fp, "ForgWatch_Extension=%s\n", filetype);
  fclose(fp);
  printf(".forgewatchrc created.\n");
  exit(0);
}

#ifdef _WIN32
DWORD WINAPI windows_watch_thread(LPVOID lpParam) {
  const char *watch_dir = (const char *)lpParam;
  HANDLE hDir = CreateFileA(
    watch_dir,
    FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,
    NULL);

  if (hDir == INVALID_HANDLE_VALUE) {
    error_printf("Failed to watch %s", watch_dir);
    return 1;
  }

  char buffer[4096];
  DWORD bytesReturned;
  while (1) {
    if (ReadDirectoryChangesW(
          hDir, buffer, sizeof(buffer), TRUE,
          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
          &bytesReturned, NULL, NULL)) {
      run_build();
    }
  }

  CloseHandle(hDir);
  return 0;
}
#else
int add_watch_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  if (typeflag == FTW_D) {
    int wd = inotify_add_watch(inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd >= 0 && watch_fd_count < MAX_WATCHES)
      watch_fds[watch_fd_count++] = wd;
  }
  return 0;
}
void watch_all_subdirs(const char *path) {
  nftw(path, add_watch_cb, 32, FTW_PHYS);
}
#endif

void cleanup(int sig) {
  kill_current();
#ifndef _WIN32
  for (int i = 0; i < watch_fd_count; ++i)
    inotify_rm_watch(inotify_fd, watch_fds[i]);
  close(inotify_fd);
#endif
  for (int i = 0; i < watch_dir_count; ++i)
    free(watch_dirs[i]);
  free(build_cmd);
  free(watch_exts);
  printf("\nExited cleanly.\n");
  exit(0);
}

int main(int argc, char **argv) {
#ifndef _WIN32
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);
#endif

  if (argc >= 2 && strcmp(argv[1], "init") == 0)
    create_config_interactive();

  if (access(".forgewatchrc", F_OK) == 0)
    load_config();
  else if (argc >= 3) {
    char *token = strtok(argv[1], " ");
    while (token && watch_dir_count < MAX_WATCH_DIRS) {
      char *resolved = resolve_path(token);
      if (resolved) watch_dirs[watch_dir_count++] = resolved;
      token = strtok(NULL, " ");
    }
    build_cmd = strdup(argv[2]);
  }

  if (watch_dir_count == 0 || !build_cmd) {
    fprintf(stderr, "Usage:\n  %s \"<dir1> <dir2>\" <build_cmd>\n  %s init\n", argv[0], argv[0]);
    return 1;
  }

  printf("Watching directories:\n");
  for (int i = 0; i < watch_dir_count; ++i)
    printf("  - %s\n", watch_dirs[i]);
  printf("Build command: %s\n", build_cmd);

#ifndef _WIN32
  inotify_fd = inotify_init();
  if (inotify_fd < 0) {
    error_printf("inotify_init failed");
    return 1;
  }
  for (int i = 0; i < watch_dir_count; ++i)
    watch_all_subdirs(watch_dirs[i]);

  run_build();
  char buffer[BUF_LEN];
  while (1) {
    int length = read(inotify_fd, buffer, BUF_LEN);
    if (length < 0) {
      error_printf("read failed");
      break;
    }

    for (int i = 0; i < length;) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->len && !(event->mask & IN_ISDIR) && has_valid_extension(event->name))
        run_build();
      i += EVENT_SIZE + event->len;
    }
  }
#else
  for (int i = 0; i < watch_dir_count; ++i)
    CreateThread(NULL, 0, windows_watch_thread, watch_dirs[i], 0, NULL);
  run_build();
  while (1) Sleep(1000);
#endif

  cleanup(0);
  return 0;
}
