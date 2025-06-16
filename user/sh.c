#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
/*
* 根据解析到的token性质返回信息
* - 0：解析到字符串末尾
* - <：输入重定向
* - >：输出重定向
* - |：管道
* - w：词，可以是指令、文件名、其他所有情况
*/
int _gettoken(char *str, char **token_pointer, char **next_token_pointer) {
	*token_pointer = 0;
	*next_token_pointer = 0;
	// 如果是null指针，返回0
	if (str == 0) {
		return 0;
	}

	// 跳过空白符
	while (strchr(WHITESPACE, *str)) {
		*str++ = 0;
	}
	// 如果来到字符串末尾
	if (*str == 0) {
		return 0;
	}

	// 解析到关键字符"<|>&;()"
	if (strchr(SYMBOLS, *str)) {
		int t = *str;
		*token_pointer = str;
		*str++ = 0;
		*next_token_pointer = str;
		return t;
	}

	//解析到有意义字符
	*token_pointer = str;
	while (*str && !strchr(WHITESPACE SYMBOLS, *str)) {
		str++;
	}
	*next_token_pointer = str;
	return 'w';
}

int gettoken(char *s, char **token_pointer) {
	// 设为静态变量，保证操作的连续
	static int c, nc;
	static char *token, *next_token;

	if (s) {
		nc = _gettoken(s, &token, &next_token);
		return 0;
	}
	// 对于获取上一个token的情况，传入s为0即可
	c = nc;
	*token_pointer = token;
	nc = _gettoken(next_token, &token, &next_token);
	return c;
}

// 最大参数数量
#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			return argc;

		// 一般情况，解析到词
		case 'w':
			// 如果参数过多
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit();
			}
			// 填入参数到argv
			argv[argc++] = t;
			break;

		// 遇到输入重定向的情况
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit();
			}
			// 打开对应的文件，如果打开失败，则推出
			if ((fd = open(t, O_RDONLY)) < 0)
			{
				debugf("open %s: %d\n", t, fd);
				exit();
			}
			// 进行输入重定向
			dup(fd, 0);
			close(fd);
			break;

		// 遇到输出重定向的情况
		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// 打开对应的文件，如果打开失败，则退出
			if ((fd = open(t, O_WRONLY | O_CREAT | O_TRUNC)) < 0)
			{
				debugf("open %s: %d\n", t, fd);
				exit();
			}
			// 进行输出重定向
			dup(fd, 1);
			close(fd);
			break;

		// 遇到管道的情况
		case '|':;
			// 创建一个管道
			int p[2];
			r = pipe(p);
			// 管道创建失败则退出
			if (r != 0) {
				debugf("pipe: %d\n", r);
				exit();
			}
			// 创建一个进程执行管道操作
			r = fork();
			// fork失败则退出
			if (r < 0) {
				debugf("fork: %d\n", r);
				exit();
			}
			*rightpipe = r;
			// 如果是子进程，也就是管道的右边（接收端）
			if (r == 0) {
				// 对输入重定向
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				// 执行管道的右边
				return parsecmd(argv, rightpipe);
			} else {
				// 如果是父进程，即管道的左边（发送端）
				// 对输出重定向
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			break;
		}
	}

	return argc;
}

// 运行指令
void runcmd(char *s) {
	gettoken(s, 0);
	// argc为参数列表，形式为字符串
	char *argv[MAXARGS];
	int rightpipe = 0;
	// 解析参数到argv
	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	if (strcmp("cd", argv[0]) == 0) {
		useCD(argc, argv[1]);
		return;
	}
	// 创建一个进程执行命令
	int child = spawn(argv[0], argv);
	// 关闭所有打开的文件
	close_all();
	// 如果执行命令的是子进程
	if (child >= 0) {
		wait(child);
	} else { // 如果是父进程
		debugf("spawn %s: %d\n", argv[0], child);
	}
	// 如果有管道，则等待执行完毕
	if (rightpipe) {
		wait(rightpipe);
	}
	// 退出
	exit();
}

