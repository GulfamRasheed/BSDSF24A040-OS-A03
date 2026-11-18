# BSDSF24A040-OS-A03

Command Line Interpreter (Shell) built for Operating Systems Programming Assignment 03.

## Features

- **Built-in commands:** `cd`, `exit`, `help`, `jobs`, `history`, `set`
- **Command history** with `!n` re-execution
- **Tab completion & line editing** via GNU Readline
- **I/O redirection:** `<` (input), `>` (output)
- **Pipes:** `cmd1 | cmd2`
- **Command chaining:** `cmd1 ; cmd2`
- **Background execution:** `cmd &` with `jobs` listing and zombie reaping
- **Control structure:** `if / then / else / fi`
- **Shell variables:** `VAR=value`, `$VAR` expansion, `set` to list

## Build

Requires GCC, Make, and GNU Readline (`libreadline-dev`).

```bash
make
```

## Run

```bash
./bin/myshell
```

## Project Structure

```
BSDSF24A040-OS-A03/
├── src/
│   ├── main.c
│   ├── shell.c
│   └── execute.c
├── include/
│   └── shell.h
├── Makefile
└── README.md
```

## Git Tags

| Tag | Feature |
|-----|---------|
| v1.0-base | The Starting Point (Base Shell) |
| v2 | Built-in Commands |
| v3 | Command History |
| v4 | Tab Completion with Readline |
| v5 | I/O Redirection and Pipes |
| v6 | Command Chaining and Background Execution |
| v7 | if-then-else-fi Control Structure |
| v8 | Shell Variables |

## Author

- **Roll No:** BSDSF24A040
- **Course:** Operating Systems (Programming Assignment 03)