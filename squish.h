#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#define GREEN "\e[0;92m"
#define RESET "\e[0m"

typedef struct cmd_t {
    char** args;        // args[0] = command name, args[1:] = NULL terminated argv
    char* redirect_t;   // > file
    char* redirect_a;   // >> file
    char* redirect_i;   // < file
    struct cmd_t* pipe; // next command in pipeline
} cmd_t;

typedef enum status_t {
    EXIT,
    CRASH,
    RETRY,
    OK,
    NOT_FOUND
} status_t;

/** main loop */

char* sq_read(status_t* status);
status_t sq_parse(char* line, cmd_t* command); // change to return int later for error handling
status_t sq_execute(cmd_t* command);

/** helpers */
void sq_reset(char* line, cmd_t* command);
void sq_dup(int fd, int replace);
void sq_redirection(cmd_t* command);
void sq_exec(cmd_t* command);

/** builtin commands */

status_t sq_cd(char* args[]);
status_t sq_exit(char* args[]);

int NUM_BUILTINS = 2;
char* BUILTINS[] = {
    "cd",
    "exit",
};

status_t (*BUILTIN_FUNCS[]) (char* []) = {
    &sq_cd,
    &sq_exit,
};