// 从标准控制台读入一行命令，保存到buf中
// n实际上取了buf的大小
void readline(char *buf, u_int n) {
	int r;
	for (int i = 0; i < n; i++) {
		// 挨个字节读取
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit();
		}
		// 如果是退格
		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				printf("\b");
			}
		}
		// 如果是换行符，代表命令的结束，停止解析
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			return;
		}
	}
	// 遇到命令过长的情况，不解析当行
	debugf("line too long\n");
	// 吃掉剩下的字符，避免缓冲区溢出
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

int parseCD(char *buf) {
	char *p = buf;
	if (strlen(buf) < 2) {
		return 0;
	}
	if (*p == 'c' && *(p + 1) == 'd') {
		return 1;
	} else {
		for (int i = 0;i<strlen(buf) - 2;i++) {
			if (*(p+i) == ';' || *(p+i) == '&') {
				i++;
				while (*(p+i) == ' ') {
					i++;
				}
				if (i <= strlen(buf) - 2 && *(p + i) == 'c' && *(p+i+1) == 'd') {
					return 1;
				}
			}
		}
	}
	return 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit();
}

int main(int argc, char **argv) {
	int r;
	// 是否为交互式终端
	int interactive = iscons(0);
	// 是否要输出输入的命令
	int echocmds = 0;
	char curPath[256] = {0};
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2025                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	// 参数解析部分
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	// 如果需要执行脚本，则关闭标准输入，改为文件作为输入
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}
	// 在循环中不断读入命令行并进行处理
	for (;;) {
		// 作为交互式终端，先打印一个"$"
		if (interactive) {
			if ((r = getcwd(curPath)) < 0) {
				printf("G");
				exit();
			}
			printf("\n$ ");
		}
		// 读入一份命令到buf
		readline(buf, sizeof buf);

		// 忽略以"#"开头的注释
		if (buf[0] == '#') {
			continue;
		}
		// 在echocmds模式下输出读入的命令
		if (echocmds) {
			printf("# %s\n", buf);
		}
		if (parseCD(buf) == 0) {
			if ((r = fork()) < 0) {
				user_panic("fork: %d", r);
			}
		} else {
			runcmd(buf);
			continue;
		}
		// 对于父子进程
		// 子进程执行命令
		if (r == 0) {
			runcmd(buf);
			exit();
		} else { // 父进程等待子进程执行完毕
			wait(r);
		}
	}
	return 0;
}

void useCD(int argc, char* argv) {
	int r;
	char cur[1024] = {0};
	struct Stat st = {0};

	// 检查参数数量是否超过1个 (cd 本身是第1个，所以 argc > 2意味着有多个参数)
	if (argc > 2) {
		printf("Too many args for cd command\n");
		return; // 返回，不继续执行
	}

	if (argc == 1) {
		cur[0] = '/';
	} else if (strcmp(argv, ".") == 0) {
		return;
	} else if (strcmp(argv, "..") == 0) {
		syscall_get_rpath(cur);
		char *last_slash = strrchr(cur, '/');
		if (last_slash != cur) {
			*last_slash = '\0';
		} else {
			cur[0] = '/';
			cur[1] = '\0';
		}
	} else if (argv[0] != '/') {
		char *p = argv;
		if (argv[0] == '.') {
			p += 2;
		}
		syscall_get_rpath(cur);
		int len1 = strlen(cur);
		int len2 = strlen(p);
		if (len1 == 1) {
			strcpy(cur + 1, p);
		} else {
			cur[len1] = '/';
			strcpy(cur + len1 + 1, p);
			cur[len1 + 1 + len2] = '\0';
		}
	} else {
		strcpy(cur, argv);
	}

	if ((r = stat(cur, &st)) < 0) {
		printf("cd: The directory %s does not exist\n", cur);
		return;
	}
	if (!st.st_isdir) {
		printf("cd: %s is not a directory\n", cur);
		return;
	}
	if ((r = chdir(cur)) < 0) {
		printf("6");
		exit();
	}
	return;
}