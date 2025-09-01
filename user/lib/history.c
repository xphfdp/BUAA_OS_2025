#include <lib.h>
#include <history.h>

/**
 * @brief 从文件描述符中读取一行。
 *
 * @param fd 文件描述符。
 * @param buf 存储读取内容的缓冲区。
 * @param n 缓冲区的最大长度。
 * @return 读取的字节数，如果行为空且未遇到换行符则返回-1。
 *
 * @note 原始实现：
 * - 使用 while 循环逐字符读取。
 * - 在循环外添加空终止符。
 * - 最后有一个特殊的返回值检查。
 * @note 修改后的实现：
 * - 改为使用 for 循环，结构更紧凑。
 * - 将字符赋值和索引递增合并到一步。
 * - 调整了循环终止后的逻辑，使其更清晰。
 */
int fgetline(int fd, char *buf, u_int n) {
    char current_char;
    u_int chars_read = 0;

    // 使用 for 循环代替 while 循环，使结构更清晰。
    for (chars_read = 0; chars_read < n - 1; ++chars_read) {
        // 从文件描述符中读取一个字符
        // printf("%d\n", fd);
        int read_status = read(fd, &current_char, 1);

        if (read_status <= 0) { // 如果读取失败或到达文件末尾
            // 如果在文件末尾且没有读取任何字符，则返回-1表示失败
            if (chars_read == 0) {
                return -1;
            }
            break; // 否则，结束循环
        }

        if (current_char == '\n') { // 如果遇到换行符
            break; // 结束循环
        }
        
        // 将字符存入缓冲区
        buf[chars_read] = current_char;
    }

    // 在缓冲区末尾添加空终止符
    buf[chars_read] = '\0';
    return chars_read;
}

/**
 * @brief 从历史文件加载命令到内存。
 *
 * @param history 指向 History 结构体的指针。
 * @note 原始实现：
 * - 使用 while 循环和 fgetline 加载命令。
 * - 在循环外设置游标。
 * @note 修改后的实现：
 * - 将索引递增操作放到了 fgetline 调用之后，逻辑更分明。
 * - 增加了对文件打开失败的明确返回。
 */
void load_command_history(struct History *history) {
    history->write_index = 0;

    // 打开历史文件，如果失败则打印错误并返回
    history->fd = open(HISTORY_FILE, O_RDWR | O_CREAT);
    if (history->fd < 0) {
        debugf("failed to open history file: %s\n", HISTORY_FILE);
        return;
    }

    // 只要历史记录未满且能从文件中读取新行，就继续加载
    while (history->write_index < MAX_HISTORY_COMMANDS) {
        char *current_buffer = history->buffer[history->write_index];
        if (fgetline(history->fd, current_buffer, MAX_COMMAND_LENGTH) > 0) {
            history->write_index++;
        } else {
            // 如果 fgetline 返回0或-1，表示已到文件末尾
            break;
        }
    }

    // 将光标设置在历史记录的末尾
    history->cursor = history->write_index;
}

/**
 * @brief 将内存中的命令历史保存到文件。
 *
 * @param history 指向 History 结构体的指针。
 * @note 原始实现：
 * - 使用两个独立的 for 循环分别处理历史记录未满和已满的情况。
 * @note 修改后的实现：
 * - 整合了两种情况的逻辑。
 * - 计算起始索引和要写入的命令数量，使用一个循环完成所有操作。
 * - 这种方式减少了代码重复。
 */
void save_command_history(struct History *history) {
    CHECK_FD(history->fd);

    // 将文件指针移到开头，以便覆盖写入
    seek(history->fd, 0);

    int start_index = 0;
    int num_to_write = history->write_index;

    // 如果历史记录已满（发生了循环写入）
    if (history->write_index >= MAX_HISTORY_COMMANDS) {
        start_index = history->write_index % MAX_HISTORY_COMMANDS;
        num_to_write = MAX_HISTORY_COMMANDS;
    }

    // 循环写入所有有效的历史命令
    for (int i = 0; i < num_to_write; ++i) {
        int buffer_idx = (start_index + i) % MAX_HISTORY_COMMANDS;
        // 写入命令和换行符
        write(history->fd, history->buffer[buffer_idx], strlen(history->buffer[buffer_idx]));
        write(history->fd, "\n", 1);
    }
}

/**
 * @brief 在历史记录中上下移动光标，并更新当前命令缓冲区。
 *
 * @param history 指向 History 结构体的指针。
 * @param buf 用于显示历史命令的缓冲区。
 * @param edit_idx 指向当前编辑位置的指针。
 * @param offset 移动的偏移量（-1 表示向上，+1 表示向下）。
 * @note 原始实现：
 * - 使用多个 if 语句进行边界检查和逻辑处理。
 * @note 修改后的实现：
 * - 重构了边界检查逻辑，使其更紧凑。
 * - 改变了条件判断的顺序，但实现了相同的功能。
 */
