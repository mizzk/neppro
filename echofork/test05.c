#include <sys/wait.h>

#include "mynet.h"

int main() {
    int i;
    for (i = 0; i < 3; i++) {
        printf("i = %d\n", i);
        if (fork() == 0) {
            printf("Hello\n");
        } else {
            printf("Goodbye\n");
        }
    }
}