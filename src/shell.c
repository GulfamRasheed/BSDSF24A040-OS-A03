#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

static char *history_buf[HISTORY_SIZE];
static int history_count = 0;
static job_t jobs[MAX_JOBS];
static int job_count = 0;
static char *var_names[MAX_VARS];
static char *var_values[MAX_VARS];
static int var_count = 0;

char *read_cmd(const char *prompt) {
    char *line = readline(prompt);
    return line;
}

void add_history_entry(const char *line) {
    if (line == NULL || line[0] == '\0') return;
    if (history_count < HISTORY_SIZE) {
        history_buf[history_count] = strdup(line);
        history_count++;
    } else {
        free(history_buf[0]);
        memmove(history_buf, history_buf + 1, (HISTORY_SIZE - 1) * sizeof(char *));
        history_buf[HISTORY_SIZE - 1] = strdup(line);
    }
}

void print_history(void) {
    int i;
    for (i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history_buf[i]);
    }
}

char *expand_history(const char *line) {
    if (line[0] != '!') return strdup(line);
    if (line[1] == '!') {
        if (history_count == 0) return strdup("");
        return strdup(history_buf[history_count - 1]);
    }
    int n = atoi(line + 1);
    if (n < 1 || n > history_count) {
        fprintf(stderr, "myshell: %d: event not found\n", n);
        return strdup("");
    }
    return strdup(history_buf[n - 1]);
}

int set_variable(const char *arg) {
    const char *eq = strchr(arg, '=');
    if (eq == NULL || eq == arg) return 0;
    size_t name_len = eq - arg;
    char name[256];
    char value_buf[1024];
    if (name_len >= sizeof(name)) return 0;
    strncpy(name, arg, name_len);
    name[name_len] = '\0';
    const char *raw_value = eq + 1;
    const char *value = raw_value;
    if (*raw_value == '"' || *raw_value == '\'') {
        char quote = *raw_value;
        size_t vlen = strlen(raw_value);
        if (vlen >= 2 && raw_value[vlen - 1] == quote) {
            size_t copy_len = vlen - 2;
            if (copy_len >= sizeof(value_buf)) copy_len = sizeof(value_buf) - 1;
            memcpy(value_buf, raw_value + 1, copy_len);
            value_buf[copy_len] = '\0';
            value = value_buf;
        }
    }
    int i;
    for (i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            free(var_values[i]);
            var_values[i] = strdup(value);
            return 1;
        }
    }
    if (var_count < MAX_VARS) {
        var_names[var_count] = strdup(name);
        var_values[var_count] = strdup(value);
        var_count++;
        return 1;
    }
    return 0;
}

void print_variables(void) {
    int i;
    for (i = 0; i < var_count; i++) {
        printf("%s=%s\n", var_names[i], var_values[i]);
    }
    fflush(stdout);
}

void expand_vars(char **args) {
    int i;
    for (i = 0; args[i] != NULL; i++) {
        if (args[i][0] == '$') {
            const char *name = args[i] + 1;
            int j;
            for (j = 0; j < var_count; j++) {
                if (strcmp(var_names[j], name) == 0) {
                    free(args[i]);
                    args[i] = strdup(var_values[j]);
                    break;
                }
            }
            if (j == var_count) {
                free(args[i]);
                args[i] = strdup("");
            }
        }
    }
}

int shell_cd(char **args) {
    const char *dir = args[1] ? args[1] : getenv("HOME");
    if (dir == NULL) dir = "/";
    if (chdir(dir) != 0) {
        perror("myshell: cd");
        return 1;
    }
    return 1;
}

int shell_exit(char **args) {
    int code = 0;
    if (args[1]) code = atoi(args[1]);
    exit(code);
}

