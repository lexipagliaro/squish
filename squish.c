#include "squish.h"

int main(int argc, char* argv[]) {
    char* line = NULL;
    char** args = NULL;
    int status = 1;

    do {
        printf("%s-~> %s", GREEN, RESET);
        line = read();
        args = split(line);
        status = execute(args);
        
        free(line);
        free(args);
    } while (status);
}

char* read(void) {
    char* line = NULL;
    size_t bufsize = 0;

    /* on failure to read, free line buffer and end shell process */
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) { /* hit end of script file or user typed ctrl+d */
            free(line); 
            printf("%sbye squish!%s\n", GREEN, RESET);
            exit(EXIT_SUCCESS); 
        } else { /* some unexpected error */
            free(line);
            fprintf(stderr, "%sfailed to read command line: %s%s\n", GREEN, strerror(errno), RESET);
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

/** 
 * later debugging note: exiting from split and execute leaks memory allocated
 * in earlier functions/referenced in main! fix!
 */

char** split(char* line) {
    int bufsize = 64;
    char* delims = " \t\r\n\a";
    char** tokens = malloc(bufsize * sizeof(char*));
    char* token;
    int i = 0;

    if (tokens == NULL) {
        fprintf(stderr, "%sfailed to allocate memory for command line parsing: %s%s\n", GREEN, strerror(errno), RESET);
        exit(EXIT_FAILURE);
    }

    // parse and terminate token list with NULL pointer
    token = strtok(line, delims);
    while (token != NULL) {
        tokens[i] = token;

        i++;
        if (i == bufsize) { // filled buffer, reallocate with more space
            bufsize *= 2;
            char** new = realloc(tokens, bufsize * sizeof(char*));
            if (new == NULL) {
                free(tokens);
                fprintf(stderr, "%sfailed to reallocate memory for command line parsing: %s%s\n", GREEN, strerror(errno), RESET);
                exit(EXIT_FAILURE);
            }
            tokens = new;
        }

        token = strtok(NULL, delims); // NULL -> continue on same str
    }
    tokens[i] = NULL;

    return tokens;
}

int execute(char* args[]) {
    return 0;
}