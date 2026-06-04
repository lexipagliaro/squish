#include "squish.h"

int main(int argc, char* argv[]) {
    char* line = NULL;
    char** args = NULL;
    int status = 0;

    do {
        printf("%s-~> %s", GREEN, RESET);
        line = read();
        args = split();
        status = execute(args);
        
        free(line);
        free(args);
    } while (status);
}

char* read(void) {
    char* line = NULL;
    size_t size = 0;

    /* on failure to read, free line buffer and end shell process */
    if (getline(&line, &size, stdin) == -1) {
        if (feof(stdin)) { /* hit end of script file or user typed ctrl+d */
            free(line); 
            printf("%sbye squish!%s\n", GREEN, RESET);
            exit(EXIT_SUCCESS); 
        } else { /* some unexpected error */
            free(line);
            perror("Failed to read command line");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

char** split(void) {
    return NULL;
}

int execute(char* args[]) {
    return 0;
}