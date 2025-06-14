#include <lib.h>

void usage(void) {
    printf("usage: exit\n");
    exit();
}

int main(int argc, char **argv) {
    if (argc != 1) {
        usage();
    }

    exit();
    return 0;
}