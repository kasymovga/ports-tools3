struct shell;
typedef struct shell shell_t;

shell_t *shell_new();
void shell_process_file(shell_t *shell, const char *path);
void shell_process(shell_t *shell, const char *cmd);
char *shell_get_var(shell_t *shell, const char *name);

exception_type_t shell_exception_process_died;
