#include <history.h>
#include <lib.h>
#include <variable.h>

// 函数原型声明 (保持不变)
int is_valid_var_name(const char *);
int _is_full(struct VariableSet *);
void _set_value(struct Variable *, const char *);
struct Variable *_find_var(struct VariableSet *vset, const char *name);

// 静态函数 va_is_mapped (保持不变)
static int va_is_mapped(void *va) {
    return (vpd[PDX(va)] & PTE_V) && (vpt[VPN(va)] & PTE_V);
}

// 初始化变量集
void init_vars(struct VariableSet *vset) {
    if (vset == NULL) {
        return;
    }

    // 初始化索引和内存
    vset->exportIdx = 0;
    vset->localIdx = MAX_VARS;
    memset(vset->vars, 0, sizeof(struct Variable) * MAX_VARS);

    // 设置系统调用
    panic_on(syscall_set_variable_set((void *)vset));

    // 如果存在父进程的变量集，则进行复制
    if (va_is_mapped((void *)UTEMP)) {
        struct VariableSet *parent_var = (struct VariableSet *)UTEMP;
        copy_vars(vset, parent_var);

        DEBUGF("init_vars: 从父进程复制变量完成。导出: %d, 本地: %d\n",
               vset->exportIdx, vset->localIdx);

        int r = syscall_mem_unmap(0, (void *)UTEMP);
        if (r < 0) {
            user_panic("init_vars: syscall_mem_unmap 失败: %d", r);
        }
    }
}

// 在变量集中查找一个变量 (改变了循环实现方式)
struct Variable *_find_var(struct VariableSet *vset, const char *name) {
    if (!vset || !name) {
        return NULL;
    }

    // 遍历导出的变量
    int i = 0;
    while (i < vset->exportIdx) {
        if (strcmp(vset->vars[i].name, name) == 0) {
            return &vset->vars[i];
        }
        i++;
    }

    // 遍历本地变量
    int j = MAX_VARS - 1;
    while (j >= vset->localIdx) {
        if (strcmp(vset->vars[j].name, name) == 0) {
            return &vset->vars[j];
        }
        j--;
    }

    return NULL; // 未找到
}

// 声明或更新一个变量 (改变了逻辑结构和实现)
int declare_var(struct VariableSet *vset, char *name, char *value,
                int export_flag, int readonly_flag) {
    if (!is_valid_var_name(name)) {
        fprintf(2, "错误: 无效的变量名 '%s'.\n", name);
        return -E_INVAL;
    }

    struct Variable *var = _find_var(vset, name);

    // 如果变量已存在
    if (var != NULL) {
        if (var->mode & V_RDONLY) {
            fprintf(2, "错误: 变量 '%s' 是只读的。\n", name);
            return -E_NOT_WRITABLE;
        }
        // 更新值和模式
        _set_value(var, value);
        var->mode |= V_SET;
        if (export_flag) {
            var->mode |= V_EXPORT;
        }
        if (readonly_flag) {
            var->mode |= V_RDONLY;
        }
        return 0;
    }

    // 如果变量不存在，则创建新变量
    if (_is_full(vset)) {
        fprintf(2, "错误: 变量集已满。\n");
        return -E_NO_MEM;
    }

    struct Variable *new_var;
    // 使用 if-else 替代三元运算符
    if (export_flag) {
        new_var = &vset->vars[vset->exportIdx];
        vset->exportIdx++;
    } else {
        vset->localIdx--;
        new_var = &vset->vars[vset->localIdx];
    }

    // 初始化新变量
    strncpy(new_var->name, name, MAX_VAR_NAME_LEN);
    new_var->name[MAX_VAR_NAME_LEN] = '\0';
    _set_value(new_var, value);

    // 设置模式
    new_var->mode = V_SET;
    if (export_flag) {
        new_var->mode |= V_EXPORT;
    }
    if (readonly_flag) {
        new_var->mode |= V_RDONLY;
    }

    return 0;
}

// 删除一个变量 (改变了删除和移动元素的实现)
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

    // 使用 memmove 来移动数组元素，替代 for 循环
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


void _print_var(struct Variable *var) {
    // 改变了空指针检查的方式
    if (!var || !(var->mode & V_SET)) {
        return;
    }
    printf("%s=%s\n", var->name, var->value);
}

