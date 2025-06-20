#include <history.h>
#include <lib.h>
#include <variable.h>

int is_valid_var_name(const char *);
int _is_full(struct VariableSet *);
void _set_value(struct Variable *, const char *);
struct Variable *_find_var(struct VariableSet *vset, const char *name);

static int va_is_mapped(void *va) {
    return (vpd[PDX(va)] & PTE_V) && (vpt[VPN(va)] & PTE_V);
}

void init_vars(struct VariableSet *vset) {
    struct VariableSet *parent_var;
    int r;

    if (vset == NULL) return;

    vset->exportIdx = 0;        
    vset->localIdx = MAX_VARS;  
    memset(vset->vars, 0, sizeof(vset->vars));  
    panic_on(syscall_set_variable_set((void *)vset));
    if (va_is_mapped((void *)UTEMP)) {
        parent_var = (struct VariableSet *)UTEMP;
        copy_vars(vset, parent_var);
        if ((r = syscall_mem_unmap(0, (void *)UTEMP)) < 0) {
            user_panic("init_vars: syscall_mem_unmap failed: %d", r);
        }

        DEBUGF("init_vars: copied parent variable set. %d %d\n",
               vset->exportIdx, vset->localIdx);
    }
}

struct Variable *_find_var(struct VariableSet *vset, const char *name) {
    if (name == NULL || vset == NULL) {
        return NULL;
    }
    for (int i = 0; i < vset->exportIdx; i++) {
        if (strcmp(vset->vars[i].name, name) == 0) {
            return &vset->vars[i];
        }
    }
    for (int i = MAX_VARS - 1; i >= vset->localIdx; i--) {
        if (strcmp(vset->vars[i].name, name) == 0) {
            return &vset->vars[i];
        }
    }
    return NULL;
}

int declare_var(struct VariableSet *vset, char *name, char *value,
                int export_flag, int readonly_flag) {
    if (!is_valid_var_name(name)) {
        fprintf(2, "Error: Invalid variable name '%s'.\n", name);
        return -E_INVAL;
    }

    struct Variable *var = _find_var(vset, name);

    if (var) {
        if (var->mode & V_RDONLY) {
            fprintf(2, "Error: Variable '%s' is read-only.\n", name);
            return -E_NOT_WRITABLE;
        }
        
        _set_value(var, value);
        var->mode |= V_SET;
        if (export_flag) var->mode |= V_EXPORT;
        if (readonly_flag) var->mode |= V_RDONLY;

    } else { 
        if (_is_full(vset)) {
            fprintf(2, "Error: Variable set is full.\n");
            return -E_NO_MEM;
        }

        var = export_flag ? &vset->vars[vset->exportIdx++]
                          : &vset->vars[--vset->localIdx];
        strncpy(var->name, name, MAX_VAR_NAME_LEN);
        var->name[MAX_VAR_NAME_LEN] = '\0';

        _set_value(var, value);
        var->mode = V_SET;
        if (export_flag) var->mode |= V_EXPORT;
        if (readonly_flag) var->mode |= V_RDONLY;
    }
    return 0;
}

int unset_var(struct VariableSet *vset, char *name) {
    if (vset == NULL || name == NULL) {
        return -E_INVAL;
    }
    struct Variable *var_to_remove = NULL;
    int idx_to_remove = -1;
    int is_export = 0;

    var_to_remove = _find_var(vset, name);
    if (!var_to_remove) {
        fprintf(2, "Error: Variable '%s' not found.\n",name);
        return 0;
    }

    idx_to_remove = var_to_remove - vset->vars;
    is_export = (idx_to_remove < vset->exportIdx);

    if (var_to_remove->mode & V_RDONLY) {
        printf("Error: Variable '%s' is read-only.\n", name);
        return -E_NOT_WRITABLE;
    }

    memset(var_to_remove, 0, sizeof(struct Variable));

    if (is_export) {
        for (int i = idx_to_remove; i < vset->exportIdx - 1; i++) {
            vset->vars[i] = vset->vars[i + 1];
        }
        if (vset->exportIdx > 0) {
            memset(&vset->vars[vset->exportIdx - 1], 0,
                   sizeof(struct Variable));
            vset->exportIdx--;
        }
    } else { 
        for (int i = idx_to_remove; i > vset->localIdx; i--) {
            vset->vars[i] = vset->vars[i - 1];
        }
        if (vset->localIdx < MAX_VARS) {
            memset(&vset->vars[vset->localIdx], 0,
                   sizeof(struct Variable)); 
            vset->localIdx++;
        }
    }
    return 0;
}

