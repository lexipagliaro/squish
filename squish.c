#include "squish.h"

/* 
    TODO: add input/output redirection 
        REMAINING: >>, <, <<, <<<

    FIXME: refactor error handling from scattered exits -> centralized cleanup
    for graceful exit without memory leaks of regions whose pointers are out 
    of scope
*/

int main(int argc, char* argv[]) {
    char* line = NULL;
    char** args = NULL;
    int status = 1;
    char* cwd = malloc(BUFSIZ);
    char* output = NULL;
    char* input = NULL;

    do {
        cwd = getcwd(cwd, BUFSIZ);
        printf("%s%s -~> %s", GREEN, cwd, RESET);
        
        line = sq_read();
        args = sq_split(line, &output, &input);
        status = sq_execute(args, output, input);

        // reset input/output back to stdin/out for next command
        output = NULL;
        input = NULL;
        
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

char** sq_split(char* line, char** output, char** input) {
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
        if (strcmp(token, ">") == 0) {
            *output = strtok(NULL, delims); // FIXME: once error handling is sorted, cease command parsing/execution here if *output == NULL
            break;
        }

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

int sq_execute(char* args[], char* output, char* input) {
    char* cmd = args[0];
    pid_t pid;

    if (cmd == NULL) { // user gave no command
        return 1; // do nothing and continue shell prompt
    }

    // if builtin, call the function
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(cmd, BUILTINS[i]) == 0) {
            return (*BUILTIN_FUNCS[i])(args);
        }
    }

    // else search for program in path directories and run in new process
    pid = fork();
    if (pid == 0) { // child process
        if (output) { // output redirection >
            int fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("failed to open file for output redirection");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (execvp(cmd, args) == -1) {
            perror("failed to execute command");
            exit(EXIT_FAILURE); // terminates child - does not quit main shell
        }
    } else if (pid > 0) { // parent process
        waitpid(pid, NULL, 0);
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