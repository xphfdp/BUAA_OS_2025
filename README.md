本仓库记录了2025年操作系统课程实验代码，本课程使用C语言在Linux上实现了一个可以在MIPS平台上运行的小型操作系统，包括操作系统启动、物理内存管理、虚拟内存管理、进程管理、中断处理、系统调用、文件系统、Shell等主要操作系统主要功能。
lab0~lab6见相应分支，exam全部实现，extra部分实现，挑战性任务为Shell增强。

***

# OS Lab6 挑战性任务 Shell增强

### 实现不带`.b`后缀指令

这是本次任务中最简单的，我们只需要在`spawn.c`中的`spawn`函数中进行额外判断即可，首先尝试打开`file_path`路径的文件，如果不存在，在为这个路径添加`.b`后缀后再次尝试打开，如果打开成功，则继续执行，否则返回错误。

```C
	char cmd[1024] = {0};
	if ((fd = open(file_path, O_RDONLY)) < 0) {
		strcpy(cmd, file_path);
		strcat(cmd, ".b\0");
		if ((fd = open(cmd, O_RDONLY)) < 0) {
			return fd;
		}
	}
```

### 更多指令

#### touch

`touch`指令相对容易实现，因为`open`函数在之前的lab中就已经实现了，我们可以直接调用`open`函数进行创建文件。

```C
	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_WRONLY | O_CREAT | O_EXCL);
		if (fd >= 0) {
			r = close(fd);
			if (r < 0) {
				fprintf(2, "touch: failed to close %s\n", argv[i]);
				exit(1);
			}
		} else if (fd == -E_NOT_FOUND) {
            fprintf(2, "touch: cannot touch '%s': No such file or directory\n", argv[i]);
			exit(1);
		}
	}
```

#### mkdir

`mkdir`指令难点在于要解析它的`-p`选项，我们可以使用课程组已经实现好的位于`args.h`文件中的`ARGBEGIN`和`ARGEND`宏：

```C
	ARGBEGIN {
	case 'p':
		p_flag = 1;
		break;
	default:
		usage();
	} ARGEND;
```

对于目录的创建，增加以下内容：

```C
// serv.h
int file_mkdir(u_int envid, char *path, int isRecursive);

// fsreq.h
enum {
    ...,
    FSREQ_MKDIR,
	MAX_FSREQNO,
};

struct Fsreq_mkdir {
	char req_path[MAXPATHLEN];
	u_int isRecursive;
};

// lib.h
int fsipc_mkdir(const char *path, int isRecursive);
int mkdir(const char *path, int isRecursive);

// file.c
int mkdir(const char *path, int isRecursive) {
	return fsipc_mkdir(path, isRecursive);
}

// fsipc.c
int fsipc_mkdir(const char *path, int isRecursive) {
	if(path[0] == '\0' || strlen(path) >= MAXPATHLEN)
		return -E_BAD_PATH;

	struct Fsreq_mkdir *req = (struct Fsreq_mkdir *)fsipcbuf;
	strcpy((char *)req->req_path, path);
	req->isRecursive = isRecursive;
	return fsipc(FSREQ_MKDIR, req, 0, 0);
}
```

接着，在实现的时候可以这样做：

```C
	for(i = 0; i < argc; ++i) {
		if((r = mkdir(argv[i], p_flag)) < 0) {
			if(r == -E_FILE_EXISTS && !p_flag) {
				fprintf(2, "mkdir: cannot create directory '%s': File exists\n", argv[i]);
				exit(1);
			} else if(r == -E_NOT_FOUND) {
				fprintf(2, "mkdir: cannot create directory '%s': No such file or directory\n", argv[i]);
				exit(1);
			}
		}
	}
```

这样就实现了目录创建。

#### rm

类似于`mkdir`，`rm`也需要解析选项，我们依然选择使用`ARGBEGIN`和`ARGEND`宏：
```C
    ARGBEGIN {
        default:
            usage();
        case 'r':
            recursive = 1;
            break;
        case 'f':
            force = 1;
            break;
    }
    ARGEND
```

