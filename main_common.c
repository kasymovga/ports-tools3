#include <stdarg.h>
#include <stdio.h>
#include <kga/kga.h>
#include "main_common.h"
exception_type_t interrupted_by_signal;

void interrupted(int dummy) {
	throw(interrupted_by_signal, 0, "Interrupted by signal", NULL);
};

int common_confirm(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("[y/n]");
	char buffer[16];
	buffer[15] = '\0';
	while (fgets(buffer, 15, stdin)) {
		printf("\n");
		if (!strcmp(buffer, "y\n")) return 1;
		if (!strcmp(buffer, "n\n")) return 0;
		printf("[y/n]");
	};
	throw_errno();
	return 1;
};
