#include <lib.h>
#include <args.h>

void usage(void) {
	debugf("usage: pwd\n");
	exit();
}

int main(int argc, char **argv) {

    if (argc != 1) {
        printf("pwd: expected 0 arguments; got %d\n", argc - 1);
        // 以状态码 2 返回，表示出错
        return 2;
    } else {
        char path[128] = {0};
        getcwd(path);
        printf("%s\n", path);
    }

    return 0;
}