`rm`相关的函数也在之前的lab中就已经实现，我们直接调用即可：

```C
// 检查是否为目录
    if (st.st_isdir) {
        if (!recursive) {
            fprintf(2, "rm: cannot remove '%s': Is a directory\n", path);
            exit(1);
        }
        // 递归删除目录内容
        int fd;
        struct File f;
        if ((fd = open(path, O_RDONLY)) < 0) {
            if (!force) {
                fprintf(2, "rm: cannot open '%s': %d\n", path, fd);
                exit(1);
            }
            return;
        }
        // 保存原始路径长度
        int original_len = strlen(path);
        // 确保路径以 '/' 结尾
        if (original_len > 0 && original_len < MAXPATHLEN &&
            path[original_len - 1] != '/') {
            path[original_len] = '/';
            original_len++;
        }
        // 读取目录项并递归删除
        while ((n = readn(fd, &f, sizeof(f))) == sizeof(f)) {
            if (f.f_name[0]) {
                int i = 0;
                int current_len = original_len;
                // 在原路径后追加文件名
                while (current_len < MAXPATHLEN - 1 && f.f_name[i]) {
                    path[current_len++] = f.f_name[i++];
                }
                if (current_len >= MAXPATHLEN - 1) {
                    path[original_len] = '\0';
                    fprintf(2, "rm: path too long for '%s/%s'\n", path,
                            f.f_name);
                    exit(1);
                }
                path[current_len] = '\0';
                remove_file(path, recursive, force);
                // 恢复原始路径
                path[original_len] = '\0';
            }
        }

	// 删除文件或空目录
    if ((r = remove(path)) < 0 && !force) {
        fprintf(2, "rm: cannot remove '%s': %d\n", path, r);
        exit(1);
    }
    DEBUGF("rm: removed '%s'\n", path);
```

#### 相对路径

首先，我们要修改`Env`结构体，添加属性`char r_path[MAXPATHLEN]`，表示进程当前的工作目录，然后还要确保`fork`时子进程能够继承父进程的工作目录，因此要在`sys_exofork()`函数中添加以下内容：

```C
// syscall_all.c
int sys_exofork(void) {
    ...
    strcpy(e->r_path, curenv->r_path);
    ...
}
```

然后在`sh.c`文件中添加静态变量`static char rPath[MAXPATHLEN]`，表示当前进程的工作目录，用于`cd`和`pwd`指令的输出，每次读入命令前都使用`strcpy(rPath, (const char *)env->r_path)`确定当前工作目录。

同时，我们需要一个系统调用`sys_chdir`来实现通过系统调用向用户态提供更改 `r_path` 的接口：

```C
int sys_chdir(u_int envid, struct File *f, const char *path) {
	struct Env *e;
	if (f == NULL) {
		return -E_INVAL;
	}
	if (f->f_type != FTYPE_DIR) {
		return -E_NOT_DIR;
	}
	try(envid2env(envid, &e, 0));
	try(set_rPath(e->r_path, path));
	e->cwd = f;
	return 0;
}
```

接下来，实现`cd`指令：

```C
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
```

以及`pwd`指令：

```C
    if (argc > 1) {
        fprintf(2, "pwd: expected 0 arguments; got %d\n", argc - 1);
        return -E_INVAL;
    }

    printf("%s\n", rPath);
    return 0;
```

二者均是直接在`sh.c`文件中实现。

#### 实现注释功能

也相对简单，只需要在`sh.c`的main函数中判断一下`#`字符即可：

```C
		comment = (char *)strchr(buf, '#');
        if (comment) {
            *comment = 0; // truncate at comment
        }
```

#### 实现一行多指令

在`parsecmd`函数中实现：
```C
		case ';':
			int r;
            if ((r = fork()) < 0) {
                exit(1);
            }
			if (r) {
				wait(r);
				restore_01(storedFd);
				return parsecmd(argv, rightpipe, isChild);
			}
			*isChild = 1;
			return argc;
```