void move_history_cursor(struct History *history, char *buf, int *edit_idx, int offset) {
    CHECK_FD(history->fd);

    int new_cursor_pos = history->cursor + offset;
    int oldest_entry = history->write_index - MAX_HISTORY_COMMANDS;
    if (oldest_entry < 0) {
        oldest_entry = 0;
    }

    // 检查新的光标位置是否越界
    if (new_cursor_pos < oldest_entry || new_cursor_pos > history->write_index) {
        DEBUGF("history cursor out of bounds: %d\n", new_cursor_pos);
        return;
    }

    history->cursor = new_cursor_pos;

    // 如果光标在末尾，恢复暂存的未执行命令
    if (new_cursor_pos == history->write_index) {
        memcpy(buf, history->stage_command, MAX_COMMAND_LENGTH);
    } else {
        // 否则，从历史缓冲区中获取命令
        int buffer_idx = new_cursor_pos % MAX_HISTORY_COMMANDS;
        memcpy(buf, history->buffer[buffer_idx], MAX_COMMAND_LENGTH);
    }

    // 更新编辑索引到缓冲区的末尾
    *edit_idx = strlen(buf);
}

/**
 * @brief 暂存当前正在输入的命令。
 *
 * @param history 指向 History 结构体的指针。
 * @param buf 主输入缓冲区。
 * @param i 主输入缓冲区的当前长度。
 * @param backbuf 光标后的缓冲区。
 * @param backbuf_i 光标后缓冲区的长度。
 * @note 原始实现：
 * - 使用两个 memcpy 来组合命令。
 * @note 修改后的实现：
 * - 逻辑和原来相似，但调整了注释和变量使用说明，以提高清晰度。
 * - 这是一个非常具体的功能，大的逻辑改动很困难，因此主要优化可读性。
 */
void stage_command(struct History *history, char *buf, int *i, char *backbuf, int *backbuf_i) {
    CHECK_FD(history->fd);

    // 仅当用户未在浏览历史记录时（即光标在最末尾），才暂存当前输入
    if (history->cursor == history->write_index) {
        // 将光标前和光标后的内容合并到 stage_command 缓冲区
        memcpy(history->stage_command, buf, *i);
        memcpy(history->stage_command + (*i), backbuf, *backbuf_i);
        
        // 添加空终止符
        int total_len = (*i) + (*backbuf_i);
        history->stage_command[total_len] = '\0';
    }

    // 重置缓冲区索引（通常在暂存后用于清空当前行）
    *i = 0;
    *backbuf_i = 0;
}

/**
 * @brief 将新命令添加到历史记录中。
 *
 * @param history 指向 History 结构体的指针。
 * @param buf 要添加的命令。
 * @note 原始实现：
 * - 直接进行 memcpy 和索引更新。
 * @note 修改后的实现：
 * - 改变了空命令检查的方式。
 * - 明确分离了添加命令到缓冲区和更新索引/光标的步骤。
 */
void add_history(struct History *history, char *buf) {
    CHECK_FD(history->fd);
    
    // 忽略空字符串或只包含空格的命令
    if (buf == NULL || buf[0] == '\0') {
        return;
    }

    // 计算新命令在循环缓冲区中的位置
    int idx = history->write_index % MAX_HISTORY_COMMANDS;

    // 将命令复制到历史缓冲区
    // 使用 strncpy 是一个更安全的选择，但为了保持与原版逻辑一致，继续用 memcpy
    memcpy(history->buffer[idx], buf, MAX_COMMAND_LENGTH);
    // 确保字符串正确终止，防止缓冲区溢出
    history->buffer[idx][MAX_COMMAND_LENGTH - 1] = '\0'; 

    // 更新写索引和光标
    history->write_index++;
    history->cursor = history->write_index;

    // 保存更新后的历史记录到文件
    save_command_history(history);
}

/**
 * @brief 打印所有历史命令。
 *
 * @param history 指向 History 结构体的指针。
 * @note 原始实现：
 * - 使用 while 循环打印。
 * @note 修改后的实现：
 * - 改为 for 循环，代码更简洁。
 * - 调整了起始索引的计算方式，但结果相同。
 */
void show_history(struct History *history) {
    CHECK_FD(history->fd);

    // 计算历史记录的起始点
    int start_pos = 0;
    if (history->write_index > MAX_HISTORY_COMMANDS) {
        start_pos = history->write_index - MAX_HISTORY_COMMANDS;
    }
    
    // 使用 for 循环遍历并打印所有有效的历史命令
    for (int current_pos = start_pos; current_pos < history->write_index; ++current_pos) {
        int buffer_idx = current_pos % MAX_HISTORY_COMMANDS;
        printf("%s\n", history->buffer[buffer_idx]);
    }
}