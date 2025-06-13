#define _GNU_SOURCE
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define MAX_WATCHES 1024
#define DEBOUNCE_MS 1000

#ifdef NO_FANSCY_ERROR
#define error_printf(fmt, ...)                                                 \
  fprintf(stderr, "[ ERROR ] " fmt "\n", ##__VA_ARGS__)
#else
#define error_printf(fmt, ...)                                                 \
  fprintf(stderr, "\033[1;41;97m[ ERROR ]\033[0m " fmt "\n", ##__VA_ARGS__)
#endif

#ifdef DEBUG
#define debug_printf(fmt, ...)                                                 \
  fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...) (void)0
#endif

pid_t current_pid = -1;
char *watch_dir = NULL;
char *build_cmd = NULL;
char *watch_exts = NULL;

int inotify_fd = -1;
int watch_fds[MAX_WATCHES];
int watch_fd_count = 0;
struct timeval last_build_time = {0};

void kill_current() {
  if (current_pid > 0) {
    debug_printf("Killing process %d", current_pid);
    kill(current_pid, SIGTERM);
    waitpid(current_pid, NULL, 0);
    debug_printf("Process %d terminated", current_pid);
    current_pid = -1;
  }
}

void run_build() {
  struct timeval now;
  gettimeofday(&now, NULL);

  long elapsed = (now.tv_sec - last_build_time.tv_sec) * 1000 +
                 (now.tv_usec - last_build_time.tv_usec) / 1000;
  if (elapsed < DEBOUNCE_MS) {
    debug_printf("Debounce: skipping build (elapsed %ldms)", elapsed);
    return;
  }

  last_build_time = now;

  kill_current();
  current_pid = fork();
  if (current_pid == 0) {
    execl("/bin/sh", "sh", "-c", build_cmd, NULL);
    error_printf("Failed to exec build command");
    exit(1);
  } else if (current_pid < 0) {
    error_printf("Fork failed");
  } else {
    debug_printf("Started build process with PID %d", current_pid);
  }
}

int add_watch_cb(const char *path, const struct stat *sb, int typeflag,
                 struct FTW *ftwbuf) {
  if (typeflag == FTW_D) {
    int wd =
        inotify_add_watch(inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd < 0) {
      error_printf("inotify_add_watch failed");
    } else {
      if (watch_fd_count < MAX_WATCHES) {
        watch_fds[watch_fd_count++] = wd;
        debug_printf("Watching: %s", path);
      } else {
        fprintf(stderr, "Too many watches\n");
      }
    }
  }
  return 0;
}

void watch_all_subdirs(const char *path) {
  if (nftw(path, add_watch_cb, 32, FTW_PHYS) < 0) {
    error_printf("nftw failed");
  }
}

bool is_temporary_file(const char *filename) {
  if (!filename || filename[0] == '\0')
    return true;

  // Ignore files starting with '.' or '.#'
  if (filename[0] == '.')
    return true;
  if (strncmp(filename, ".#", 2) == 0)
    return true;

  // Ignore known temp suffixes
  const char *suffixes[] = {".swp", ".swo", ".tmp", "~", NULL};
  for (int i = 0; suffixes[i]; ++i) {
    size_t len = strlen(filename);
    size_t suffix_len = strlen(suffixes[i]);
    if (len >= suffix_len &&
        strcmp(filename + len - suffix_len, suffixes[i]) == 0) {
      return true;
    }
  }

  return false;
}

bool has_valid_extension(const char *filename) {
  if (is_temporary_file(filename)) {
    debug_printf("Ignored temporary/cache file: %s", filename);
    return false;
  }

  if (!watch_exts)
    return true;

  const char *ext = strrchr(filename, '.');
  if (!ext)
    return false;

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
  if (!fp) {
    debug_printf("Failed to open .forgewatchrc");
    return;
  }

  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = 0;

    if (strncmp(line, "ForgWatch_path=", 15) == 0) {
      char *val = line + 15;
      char *resolved = realpath(val, NULL);
      if (resolved) {
        if (watch_dir)
          free(watch_dir);
        watch_dir = resolved;
        debug_printf("Loaded watch_dir: %s", watch_dir);
      } else {
        fprintf(stderr, "Failed to resolve watch_dir path '%s'\n", val);
      }
    } else if (strncmp(line, "ForgWatch_build=", 16) == 0) {
      char *val = line + 16;
      if (build_cmd)
        free(build_cmd);
      build_cmd = strdup(val);
      debug_printf("Loaded build_cmd: %s", build_cmd);
    } else if (strncmp(line, "ForgWatch_Extension=", 20) == 0) {
      char *val = line + 20;
      if (watch_exts)
        free(watch_exts);
      watch_exts = strdup(val);
      debug_printf("Loaded watch_exts: %s", watch_exts);
    }
  }

  fclose(fp);
}

