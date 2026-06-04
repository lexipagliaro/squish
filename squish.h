#include <stdio.h>
#include <stdlib.h>

#define GREEN "\e[0;92m"
#define RESET "\e[0m"

char* read(void);

char** split(void);

int execute(char* args[]);