我们只需要识别到`;`后递归调用`parsecmd`函数即可，不过要注意在递归调用之前使用`wait`函数修改当前进程的状态。

#### 实现指令自由输入

查阅资料可知：在终端中，ANSI 标准声明左右方向键分别为 `\033[D` 和 `\033[C`，也就是说，在向控制台输入 “←” 时，实际上会**解析成三个字符**，即 `\033` 、`[`、 `D` 。所以如果我们读入了字符`\033`，就可以开始对方向键进行判断，然后连续获取到左、右键代表的三个字符后，才能令光标变量做出对应的修改。

首先，我使用了两个缓冲区来实现在任意位置插入和删除：

- `char *buf`: 存储光标**左侧**的所有字符。变量 `i` 记录了 `buf` 中字符的数量，也代表了光标的逻辑位置。
- `static char backbuf[1024]`: 存储光标**右侧**的所有字符。变量 `backbuf_i` 记录了 `backbuf` 中的字符数。

光标的移动，本质上就是字符在 `buf` 和 `backbuf` 之间的移动。

在`readline`函数中修改：

```C
} else { // state == GOT_BRACKET
    switch (c) {
        // ...
        case 'C': // 右箭头 (Right Arrow)
            if (backbuf_i > 0) { // 如果光标右侧还有字符
                PRINTF("\033[C"); // 发送终端控制码，让屏幕上的光标右移
                buf[i++] = backbuf[--backbuf_i]; // 将 backbuf 的一个字符移回 buf
            }
            break;
        case 'D': // 左箭头 (Left Arrow)
            if (i > 0) { // 如果光标左侧还有字符
                PRINTF("\033[D"); // 发送终端控制码，让屏幕上的光标左移
                backbuf[backbuf_i++] = buf[--i]; // 将 buf 的最后一个字符移到 backbuf
            }
            break;
        // ...
    }
    state = NORMAL;
}
```

对于增加字符，这样实现：
```C
						buf[i++] = c;
						if (interactive) {
							PUT_CHAR(c);

							for (int k = backbuf_i - 1; k>= 0; k--) {
								PUT_CHAR(backbuf[k]);
							}

							if (backbuf_i > 0) {
								printf("\033[%dD", backbuf_i);
							}
						}
```

对于删除字符，则这样实现：
```C
						if (i > 0) {
							i--;
							if (interactive) {
								printf("\b");
								for (int k = backbuf_i - 1;k >= 0;k--) {
									PUT_CHAR(backbuf[k]);
								}
								printf(" \033[%dD", backbuf_i + 1);
							}
						}
						break;
```

#### 实现快捷键

光标移动和删除在指令自由输入中已经实现，接下来我们要实现:

| 快捷键 | 行为                                                         |
| ------ | ------------------------------------------------------------ |
| Ctrl-E | 光标跳至最后                                                 |
| Ctrl-A | 光标跳至最前                                                 |
| Ctrl-K | 删除从当前光标处到最后的文本                                 |
| Ctrl-U | 删除从最开始到光标前的文本                                   |
| Ctrl-W | 向左删除最近一个 word：先越过空白(如果有)，再删除连续非空白字符 |

首先我们要定义一个宏`#define C(x) ((x) - '@')`用来表示`Ctrl`键和其他键的组合，然后和刚才指令自由输入中一样，实现如下：

```C
					case C('E'):
						if(backbuf_i > 0) {
							PRINTF("\033[%dC", backbuf_i);
							for(int k = backbuf_i - 1; k >= 0; k--){
								buf[i++] = backbuf[k];
							}
							backbuf_i = 0;
						}
						break;

					case C('A'):
						if(i > 0) {
							PRINTF("\033[%dD", i);
							for(int k = i - 1; k >= 0; --k){
								backbuf[backbuf_i++] = buf[k];
							}
							i = 0;
						}
						break;

					case C('K'):
						PRINTF("\033[K");
						backbuf_i = 0;
						break;

					case C('U'):
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
```

