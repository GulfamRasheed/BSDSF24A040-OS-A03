#ifndef SHELL_H
#define SHELL_H

#define MAX_ARGS 64
#define HISTORY_SIZE 20
#define MAX_LINE 1024
#define MAX_VARS 64

typedef struct {
    char *args[MAX_ARGS];
    char *infile;
    char *outfile;
    int foreground;
    int pipe_count;
} command_t;

typedef struct {
    char *commands[HISTORY_SIZE];
    int count;
    int index;
} history_t;

typedef struct {
    char *var_name;
    char *var_value;
} shell_var_t;

extern char **environ;

int shell_launch(command_t *cmd);
int shell_execute(command_t *cmd);
int shell_cd(char **args);
int shell_exit(char **args);
int shell_help(char **args);
int shell_jobs(char **args);
int shell_history(char **args);
int shell_set(char **args);
void shell_loop(void);
char **shell_split_line(char *line);
command_t *shell_parse_line(char *line);
void shell_free_command(command_t *cmd);

#endif