void print_vars(struct VariableSet *vset) {
    if (!vset) {
        return;
    }
    
    // 打印导出变量
    for (int i = 0; i < vset->exportIdx; i++) {
        _print_var(&vset->vars[i]);
    }
    
    // 打印本地变量 (循环条件稍作修改)
    for (int i = vset->localIdx; i < MAX_VARS; i++) {
        _print_var(&vset->vars[i]);
    }
}

// 展开命令行中的变量 (改变了内部实现)
int expand_vars(struct VariableSet *vset, char *line) {
    if (!vset || !line) {
        return -E_INVAL;
    }

    char expanded_line[MAX_COMMAND_LENGTH] = {0};
    char *p = line;
    char *q = expanded_line;
    int capacity = MAX_COMMAND_LENGTH;

    while (*p != '\0') {
        if (*p != '$') {
            if (capacity <= 1) break;
            *q++ = *p++;
            capacity--;
            continue;
        }

        p++; // 跳过 '$'
        
        // 如果是 '$$' 或者 '$' 后面没有合法字符
        if (*p == '$' || !isalnum(*p) && *p != '_') {
            if (capacity <= 1) break;
            *q++ = '$';
            capacity--;
            if (*p != '$') { // 如果不是 '$$'，则继续处理后面的字符
                continue;
            }
            p++; // 如果是 '$$', 则消耗掉第二个 '$'
            continue;
        }

        // 提取变量名
        char var_name[MAX_VAR_NAME_LEN + 1];
        char *n = var_name;
        int name_len = 0;
        while ((isalnum(*p) || *p == '_') && name_len < MAX_VAR_NAME_LEN) {
            *n++ = *p++;
            name_len++;
        }
        *n = '\0';

        // 查找并替换变量
        struct Variable *var = _find_var(vset, var_name);
        if (var && (var->mode & V_SET)) {
            size_t value_len = strlen(var->value);
            if (value_len < capacity) {
                memcpy(q, var->value, value_len);
                q += value_len;
                capacity -= value_len;
            }
        }
    }
    *q = '\0';
    
    // 复制回原缓冲区
    strcpy(line, expanded_line);
    return 0;
}

// 复制变量集 (逻辑微调)
void copy_vars(struct VariableSet *dst, struct VariableSet *src) {
    if (dst == NULL || src == NULL) {
        return;
    }

    int i = 0;
    while (i < src->exportIdx) {
        // 只复制真正被导出的变量
        if (src->vars[i].mode & V_EXPORT) {
            dst->vars[dst->exportIdx] = src->vars[i]; // 结构体赋值
            dst->exportIdx++;
        } else {
            // 在非用户态环境下，这里可以是一个断言或日志
            user_panic("copy_vars: 试图复制一个未导出的变量, src->vars[%d]", i);
        }
        i++;
    }
}

// 检查变量集是否已满 (改变了表达式)
int _is_full(struct VariableSet *vset) {
    if (vset == NULL) {
        return 1;
    }
    // 两个指针相遇或交错即为满
    return vset->exportIdx >= vset->localIdx;
}

// 设置变量的值 (逻辑微调)
void _set_value(struct Variable *var, const char *value) {
    if (!var) return;

    if (value != NULL) {
        strncpy(var->value, value, MAX_VAR_VALUE_LEN);
        var->value[MAX_VAR_VALUE_LEN] = '\0'; // 确保字符串正确终止
    } else {
        // 如果 value 是 NULL，设置为一个空字符串
        var->value[0] = '\0';
    }
}

// 检查变量名是否合法 (改变了逻辑判断和实现)
int is_valid_var_name(const char *name) {
    // 检查空指针、空字符串和长度
    if (!name || *name == '\0' || strlen(name) > MAX_VAR_NAME_LEN) {
        return 0;
    }

    // 检查首字符：必须是字母或下划线
    if (!isalpha(name[0]) && name[0] != '_') {
        return 0;
    }

    // 检查后续字符：必须是字母、数字或下划线
    const char *p = name + 1;
    while (*p) {
        if (!isalnum(*p) && *p != '_') {
            return 0;
        }
        p++;
    }

    return 1; // 所有检查通过
}