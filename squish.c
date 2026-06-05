#include "squish.h"

int main(int argc, char* argv[]) {
    char* line = NULL;
    char** args = NULL;
    int status = 1;
    char* cwd = malloc(BUFSIZ);

    do {
        cwd = getcwd(cwd, BUFSIZ);
        printf("%s%s -~> %s", GREEN, cwd, RESET);
        
        line = sq_read();
        args = sq_split(line);
        status = sq_execute(args);
        
        free(line);
        free(args);
    } while (status);
    
    free(cwd);
}

char* sq_read(void) {
    char* line = NULL;
    size_t bufsize;

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
 * TODO: current error handling leaks memory! 
 * eg. exiting with error from the split stage leaves *line unfreed
 */

char** sq_split(char* line) {
    size_t bufsize = BUFSIZ;
    char* delims = " \t\r\n\a";
    char** tokens = malloc(BUFSIZ);
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
            char** new = realloc(tokens, bufsize);
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
    pid_t pid;

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
    pid = fork();
    if (pid == 0) { // child process
        if (execvp(cmd, args) == -1) {
            perror("failed to execute command");
            exit(EXIT_FAILURE); // terminates child - does not quit main shell
        }
    } else if (pid > 0) { // parent process
        waitpid(pid, NULL, 0); // TODO: unblock for exit only - no job control yet
    } else { // forking error
        perror("failed to fork in command execution");
    }
    return 1; // command done running. continue
}

// https://man7.org/linux/man-pages/man1/cd.1p.html
int sq_cd(char* args[]) {
    char* path;

    if (args[1] == NULL && getenv("HOME") == NULL) { 
        // 1. if no directory given and HOME env variable is not defined
        return 1; 
    } else if (args[1] == NULL) { 
        // 2. else if no directory given, change directory to HOME
        path = getenv("HOME"); 
    } else {
        // TODO: support options and -
        path = args[1];
    }

    if (chdir(path) == -1) { 
        perror("failed to change directory"); 
    }

    return 1;
}

int sq_exit(char* args[]) {
    printf("%sbye squish!%s\n", GREEN, RESET);
    return 0;
}