int shell_help(char **args) {
    (void)args;
    printf("myshell built-in commands:\n");
    printf("  cd [dir]    - Change directory\n");
    printf("  exit [n]    - Exit the shell\n");
    printf("  help        - Show this help\n");
    printf("  jobs        - List background jobs\n");
    printf("  history     - Show command history\n");
    printf("  set         - Show shell variables\n");
    printf("Special features:\n");
    printf("  VAR=value   - Set a shell variable\n");
    printf("  $VAR        - Expand a variable\n");
    printf("  !n          - Re-execute history command n\n");
    printf("  cmd1 ; cmd2 - Sequential execution\n");
    printf("  cmd &       - Background execution\n");
    printf("  cmd1 | cmd2 - Pipe\n");
    printf("  cmd < in > out - I/O redirection\n");
    printf("  if/then/else/fi - Conditional execution\n");
    return 1;
}

int shell_jobs(char **args) {
    (void)args;
    print_jobs();
    return 1;
}

int shell_history_cmd(char **args) {
    (void)args;
    print_history();
    return 1;
}

int shell_set_cmd(char **args) {
    (void)args;
    print_variables();
    return 1;
}

void add_job(int pid, const char *cmd) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].cmd, cmd, MAX_LINE - 1);
        jobs[job_count].cmd[MAX_LINE - 1] = '\0';
        job_count++;
    }
}

void reap_background(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int i;
        for (i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                jobs[i] = jobs[job_count - 1];
                job_count--;
                break;
            }
        }
    }
}

void print_jobs(void) {
    int i;
    if (job_count == 0) {
        printf("No background jobs\n");
        return;
    }
    for (i = 0; i < job_count; i++) {
        printf("[%d] %d  %s\n", i + 1, jobs[i].pid, jobs[i].cmd);
    }
}

int handle_builtin(char **args) {
    if (args[0] == NULL) return 1;
    if (strcmp(args[0], "cd") == 0) return shell_cd(args);
    if (strcmp(args[0], "exit") == 0) return shell_exit(args);
    if (strcmp(args[0], "help") == 0) return shell_help(args);
    if (strcmp(args[0], "jobs") == 0) return shell_jobs(args);
    if (strcmp(args[0], "history") == 0) return shell_history_cmd(args);
    if (strcmp(args[0], "set") == 0) return shell_set_cmd(args);
    return 0;
}

command_t *parse_command(char *segment) {
    command_t *cmd = calloc(1, sizeof(command_t));
    if (!cmd) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }
    char *saveptr;
    char *token = strtok_r(segment, " \t\r\n", &saveptr);
    int argc = 0;
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            if (token) cmd->infile = strdup(token);
        } else if (strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            if (token) cmd->outfile = strdup(token);
        } else if (strcmp(token, "&") == 0) {
            cmd->background = 1;
        } else {
            if (argc < MAX_ARGS - 1) {
                cmd->args[argc] = strdup(token);
                argc++;
            }
        }
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    cmd->args[argc] = NULL;
    return cmd;
}

void free_command(command_t *cmd) {
    int i;
    for (i = 0; cmd->args[i] != NULL; i++) free(cmd->args[i]);
    if (cmd->infile) free(cmd->infile);
    if (cmd->outfile) free(cmd->outfile);
    free(cmd);
}

