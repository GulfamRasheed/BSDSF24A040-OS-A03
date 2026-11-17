#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

static history_t sh_history;
static int background_pids[64];
static int num_background = 0;
static shell_var_t var_list[MAX_VARS];
static int num_vars = 0;

char **shell_split_line(char *line) {
    int buf = MAX_ARGS, position = 0;
    char **tokens = malloc(buf * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, " \t\r\n\b");
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= buf) {
            buf += MAX_ARGS;
            tokens = realloc(tokens, buf * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, " \t\r\n\b");
    }
    tokens[position] = NULL;
    return tokens;
}

command_t *shell_parse_line(char *line) {
    command_t *cmd = calloc(1, sizeof(command_t));
    if (!cmd) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    char *line_copy = strdup(line);
    char *saveptr;
    char *token = strtok_r(line_copy, " \t\r\n", &saveptr);

    int arg_idx = 0;
    int pipe_count = 0;

    while (token != NULL) {
        // Check for variable assignment: VARNAME=value
        if (strchr(token, '=') != NULL && strchr(token, '=') != strchr(token, '=') + 1) {
            // This is an assignment - find the = sign
            char *eq = strchr(token, '=');
            *eq = '\0'; // Terminate the name part
            
            // Check if this is a valid assignment (no spaces around =)
            // and store the variable
            int found = 0;
            int i;
            for (i = 0; i < num_vars; i++) {
                if (strcmp(var_list[i].var_name, token) == 0) {
                    free(var_list[i].var_value);
                    var_list[i].var_value = strdup(eq + 1);
                    found = 1;
                    break;
                }
            }
            if (!found && num_vars < MAX_VARS) {
                var_list[num_vars].var_name = strdup(token);
                var_list[num_vars].var_value = strdup(eq + 1);
                num_vars++;
            }
            
            *eq = '='; // Restore
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            continue;
        }

        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            if (token != NULL) {
                cmd->infile = strdup(token);
            }
        } else if (strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " \t\r\n", &saveptr);
            if (token != NULL) {
                cmd->outfile = strdup(token);
            }
        } else if (strcmp(token, "|") == 0) {
            pipe_count++;
        } else if (strcmp(token, "&") == 0) {
            cmd->foreground = 0;
        } else if (strcmp(token, ";") == 0) {
            break;
        } else {
            if (arg_idx < MAX_ARGS) {
                cmd->args[arg_idx] = strdup(token);
                arg_idx++;
            }
        }
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    cmd->args[arg_idx] = NULL;
    cmd->foreground = 1;
    cmd->pipe_count = pipe_count;
    cmd->pipe_fd[0] = -1;
    cmd->pipe_fd[1] = -1;

    free(line_copy);
    return cmd;
}

void shell_free_command(command_t *cmd) {
    int i;
    for (i = 0; i < MAX_ARGS && cmd->args[i] != NULL; i++) {
        free(cmd->args[i]);
    }
    if (cmd->infile) free(cmd->infile);
    if (cmd->outfile) free(cmd->outfile);
    free(cmd);
}

void shell_var_expand(char **args) {
    int i;
    for (i = 0; args[i] != NULL; i++) {
        if (args[i][0] == '$') {
            // Look up the variable
            int j;
            for (j = 0; j < num_vars; j++) {
                if (strcmp(args[i] + 1, var_list[j].var_name) == 0) {
                    // Replace with variable value
                    free(args[i]);
                    args[i] = strdup(var_list[j].var_value);
                    break;
                }
            }
            // If variable not found, replace with empty string
            if (j == num_vars) {
                free(args[i]);
                args[i] = strdup("");
            }
        }
    }
}

int shell_var_set(char **args) {
    if (args[1] == NULL) {
        // Print all variables
        int i;
        printf("Shell variables:\n");
        for (i = 0; i < num_vars; i++) {
            printf("  %s=%s\n", var_list[i].var_name, var_list[i].var_value);
        }
        if (num_vars == 0) {
            printf("  (none defined)\n");
        }
    } else {
        // Set a specific variable
        int i;
        for (i = 1; args[i] != NULL; i++) {
            char *eq = strchr(args[i], '=');
            if (eq != NULL) {
                *eq = '\0';
                int j;
                for (j = 0; j < num_vars; j++) {
                    if (strcmp(var_list[j].var_name, args[i]) == 0) {
                        free(var_list[j].var_value);
                        var_list[j].var_value = strdup(eq + 1);
                        *eq = '=';
                        break;
                    }
                }
                if (j == num_vars && num_vars < MAX_VARS) {
                    var_list[num_vars].var_name = strdup(args[i]);
                    var_list[num_vars].var_value = strdup(eq + 1);
                    num_vars++;
                }
                *eq = '=';
            } else {
                // Simple variable assignment without =
                // This is tricky - we'll just print an error
                fprintf(stderr, "Usage: set VARNAME=value\n");
            }
        }
    }
    return 1;
}

