#define _XOPEN_SOURCE 500
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <kga/kga.h>
#include <kga/string.h>
#include "shell.h"
#include "kga_wrappers.h"

struct shell {
	pid_t pid, parent_pid;
	FILE *in, *out;
	int fd_in;
	int fd_out;
};

exception_type_t shell_exception_process_died;

static void shell_free(void *ptr) {
	shell_t *shell = ptr;
	if (shell->pid > 0 && shell->parent_pid > 0 && shell->parent_pid == getpid()) {
		kill(shell->pid, SIGINT);
		int status;
		waitpid(shell->pid, &status, 0);
	};
	free(shell);
};

shell_t *shell_new() {
	shell_t *shell = kga_malloc(sizeof(struct shell));
	shell->parent_pid = -1;
	shell->pid = -1;
	shell->fd_in = -1;
	shell->fd_out = -1;
	shell->in = NULL;
	shell->out = NULL;
	scope_add(shell, shell_free);
	scope {
		int *pipe_in = kga_pipe();
		int *pipe_out = kga_pipe();
		pid_t shell_pid = kga_fork();
		if (!shell_pid) {
			kga_dup2(pipe_in[0], STDIN_FILENO);
			kga_dup2(pipe_out[1], STDOUT_FILENO);
			while(scope_current()) scope_end();
			execl("/bin/sh", "sh", NULL);
			exit(EXIT_FAILURE);
		};
		scope_use_previous {
			shell->parent_pid = getpid();
			shell->pid = shell_pid;
			shell->fd_in = pipe_in[1];
			shell->in = kga_fdopen(shell->fd_in, "w");
			pipe_in[1] = -1;
			shell->fd_out = pipe_out[0];
			shell->out = kga_fdopen(shell->fd_out, "r");
			pipe_out[0] = -1;
		};
	};
	//if (fcntl(shell->fd_out, F_GETFL) == -1) throw_errno();
	//if (fcntl(shell->fd_in, F_GETFL) == -1) throw_errno();
	return shell;
};

void shell_process(shell_t *shell, const char *cmd) {
	//printf(cmd);
	if (fprintf(shell->in, "%s", cmd) < 0) {
		throw_errno();
	};
	fflush(shell->in);
};

char *shell_get_line(shell_t *shell) {
	char buffer[16384];
	char *line = string_new();
	try_scope {
		if (!fgets(buffer, 16384, shell->out)) {
			int status, wait_ret;
			if ((wait_ret = waitpid(shell->pid, &status, WNOHANG)) > 0) {
				char *message = malloc(64);
				if (message) snprintf(message, 64, "status = %i", status);
				throw(shell_exception_process_died, 1, "Shell process died", message);
			} else if (wait_ret < 0) {
				char *message = malloc(64);
				if (message) snprintf(message, 64, "waitpid say: %s", strerror(errno));
				throw(shell_exception_process_died, errno, "Shell process died", message);
			};
			throw_errno();
		};
		string_cat(line, buffer);
		if (line[string_length(line) - 1] == '\n') {
			string_cut(line, string_length(line) - 1, 1);
		};
	};
	catch throw_proxy();
	return line;
};

char *shell_get_var(shell_t *shell, const char *var_name) {
	try_scope {
		char *cmd = string_new();
		string_fmt(cmd, "echo $%s\n", var_name);
		shell_process(shell, cmd);
	};
	catch throw_proxy();
	return shell_get_line(shell);
};

void shell_process_file(shell_t *shell, const char *file_path) {
	try_scope {
		char *cmd = string_new();
		string_fmt(cmd, "test -f %s && . %s >&2\n", file_path, file_path);
		shell_process(shell, cmd);
	};
	catch throw_proxy();
};

