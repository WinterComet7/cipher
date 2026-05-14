#include <ctype.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROMPT_LEN 7        // Not including the null terminator (e.g., "1234567" is valid).
#define MAX_DIRECTORY_LEN 1000  // Not including the null terminator.
#define MAX_TOKEN_SIZE 1000
#define MAX_TOKEN_COUNT 100
#define MAX_INPUT_SIZE 10000
#define MAX_BUFFER 4096

char* prompt_value = NULL;
int token_count = 0;
int optional_token_count = 0;
char** token_values = NULL;
char* input_redirect = NULL;
char* output_redirect = NULL;
int background_active = 0;

int debug_level = 0;
int exit_status = 0;
int exit_active = 0;

char* working_directory = NULL;

/* ------------------------------------------------------[ Token initialization ]------------------------------------------------------ */

// Initializes the table of strings that stores tokenized strings.
void init_tokens()
{
    token_values = (char**) malloc(sizeof(char*) * MAX_TOKEN_COUNT);
    for (int ix = 0; ix < MAX_TOKEN_COUNT; ix++) token_values[ix] = (char*) malloc(sizeof(char) * MAX_TOKEN_SIZE);
}

// Deinitializes the table of strings that stores tokenized strings.
void deinit_tokens()
{
    for (int ix = 0; ix < MAX_TOKEN_COUNT; ix++) free(token_values[ix]);
    free(token_values);
}

/* ----------------------------------------------------------[ Help command ]---------------------------------------------------------- */

void print_help()
{
    printf("Commands:\n");
    printf("  Basic:\n");
    printf("    debug [LEVEL]         Set debug level (0=off, higher=more verbose)\n");
    printf("    prompt [POZIV]        Set the shell prompt\n");
    printf("    status                Show last command exit status\n");
    printf("    exit [STATUS]         Exit the shell\n");
    printf("    help                  Show this help message\n");
    printf("\n");
    printf("  Output:\n");
    printf("    print ARGS...         Print arguments without newline\n");
    printf("    echo ARGS...          Print arguments with newline\n");
    printf("    len ARGS...           Print total length of all arguments\n");
    printf("    sum NUMS...           Print sum of all numeric arguments\n");
    printf("    calc NUM OP NUM       Calculate: +, -, *, /, %%\n");
    printf("\n");
    printf("  Paths:\n");
    printf("    basename PATH         Print final component of path\n");
    printf("    dirname PATH          Print directory component of path\n");
    printf("\n");
    printf("  Directories:\n");
    printf("    dirch [DIR]           Change working directory (default: /)\n");
    printf("    dirwd [MODE]          Print working directory (base/full)\n");
    printf("    dirmk DIR             Create directory\n");
    printf("    dirrm DIR             Remove directory\n");
    printf("    dirls [DIR]           List directory contents\n");
    printf("\n");
    printf("  Files:\n");
    printf("    rename OLD NEW        Rename file or directory\n");
    printf("    unlink FILE           Remove file\n");
    printf("    remove FILE           Remove file or directory\n");
    printf("    cpcat FILE            Display file contents\n");
    printf("\n");
    printf("  Links:\n");
    printf("    linkhard SRC DST      Create hard link\n");
    printf("    linksoft SRC DST      Create symbolic link\n");
    printf("    linkread LINK         Read symbolic link target\n");
    printf("    linklist FILE         List hard links to file\n");
    printf("\n");
    fflush(stdout);
}

void print_builtin_foreground()
{
    assert(token_values != NULL);
    printf("Executing builtin '%s' in foreground\n", token_values[0]);
    fflush(stdout);
}

void print_builtin_background()
{
    assert(token_values != NULL);
    printf("Executing builtin '%s' in background\n", token_values[0]);
    fflush(stdout);
}

void print_builtin_execution()
{
    if (debug_level == 0) return;
    if (background_active) print_builtin_background();
    else print_builtin_foreground();
    fflush(stdout);
}

void print_external()
{
    printf("External command '");
    int effective_count = token_count - optional_token_count;
    for (int ix = 0; ix < effective_count; ix++) printf("%s%s", token_values[ix], ix < effective_count - 1 ? " " : "");
    printf("'\n");
    fflush(stdout);
}