#### 指令条件执行

首先，根据指导书提示，我们要修改`exit`的实现，使其能够返回值：

```C
void exit(int status) {
	// After fs is ready (lab5), all our open files should be closed before dying.
#if !defined(LAB) || LAB >= 5
	close_all();
#endif
	DEBUGF("exiting env %d status: %d\n", env->env_id, status);
	syscall_set_exit_status(status);
	syscall_env_destroy(0);
	user_panic("unreachable code");
}
```

然后在`_gettoken`函数中对`&&`和`||`进行识别：

```C
// 文件: sh.c
// 函数: _gettoken

// ...
	static const struct {
        char c1;
        char c2;
        int ret_val;
    } double_char_tokens[] = {
        {'>', '>', 'a'}, {'&', '&', 'A'}, {'|', '|', 'O'}, {0, 0, 0}};
// ...
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
//...
```

最后在`parsecmd`函数中添加逻辑，负责根据前一个命令的执行结果，来决定是否执行后一个命令：

```C
// 文件: sh.c
// 函数: parsecmd

// ...
		case 'A': // 对应 &&
		case 'O': // 对应 ||
            int child = fork1();
            if (child == 0) {
                // child process
                *isChild = 1;
                return argc;
            }
			r = wait(child); // 等待 command1 执行完毕，并获取其返回值
            DEBUGF("wait %d: %d\n", child, r);
            
            // 这是实现条件执行的核心判断
            if ((c == 'A' && r == 0) || (c == 'O' && r != 0)) {
                return parsecmd(argv, rightpipe, isChild);
            }

            return 0;
// ...

```

可以这样理解：当 `parsecmd` 解析到 `'A'` (`&&`) 或 `'O'` (`||`) 时，它首先会 `fork` 一个子进程来执行**操作符之前**的命令 (`command1`)。父进程则会调用 `wait(child)`，暂停执行，直到 `command1` 运行结束，并将其退出状态码（exit code）保存在变量 `r` 中。接下来是最关键的一步，程序会检查条件：如果操作符是 `&&` 并且 `command1` 的退出码为 `0` (表示成功)。如果操作符是 `||` 并且 `command1` 的退出码为非 `0` (表示失败)。**如果上述条件之一为真**，函数会递归调用 `parsecmd()` 继续解析并准备执行**操作符之后**的命令 (`command2`)。**如果条件为假**，函数会直接 `return 0`，这将导致后续的命令被忽略，从而实现了短路求值的效果。

#### 实现追加重定向

首先我们需要实现文件操作的`O_APPEND`模式，即追加写功能

```C
// lib.h
#define O_APPEND 0x1000

// serv.c
void serve_open(u_int envid, struct Fsreq_open *rq) {
    // ...
	if (rq->req_omode & O_APPEND) {
		o->o_ff->f_fd.fd_offset = f->f_size;
	}
    // ...
}
```

然后与刚才的指令条件执行同理，我们也在`_gettoken`函数中对`>>`进行解析，然后在`parsecmd`函数中进行处理:

```C
// ...
// 遇到输出重定向的情况
case '>':
case 'a': // 'a' 代表 '>>'
    // 这是实现两种重定向模式切换的核心
    int mode = c == '>' ? O_TRUNC : O_APPEND;

    if (gettoken(0, &t) != 'w') {
        fprintf(2, "syntax error: %s not followed by word\n", c == '>' ? ">" : ">>");
        exit(1);
    }
    // ... 检查是否为目录 ...

    // 打开对应的文件，如果打开失败，则退出
    // 注意这里使用了上面设置的 mode 变量
    if ((fd = open(t, O_WRONLY | O_CREAT | mode)) < 0) {
        fprintf(2, "open %s: %d\n", t, fd);
        exit(1);
    }
    // 进行输出重定向 (将标准输出stdout重定向到文件)
    dup(fd, 1);
    close(fd);
    break;
// ...
```

#### 实现反引号

