#pragma once

#include "tokenizer.h"

/* Struct process */
struct process {
    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    char *command;
    char **args;
    struct process *next;
    struct process *prev;
    int redirect;
    int bg_proc;
    char *file_arg;
};

/* Path resolution */
char *resolve_path(char *command);

/* Create a valid process */
struct process *create_process(struct tokens *tokens);

/* Run a process */
void run_process(struct process *proc);