int shell_launch(command_t *cmd) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process

        // Handle input redirection (<)
        if (cmd->infile != NULL) {
            int fd = open(cmd->infile, O_RDONLY);
            if (fd < 0) {
                perror("shell");
            } else {
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
        }

        // Handle output redirection (>)
        if (cmd->outfile != NULL) {
            int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("shell");
            } else {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }

        execvp(cmd->args[0], cmd->args);
        perror("shell");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("shell");
    } else {
        // Parent process
        if (cmd->foreground) {
            waitpid(pid, &status, WUNTRACED);
        } else {
            background_pids[num_background++] = pid;
        }
    }

    return 1;
}

int shell_execute(command_t *cmd) {
    int i;

    if (cmd->args[0] == NULL) {
        return 1;
    }

    // Check for built-in commands
    if (strcmp(cmd->args[0], "cd") == 0) {
        return shell_cd(cmd->args);
    } else if (strcmp(cmd->args[0], "exit") == 0) {
        return shell_exit(cmd->args);
    } else if (strcmp(cmd->args[0], "help") == 0) {
        return shell_help(cmd->args);
    } else if (strcmp(cmd->args[0], "jobs") == 0) {
        return shell_jobs(cmd->args);
    } else if (strcmp(cmd->args[0], "history") == 0) {
        return shell_history(cmd->args);
    } else if (strcmp(cmd->args[0], "set") == 0) {
        return shell_var_set(cmd->args + 1);
    }

    // Expand variables in arguments
    shell_var_expand(cmd->args);

    // External command
    return shell_launch(cmd);
}

void shell_loop(void) {
    char *line;
    command_t *cmd;
    int status;
    int i;

    // Initialize history
    sh_history.count = 0;
    sh_history.index = 0;
    for (i = 0; i < HISTORY_SIZE; i++) {
        sh_history.commands[i] = NULL;
    }
    num_background = 0;
    num_vars = 0;

    signal(SIGCHLD, SIG_IGN);

    do {
        line = readline("myshell> ");
        if (!line) {
            printf("\n");
            break;
        }

        if (line[0] != '\0') {
            add_history(line);
        }

        // Expand !n history before processing
        line = expand_history(line);

        // Add to our custom history structure (skip duplicates)
        if (sh_history.count > 0 &&
            sh_history.commands[sh_history.count - 1] != NULL &&
            strcmp(sh_history.commands[sh_history.count - 1], line) == 0) {
            free(line);
            line = strdup("");
            continue;
        }

        if (sh_history.count < HISTORY_SIZE) {
            sh_history.commands[sh_history.count] = strdup(line);
            sh_history.count++;
        } else {
            free(sh_history.commands[sh_history.index]);
            sh_history.commands[sh_history.index] = strdup(line);
            sh_history.index = (sh_history.index + 1) % HISTORY_SIZE;
        }

        cmd = shell_parse_line(line);
        if (cmd == NULL) {
            free(line);
            line = strdup("");
            continue;
        }

        // Handle built-in commands
        if (strcmp(cmd->args[0], "exit") == 0) {
            status = shell_exit(cmd->args);
        } else if (strcmp(cmd->args[0], "cd") == 0) {
            status = shell_cd(cmd->args);
        } else if (strcmp(cmd->args[0], "help") == 0) {
            status = shell_help(cmd->args);
        } else if (strcmp(cmd->args[0], "jobs") == 0) {
            status = shell_jobs(cmd->args);
        } else if (strcmp(cmd->args[0], "history") == 0) {
            status = shell_history(cmd->args);
        } else if (strcmp(cmd->args[0], "set") == 0) {
            status = shell_var_set(cmd->args + 1);
        } else {
            // External command with variable expansion
            status = shell_execute(cmd);
        }

        // Reap background processes
        reap_background();

        shell_free_command(cmd);
        free(line);
        line = strdup("");
    } while (status);
}