我们可以将工作分解为四个步骤：截取指令、执行指令、读取输出、替换原文。

首先我们要查找反引号：

```C
char *start = (char *)strchr(cmd, '`');
if (!start) {
    return result; // 没有反引号，直接返回原指令
}
// ...
char *end = (char *)strchr(start + 1, '`');
```

函数首先查找成对出现的反引号，以确定需要被执行和替换的指令部分。

然后创建管道和子进程：

```C
int p[2];
if(pipe(p) < 0) { /* ... 错误处理 ... */ }

int child = fork1();
```

之后是子进程执行指令并重定向输出：

```C
if (child == 0) { // 子进程
    close(p[0]); // 关闭读取端
    // 将标准输出重定向到管道的写入端
    if (dup(p[1], 1) < 0) { /* ... 错误处理 ... */ }
    close(p[1]);
    runcmd(start + 1); // 执行反引号内的指令
    exit(0);
}
```

在子进程中，通过 `dup(p[1], 1)` 将其标准输出重定向到管道的写入端。这样，当 `runcmd` 执行反引号内的指令时，所有的输出内容不会显示在屏幕上，而是被写入了管道。

然后父进程读取管道内容：

```C
// 父进程
close(p[1]); // 关闭写入端

char output[MAX_COMMAND_LENGTH] = {0};
int output_len = 0;

// 从管道的读取端逐字节读取子进程的输出
while (output_len < MAX_COMMAND_LENGTH - 1 &&
        read(p[0], &ch, 1) == 1) {
    if (ch == '\n') {
        ch = ' '; // 将换行符替换为空格
    }
    output[output_len++] = ch;
}
close(p[0]);
wait(child); // 等待子进程执行完毕
```

父进程从管道的读取端 `p[0]` 读取子进程的全部输出，并将其存储在 `output` 字符数组中。值得注意的是，代码会将输出中的换行符 `\n` 替换为空格，这是shell指令替换的标准行为。

最后就是构建新指令，父进程会用 `output` 中捕获到的内容，来替换掉原指令字符串中的 ``...`` 部分，从而构建出一条全新的指令字符串，并将其返回。

#### 实现历史指令

首先我们可以定义一个结构体：
```C
struct History {
	int fd;
	int write_index; // 这是一个持续递增的计数器，记录了总共执行过的指令数。通过 write_index % 20 可以得到新指令在循环缓冲区中的存放位置。
	int cursor; //指示用户当前通过上下箭头选中的是哪一条历史指令。
	char buffer[MAX_HISTORY_COMMANDS][MAX_COMMAND_LENGTH]; //这是一个在内存中的循环缓冲区，用于存放最近的20条历史指令。
	char stage_command[MAX_COMMAND_LENGTH]; // 这是一个“暂存区”，用于保存用户当前正在输入但还未执行的命令。这是实现“可以切换回当前输入”功能的关键。
};
```

然后具体实现流程如下：

1. 当shell启动时，`sh.c` 的 `main` 函数会调用 `load_command_history()` 函数。该函数会打开 `/.mos_history` 文件，逐行读取最多20条指令，加载到 `history.buffer` 内存缓冲区中，并设置好 `write_index` 和 `cursor` 的初始值。

2. 当在 `sh.c` 的 `main` 循环中输入并执行一条指令后，会立即调用 `add_history()` 函数。

   **添加到内存**：`add_history` 将新的指令字符串拷贝到 `history.buffer` 循环缓冲区的下一个可用位置 `(history->write_index % MAX_HISTORY_COMMANDS)`。

   **更新索引**：然后增加 `write_index` 的值，并将 `cursor` 指针也更新到最新的位置，表示当前焦点在新输入的指令之后。

   **持久化保存**：紧接着，`add_history` 会调用 `save_command_history()` 函数。此函数会将内存缓冲区中最近的20条指令（通过模运算正确处理循环情况）完整地覆写到 `/.mos_history` 文件中，从而实现历史记录的持久化。

3. 当输入 `history` 命令时，`sh.c` 的 `runcmd` 函数会匹配到这个内置命令，直接调用的 `show_history()`函数。`show_history()` 函数会遍历 `history.buffer` 缓冲区中的所有有效指令，并逐行打印出来，从而实现了 `history` 命令的功能。

4. 当在 `readline` 函数中按下**上箭头** (`Up`) 或**下箭头** (`Down`) 时，会触发 `case 'A'` 或 `case 'B'` 的逻辑：

   **暂存当前输入**：

   - 在切换历史指令之前，程序会先调用 `stage_command()` 函数。
   - 这个函数的作用是：如果用户当前正在浏览历史（即 `cursor` 不在末尾），则不做任何事；如果用户正准备输入新命令（`cursor` 在末尾），则将当前输入行（包括光标前和光标后的内容）完整地保存到 `history.stage_command` 暂存区中。

   **切换历史指令**：

   - 接着，程序调用 `move_history_cursor()`，并传入偏移量 `-1`（向上）或 `+1`（向下）。
   - 该函数首先更新 `cursor` 的值，并进行**边界检查**，确保光标不会移出有效历史记录的范围（最早的指令和暂存的当前指令之间）。这实现了你提到的“再次 Up 应保留在该指令”的功能。
   - 如果更新后的 `cursor` 指向某条历史指令，函数就从 `history.buffer` 中拷贝该指令到 `sh.c` 的主输入缓冲区 `buf` 中。
   - 如果更新后的 `cursor` 正好指向了末尾（即回到了你开始浏览历史之前的位置），函数则会从 `history.stage_command` 中拷贝出之前暂存的 `echo` 命令，并放回主输入缓冲区 `buf`。

   **刷新显示**：

   - `move_history_cursor` 执行完毕后，`readline` 函数会立即清空当前行并重新打印提示符和 `buf` 中的新内容，让你在屏幕上看到切换后的指令。

#### 实现环境变量

首先对于`declare`指令，我们依然先使用`ARGBEGIN`和`ARGEND`来解析参数：
```C
    ARGBEGIN {
        case 'x':
            export_flag = 1;
            break;
        case 'r':
            readonly_flag = 1;
            break;
    }
    ARGEND
