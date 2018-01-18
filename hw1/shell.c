#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Extern variable */
extern int errno;

/* Constant Variable */
#define PATH_MAX 512

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "show the current working directory"},
  {cmd_cd, "cd", "move the currennt working dir to the new one"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
    printf("Bye.\n");
    exit(0);
}

/* Prints the current working directory to standard output */
int cmd_pwd(unused struct tokens *tokens) {
    char *cwd = (char *) malloc(sizeof(char) * PATH_MAX);

    if ( (getcwd(cwd, PATH_MAX) == NULL) && (errno == ERANGE)) {
        printf("The path length is too long. Please allocate your memory more.\n");
        return -1;
    }
    
    printf("%s\n", cwd);

    return 1;
}

/* Takes one argumnet, a directory path, and changes the current working directory 
 * to that directory */
int cmd_cd(unused struct tokens *tokens) {
    char *tar_dir = tokens_get_token(tokens, 1);
    int result;

    if (tar_dir == NULL) {
        printf("You should put at least one argument for this cmd.\n");
        return -1;
    }

    result = chdir(tar_dir);

    if (result < 0) {
        switch(errno) {
            case ENOENT:
                printf("No such file or directory.\n");
                break;
            case ENOTDIR:
                printf("Not a directory.\n");
                break;
            default:
                printf("Error: %d\n", errno);
                break;
        }

        return -1;
    }

    return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

char *find(char *file_name, char *path) {
   char *full_path = NULL;
   char *ptr;
   char *temp_path = (char *) malloc(sizeof(char) * strlen(path) + 1);
   unsigned int length = strlen(file_name);

   strncpy(temp_path, path, strlen(path) + 1);
   ptr = strtok(temp_path, ":");
   while (ptr != NULL) {
       full_path = (char *) malloc(sizeof(char) * (length + 1 + strlen(ptr) + 1));

       strcpy(full_path, ptr);
       strcat(full_path, "/");
       strcat(full_path, file_name);
 
       if (access(full_path, F_OK) == 0)
           break;

       free(full_path);
       full_path = NULL;
       ptr = strtok(NULL, ":");
   }

   return full_path;
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
        pid_t ch_pid, w;
        int status;
        char *path = getenv("PATH");
        char *cmd_name = tokens_get_token(tokens, 0);
        char *part;
        char *file_arg;
        
        char **ch_arg = NULL;
        int ch_arg_len = 0;

        int pipefd[2];
        const int MODE_INPUT = 0,
            MODE_OUTPUT = 1,
            MODE_NORMAL = 2;
        int mode = MODE_NORMAL;
        char buf;

        part = strchr(cmd_name, '/');

        if (part == NULL)
            cmd_name = find(cmd_name, path);

        if (cmd_name == NULL)
            cmd_name = tokens_get_token(tokens, 0);
        
        if (pipe(pipefd) == -1) {
            printf("Error in pipe(): %d\n", errno);
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < tokens_get_length(tokens); i++) {
            char *temp = tokens_get_token(tokens, i);
            size_t temp_len = strlen(temp);

            if (i != 0 && (strcmp("<", temp) == 0)) {
                mode = MODE_INPUT;
                file_arg = tokens_get_token(tokens, i + 1);
                break;
            } else if (i != 0 && (strcmp(">", temp) == 0)) {
                mode = MODE_OUTPUT;
                file_arg = tokens_get_token(tokens, i + 1);
                break;
            } else if (strcmp(">", temp) == 0 ||
                strcmp("<", temp) == 0) {
                printf("Invalid argument.\n");
                // Invalid argument
                exit(2);
            } else {
                ch_arg_len++;
                ch_arg = (char **) realloc(ch_arg, sizeof(char *) * (ch_arg_len + 1));
                ch_arg[ch_arg_len - 1] = (char *) malloc(sizeof(char) * temp_len + 1);

                strcpy(ch_arg[ch_arg_len - 1], temp);
            }
        }

        ch_arg[ch_arg_len] = NULL;

        ch_pid = fork();
        if (ch_pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
      
        if (ch_pid == 0) {
            /* Child process */
            int result;
            const int MAX_NAME_SIZE = 128;
            char *file_name = (char *) malloc(sizeof(char) * MAX_NAME_SIZE);
            int input_fd;

            if (mode == MODE_INPUT) {
                close(pipefd[1]);

                read(pipefd[0], file_name, MAX_NAME_SIZE);

                if ((input_fd = open(file_name, O_RDONLY)) < 0) {
                    printf("Error in input file: %d\n", errno);
                    exit(EXIT_FAILURE);
                }

                dup2(input_fd, STDIN_FILENO);

            } else if (mode == MODE_OUTPUT) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
            }

            if (ch_arg_len > 1) {
                /* Using an array of argument */
                result = execv(cmd_name, ch_arg);
            } else {
                /* Only a command. No arguments */
                result = execl(cmd_name, cmd_name, NULL);
            }

            /* Check exec() function error */
            if (result < 0) {
                switch(errno) {
                    case ENOENT:
                        printf("%s: command not found\n", cmd_name);
                        break;
                    default:
                        printf("Error in child process: %d\n", errno);
                }

                exit(errno);
            }
        } else {
            /* Parent process */

            /* If a symbol is '<' */
            if (mode == MODE_INPUT) {
                close(pipefd[0]);
                write(pipefd[1], file_arg, strlen(file_arg));
                close(pipefd[1]);
            }

            /* Wait for termination of child process */
            while ((w = waitpid(-1, &status, 0)) != ch_pid) {
                if (w < 0 && errno == ECHILD) {
                    printf("Error in waitpid: %d\n", errno);
                    break;
                }
                errno = 0;
            }

            /* If a symbol is '>' */
            if (mode == MODE_OUTPUT) {
                int output_fd;
                close(pipefd[1]);

                if ((output_fd = open(file_arg, O_WRONLY | O_CREAT, 
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
                    printf("Error in open(): %d\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                while (read(pipefd[0], &buf, 1) > 0) {
                    write(output_fd, &buf, 1);
                }
                
                close(pipefd[0]);
            }
            
            /* Flushing all STD stream */
            fsync(STDIN_FILENO);
            fsync(STDOUT_FILENO);

            /* Destroy all buffers */
            for (int i = 0; i < ch_arg_len; i++) {
                free(ch_arg[i]);
            }

            free(ch_arg);

        }
        //fprintf(stdout, "This shell doesn't know how to run programs.\n");
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
