#ifndef _HISTORY_H_
#define _HISTORY_H_

#include <types.h>
#include <fs.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_HISTORY_COMMANDS 20
#define HISTORY_FILE "/.mos_history"

#define CHECK_FD(fd) \
	do { \
		if ((fd) < 0) { \
			debugf("bad fd %d\n", (fd)); \
			return; \
		} \
	} while (0)

    
struct History {
	int fd;
	int write_index;
	int cursor;
	char buffer[MAX_HISTORY_COMMANDS][MAX_COMMAND_LENGTH];
	char stage_command[MAX_COMMAND_LENGTH];
};

// using buffer, read a line from fd into buf
int fgetline(int fd, char *buf, u_int n);
void load_command_history(struct History *history);
void save_command_history(struct History *history);
void move_history_cursor(struct History *history, char *buf, int *edit_idx, int offset);
void stage_command(struct History *history, char *buf, int *i, char *backbuf, int *backbuf_i);
void add_history(struct History *history, char *buf);
void show_history(struct History *history);

#endif