void create_config_interactive() {
  char path[256], cmd[256], filetype[256];
  printf("Enter directory to watch: ");
  if (!fgets(path, sizeof(path), stdin)) {
    fprintf(stderr, "Failed to read directory path\n");
    exit(1);
  }
  path[strcspn(path, "\n")] = 0;

  printf("Enter build/run command: ");
  if (!fgets(cmd, sizeof(cmd), stdin)) {
    fprintf(stderr, "Failed to read build command\n");
    exit(1);
  }
  cmd[strcspn(cmd, "\n")] = 0;

  printf("Enter every file type you would like to watch. this is in the format "
         "of .<extension>\n space delimited: ");
  if (!fgets(filetype, sizeof(filetype), stdin)) {
    fprintf(stderr, "unable to read the entry\n");
    exit(1);
  }
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

  printf(".forgewatchrc created successfully.\n");
  exit(0);
}

void cleanup(int sig) {
  kill_current();
  for (int i = 0; i < watch_fd_count; ++i) {
    inotify_rm_watch(inotify_fd, watch_fds[i]);
  }
  close(inotify_fd);
  free(watch_dir);
  free(build_cmd);
  free(watch_exts);
  printf("\nExited cleanly.\n");
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);

  if (getenv("FORGEWATCH_IS") && strcmp(getenv("FORGEWATCH_IS"), "1") == 0) {
    error_printf("Refused to start: already running inside ForgeWatch.");
    exit(1);
  }


  if (argc >= 2 && strcmp(argv[1], "init") == 0) {
    create_config_interactive();
  }

  if (access(".forgewatchrc", F_OK) == 0) {
    load_config();
  }

  if (!watch_dir && argc >= 3) {
    char *resolved_path = realpath(argv[1], NULL);
    if (!resolved_path) {
      fprintf(stderr, "Failed to resolve watch directory path '%s'\n", argv[1]);
      return 1;
    }
    watch_dir = resolved_path;
    build_cmd = strdup(argv[2]);
  }

  if (!watch_dir || !build_cmd) {
    fprintf(stderr,
            "Usage:\n  %s <watch_dir> <build_cmd>\n  %s init  # to create "
            ".forgewatchrc\n",
            argv[0], argv[0]);
    return 1;
  }

  printf("Watching directory: %s\n", watch_dir);
  printf("Build command: %s\n", build_cmd);

  inotify_fd = inotify_init();
  if (inotify_fd < 0) {
    error_printf("inotify_init failed");
    return 1;
  }

  watch_all_subdirs(watch_dir);
  setenv("FORGEWATCH_IS", "1", 1);
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
      if (event->len) {
        debug_printf("Detected change: %s", event->name);

        if (event->mask & IN_CREATE && (event->mask & IN_ISDIR)) {
          char new_path[PATH_MAX];
          snprintf(new_path, sizeof(new_path), "%s/%s", watch_dir, event->name);
          watch_all_subdirs(new_path);
        }

        if (!(event->mask & IN_ISDIR) && has_valid_extension(event->name)) {
          run_build();
        }
      }
      i += EVENT_SIZE + event->len;
    }
  }

  cleanup(0);
  return 0;
}
