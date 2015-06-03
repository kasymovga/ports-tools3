
char **file_lines(const char *path);
char *string_from_file(const char *path);
void lines_to_file(char **lines, const char *path);
void *rmrf(const char *path);
void set_signal_handler(int num, void (*handler)(int));
char *shell_escape(const char *string);