// Tokenizes the string, pointed to by `str`.
void set_tokens(const char* str)
{
    char buffer[MAX_TOKEN_SIZE];
    int token_ix = 0;
    int buffer_ix = 0;
    int str_ix = 0;
    int str_len = strlen(str);
    while (str_ix < str_len)
    {
        // Trim leading whitespaces
        while (str_ix < str_len && isspace(str[str_ix])) str_ix++;
        if (str_ix >= str_len) break;

        buffer_ix = 0;
        // Handle quoted strings
        if (str[str_ix] == '"')
        {
            str_ix++;
            while (str_ix < str_len && str[str_ix] != '"') buffer[buffer_ix++] = str[str_ix++];
            if (str_ix < str_len && str[str_ix] == '"') str_ix++;
            buffer[buffer_ix] = '\0';
        }
        // Handle comments
        else if (str[str_ix] == '#') break;
        // Handle regular tokens
        else
        {
            while (str_ix < str_len && !isspace(str[str_ix])) buffer[buffer_ix++] = str[str_ix++];
            buffer[buffer_ix] = '\0';
        }

        if (buffer_ix > 0)
        {
            if (debug_level > 0) printf("Token %d: '%s'\n", token_ix++, buffer);
            assert(token_values[token_count] != NULL);
            memcpy(token_values[token_count++], buffer, strlen(buffer) + 1);
        }
    }
    fflush(stdout);
}

// Sets the values for `input_redirect`, `output_redirect` and `background`, if these arguments are present and correctly formatted in the command.
void set_optional_tokens()
{
    for (int ix = token_count - 1; ix >= 0; ix--)
    {
        if (token_values[ix] == NULL) continue;
        switch (token_values[ix][0])
        {
        case '<':
            input_redirect = token_values[ix] + 1;
            optional_token_count++;
            break;
        case '>':
            output_redirect = token_values[ix] + 1;
            optional_token_count++;
            break;
        case '&':
            background_active = 1;
            optional_token_count++;
            break;
        default:
            break;
        }
    }

    if (debug_level > 0)
    {
        if (input_redirect != NULL) printf("Input redirect: '%s'\n", input_redirect);
        if (output_redirect != NULL) printf("Output redirect: '%s'\n", output_redirect);
        if (background_active) printf("Background: 1\n");
    }
    fflush(stdout);
}

void handle_debug_command()
{
    if (token_count == 1) printf("%d\n", debug_level);
    else debug_level = atoi(token_values[1]);
    exit_status = 0;
    fflush(stdout);
}

void handle_prompt_command()
{
    exit_status = (token_count > 1 && strlen(token_values[1]) > MAX_PROMPT_LEN) ? 1 : 0;
    if (exit_status == 0 && token_count > 1) strcpy(prompt_value, token_values[1]);
    if (token_count == 1) printf("%s\n", prompt_value);
    fflush(stdout);
}

void handle_status_command()
{
    printf("%d\n", exit_status);
    fflush(stdout);
}

void handle_exit_command()
{
    if (token_count > 1) exit_status = atoi(token_values[1]);
    exit_active = 1;
    fflush(stdout);
}

void handle_help_command()
{
    print_help();
    exit_status = 0;
    fflush(stdout);
}

void handle_print_command()
{
    int effective_count = token_count - optional_token_count;
    for (int ix = 1; ix < effective_count; ix++) printf("%s%s", token_values[ix], ix < token_count - 1 ? " " : "");
    exit_status = 0;
    fflush(stdout);
}

void handle_echo_command()
{
    handle_print_command();
    printf("\n");
    exit_status = 0;
    fflush(stdout);
}

void handle_len_command()
{
    int len = 0;
    int effective_count = token_count - optional_token_count;
    for (int ix = 1; ix < effective_count; ix++) len += strlen(token_values[ix]);
    printf("%d\n", len);
    exit_status = 0;
    fflush(stdout);
}

void handle_sum_command()
{
    int sum = 0;
    int effective_count = token_count - optional_token_count;
    for (int ix = 1; ix < effective_count; ix++) sum += atoi(token_values[ix]);
    printf("%d\n", sum);
    exit_status = 0;
    fflush(stdout);
}

