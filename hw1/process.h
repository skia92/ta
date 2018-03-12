#pragma once

#include "tokenizer.h"

/* Struct process */
struct process {
    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    char *command;
    char **args;
    int args_length; // including command arg
    struct process *next;
    struct process *prev;
    int redirect;
    unsigned int background;
    char *file_arg;
};

/* Path resolution */
char *resolve_path(char *command);

/* Create a valid process */
struct process *create_process(struct tokens *tokens);

/* Run a process */
void run_process(struct process *proc);

/* Destroy a process */
void destroy_process(struct process *proc);
