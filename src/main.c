#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#ifdef DEBUG
#define debug_printf(fmt, ...)                                                 \
  fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...) (void)0
#endif

pid_t current_pid = -1;
char *watch_dir = NULL;
char *build_cmd = NULL;

void kill_current() {
  if (current_pid > 0) {
    debug_printf("Killing process %d", current_pid);
    kill(current_pid, SIGTERM);
    waitpid(current_pid, NULL, 0);
    debug_printf("Process %d terminated", current_pid);
  }
}

void run_build() {
  kill_current();

  current_pid = fork();
  if (current_pid == 0) {
    execl("/bin/sh", "sh", "-c", build_cmd, NULL);
    perror("Failed to exec build command");
    exit(1);
  } else if (current_pid < 0) {
    perror("Fork failed");
  } else {
    debug_printf("Started build process with PID %d", current_pid);
  }
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
      while (*val == ' ')
        val++;
      char *resolved = realpath(val, NULL);
      if (resolved) {
        watch_dir = resolved;
        debug_printf("Loaded watch_dir: %s", watch_dir);
      } else {
        fprintf(stderr, "Failed to resolve watch_dir path '%s'\n", val);
      }
    } else if (strncmp(line, "ForgWatch_build=", 16) == 0) {
      char *val = line + 16;
      while (*val == ' ')
        val++;
      build_cmd = strdup(val);
      debug_printf("Loaded build_cmd: %s", build_cmd);
    }
  }
  fclose(fp);
}

void create_config_interactive() {
  char path[256], cmd[256];
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

  FILE *fp = fopen(".forgewatchrc", "w");
  if (!fp) {
    perror("Failed to create .forgewatchrc");
    exit(1);
  }

  fprintf(fp, "ForgWatch_path=%s\n", path);
  fprintf(fp, "ForgWatch_build=%s\n", cmd);
  fclose(fp);

  printf(".forgewatchrc created successfully.\n");
  exit(0);
}

int main(int argc, char **argv) {
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

  run_build();

  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init failed");
    return 1;
  }

  int wd = inotify_add_watch(fd, watch_dir, IN_MODIFY | IN_CREATE | IN_DELETE);
  if (wd < 0) {
    perror("inotify_add_watch failed");
    close(fd);
    return 1;
  }

  char buffer[BUF_LEN];
  while (1) {
    int length = read(fd, buffer, BUF_LEN);
    if (length < 0) {
      perror("read failed");
      break;
    }

    for (int i = 0; i < length;) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->len) {
        debug_printf("Detected file change: %s", event->name);
        run_build();
      }
      i += EVENT_SIZE + event->len;
    }
  }

  inotify_rm_watch(fd, wd);
  close(fd);
  free(watch_dir);
  free(build_cmd);
  return 0;
}
