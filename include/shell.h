#ifndef SHELL_H
#define SHELL_H

#define _POSIX_C_SOURCE 200809L

#define MAX_ARGS 64
#define HISTORY_SIZE 20
#define MAX_LINE 4096
#define MAX_VARS 128
#define MAX_JOBS 64
#define MAX_PIPELINE 16

typedef struct {
    char *args[MAX_ARGS];
    char *infile;
    char *outfile;
    int background;
} command_t;

typedef struct {
    int pid;
    char cmd[MAX_LINE];
} job_t;

void shell_loop(void);
char *read_cmd(const char *prompt);
char *expand_history(const char *line);
int handle_builtin(char **args);
int run_line(char *line);
int run_pipeline(char *line, int background);
int execute_command(command_t *cmd, int use_pipe, int pipe_read, int pipe_write);
int parse_and_run(char *segment, int background);
command_t *parse_command(char *segment);
void free_command(command_t *cmd);
void add_history_entry(const char *line);
void print_history(void);
void expand_vars(char **args);
int set_variable(const char *arg);
void print_variables(void);
void add_job(int pid, const char *cmd);
void reap_background(void);
void print_jobs(void);
int shell_cd(char **args);
int shell_exit(char **args);
int shell_help(char **args);
int shell_jobs(char **args);
int shell_history_cmd(char **args);
int shell_set_cmd(char **args);

#endif
