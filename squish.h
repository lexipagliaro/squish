#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#define GREEN "\e[0;92m"
#define RESET "\e[0m"

/** main loop */

char* sq_read(void);
char** sq_split(char* line, char** output, char** input);
int sq_execute(char* args[], char* output, char* input);

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