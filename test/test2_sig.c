#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
void parent_trap(int sig) {fprintf(stderr, "They got back together!\n");}
void child_trap(int sig) {fprintf(stderr, "Caught signal in CHILD.\n");}
int main(int argc, char **argv) {
    if (!fork()) {
        // signal(SIGINT, &child_trap);
        sleep(1000);
        exit(0);
    }
    signal(SIGINT, &parent_trap);
    sleep(1000);
    return 0;
}
