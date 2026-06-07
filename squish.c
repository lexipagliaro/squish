#include "squish.h"

/* 
    TODO: add << (here-documents), <<< (here-strings)

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
        }
        if (strcmp(token, ">>") == 0) {
            command->redirect_a = strtok(NULL, delims); 
            break;
        }
        if (strcmp(token, "<") == 0) {
            command->redirect_i = strtok(NULL, delims);
            break;
        }

        if (strcmp(token, "|") == 0) {
            // finish argv for pipe input command
            args[i] = NULL;
            command->args = args;

            // reset for pipe output command
            bufsize = BUFSIZ;
            args = malloc(BUFSIZ);
            i = 0;

            // establish pipe and proceed with next command
            cmd_t* next = malloc(sizeof(cmd_t));
            memset(next, 0, sizeof(*next));
            command->pipe = next;
            command = next;

            token = strtok(NULL, delims);
            continue;
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
    char* cmd;
    pid_t pid;
    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};

    if (command->pipe == NULL) { // no pipeline, a builtin command should execute in main shell process
        cmd = command->args[0];

        // if builtin, call the function
        for (int i = 0; i < NUM_BUILTINS; i++) { // FIXME: refactor to bring redirection into here as well (but right now my only bultins are cd and exit so it doesnt really matter)
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
        } else if (pid < 0) { // forking error
            perror("failed to fork in command execution");
        }
    } else { // there is a pipeline of 2+ commands - execute builtins in subshells
        while (command != NULL) { // 
            cmd = command->args[0];
            if (command->pipe != NULL) { // need to create pipe to next command
                pipe(pipe_out);
            }

            if ((pid = fork()) == 0) { // child process
                // handle pipeline
                if (pipe_in[0] != -1) { // connect pipe from prev command
                    close(pipe_in[1]); // close write end
                    dup2(pipe_in[0], STDIN_FILENO); // stdin reads from pipe
                }
                if (pipe_out[0] != -1) { // connect pipe to next command
                    close(pipe_out[0]); // close read end
                    dup2(pipe_out[1], STDOUT_FILENO); // stdout writes to pipe
                }

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

                // if builtin, call the function and return from child
                for (int i = 0; i < NUM_BUILTINS; i++) {
                    if (strcmp(cmd, BUILTINS[i]) == 0) {
                        (*BUILTIN_FUNCS[i])(command->args);
                        exit(EXIT_SUCCESS);
                    }
                }

                if (execvp(cmd, command->args) == -1) {
                    perror("failed to execute command");
                    exit(EXIT_FAILURE); // terminates child - does not quit main shell
                }
            } else if (pid < 0) { // forking error
                perror("failed to fork in command execution");
            }

            if (pipe_in[0] != -1) { close(pipe_in[0]); close(pipe_in[1]); }
            pipe_in[0] = pipe_out[0], pipe_in[1] = pipe_out[1];
            pipe_out[0] = pipe_out[1] = -1;

            command = command->pipe;
        }
        if (pipe_in[0] != -1) { close(pipe_in[0]); close(pipe_in[1]); } // make sure all fds are closed everywhere (in the parent too) - otherwise readings procs won't get EOF 
    }

    while(wait(NULL) != -1); // wait for all child processes to finish

    return 1; // commands done running. continue
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