```

然后对于无参数的情况，直接遍历并打印当前 shell 中的所有变量。

对于有参数的情况，解析 `NAME=VALUE` 格式的字符串。将等号前的内容视为变量名 `name`，等号后的内容视为值 `value`。然后进行相关处理。

对于`unset`指令，可以这样实现：

```C
int unset_var(struct VariableSet *vset, char *name) {
    if (vset == NULL || name == NULL) {
        return -E_INVAL;
    }

    struct Variable *var_to_remove = _find_var(vset, name);
    if (var_to_remove == NULL) {
        // 变量不存在不是一个致命错误，静默返回
        return 0;
    }

    if (var_to_remove->mode & V_RDONLY) {
        printf("错误: 变量 '%s' 是只读的。\n", name);
        return -E_NOT_WRITABLE;
    }

    int idx_to_remove = var_to_remove - vset->vars;
    int is_export = (idx_to_remove < vset->exportIdx);

    // 使用 memmove 来移动数组元素
    if (is_export) {
        int remaining_count = vset->exportIdx - idx_to_remove - 1;
        if (remaining_count > 0) {
            memmove(&vset->vars[idx_to_remove], &vset->vars[idx_to_remove + 1], sizeof(struct Variable) * remaining_count);
        }
        vset->exportIdx--;
        memset(&vset->vars[vset->exportIdx], 0, sizeof(struct Variable));
    } else {
        int remaining_count = idx_to_remove - vset->localIdx;
        if (remaining_count > 0) {
            memmove(&vset->vars[vset->localIdx + 1], &vset->vars[vset->localIdx], sizeof(struct Variable) * remaining_count);
        }
        vset->localIdx++;
        memset(&vset->vars[idx_to_remove], 0, sizeof(struct Variable)); // 清理移动后的旧位置
    }

    return 0;
}
```

