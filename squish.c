#include "squish.h"

/* 
    TODO: add input/output redirection 
        REMAINING: << (here-documents), <<< (here-strings)

    FIXME: refactor error handling from scattered exits -> centralized cleanup
    for graceful exit without memory leaks of regions whose pointers are out 
    of scope
*/

int main(int argc, char* argv[]) {
    char* line = NULL;
    cmd_t command = { NULL };
    int status = 1;
    char* cwd = malloc(BUFSIZ);

    do {
        cwd = getcwd(cwd, BUFSIZ);
        printf("%s%s -~> %s", GREEN, cwd, RESET);
        
        line = sq_read();
        sq_parse(line, &command);
        status = sq_execute(&command);

        free(line);
        free(command.args);
        memset(&command, 0, sizeof(command)); // reset dangling pointer command fields
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

void sq_parse(char* line, cmd_t* command) {
    size_t bufsize = BUFSIZ;
    char* delims = " \t\r\n\a";
    char** args = malloc(BUFSIZ);
    char* token;
    int i = 0;

    if (args == NULL) {
        perror("failed to allocate memory for command line parsing");
        exit(EXIT_FAILURE);
    }

    // parse and terminate token list with NULL pointer
    token = strtok(line, delims);
    while (token != NULL) {
        // FIXME: once error handling is sorted, quit command parsing/execution here if no token after redirection symbol
        if (strcmp(token, ">") == 0) { 
            command->redirect_t = strtok(NULL, delims); 
            break;
        } else if (strcmp(token, ">>") == 0) {
            command->redirect_a = strtok(NULL, delims); 
            break;
        } else if (strcmp(token, "<") == 0) {
            command->redirect_i = strtok(NULL, delims);
            break;
        }

        args[i] = token;

        i++;
        if (i == bufsize) { // filled buffer, reallocate with more space
            bufsize *= 2;
            char** new = realloc(args, bufsize);
            if (new == NULL) {
                free(args);
                perror("failed to reallocate memory for command line parsing");
                exit(EXIT_FAILURE);
            }
            args = new;
        }

        token = strtok(NULL, delims); // NULL -> continue on same str
    }
    args[i] = NULL;

    command->args = args;
}

void redirect(int fd, int replace) {
    if (fd == -1) {
        perror("failed to open file for redirection");
        exit(EXIT_FAILURE);
    }
    dup2(fd, replace);
    close(fd);
}

int sq_execute(cmd_t* command) {
    char* cmd = command->args[0];
    pid_t pid;

    if (cmd == NULL) { // user gave no command
        return 1; // do nothing and continue shell prompt
    }

    // if builtin, call the function
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(cmd, BUILTINS[i]) == 0) {
            return (*BUILTIN_FUNCS[i])(command->args);
        }
    }

    // else search for program in path directories and run in new process
    pid = fork();
    if (pid == 0) { // child process
        int fd;
        // handle redirection
        if  (command->redirect_t) { // >
            fd = open(command->redirect_t, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            redirect(fd, STDOUT_FILENO);
        } else if (command->redirect_a) { // >>
            fd = open(command->redirect_a, O_WRONLY | O_CREAT | O_APPEND, 0644);
            redirect(fd, STDOUT_FILENO);
        } else if (command->redirect_i) { // <
            fd = open(command->redirect_i, O_RDONLY);
            redirect(fd, STDIN_FILENO);
        }

        if (execvp(cmd, command->args) == -1) {
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