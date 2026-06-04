#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GREEN "\e[0;92m"
#define RESET "\e[0m"

char* read(void);

char** split(char* line);

int execute(char* args[]);