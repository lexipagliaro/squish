#include "squish.h"

/* 
    TODO:   << (here-documents), <<< (here-strings),
            single and double quoting,
            variable expansion,
*/

void sq_reset(char* line, cmd_t* command) {
    free(line);

    free(command->args);
    command->args = NULL; 
    command->redirect_t = command->redirect_a = command->redirect_i = NULL;

    // free subsequent heap allocated command structs (and their args)
    cmd_t *curr = command->pipe;
    while (curr != NULL) {
        cmd_t *next = curr->pipe;

        free(curr->args);
        free(curr);

        curr = next;
    }

    command->pipe = NULL;
}

int main(int argc, char* argv[]) {
    char* line = NULL;
    cmd_t command = { NULL };
    status_t status = OK;
    char cwd[BUFSIZ];

    do {
        getcwd(cwd, BUFSIZ);
        printf("%s%s -~> %s", GREEN, cwd, RESET);
        
        line = sq_read(&status);
        if (status == OK) {
            status = sq_parse(line, &command);
            if (status == OK) {
                status = sq_execute(&command);
            }
        }
        
        sq_reset(line, &command);
    } while (status > 1);

    printf("%sbye squish!%s\n", GREEN, RESET);

    return status;
}

char* sq_read(status_t* status) {
    char* line = NULL;
    size_t bufsize;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) { // hit end of script file or user typed ctrl+d
            *status = EXIT;
        } else { 
            perror("failed to read command line");
            *status = CRASH;
        }
    }

    return line;
}

status_t sq_parse(char* line, cmd_t* command) {
    char* delims = " \t\r\n\a";
    char* token;
    size_t bufsize = BUFSIZ;
    command->args = malloc(BUFSIZ);
    int i = 0;
    
    if (command->args == NULL) {
        perror("failed to allocate memory for command line parsing");
        return CRASH;
    }

    // parse and terminate token list with NULL pointer
    token = strtok(line, delims);
    while (token != NULL) {
        if (strcmp(token, ">") == 0) { 
            // FIXME: keep reading after redirection (eg. wc -w < hello.txt | ...)
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
            command->args[i] = NULL;

            // establish pipe and proceed with next command
            cmd_t* next = malloc(sizeof(cmd_t));
            memset(next, 0, sizeof(*next));
            command->pipe = next;
            command = next;

            bufsize = BUFSIZ;
            i = 0;
            command->args = malloc(BUFSIZ);

            token = strtok(NULL, delims);
            if (token == NULL) {}
            continue;
        }

        command->args[i] = token;

        i++;
        if (i == bufsize) { // filled buffer, reallocate with more space
            bufsize *= 2;
            char** new = realloc(command->args, bufsize);
            if (new == NULL) {
                perror("failed to reallocate memory for command line parsing");
                return CRASH;
            }
            command->args = new;
        }

        token = strtok(NULL, delims); // NULL -> continue on same str
    }
    command->args[i] = NULL;

    return OK;
}

// run from child OR main shell
status_t sq_builtin(cmd_t* command) {
    for (int i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(command->args[0], BUILTINS[i]) == 0) {
            return (*BUILTIN_FUNCS[i])(command->args);
        }
    }
    return NOT_FOUND;
}

// run from child
void sq_dup(int fd, int replace) {
    if (fd == -1) {
        perror("failed to open file for redirection");
        exit(EXIT_FAILURE);
    }
    dup2(fd, replace);
    close(fd);
}

// run from child
void sq_redirection(cmd_t* command) {
    int fd;
    if  (command->redirect_t) { // >
        fd = open(command->redirect_t, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        sq_dup(fd, STDOUT_FILENO);
    } else if (command->redirect_a) { // >>
        fd = open(command->redirect_a, O_WRONLY | O_CREAT | O_APPEND, 0644);
        sq_dup(fd, STDOUT_FILENO);
    } else if (command->redirect_i) { // <
        fd = open(command->redirect_i, O_RDONLY);
        sq_dup(fd, STDIN_FILENO);
    }
}

// run from child
void sq_exec(cmd_t* command) {
    if (execvp(command->args[0], command->args) == -1) {
        perror("failed to execute command");
        exit(EXIT_FAILURE); 
    }
}

status_t sq_execute(cmd_t* command) {
    char* cmd;
    pid_t pid;
    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};

    if (command->pipe == NULL) { // no pipeline, a builtin command should execute in main shell process
        status_t status = sq_builtin(command);
        if (status != NOT_FOUND) {
            return status;
        }

        // else search for program in path directories and run in new process
        pid = fork();
        if (pid == 0) { // child process
            sq_redirection(command);

            sq_exec(command);
        } else if (pid < 0) {
            perror("forking error");
            return RETRY;
        }
    } else { // pipeline of 2+ commands - execute builtins in subshells
        while (command != NULL) { // 
            if (command->pipe != NULL) { // create pipe to next command
                pipe(pipe_out);
            }

            pid = fork();
            if (pid == 0) { // child process
                if (pipe_in[0] != -1) { 
                    close(pipe_in[1]); 
                    dup2(pipe_in[0], STDIN_FILENO); // stdin reads from input pipe
                }
                if (pipe_out[0] != -1) { 
                    close(pipe_out[0]); 
                    dup2(pipe_out[1], STDOUT_FILENO); // stdout writes to output pipe
                }

                sq_redirection(command);

                if (sq_builtin(command) != -1) {
                    exit(EXIT_SUCCESS);
                }

                sq_exec(command);
            } else if (pid < 0) { 
                perror("forking error");
                return RETRY;
            }

            // reading procs won't get EOF if fds still open anywhere (ie in the parent)
            if (pipe_in[0] != -1) { close(pipe_in[0]); close(pipe_in[1]); }

            pipe_in[0] = pipe_out[0], pipe_in[1] = pipe_out[1];
            pipe_out[0] = pipe_out[1] = -1;
            command = command->pipe;
        }
        if (pipe_in[0] != -1) { close(pipe_in[0]); close(pipe_in[1]); } 
    }

    // TODO: handle children exiting with failure (return appropriate status) 
    while(wait(NULL) != -1); 

    return OK;
}

// https://man7.org/linux/man-pages/man1/cd.1p.html
status_t sq_cd(char* args[]) {
    char* path;
    char old[BUFSIZ];

    if (args[1] == NULL && getenv("HOME") == NULL) { 
        // 1. if no directory given and HOME env variable is not defined
        return 1; 
    } else if (args[1] == NULL) { 
        // 2. else if no directory given, change directory to HOME
        path = getenv("HOME"); 
    } else if (args[1] != NULL && strcmp(args[1], "-") == 0) {
        // support cd - to return to previous wd
        path = getenv("OLDPWD");
    } else {
        path = args[1];
    }

    getcwd(old, BUFSIZ);
    if (chdir(path) == -1) { 
        perror("failed to change directory"); 
        return RETRY;
    } else {
        setenv("OLDPWD", old, 1);
    }

    return OK;
}

status_t sq_exit(char* args[]) {
    printf("%sbye squish!%s\n", GREEN, RESET);
    return EXIT;
}