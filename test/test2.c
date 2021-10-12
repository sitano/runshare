#define _GNU_SOURCE

#include <sched.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

void *routine(void *arg) {
  printf("thread start %d\n", getpid());
  sleep(10);
  printf("thread end %d\n", getpid());
  return NULL;
}

int main() {
  int err = unshare(CLONE_NEWPID);
  if (err) {
    printf("err = %s", strerror(errno));
    return 1;
  }

  pthread_t tid;
  pthread_create(&tid, NULL, routine, NULL);

  int pid = fork();
  if (pid) {
    int status;
    printf("parent see %d\n", pid);
    pid = waitpid(pid, &status, 0);
    printf("got status %d\n", status);
    printf("result %d\n", pid);
  } else {
    printf("child see %d\n", getpid());
    sleep(3);
  }

  return 0;
}
