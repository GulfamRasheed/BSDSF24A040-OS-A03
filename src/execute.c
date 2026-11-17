#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

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

        // Handle pipe
        if (cmd->pipe_fd[0] != -1) {
            dup2(cmd->pipe_fd[0], STDIN_FILENO);
            close(cmd->pipe_fd[0]);
        }
        if (cmd->pipe_fd[1] != -1 && cmd->is_pipe_left) {
            dup2(cmd->pipe_fd[1], STDOUT_FILENO);
            close(cmd->pipe_fd[1]);
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