int execute_command(command_t *cmd, int use_pipe, int pipe_read, int pipe_write) {
    if (cmd->args[0] == NULL) return 1;

    if (handle_builtin(cmd->args)) return 1;

    expand_vars(cmd->args);

    pid_t pid = fork();
    if (pid == 0) {
        if (use_pipe) {
            if (pipe_read >= 0) {
                dup2(pipe_read, STDIN_FILENO);
                close(pipe_read);
            }
            if (pipe_write >= 0) {
                dup2(pipe_write, STDOUT_FILENO);
                close(pipe_write);
            }
        }
        if (cmd->infile) {
            int fd = open(cmd->infile, O_RDONLY);
            if (fd < 0) {
                perror("myshell");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (cmd->outfile) {
            int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("myshell");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execvp(cmd->args[0], cmd->args);
        fprintf(stderr, "myshell: %s: command not found\n", cmd->args[0]);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("myshell");
    } else {
        if (cmd->background) {
            add_job(pid, cmd->args[0]);
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
    return 1;
}

int parse_and_run(char *segment, int background) {
    char *eq_pos = strchr(segment, '=');
    if (eq_pos != NULL && eq_pos > segment) {
        char *saveptr;
        char *first = strtok_r(segment, "=", &saveptr);
        char *rest = saveptr;
        if (first != NULL && rest != NULL && strchr(first, ' ') == NULL &&
            strlen(first) > 0 && (isalpha((unsigned char)first[0]) || first[0] == '_')) {
            int all_alpha = 1;
            size_t i;
            for (i = 0; first[i]; i++) {
                if (!isalnum((unsigned char)first[i]) && first[i] != '_') {
                    all_alpha = 0;
                    break;
                }
            }
            if (all_alpha) {
                char assignment[MAX_LINE];
                snprintf(assignment, sizeof(assignment), "%s=%s", first, rest);
                if (set_variable(assignment)) return 1;
            }
        }
    }
    char *line_copy = segment;
    command_t *cmd = parse_command(line_copy);
    if (cmd->args[0] == NULL) {
        free_command(cmd);
        return 1;
    }
    if (background) cmd->background = 1;
    int result = execute_command(cmd, 0, -1, -1);
    free_command(cmd);
    return result;
}

int run_pipeline(char *line, int background) {
    char *cmds[MAX_PIPELINE];
    int n = 0;
    char *saveptr;
    char *token = strtok_r(line, "|", &saveptr);
    while (token != NULL && n < MAX_PIPELINE) {
        while (*token == ' ') token++;
        char *end = token + strlen(token);
        while (end > token && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (*token) {
            cmds[n] = token;
            n++;
        }
        token = strtok_r(NULL, "|", &saveptr);
    }
    if (n == 0) return 1;
    if (n == 1) return parse_and_run(cmds[0], background);

    int pipes[MAX_PIPELINE - 1][2];
    int i;
    for (i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("myshell: pipe");
            return 1;
        }
    }
    pid_t pids[MAX_PIPELINE];
    for (i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            int j;
            for (j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            command_t *cmd = parse_command(cmds[i]);
            if (cmd->args[0] == NULL) exit(EXIT_SUCCESS);
            expand_vars(cmd->args);
            if (cmd->infile) {
                int fd = open(cmd->infile, O_RDONLY);
                if (fd >= 0) {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }
            if (cmd->outfile) {
                int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) {
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
            }
            execvp(cmd->args[0], cmd->args);
            fprintf(stderr, "myshell: %s: command not found\n", cmd->args[0]);
            exit(EXIT_FAILURE);
        }
    }
    for (i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    if (background) {
        add_job(pids[n - 1], cmds[0]);
    } else {
        for (i = 0; i < n; i++) {
            int status;
            waitpid(pids[i], &status, 0);
        }
    }
    return 1;
}

int run_line(char *line) {
    char *commands[64];
    int n = 0;
    char *saveptr;
    char *token = strtok_r(line, ";", &saveptr);
    while (token != NULL && n < 64) {
        while (*token == ' ') token++;
        char *end = token + strlen(token);
        while (end > token && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (*token) {
            commands[n] = token;
            n++;
        }
        token = strtok_r(NULL, ";", &saveptr);
    }
    int i;
    int result = 1;
    for (i = 0; i < n; i++) {
        char *seg = commands[i];
        int background = 0;
        size_t len = strlen(seg);
        if (len > 0 && seg[len - 1] == '&') {
            background = 1;
            seg[len - 1] = '\0';
            len--;
            while (len > 0 && (seg[len - 1] == ' ' || seg[len - 1] == '\t')) {
                seg[len - 1] = '\0';
                len--;
            }
        }
        if (strchr(seg, '|') != NULL) {
            result = run_pipeline(seg, background);
        } else {
            result = parse_and_run(seg, background);
        }
    }
    return result;
}

static int run_if_block(char *line) {
    char *saveptr;
    char *token = strtok_r(line, " \t", &saveptr);
    if (token == NULL || strcmp(token, "if") != 0) return 1;
    char *if_cmd = strtok_r(NULL, "", &saveptr);
    if (if_cmd == NULL) return 1;
    while (*if_cmd == ' ') if_cmd++;

    char then_block[MAX_LINE * 4] = "";
    char else_block[MAX_LINE * 4] = "";
    int in_else = 0;
    int found_fi = 0;

    while (!found_fi) {
        char *cont = readline("... ");
        if (!cont) break;
        add_history(cont);
        add_history_entry(cont);

        char *t = cont;
        while (*t == ' ' || *t == '\t') t++;
        char *tend = t + strlen(t);
        while (tend > t && (tend[-1] == '\n' || tend[-1] == '\r')) tend--;
        *tend = '\0';

        if (strcmp(t, "then") == 0) {
            free(cont);
            continue;
        }
        if (strcmp(t, "else") == 0) {
            in_else = 1;
            free(cont);
            continue;
        }
        if (strcmp(t, "fi") == 0) {
            free(cont);
            found_fi = 1;
            continue;
        }
        if (*t) {
            if (in_else) {
                if (strlen(else_block) + strlen(t) + 2 < sizeof(else_block)) {
                    strcat(else_block, t);
                    strcat(else_block, "\n");
                }
            } else {
                if (strlen(then_block) + strlen(t) + 2 < sizeof(then_block)) {
                    strcat(then_block, t);
                    strcat(then_block, "\n");
                }
            }
        }
        free(cont);
    }

    if (!found_fi) {
        fprintf(stderr, "myshell: syntax error: missing fi\n");
        return 1;
    }

    char if_line[MAX_LINE];
    strncpy(if_line, if_cmd, MAX_LINE - 1);
    if_line[MAX_LINE - 1] = '\0';

    int if_status = 0;
    pid_t pid = fork();
    if (pid == 0) {
        char *args[MAX_ARGS];
        int argc = 0;
        char *save;
        char *tok = strtok_r(if_line, " \t", &save);
        while (tok != NULL && argc < MAX_ARGS - 1) {
            args[argc] = tok;
            argc++;
            tok = strtok_r(NULL, " \t", &save);
        }
        args[argc] = NULL;
        if (argc == 0) exit(0);
        if (handle_builtin(args)) exit(0);
        expand_vars(args);
        execvp(args[0], args);
        exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) if_status = WEXITSTATUS(status);
    }

    char *block = (if_status == 0) ? then_block : else_block;
    if (block[0] != '\0') {
        char *lines[64];
        int ln = 0;
        char *ls = block;
        char *lsave;
        char *lt = strtok_r(ls, "\n", &lsave);
        while (lt != NULL && ln < 64) {
            lines[ln] = lt;
            ln++;
            lt = strtok_r(NULL, "\n", &lsave);
        }
        int k;
        for (k = 0; k < ln; k++) {
            run_line(lines[k]);
        }
    }
    return 1;
}

void shell_loop(void) {
    char *line;
    int status = 1;
    signal(SIGINT, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (status) {
        reap_background();
        fflush(stdout);
        line = read_cmd("myshell> ");
        if (!line) {
            printf("\n");
            break;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }
        add_history(line);
        add_history_entry(line);

        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (strncmp(trimmed, "if ", 3) == 0 || strcmp(trimmed, "if") == 0) {
            status = run_if_block(trimmed);
            free(line);
            continue;
        }

        char *expanded = expand_history(trimmed);
        if (expanded[0] != '\0') {
            status = run_line(expanded);
        }
        free(expanded);
        free(line);
    }
}