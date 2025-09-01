#include <lib.h>

int main(int argc, char **argv) {
	int i, fd, r;

	if (argc < 2) {
		fprintf(2, "Usage: touch <file1> [file2 ...]\n");
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_WRONLY | O_CREAT | O_EXCL);
		if (fd >= 0) {
			r = close(fd);
			if (r < 0) {
				fprintf(2, "touch: failed to close %s (error %d)\n", argv[i], r);
				exit(1);
			}
		} else if (fd == -E_NOT_FOUND) {
            fprintf(2, "touch: cannot touch '%s': No such file or directory\n", argv[i]);
			exit(1);
		}
	}

	return 0;
}