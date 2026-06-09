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

/** main loop */

char* sq_read(void);
void sq_parse(char* line, cmd_t* command); // change to return int later for error handling
int sq_execute(cmd_t* command);

/** helpers */
void sq_reset(cmd_t* command);
void sq_dup(int fd, int replace);

/** builtin commands */

int sq_cd(char* args[]);
int sq_exit(char* args[]);

int NUM_BUILTINS = 2;
char* BUILTINS[] = {
    "cd",
    "exit",
};

int (*BUILTIN_FUNCS[]) (char* []) = {
    &sq_cd,
    &sq_exit,
};