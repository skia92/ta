#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "process.h"
#include "tokenizer.h"

#define INPUT 0
#define OUTPUT 1
#define TRUE 1
#define FALSE 0

char *resolve_path(char *command) {
    char *path = NULL;
    char *ptr;
    char *env_path = getenv("PATH");
    char *temp_path = (char *) malloc(sizeof(char) * strlen(env_path));

    strcpy(temp_path, env_path);
    ptr = strtok(temp_path, ":");

    while (ptr != NULL) {
        path = (char *) malloc(sizeof(char) * (strlen(command) + strlen(ptr)));

        strcpy(path, ptr);
        strcat(path, "/");
        strcat(path, command);

        if (access(path, F_OK) == 0)
            return path;

        ptr = strtok(NULL, ":");
    }

    free(path);
    free(temp_path);

    return NULL;
}

struct process *create_process(struct tokens *tokens) {
    char *command = tokens_get_token(tokens, 0);
    char **args = tokens->tokens;
    int length = tokens_get_length(tokens);
    unsigned int count = 0;
    struct process *proc = NULL;

    if (access(command, F_OK) < 0) {
        command = resolve_path(command);
    }

    if (command != NULL) {
        proc = (struct process *) malloc(sizeof(struct process));
        proc->next = NULL;
        proc->prev = NULL;
        proc->bg_proc = 0;
        proc->args_length = 0;
        proc->file_arg = NULL;
        proc->redirect = -1;

        proc->command = (char *) malloc(sizeof(char) * strlen(command));
        strcpy(proc->command, command);

        for (int i = 0; i < length; i++) {

            if (strcmp("<", args[i]) == 0) {
                proc->redirect = INPUT;
                proc->file_arg = args[i + 1];
                break;
            } else if (strcmp(">", args[i]) == 0) {
                proc->redirect = OUTPUT;
                proc->file_arg = args[i + 1];
                break;
            } else if (strcmp("&", args[i]) == 0) {
                proc->bg_proc = TRUE;
            }

            count++;
        }

        proc->args_length = count;
        proc->args = (char **) malloc(sizeof(char *) * count);
        for (int i = 0; i < count; i++) {
            proc->args[i] = (char *) malloc(sizeof(char) * strlen(args[i]));
            strcpy(proc->args[i], args[i]);
        }
    }

    return proc;
}

void run_process(struct process *proc) {
    int fd;

    /* Setting pgid as itself */
    setpgid(0, 0);

    /* Setting foreground as current child process */
    //tcsetpgrp(STDIN_FILENO, getpid());

    switch(proc->redirect) {
        case INPUT:
            fd = open(proc->file_arg, O_RDONLY);
            dup2(fd, STDIN_FILENO);
            break;
        case OUTPUT:
            fd = open(proc->file_arg, O_WRONLY | O_CREAT,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(fd, STDOUT_FILENO);
            break;
        default:
            break;
    }

    for (int i = 0; i < proc->args_length; i++) {
        printf("args[%d]: %s\n", i, proc->args[i]);
    }

    execv(proc->command, proc->args);

    return;
}

void detroy_process(struct process *proc) {

}