void _print_var(struct Variable *var) {
    if (var == NULL) return;
    printf("%s=%s\n", var->name, var->value);
}

void print_vars(struct VariableSet *vset) {
    if (vset == NULL) return;
    
    for (int i = 0; i < vset->exportIdx; i++) {
        _print_var(&vset->vars[i]);
    }
    
    for (int i = MAX_VARS - 1; i >= vset->localIdx; i--) {
        _print_var(&vset->vars[i]);
    }
}

int expand_vars(struct VariableSet *vset, char *line) {
    if (vset == NULL || line == NULL) {
        return -E_INVAL;
    }

    char expanded_line[MAX_COMMAND_LENGTH];
    char var_name[MAX_VAR_NAME_LEN + 1];
    char *p = line;
    char *q = expanded_line;
    int remaining_len = MAX_COMMAND_LENGTH - 1;

    while (*p && remaining_len > 0) {
        if (*p == '$') {
            p++;
            int i = 0;

            while (*p && (isalnum(*p) || *p == '_') && i < MAX_VAR_NAME_LEN) {
                var_name[i++] = *p++;
            }
            var_name[i] = '\0';

            if (i > 0) { 
                struct Variable *var = _find_var(vset, var_name);
                if (var) {
                    panic_on((var->mode & V_SET) == 0);
                    char *val = var->value;
                    while (*val && remaining_len > 0) {
                        *(q++) = *(val++);
                        remaining_len--;
                    }
                }
            } else if (*(p - 1) == '$') { 
                if (remaining_len > 0) {
                    *(q++) = '$';
                    remaining_len--;
                }
            }
        } else {
            if (remaining_len > 0) {
                *(q++) = *(p++);
                remaining_len--;
            } else {
                break;
            }
        }
    }
    if (remaining_len <= 0) {
        debugf("error might occur\n");
    }
    *q = '\0';
    
    strcpy(line, expanded_line);
    return 0;
}

void copy_vars(struct VariableSet *dst, struct VariableSet *src) {
    if (dst == NULL || src == NULL) return;

    // Copy exported variables
    for (int i = 0; i < src->exportIdx; i++) {
        if (src->vars[i].mode &
            V_EXPORT) {  // Only copy if it's truly an exported var
            dst->vars[dst->exportIdx] = src->vars[i];  // Direct struct copy
            dst->exportIdx++;
        } else {
            user_panic("copy_vars: src->vars[%d] is not exported", i);
        }
    }
}

int _is_full(struct VariableSet *vset) {
    if (vset == NULL) return 1;  // Treat null as full
    return vset->exportIdx > vset->localIdx;
}

void _set_value(struct Variable *var, const char *value) {
    if (value) {
        strncpy(var->value, value, MAX_VAR_VALUE_LEN);
        var->value[MAX_VAR_VALUE_LEN] = '\0';
    } else {
        var->value[0] = '\0';  // Set to empty string
    }
}

int is_valid_var_name(const char *name) {
    if (!name || strlen(name) == 0 || strlen(name) > MAX_VAR_NAME_LEN) {
        return 0;
    }

    // 第一个字符必须是字母或下划线
    if (!((name[0] >= 'a' && name[0] <= 'z') ||
          (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_')) {
        return 0;
    }

    // 其余字符必须是字母、数字或下划线
    for (int i = 1; name[i]; i++) {
        int mark;
        if ((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9')) {
            mark = 1;
        } else {
            mark = 0;
        }
        if (!(mark || name[i] == '_')) {
            return 0;
        }
    }
    return 1;
}