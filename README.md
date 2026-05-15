# Cipher

Cipher is a small Unix-like shell implemented in C. It was developed as a university Operating Systems course project and focuses on basic shell behavior, command parsing, built-in commands, external command execution, background processes, and interaction with the Linux process file system.

## Features

- Custom interactive shell prompt
- Built-in command handling
- External command execution using `fork()` and `execvp()`
- Exit status tracking
- Basic token parsing, including quoted strings and comments
- Input and output redirection tokens
- Background process execution
- File, directory, and link operations
- Process and user/group information commands
- Basic `/proc`-based process listing

## Build

The project is implemented as a single C source file. On a Linux system, it can be compiled with GCC:

```bash
gcc cipher.c -o cipher
```

## Run

```bash
./cipher
```

The shell starts with the default prompt:

```text
mysh>
```

## Built-in commands

### Basic

| Command | Description |
| --- | --- |
| `debug [LEVEL]` | Set or display the debug level |
| `prompt [PROMPT]` | Set or display the shell prompt |
| `status` | Display the last command exit status |
| `exit [STATUS]` | Exit the shell |
| `help` | Display the help message |

### Output and arithmetic

| Command | Description |
| --- | --- |
| `print ARGS...` | Print arguments without a trailing newline |
| `echo ARGS...` | Print arguments with a trailing newline |
| `len ARGS...` | Print the total length of all arguments |
| `sum NUMS...` | Print the sum of numeric arguments |
| `calc NUM OP NUM` | Calculate an expression using `+`, `-`, `*`, `/`, or `%` |

### Paths and directories

| Command | Description |
| --- | --- |
| `basename PATH` | Print the final component of a path |
| `dirname PATH` | Print the directory component of a path |
| `dirch [DIR]` | Change the working directory; defaults to `/` |
| `dirwd [MODE]` | Print the working directory in `base` or `full` mode |
| `dirmk DIR` | Create a directory |
| `dirrm DIR` | Remove a directory |
| `dirls [DIR]` | List directory contents |

### Files and links

| Command | Description |
| --- | --- |
| `rename OLD NEW` | Rename a file or directory |
| `unlink FILE` | Remove a file link |
| `remove FILE` | Remove a file or directory |
| `cpcat FILE [OUT]` | Print or copy file contents |
| `linkhard SRC DST` | Create a hard link |
| `linksoft SRC DST` | Create a symbolic link |
| `linkread LINK` | Read a symbolic link target |
| `linklist FILE` | List hard links to a file in the current working directory |

### Process information

| Command | Description |
| --- | --- |
| `pid` | Print the current process ID |
| `ppid` | Print the parent process ID |
| `uid` | Print the real user ID |
| `euid` | Print the effective user ID |
| `gid` | Print the real group ID |
| `egid` | Print the effective group ID |
| `sysinfo` | Print basic system information |

### Procfs and background processes

| Command | Description |
| --- | --- |
| `proc [DIR]` | Show or set the procfs directory; defaults to `/proc` |
| `pids` | List process IDs from the configured procfs directory |
| `pinfo` | Print process information from procfs |
| `waitone [PID]` | Wait for one background process |
| `waitall` | Wait for all background processes |