#include "squish.h"

int main(int argc, char* argv[]) {
    char* line = NULL;
    char** args = NULL;
    int status = 1;

    do {
        printf("%s-~> %s", GREEN, RESET);
        line = sq_read();
        args = sq_split(line);
        status = sq_execute(args);
        
        free(line);
        free(args);
    } while (status);
}

char* sq_read(void) {
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
            perror("failed to read command line");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

/** 
 * later debugging note: exiting from split and execute leaks memory allocated
 * in earlier functions/referenced in main! fix!
 */

char** sq_split(char* line) {
    int bufsize = 64;
    char* delims = " \t\r\n\a";
    char** tokens = malloc(bufsize * sizeof(char*));
    char* token;
    int i = 0;

    if (tokens == NULL) {
        perror("failed to allocate memory for command line parsing");
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
                perror("failed to reallocate memory for command line parsing");
                exit(EXIT_FAILURE);
            }
            tokens = new;
        }

        token = strtok(NULL, delims); // NULL -> continue on same str
    }
    tokens[i] = NULL;

    return tokens;
}

int sq_execute(char* args[]) {
    char* cmd = args[0];
    if (cmd == NULL) { // user gave no command
        return 1; // do nothing and continue shell prompt
    };

    // if builtin, call the function
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(cmd, BUILTINS[i]) == 0) {
            return (*BUILTIN_FUNCS[i])(args);
        }
    }

    // else search for program in path directories and run in new process
    pid_t pid = fork();
    if (pid == 0) { // child process
        if (execvp(cmd, args) == -1) {
            perror("error executing command");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) { // parent process
        waitpid(pid, NULL, 0); // unblock for exit only - no job control yet
    } else { // forking error
        perror("forking error in command execution");
        exit(EXIT_FAILURE);
    }
    return 1; // command done running. continue
}

// TODO
int sq_cd(char* args[]) {
    return 1;
}

// TODO
int sq_echo(char* args[]) {
    return 1;
}

int sq_exit(char* args[]) {
    printf("%sbye squish!%s\n", GREEN, RESET);
    return 0;
}