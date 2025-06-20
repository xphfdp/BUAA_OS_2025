#include <args.h>
#include <lib.h>
#include <history.h>
#include <variable.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"
#define DEL 0x7f // 用于删除
#define PROMPT "mos> "
#define ESC 0x1b
#define BACKSPACE 0x08
#define _MOS_LOGO_ "\
\x1b[31m             ___           ___           ___     \n\x1b[0m\
\x1b[31m            /\\__\\         /\\  \\         /\\  \\    \n\x1b[0m\
\x1b[31m           /::|  |       /::\\  \\       /::\\  \\   \n\x1b[0m\
\x1b[31m          /:|:|  |      /:/\\:\\  \\     /:/\\ \\  \\  \n\x1b[0m\
\x1b[31m         /:/|:|__|__   /:/  \\:\\  \\   _\\:\\~\\ \\  \\ \n\x1b[0m\
\x1b[31m        /:/ |::::\\__\\ /:/__/ \\:\\__\\ /\\ \\:\\ \\ \\__\\\n\x1b[0m\
\x1b[31m        \\/__/~~/:/  / \\:\\  \\ /:/  / \\:\\ \\:\\ \\/__/\n\x1b[0m\
\x1b[31m              /:/  /   \\:\\  /:/  /   \\:\\ \\:\\__\\  \n\x1b[0m\
\x1b[31m             /:/  /     \\:\\/:/  /     \\:\\/:/  /  \n\x1b[0m\
\x1b[31m            /:/  /       \\::/  /       \\::/  /   \n\x1b[0m\
\x1b[31m            \\/__/         \\/__/         \\/__/    \n\x1b[0m\
\x1b[34m      ___           ___           ___           ___       ___ \n\x1b[0m\
\x1b[34m     /\\  \\         /\\__\\         /\\  \\         /\\__\\     /\\__\\\n\x1b[0m\
\x1b[34m    /::\\  \\       /:/  /        /::\\  \\       /:/  /    /:/  /\n\x1b[0m\
\x1b[34m   /:/\\ \\  \\     /:/__/        /:/\\:\\  \\     /:/  /    /:/  / \n\x1b[0m\
\x1b[34m  _\\:\\~\\ \\  \\   /::\\  \\ ___   /::\\~\\:\\  \\   /:/  /    /:/  /  \n\x1b[0m\
\x1b[34m /\\ \\:\\ \\ \\__\\ /:/\\:\\  /\\__\\ /:/\\:\\ \\:\\__\\ /:/__/    /:/__/   \n\x1b[0m\
\x1b[34m \\:\\ \\:\\ \\/__/ \\/__\\:\\/:/  / \\:\\~\\:\\ \\/__/ \\:\\  \\    \\:\\  \\   \n\x1b[0m\
\x1b[34m  \\:\\ \\:\\__\\        \\::/  /   \\:\\ \\:\\__\\    \\:\\  \\    \\:\\  \\  \n\x1b[0m\
\x1b[34m   \\:\\/:/  /        /:/  /     \\:\\ \\/__/     \\:\\  \\    \\:\\  \\ \n\x1b[0m\
\x1b[34m    \\::/  /        /:/  /       \\:\\__\\        \\:\\__\\    \\:\\__\\\n\x1b[0m\
\x1b[34m     \\/__/         \\/__/         \\/__/         \\/__/     \\/__/\x1b[0m"

static struct History history;
static struct VariableSet variable_set;
static char rPath[MAXPATHLEN];
static int interactive;
static int storedFd[2];

void runcmd(char *);
int _declare(int, char **);
int _unset(int, char **);
int _cd(int, char **);
int _pwd(int, char **);
int _history(int, char **);
int _exit(int, char **);

struct BuiltinCmd {
    const char *name;
    int (*func)(int, char **);
};

static struct BuiltinCmd builtin_cmds[] = {
    {"cd", _cd},
    {"pwd", _pwd},
    {"history", _history},
    {"declare", _declare},
    {"unset", _unset},
    {"exit", _exit},
    {0, 0}  // Sentinel
};

#define PRINTF(...)                 \
    do {                            \
        if (interactive) {          \
            printf(__VA_ARGS__);    \
        }                           \
    } while (0)

#define PUT_CHAR(c)                   \
    do {                              \
        if (!interactive) {           \
            break;                    \
        }                             \
        if ((c) < 32 || (c) >= 127) { \
            printf("?");              \
        } else {                      \
            printf("%c", (c));        \
        }                             \
    } while (0)

/*
* 根据解析到的token性质返回信息
* - 0：解析到字符串末尾
* - <：输入重定向
* - >：输出重定向
* - |：管道
* - w：词，可以是指令、文件名、其他所有情况
*/
int _gettoken(char *str, char **token_pointer, char **next_token_pointer) {
	static const struct {
        char c1;
        char c2;
        int ret_val;
    } double_char_tokens[] = {
        {'>', '>', 'a'}, {'&', '&', 'A'}, {'|', '|', 'O'}, {0, 0, 0}};

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

	for (int i = 0; double_char_tokens[i].c1 != 0; ++i) {
        if (*str == double_char_tokens[i].c1 &&
            *(str + 1) == double_char_tokens[i].c2) {
            *token_pointer = str;
            *str++ = 0;
            *str++ = 0;
            *next_token_pointer = str;
            return double_char_tokens[i].ret_val;
        }
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

int isDir(const char *path) {
    struct Stat st;
    int r;
    if ((r = stat(path, &st)) < 0) {
        // debugf("stat '%s': %d\n", path, r);
        return r;
    }

    return st.st_isdir;
}

int fork1(void) {
    int r;
    if ((r = fork()) < 0) {
        debugf("fork: %d\n", r);
        exit(1);
    }
	DEBUGF("fork1: %d\n", r);
    return r;
}

static void store_01(int p[2]) {
    int r;
    if ((r = pipe(p)) < 0) {
        user_panic("pipe: %d", r);
    }
    if ((r = dup(0, p[0])) < 0) {
        user_panic("dup store: %d", r);
    }
    if ((r = dup(1, p[1])) < 0) {
        user_panic("dup store: %d", r);
    }
}
static void restore_01(int p[2]) {
    int r;
    if ((r = dup(p[0], 0)) < 0) {
        user_panic("dup restore: %d", r);
    }
    if ((r = dup(p[1], 1)) < 0) {
        user_panic("dup restore: %d", r);
    }
}

char *process_backticks(char *cmd) {
    char *result = cmd;
    char *start = (char *)strchr(cmd, '`');
    if (!start) {
        return result;
    }

    char new_cmd[MAX_COMMAND_LENGTH] = {0};
    int new_cmd_len = 0;

    // copy the part before the first backtick
    strncpy(new_cmd, cmd, start - cmd);
    new_cmd_len = start - cmd;

    while (start) {
        char *end = (char *)strchr(start + 1, '`');
        if (!end) {
            fprintf(2, "Error: Unmatched backtick\n");
            *cmd = 0;
            return cmd;
        }
        *end = 0;

        int p[2];
        if(pipe(p) < 0) {
            fprintf(2, "process_backticks: Failed to create pipe\n");
            exit(1);
        }

        int child = fork1();
        if (child == 0) {
            close(p[0]);
            if (dup(p[1], 1) < 0) {
                fprintf(2, "process_backticks: Failed to duplicate pipe write end\n");
                close(p[1]);
                exit(1);
            }
            close(p[1]);
            runcmd(start + 1);
            exit(0);
        }

        close(p[1]);

        char output[MAX_COMMAND_LENGTH] = {0};
        int output_len = 0;

        char ch;
        while (output_len < MAX_COMMAND_LENGTH - 1 &&
                read(p[0], &ch, 1) == 1) {
            // remove newlines from the output
            if (ch == '\n') {
                ch = ' ';
            }
            output[output_len++] = ch;
        }
		close(p[0]);
		wait(child);

        if (output_len >= MAX_COMMAND_LENGTH - 1) {
            goto err;
        }
        output[output_len] = 0;

        // append the output to the new command
        for (int i = 0; i < output_len; i++) {
            if (new_cmd_len + 1 < MAX_COMMAND_LENGTH) {
                new_cmd[new_cmd_len++] = output[i];
            } else {
                goto err;
            }
        }

        start = (char *)strchr(end + 1, '`');
        char *remainder = end + 1;
        int len_to_copy = start ? (start - end - 1) : strlen(remainder);

        for (int i = 0; i < len_to_copy; i++) {
            if (new_cmd_len + 1 < MAX_COMMAND_LENGTH) {
                new_cmd[new_cmd_len++] = remainder[i];
            } else {
                goto err;
            }
        }

        restore_01(storedFd);
    }

    new_cmd[new_cmd_len] = 0;
    strcpy(cmd, new_cmd);
    return cmd;

err:
    fprintf(2, "Error: Command too long\n");
    *cmd = 0;  // clear the command
    restore_01(storedFd);
    return cmd;
}

int parsecmd(char **argv, int *rightpipe, int *isChild) {
	int argc = 0;
	char *t;
	int fd, r, c;
	while (1) {
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			return argc;

		// 一般情况，解析到词
		case 'w':
			// 如果参数过多
			if (argc >= MAXARGS) {
				fprintf(2, "too many arguments\n");
				exit(1);
			}
			// 填入参数到argv
			argv[argc++] = t;
			break;

		// 遇到输入重定向的情况
		case '<':
			if (gettoken(0, &t) != 'w') {
				fprintf(2, "syntax error: < not followed by word\n");
				exit(1);
			}
			r = isDir(t);
			if (r == 1) {
				fprintf(2, "bash: %s: Is a directory\n", t);
				return 0;
			}
			// 打开对应的文件，如果打开失败，则推出
			if ((fd = open(t, O_RDONLY)) < 0)
			{
				fprintf(2, "open %s: %d\n", t, fd);
				exit(1);
			}
			// 进行输入重定向
			dup(fd, 0);
			close(fd);
			break;

		// 遇到输出重定向的情况
		case '>':
		case 'a':
			int mode = c == '>' ? O_TRUNC : O_APPEND;
            if (gettoken(0, &t) != 'w') {
                fprintf(2, "syntax error: %s not followed by word\n", c == '>' ? ">" : ">>");
                exit(1);
                // return 0;
            }

			r = isDir(t);
			if (r == 1) {
				fprintf(2, "bash: %s: Is a directory\n",t);
				return 0;
			}

			// 打开对应的文件，如果打开失败，则退出
			if ((fd = open(t, O_WRONLY | O_CREAT | mode)) < 0)
			{
				fprintf(2, "open %s: %d\n", t, fd);
				exit(1);
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
				fprintf(2, "pipe: %d\n", r);
				exit(1);
			}
			// 创建一个进程执行管道操作
			r = fork1();
			*rightpipe = r;
			// 如果是子进程，也就是管道的右边（接收端）
			if (r == 0) {
				// 对输入重定向
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				// 执行管道的右边
				*isChild = 1;
				return parsecmd(argv, rightpipe, isChild);
			} else {
				// 如果是父进程，即管道的左边（发送端）
				// 对输出重定向
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			break;

		case ';':
			r = fork1();
			if (r) {
				wait(r);
				restore_01(storedFd);
				return parsecmd(argv, rightpipe, isChild);
			}
			*isChild = 1;
			return argc;
			
		case 'A':
		case 'O':
			// logical AND or OR
            int child = fork1();
            if (child == 0) {
                // child process
                *isChild = 1;
                return argc;
            }
			r = wait(child);
            DEBUGF("wait %d: %d\n", child, r);
            if ((c == 'A' && r == 0) || (c == 'O' && r != 0)) {
                return parsecmd(argv, rightpipe, isChild);
            }

            return 0;
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
	int isChild = 0;

	// 解析参数到argv
	int argc = parsecmd(argv, &rightpipe, &isChild);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;
	int r;
	// for (int i = 0; builtin_cmds[i].name; i++) {
    //     if (strcmp(argv[0], builtin_cmds[i].name) == 0) {
    //         r = builtin_cmds[i].func(argc, argv);
    //         goto out;
	// 	}
    // }
	if (strcmp("cd", argv[0]) == 0) {
		_cd(argc, argv);
		goto out;
	}
	if (strcmp("pwd", argv[0]) == 0) {
		_pwd(argc, argv);
		goto out;
	}
	if (strcmp("history", argv[0]) == 0) {
		_history(argc, argv);
		goto out;
	}
	if (strcmp("declare", argv[0]) == 0) {
		_declare(argc, argv);
		goto out;
	}
	if (strcmp("unset", argv[0]) == 0) {
		_unset(argc, argv);
		goto out;
	}
	if (strcmp("exit", argv[0]) == 0) {
		_exit(argc, argv);
		goto out;
	}
	// 创建一个进程执行命令
	int child = spawn(argv[0], argv);
	// 关闭所有打开的文件
	// close_all();
	// 如果执行命令的是子进程
	if (child >= 0) {
		DEBUGF("spawn %s: %d\n", argv[0], child);
		r = wait(child);
	} else { // 如果是父进程
		fprintf(2, "spawn %s: %d\n", argv[0], child);
	}
out:
	// 如果有管道，则等待执行完毕
	if (rightpipe) {
		close(1);
		r |= wait(rightpipe);
	}
	// 退出
	// exit();
	if (isChild) {
		exit(r);
	}
}

// // 从标准控制台读入一行命令，保存到buf中
// // n实际上取了buf的大小
// void readline(char *buf, u_int n) {
// 	int r;
// 	for (int i = 0; i < n; i++) {
// 		// 挨个字节读取
// 		if ((r = read(0, buf + i, 1)) != 1) {
// 			if (r < 0) {
// 				debugf("read error: %d\n", r);
// 			}
// 			exit();
// 		}
// 		// 如果是退格
// 		if (buf[i] == '\b' || buf[i] == 0x7f) {
// 			if (i > 0) {
// 				i -= 2;
// 			} else {
// 				i = -1;
// 			}
// 			if (buf[i] != '\b') {
// 				printf("\b");
// 			}
// 		}
// 		// 如果是换行符，代表命令的结束，停止解析
// 		if (buf[i] == '\r' || buf[i] == '\n') {
// 			buf[i] = 0;
// 			return;
// 		}
// 	}
// 	// 遇到命令过长的情况，不解析当行
// 	debugf("line too long\n");
// 	// 吃掉剩下的字符，避免缓冲区溢出
// 	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
// 		;
// 	}
// 	buf[0] = 0;
// }

void readline(char *buf, u_int n) {
	int r = 0;
	int i = 0;
	char c;
	static char backbuf[1024];
	int backbuf_i = 0;
	static enum {NORMAL, GOT_ESC, GOT_BRACKET} state = NORMAL;

	while (i + backbuf_i < n - 1) {
		if ((r = read(0, &c, 1)) != 1) {
			if (r < 0) {
				fprintf(2, "read error: %d\n", r);
			}
			exit(1);
		}

		back:
			if (state == NORMAL) {
				switch(c) {
					case BACKSPACE:
					case DEL:
						if (i > 0) {
							i--;
							if (interactive) {
								printf("\b");
								for (int k = backbuf_i - 1;k >= 0;k--) {
									// if (backbuf[k] < 32 || backbuf[k] >= 127) {
									// 	printf("?");
									// } else {
									// 	printf("%c",backbuf[k]);
									// }
									PUT_CHAR(backbuf[k]);
								}
								// print(" ");

								// for (int k = 0;k < backbuf_i + 1;k++) {
								// 	printf("\b");
								// }
								printf(" \033[%dD", backbuf_i + 1);
							}
						}
						break;

					case '\r':
					case '\n':
						PRINTF("\n");
						for(int k = backbuf_i - 1;k >= 0;k--) {
							buf[i++] = backbuf[k];
						}
						buf[i] = 0;
						return;

					case ESC:
						state = GOT_ESC;
						break;

					case C('E'):
						// move cursor to the endAdd commentMore actions
						if(backbuf_i > 0) {
							PRINTF("\033[%dC", backbuf_i);
							for(int k = backbuf_i - 1; k >= 0; k--){
								buf[i++] = backbuf[k];
							}
							backbuf_i = 0;
						}
						break;

					case C('A'):
						// move cursor to the start
						if(i > 0) {
							PRINTF("\033[%dD", i);
							for(int k = i - 1; k >= 0; --k){
								backbuf[backbuf_i++] = buf[k];
							}
							i = 0;
						}
						break;

					case C('K'):
						// clear line from cursor to end
						PRINTF("\033[K");
						backbuf_i = 0;
						break;

					case C('U'):
						// clear line from start to cursor
						if(i > 0) {
							if (interactive) {
								printf("\r\033[K%s", PROMPT);
								for(int k = backbuf_i - 1; k >= 0; k--){
									PUT_CHAR(backbuf[k]);
								}
								if(backbuf_i > 0) {
									printf("\033[%dD", backbuf_i);
								}
							}
							i = 0;
						}
						break;

					case C('W'):
						// delete word
						{
							int temp_i = i;
							while (temp_i > 0 && strchr(WHITESPACE, buf[temp_i - 1])) {
								temp_i--;
							}
							while (temp_i > 0 && !strchr(WHITESPACE, buf[temp_i - 1])) {
								temp_i--;
							}

							if(i > temp_i) {
								if (interactive) {
									printf("\033[%dD", i - temp_i);
									printf("\033[K");
									for(int k = backbuf_i - 1; k >= 0; k--){
										PUT_CHAR(backbuf[k]);
									}
									if(backbuf_i > 0) {
										printf("\033[%dD", backbuf_i);
									}
								}
								i = temp_i;
							}
						}
						break;

					case C('P'):
						break;

					default:
						buf[i++] = c;
						// if (c < 32 || c >= 127) {
						// 	printf("?");
						// } else {
						// 	pintf("%c", c);
						// }
						if (interactive) {
							PUT_CHAR(c);

							for (int k = backbuf_i - 1; k>= 0; k--) {
								// if (backbuf[k] < 32 || backbuf[k] >= 127) {
								// 	printf("?");
								// } else {
								// 	printf("%c", backbuf[k]);
								// }
								PUT_CHAR(backbuf[k]);
							}

							// for (int k =0; k < backbuf_i;k++) {
							// 	printf("\b");
							// }
							if (backbuf_i > 0) {
								printf("\033[%dD", backbuf_i);
							}
						}
						
				}
			} else if (state == GOT_ESC) {
				if (c == '[') {
					state = GOT_BRACKET;
				} else {
					state = NORMAL;
					goto back;
				}
			} else {
				switch (c) {
					case 'A':
						stage_command(&history, buf, &i, backbuf, &backbuf_i);
						move_history_cursor(&history, buf, &i, -1);
						PRINTF("\r\033[K%s%s", PROMPT, buf);
						break;
					case 'B':
						stage_command(&history, buf, &i, backbuf, &backbuf_i);
						move_history_cursor(&history, buf, &i, 1);
						PRINTF("\r\033[K%s%s", PROMPT, buf);
						break;
					case 'C':
						if (backbuf_i > 0) {
							PRINTF("\033[C");
							buf[i++] = backbuf[--backbuf_i];
						}
						break;
					case 'D':
						if (i > 0) {
							PRINTF("\033[D");
							backbuf[backbuf_i++] = buf[--i];
						}
						break;
					default:
						PRINTF("[");
						buf[i++] = '[';
						state = NORMAL;
						goto back;
					
				}
				state = NORMAL;
			}
	}
	fprintf(2, "line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit(1);
}

int main(int argc, char **argv) {
	int r;

	// 是否为交互式终端
	interactive = iscons(0);
	// 是否要输出输入的命令
	int echocmds = 0;
	char *comment;
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
		interactive = 0;
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}

	if (interactive) {
		printf("%s\n", _MOS_LOGO_);
		printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
		printf("::                                                         ::\n");
		printf("::                     MOS Shell 2025                      ::\n");
		printf("::                                                         ::\n");
		printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	}

	store_01(storedFd);

	load_command_history(&history);
	init_vars(&variable_set);
	strcpy(rPath, (const char *)env->r_path);
	// 在循环中不断读入命令行并进行处理
	for (;;) {
		// 作为交互式终端，先打印一个"$"
		PRINTF("\n%s%s", rPath, " $ ");
		// 读入一份命令到buf
		readline(buf, sizeof buf);
		add_history(&history, buf);

		comment = (char *)strchr(buf, '#');
        if (comment) {
            *comment = 0; // truncate at comment
        }
        if (echocmds) {
            printf("# %s\n", buf);
        }
		if (expand_vars(&variable_set, buf) < 0) {
            fprintf(2, "Error: Variable expansion failed\n");
            DEBUGF("expand_vars failed: %s\n", buf);
            continue;
        }
        char *cmd = process_backticks(buf);
        runcmd(cmd);
        // restore original 0/1 fd
		restore_01(storedFd);
    }
	return 0;
}

int _declare(int argc, char **argv) {
    int export_flag = 0, readonly_flag = 0;
    char *name = NULL, *value = NULL;

    ARGBEGIN {
        case 'x':
            export_flag = 1;
            break;
        case 'r':
            readonly_flag = 1;
            break;
    }
    ARGEND

    if (argc == 0) {
        print_vars(&variable_set);
        return 0;
    }

    name = argv[0];
    value = (char *)strchr(argv[0], '=');
    if (!value || name == value || !*(value + 1)) {
        // If no '=' found or name is empty or value is empty
        fprintf(2, "declare: syntax error: expected name=value\n");
        return -E_INVAL;
    }

    *value++ = 0;
    return declare_var(&variable_set, name, value, export_flag, readonly_flag);
}

int _unset(int argc, char **argv) {
    if(argc != 2) {
        fprintf(2, "unset: expected 1 argument; got %d\n", argc - 1);
        return -E_INVAL;
    }

    return unset_var(&variable_set, argv[1]);
}

int _cd(int argc, char **argv) {
    int r;
    switch (argc) {
        case 1:
            argv[1] = "/";
        case 2:
            if ((r = chdir(argv[1])) < 0) {
                if (r == -E_NOT_FOUND) {
                    fprintf(2, "cd: The directory '%s' does not exist\n",
                            argv[1]);
                } else if (r == -E_NOT_DIR) {
                    fprintf(2, "cd: '%s' is not a directory\n", argv[1]);
                } else {
                    fprintf(2, "cd failed %s: %d\n", argv[1], r);
                }

                return r;
            }
            strcpy(rPath, (const char *)env->r_path);

            break;

        default:
            fprintf(2, "Too many args for cd command\n");
            return -E_INVAL;
    }

    return 0;
}

int _pwd(int argc, char **argv) {
    if (argc > 1) {
        fprintf(2, "pwd: expected 0 arguments; got %d\n", argc - 1);
        return -E_INVAL;
    }

    printf("%s\n", rPath);
    return 0;
}

int _history(int argc, char **argv) {
    if (argc > 1) {
        fprintf(2, "history: expected 0 arguments; got %d\n", argc - 1);
        return -E_INVAL;
    }

    show_history(&history);
    return 0;
}

int _exit(int argc, char **argv) {
    if (argc > 1) {
        fprintf(2, "exit: expected 0 arguments; got %d\n", argc - 1);
        return -E_INVAL;
    }

    // save history before exiting
    save_command_history(&history);
    exit(0);
}