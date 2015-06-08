#define  _XOPEN_SOURCE 600
#include <kga/kga.h>
#include <kga/exception.h>
#include <kga/array.h>
#include <kga/string.h>
#include "kga_wrappers.h"
#include <string.h>
#include <signal.h>

char **file_lines(const char *path) {
	important_check(path);
	char **out = array_new(char *, 0, 0);
	scope {
		FILE *file = kga_fopen(path, "r");
		char *line = string_new();
		scope_use_previous {
			char buffer[4096];
			for (size_t readed; (readed = kga_fread(buffer, 1, 4095, file)) > 0; ) {
				for (size_t i = 0; i < readed; i++) {
					if (buffer[i] == '\n') {
						if (string_length(line)) {
							char *line_copy = string_new_set(line);
							array_push(out, line_copy);
							string_set(line, "");
						};
					} else {
						string_push(line, buffer[i]);
					};
				};
			};
			if (string_length(line)) {
				char *line_copy = string_new_set(line);
				array_push(out, line_copy);
			};
		};
	};
	return out;
};

void lines_to_file(char **lines, const char *path) {
	important_check(lines);
	scope {
		FILE *file = kga_fopen(path, "w");
		for (size_t i = 0, n = array_length(lines); i < n; i++) {
			important_check(lines[i]);
			if (fprintf(file, "%s\n", lines[i]) < 0) {
				throw_errno();
			};
		};
	};
};

char *string_from_file(const char *path) {
	important_check(path);
	char *out = string_new();
	scope {
		FILE *file = kga_fopen(path, "r");
		char buffer[4096];
		for (size_t readed; (readed = kga_fread(buffer, 1, 4095, file)) > 0; ) {
			if (readed && buffer[readed - 1] == '\n') {
				buffer[readed - 1] = '\0';
			} else {
				buffer[readed] = 0;
			};
			string_cat(out, buffer);
			if (strlen(buffer) != readed) break;
		};
	};
	return out;
};

void rmrf(const char *path) {
	struct stat st;
	if (kga_lstat_skip_enoent(path, &st)) return;
	if (S_ISDIR(st.st_mode)) {
		scope {
			char *sub_path = string_new();
			DIR *dir = kga_opendir(path);
			for (struct dirent *dirent; (dirent = readdir(dir)); ) {
				if (dirent->d_name[0] == '.' && (dirent->d_name[1] == '\0' || (dirent->d_name[1] == '.' && dirent->d_name[2] == '\0'))) {
					continue;
				};
				string_fmt(sub_path, "%s/%s", path, dirent->d_name);
				rmrf(sub_path);
			};
		};
	};
	remove(path);
};

struct restore_sigaction {
	struct sigaction sa;
	int signal;
};

void restore_sigaction(void *ptr) {
	struct restore_sigaction *prev_sa = ptr;
	sigaction(prev_sa->signal, &prev_sa->sa, NULL);
	free(ptr);
};

void set_signal_handler(int num, void (*handler)(int)) {
	struct sigaction sa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, num);
	struct restore_sigaction *prev_sa = malloc(sizeof(struct restore_sigaction));
	if (!prev_sa) throw_errno();
	prev_sa->signal = num;
	sigaction(num, &sa, &prev_sa->sa);
	scope_add(prev_sa, restore_sigaction);
};

char *shell_escape(const char *string) {
	char *escaped_string = string_new();
	string_push(escaped_string, '"');
	for (const char *c = string; *c; c++) {
		if (*c == '`' || *c == '"' || *c == '\\' || *c == '$') {
			string_cat(escaped_string, "\\");
		};
		string_push(escaped_string, *c);
	};
	string_push(escaped_string, '"');
	return escaped_string;
};
