#define _GNU_SOURCE
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

char *watch_dir = NULL;
char *script_path = "./build.sh";
pid_t current_pid = -1;

void run_script() {
  if (current_pid > 0) {
    kill(current_pid, SIGTERM);
    waitpid(current_pid, NULL, 0);
  }

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/bash", "bash", script_path, (char *)NULL);
    perror("execl failed");
    exit(1);
  } else if (pid > 0) {
    current_pid = pid;
  } else {
    perror("fork failed");
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <watch-dir> [build-script-path]\n", argv[0]);
    return 1;
  }

  watch_dir = realpath(argv[1], NULL);
  if (!watch_dir) {
    perror("realpath");
    return 1;
  }

  if (argc >= 3)
    script_path = argv[2];

  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init");
    return 1;
  }

  int wd = inotify_add_watch(fd, watch_dir, IN_MODIFY | IN_CREATE | IN_DELETE);
  if (wd == -1) {
    perror("inotify_add_watch");
    return 1;
  }

  printf("Watching %s...\n", watch_dir);
  run_script();

  char buffer[BUF_LEN];
  while (1) {
    int length = read(fd, buffer, BUF_LEN);
    if (length < 0) {
      perror("read");
      break;
    }

    for (int i = 0; i < length;) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];

      if (event->mask & (IN_MODIFY | IN_CREATE | IN_DELETE)) {
        printf("[watcher] Change detected: %s\n", event->name);
        run_script();
        break;
      }

      i += EVENT_SIZE + event->len;
    }
  }

  inotify_rm_watch(fd, wd);
  close(fd);
  free(watch_dir);
  return 0;
}