int operation_add(int op1, int op2) { return op1 + op2; }
int operation_subtract(int op1, int op2) { return op1 - op2; }
int operation_multiply(int op1, int op2) { return op1 * op2; }
int operation_divide(int op1, int op2) { return op1 / op2; }
int operation_modulus(int op1, int op2) { return op1 % op2; }

typedef struct
{
    const char* name;
    int (*handler)(int, int);
} Operation;

Operation builtin_operations[] = {
    {"+", operation_add},
    {"-", operation_subtract},
    {"*", operation_multiply},
    {"/", operation_divide},
    {"%", operation_modulus},
    {NULL, NULL}
};

void handle_calc_command()
{
    if (token_count < 4)
    {
        fprintf(stderr, "calc: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    char* operator = token_values[2];
    int operand1 = atoi(token_values[1]);
    int operand2 = atoi(token_values[3]);
    int result;
    int found = 0;
    for (int operator_ix = 0; builtin_operations[operator_ix].name != NULL; operator_ix++)
    {
        if (!strcmp(operator, builtin_operations[operator_ix].name))
        {
            result = builtin_operations[operator_ix].handler(operand1, operand2);
            printf("%d\n", result);
            exit_status = 0;
            found = 1;
            break;
        }
    }
    if (!found)
    {
        fprintf(stderr, "calc: Unknown operator '%s'\n", operator);
        exit_status = EINVAL;
    }
    exit_status = 0;
    fflush(stdout);
}

void handle_basename_command()
{
    if (token_count < 2)
    {
        exit_status = 1;
        return;
    }

    const char* directory = token_values[1];
    const char* basename = strrchr(directory, '/');

    if (basename == NULL) printf("%s\n", directory);
    else printf("%s\n", basename + 1);
    exit_status = 0;
    fflush(stdout);
}

void handle_dirname_command()
{
    if (token_count < 2)
    {
        exit_status = 1;
        return;
    }

    const char* directory = token_values[1];
    const char* last_slash = strrchr(directory, '/');

    if (last_slash == NULL)           { printf(".\n"); }
    else if (last_slash == directory) { printf("/\n"); }
    else                              { printf("%.*s\n", (int)(last_slash - directory), directory); }
    exit_status = 0;
    fflush(stdout);
}

void handle_dirch_command()
{
    const char* directory = token_count < 2 ? "/" : token_values[1];

    if (chdir(directory) != 0)
    {
        int err = errno;
        perror("dirch");
        exit_status = err;
        return;
    }

    if (getcwd(working_directory, MAX_DIRECTORY_LEN + 1) == NULL)
    {
        int err = errno;
        perror("dirch");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_dirwd_command()
{
    const char* mode = token_count > 1 ? token_values[1] : "base";
    if (strcmp(mode, "base") == 0)
    {
        if (strcmp(working_directory, "/") == 0) printf("/\n");
        else
        {
            const char* basename = strrchr(working_directory, '/');
            printf("%s\n", basename ? basename + 1 : working_directory);
        }
    }
    else if (strcmp(mode, "full") == 0) printf("%s\n", working_directory);
    else
    {
        fprintf(stderr, "dirwd: Unknown mode '%s'\n", mode);
        exit_status = EINVAL;
        return;
    }
    exit_status = 0;
    fflush(stdout);
}

void handle_dirmk_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "dirmk: Missing directory name\n");
        exit_status = EINVAL;
        return;
    }

    if (mkdir(token_values[1], S_IRWXU | S_IRWXG | S_IRWXO) != 0)
    {
        int err = errno;
        perror("dirmk");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_dirrm_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "dirrm: Missing directory name\n");
        exit_status = EINVAL;
        return;
    }

    if (rmdir(token_values[1]) != 0)
    {
        int err = errno;
        perror("dirrm");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_dirls_command()
{
    const char* directory_ch = token_count > 1 ? token_values[1] : working_directory;
    DIR* directory_dr = opendir(directory_ch);
    if (directory_dr == NULL)
    {
        int err = errno;
        perror("dirls");
        exit_status = err;
        return;
    }

    struct dirent* entry;
    int first_content = 1;
    while ((entry = readdir(directory_dr)) != NULL)
    {
        printf("%s%s", first_content ? "" : "  ", entry->d_name);
        first_content = 0;
    }
    printf("\n");

    closedir(directory_dr);
    exit_status = 0;
    fflush(stdout);
}

void handle_rename_command()
{
    if (token_count < 3)
    {
        fprintf(stderr, "rename: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    if (rename(token_values[1], token_values[2]) != 0)
    {
        int err = errno;
        perror("rename");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_unlink_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "unlink: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    if (unlink(token_values[1]) != 0)
    {
        int err = errno;
        perror("unlink");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_remove_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "remove: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    if (remove(token_values[1]) != 0)
    {
        int err = errno;
        perror("remove");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_linkhard_command()
{
    if (token_count < 3)
    {
        fprintf(stderr, "linkhard: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    if (link(token_values[1], token_values[2]) != 0)
    {
        int err = errno;
        perror("linkhard");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_linksoft_command()
{
    if (token_count < 3)
    {
        fprintf(stderr, "linksoft: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    if (symlink(token_values[1], token_values[2]) != 0)
    {
        int err = errno;
        perror("linksoft");
        exit_status = err;
        return;
    }

    exit_status = 0;
    fflush(stdout);
}

void handle_linkread_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "linkread: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    char buffer[MAX_TOKEN_SIZE];
    ssize_t len = readlink(token_values[1], buffer, sizeof(buffer) - 1);

    if (len == -1)
    {
        int err = errno;
        perror("linkread");
        exit_status = err;
        return;
    }

    buffer[len] = '\0';
    printf("%s\n", buffer);
    exit_status = 0;
    fflush(stdout);
}

void handle_linklist_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "linklist: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    struct stat file_stat;
    if (stat(token_values[1], &file_stat) != 0)
    {
        int err = errno;
        perror("linklist");
        exit_status = err;
        return;
    }

    DIR* directory_dr = opendir(working_directory);
    if (directory_dr == NULL)
    {
        int err = errno;
        perror("linklist");
        exit_status = err;
        return;
    }

    struct dirent* entry;
    struct stat entry_stat;
    int first_content = 1;
    while ((entry = readdir(directory_dr)) != NULL)
    {
        if (stat(entry->d_name, &entry_stat) == 0)
        {
            if (entry_stat.st_ino == file_stat.st_ino)
            {
                printf("%s%s", first_content ? "" : "  ", entry->d_name);
                first_content = 0;
            }
        }
    }
    printf("\n");

    closedir(directory_dr);
    exit_status = 0;
    fflush(stdout);
}

void handle_cpcat_command()
{
    if (token_count < 2)
    {
        fprintf(stderr, "cpcat: Insufficient amount of parameters\n");
        exit_status = EINVAL;
        return;
    }

    int in = -1;
    int out = -1;
    char buffer[MAX_BUFFER];
    ssize_t bytes_read;

    // Open input file
    if (strcmp(token_values[1], "-") == 0)
    {
        in = STDIN_FILENO;
    }
    else
    {
        in = open(token_values[1], O_RDONLY);
        if (in < 0)
        {
            int err = errno;
            perror("cpcat");
            exit_status = err;
            return;
        }
    }

    // Open output file if specified
    if (token_count >= 3)
    {
        out = open(token_values[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out < 0)
        {
            int err = errno;
            perror("cpcat");
            if (in != STDIN_FILENO) close(in);
            exit_status = err;
            return;
        }
    }
    else
    {
        out = STDOUT_FILENO;
    }

    // Copy contents
    while ((bytes_read = read(in, buffer, MAX_BUFFER)) > 0)
    {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read)
        {
            ssize_t result = write(out, buffer + bytes_written, bytes_read - bytes_written);
            if (result < 0)
            {
                int err = errno;
                perror("cpcat");
                if (in != STDIN_FILENO) close(in);
                if (out != STDOUT_FILENO) close(out);
                exit_status = err;
                return;
            }
            bytes_written += result;
        }
    }

    // Handle read errors
    if (bytes_read < 0)
    {
        int err = errno;
        perror("cpcat");
        if (in != STDIN_FILENO) close(in);
        if (out != STDOUT_FILENO) close(out);
        exit_status = err;
        return;
    }

    // Close files
    if (in != STDIN_FILENO) close(in);
    if (out != STDOUT_FILENO) close(out);

    exit_status = 0;
    fflush(stdout);
}

void handle_pid_command() {}
void handle_ppid_command() {}
void handle_uid_command() {}
void handle_euid_command() {}
void handle_gid_command() {}
void handle_egid_command() {}
void handle_sysinfo_command() {}

void handle_pot_command() {}
void handle_pids_command() {}
void handle_pinfo_command() {}
void handle_waitone_command() {}
void handle_waitall_command() {}

/* -------------------------------------------------------[ Execution handler ]------------------------------------------------------- */

typedef struct
{
    const char* name;
    void (*handler)(void);
} Command;

Command builtin_commands[] = {
    {"debug", handle_debug_command},
    {"prompt", handle_prompt_command},
    {"status", handle_status_command},
    {"exit", handle_exit_command},
    {"help", handle_help_command},
    {"print", handle_print_command},
    {"echo", handle_echo_command},
    {"len", handle_len_command},
    {"sum", handle_sum_command},
    {"calc", handle_calc_command},
    {"basename", handle_basename_command},
    {"dirname", handle_dirname_command},
    {"dirch", handle_dirch_command},
    {"dirwd", handle_dirwd_command},
    {"dirmk", handle_dirmk_command},
    {"dirrm", handle_dirrm_command},
    {"dirls", handle_dirls_command},
    {"rename", handle_rename_command},
    {"unlink", handle_unlink_command},
    {"remove", handle_remove_command},
    {"linkhard", handle_linkhard_command},
    {"linksoft", handle_linksoft_command},
    {"linkread", handle_linkread_command},
    {"linklist", handle_linklist_command},
    {"cpcat", handle_cpcat_command},
    {"pid", handle_pid_command},
    {"ppid", handle_ppid_command},
    {"uid", handle_uid_command},
    {"gid", handle_gid_command},
    {"egid", handle_egid_command},
    {"sysinfo", handle_sysinfo_command},
    {"pot", handle_pot_command},
    {"pids", handle_pids_command},
    {"pinfo", handle_pinfo_command},
    {"waitone", handle_waitone_command},
    {"waitall", handle_waitall_command},
    {NULL, NULL},
};

void execute_command()
{
    char* command = token_values[0];
    for (int command_ix = 0; builtin_commands[command_ix].name != NULL; command_ix++)
    {
        if (!strcmp(command, builtin_commands[command_ix].name))
        {
            print_builtin_execution();
            builtin_commands[command_ix].handler();
            return;
        }
    }
    if (debug_level) print_external();
}

/* --------------------------------------------------------------[ Main ]-------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    // Set default prompt title
    prompt_value = malloc(sizeof(char) * (MAX_PROMPT_LEN + 1));
    strcpy(prompt_value, "mysh");

    // Set default working directory
    working_directory = malloc(sizeof(char) * (MAX_DIRECTORY_LEN + 1));
    strcpy(working_directory, "/");

    init_tokens();
    char buffer[MAX_INPUT_SIZE];
    while (1)
    {
        // Set default values
        token_count = 0;
        optional_token_count = 0;
        input_redirect = NULL;
        output_redirect = NULL;
        background_active = 0;

        if (isatty(STDIN_FILENO))
        {
            printf("%s> ", prompt_value);
            fflush(stdout);
        }

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;

        int str_len = strlen(buffer);
        if (str_len > 0 && buffer[str_len - 1] == '\n') buffer[--str_len] = '\0';
        if (debug_level > 0) printf("Input line: '%s'\n", buffer);

        set_tokens(buffer);
        if (token_count == 0) continue;
        set_optional_tokens();
        execute_command();

        if (exit_active) break;
    }

    free(working_directory);
    free(prompt_value);
    deinit_tokens();
    return exit_status;
}