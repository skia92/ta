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

#include "process.h"

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
    char *target = tokens_get_token(tokens, 1);
    int result;

    if (target == NULL) {
        printf("You should put at least one argument for this cmd.\n");
        return -1;
    }

    result = chdir(target);

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

void signal_handler(int signo) {
    int status;

    if (signo == SIGCHLD) {
        /* background process 작업이 다 끝나면 update */
        waitpid(-1, &status, WUNTRACED);
    }

    return;
}

/*
 * https://www.gnu.org/software/libc/manual/html_node/Foreground-and-Background.html
 */
void put_process_in_foreground(pid_t ch_pid, int cont) {
    int status;

    /* Put a process in foreground */
    tcsetpgrp(STDIN_FILENO, ch_pid);

    if (cont) {
        // if a process has to be continued,
    }

    waitpid(-1, &status, WUNTRACED);

    /* Make Terminal back to foreground */
    tcsetpgrp(STDIN_FILENO, shell_pgid);

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
    } else if (tokens_get_length(tokens) > 0) {
        /* If tokens variable is valid */
        pid_t ch_pid;
        struct process *proc = create_process(tokens);

        /* If there is no files  */
        if (proc == NULL) { 
            printf("%s: command not found\n", tokens_get_token(tokens, 0));
        } else {
            /* signal handling */
            signal(SIGTTOU, SIG_IGN);
            signal(SIGCHLD, signal_handler);

            ch_pid = fork();

            if (ch_pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
          
            if (ch_pid == 0) {
                /* Child process */
                proc->ppid = getppid();
                run_process(proc);

            } else {
                /* Parent process */

                /* Wait for termination of child process */
                if (!proc->background) {
                    put_process_in_foreground(ch_pid, 0);
                }
                destroy_process(proc);
            }
            

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
