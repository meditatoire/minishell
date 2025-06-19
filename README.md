# Minishell

A minimal Unix-like shell implemented in C.

## Features

- Execute external commands (`fork`, `execvp`)
- Foreground & background job management
- Signal handling (`SIGCHLD`, `SIGINT`, `SIGTSTP`)
- I/O redirection (`<`, `>`)
- Built-in commands: `cd`, `dir`
- Pipe support (`|`) for chained commands
