# Cipher

Cipher is a small Unix-like shell implemented in C. It was developed as a university Operating Systems course project and focuses on basic shell behavior, command parsing, built-in commands, external command execution, standard input/output redirection, command pipelines, background processes, and interaction with the Linux process file system.

## Features

- Custom interactive shell prompt
- Built-in command handling
- External command execution using `fork()` and `execvp()`
- Exit status tracking
- Basic token parsing, including quoted strings and comments
- Standard input and output redirection for built-in and external commands
- Background process execution for built-in commands, external commands, and pipelines
- Command pipelines through the `pipes` built-in command
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

## Command syntax

Cipher supports ordinary command arguments, quoted arguments, comments, redirection, and background execution.

```text
command ARG...
command ARG... <input.txt
command ARG... >output.txt
command ARG... <input.txt >output.txt
command ARG... &
```

Redirection tokens are written as a single token, without a space between the redirection symbol and the file name:

```text
echo "hello" >hello.txt
cpcat <hello.txt
cpcat <hello.txt >copy.txt
```

Input and output redirection work for both external commands and built-in commands. When a built-in command runs in the foreground, the shell temporarily redirects its standard descriptors and restores them after the command finishes. When a built-in command runs in the background, it is executed in a child process.

## Pipelines

The `pipes` command executes two or more quoted command stages as a pipeline:

```text
pipes "stage 1" "stage 2" "stage 3" ...
```

For example:

```text
pipes "cat /etc/passwd" "cut -d: -f7" "sort" "uniq -c"
```

This is equivalent to the following shell pipeline:

```bash
cat /etc/passwd | cut -d: -f7 | sort | uniq -c
```

Each stage is written as a quoted command string. Internally, every stage is parsed and executed as an ordinary Cipher command, so stages may be built-ins or external commands.

Redirection and background execution apply to the whole pipeline, not to individual stages:

```text
pipes "rev" "cpcat" "rev" </etc/passwd >text &
```

The command above reads from `/etc/passwd`, sends the data through the three pipeline stages, writes the final output to `text`, and runs the whole pipeline in the background.

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
| `cpcat [FILE] [OUT]` | Print or copy file contents; reads from standard input when no input file is given and standard input has been redirected or supplied by a pipeline |
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

### Pipelines

| Command | Description |
| --- | --- |
| `pipes STAGE STAGE...` | Execute two or more quoted command stages as a pipeline |

## Examples

Redirect output from an external command:

```text
/bin/echo "Example text" >example.txt
cat <example.txt
```

Redirect output from a built-in command:

```text
echo "Example text" >example.txt
cpcat <example.txt
```

Copy through redirected standard input and output:

```text
cpcat <input.txt >output.txt
cpcat output.txt
```

Run a pipeline:

```text
pipes "cat /etc/passwd" "cut -d: -f7" "sort" "uniq -c"
```

Run a redirected pipeline in the background:

```text
pipes "rev" "cpcat" "rev" </etc/passwd >text &
waitall
cpcat text
```
