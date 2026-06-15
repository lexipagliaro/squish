#include "squish.h"

/* 
    TODO:   << (here-documents), <<< (here-strings),
            DRY realloc in sq_token?,
            sq_token error handling,
            documentation,
*/

void sq_reset(char* line, cmd_t* command) {
    free(line);

    // free subsequent heap allocated command structs (and their args)
    cmd_t *curr = command;
    while (curr != NULL) {
        cmd_t *next = curr->pipe;

        if (curr->args != NULL) {
            int i = 0;
            while (curr->args[i] != NULL) {
                free(curr->args[i]);
                i++;
            }   
            free(curr->args);    
        }
        free(curr->redirect_a);
        free(curr->redirect_i);
        free(curr->redirect_t);
        if (curr != command) {
            free(curr);
        }
    
        curr = next;
    }
    command->args = NULL; 
    command->redirect_t = command->redirect_a = command->redirect_i = NULL;
    command->pipe = NULL;
}

int main(void) {
    char* line = NULL;
    cmd_t command = { 0 };
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
        } 
        else { 
            perror("failed to read command line");
            *status = CRASH;
        }
    }

    return line;
}

// FIXME: crash gracefully (wrong return type rn)
char* sq_token(char** line) {
    char* ch = *line; 
    int bufsize = BUFSIZ;
    char* tokenbuf = malloc(BUFSIZ);
    if (tokenbuf == NULL) {
        perror("failed to allocate memory for tokenization");
        exit(EXIT_FAILURE);
    }
    int i = 0;

    // skip leading whitespace
    while (isspace(*ch)) {
        ch++;
    }

    // if string has been fully parsed, return NULL
    if (*ch == '\0') { free(tokenbuf); return NULL; }

    // read this token until whitespace (outside of quotes) or null
    parse_state_t state = UNQUOTED;
    while (*ch != '\0') {
        if (*ch == '$' && state != SINGLE_QUOTED) {
            // ch points to beginning of variable name, ptr navigates to (one past) the end
            ch++;
            char* ptr = ch;
            while (isalnum(*ptr) || *ptr == '_') {
                ptr++;
            }
            int varlen = ptr - ch;

            if (varlen == 0) { // eg. `echo $` shouldnt come up empty, should print `$`
                tokenbuf[i] = '$';
                i++;
            }
            else {
                // extract the variable name into memory as a null terminated string
                char* var = malloc(varlen + 1);
                if (var == NULL) {
                    perror("failed to allocate memory for variable expansion");
                    free(tokenbuf);
                    exit(EXIT_FAILURE);
                }
                memcpy(var, ch, varlen);
                var[varlen] = '\0';

                // get value of variable (if it exists) and append to token
                char* value = getenv(var);
                if (value != NULL) {
                    int valuelen = strlen(value);
                    while (valuelen > (bufsize - i)) {
                        bufsize *= 2;
                        char* newbuf = realloc(tokenbuf, bufsize);
                        if (newbuf == NULL) {
                            perror("failed to reallocate memory for variable expansion");
                            free(tokenbuf);
                            free(var);
                            exit(EXIT_FAILURE); 
                        }
                        tokenbuf = newbuf;
                    }
                    memcpy(tokenbuf + i, value, valuelen);
                    i += valuelen;
                }
                free(var);
            }
            ch = ptr - 1;
        } 
        else if (*ch == '\\' && state != SINGLE_QUOTED) {
            ch++;
            if (*ch == '$' || *ch == '"' || *ch == '`' || *ch == '\\') {
                tokenbuf[i] = *ch;
            } else {
                ch--;
                tokenbuf[i] = '\\';
            }
            i++;
        } 
        else if (state == UNQUOTED) {
            if (isspace(*ch)) { break; }
            else if (*ch == '"') { state = DOUBLE_QUOTED; }
            else if (*ch == '\'') { state = SINGLE_QUOTED; }
            else { tokenbuf[i] = *ch; i++; }
        } 
        else if (state == DOUBLE_QUOTED) {
            if (*ch == '"') { state = UNQUOTED; }
            else { tokenbuf[i] = *ch; i++; }
        } 
        else if (state == SINGLE_QUOTED) {
            if (*ch == '\'') { state = UNQUOTED; }
            else { tokenbuf[i] = *ch; i++; }
        }

        if (i == bufsize) { // filled buffer, reallocate with more space
            bufsize *= 2;
            char* newbuf = realloc(tokenbuf, bufsize);
            if (newbuf == NULL) {
                perror("failed to reallocate memory for tokenization");
                free(tokenbuf);
                exit(EXIT_FAILURE);
            }
            tokenbuf = newbuf;
        }

        ch++;  
    }
    
    // null terminate token string
    tokenbuf[i] = '\0'; 

    *line = ch; // only modifies the `line` local to sq_parse, not main
    return tokenbuf;
}

status_t sq_parse(char* line, cmd_t* command) {
    char* token;
    int bufsize = BUFSIZ;
    command->args = malloc(BUFSIZ);
    if (command->args == NULL) {
        perror("failed to allocate memory for command line parsing");
        return CRASH;
    }
    command->args[0] = NULL;
    int i = 0;

    // parse and terminate token list with NULL pointer (at all stages of parsing, to keep sq_reset memory safe)
    token = sq_token(&line);
    while (token != NULL) {
        if (strcmp(token, ">") == 0) { 
            free(token);
            command->redirect_t = sq_token(&line); 
            token = sq_token(&line);
            continue;
        }
        else if (strcmp(token, ">>") == 0) {
            free(token);
            command->redirect_a = sq_token(&line); 
            token = sq_token(&line);
            continue;
        }
        else if (strcmp(token, "<") == 0) {
            free(token);
            command->redirect_i = sq_token(&line); 
            token = sq_token(&line);
            continue;
        } 

        if (strcmp(token, "|") == 0) {
            free(token);

            // establish pipe and proceed with next command
            cmd_t* next = malloc(sizeof(cmd_t)); 
            if (next == NULL) {
                perror("failed to allocate memory for command line parsing");
                return CRASH;
            }
            memset(next, 0, sizeof(*next));
            command->pipe = next;
            command = next;

            bufsize = BUFSIZ;
            i = 0;
            command->args = malloc(BUFSIZ);
            if (command->args == NULL) {
                perror("failed to allocate memory for command line parsing");
                return CRASH;
            }
            command->args[0] = NULL;

            token = sq_token(&line);
            continue;
        }

        command->args[i] = token;
        command->args[i + 1] = NULL;

        i++;
        if (i == bufsize - 1) { // filled buffer, reallocate with more space
            bufsize *= 2;
            char** new = realloc(command->args, bufsize);
            if (new == NULL) {
                perror("failed to reallocate memory for command line parsing");
                return CRASH;
            }
            command->args = new;
        }

        token = sq_token(&line); 
    }

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
    } 
    else if (command->redirect_a) { // >>
        fd = open(command->redirect_a, O_WRONLY | O_CREAT | O_APPEND, 0644);
        sq_dup(fd, STDOUT_FILENO);
    } 
    else if (command->redirect_i) { // <
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
    } 
    else { // pipeline of 2+ commands - execute builtins in subshells
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

                if (sq_builtin(command) != NOT_FOUND) {
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
    } 
    else if (args[1] == NULL) { 
        // 2. else if no directory given, change directory to HOME
        path = getenv("HOME"); 
    } 
    else if (args[1] != NULL && strcmp(args[1], "-") == 0) {
        // support cd - to return to previous wd
        path = getenv("OLDPWD");
    } 
    else {
        path = args[1];
    }

    getcwd(old, BUFSIZ);
    if (chdir(path) == -1) { 
        perror("failed to change directory"); 
        return RETRY;
    } 
    else {
        setenv("OLDPWD", old, 1);
    }

    return OK;
}

status_t sq_exit(char* args[]) {
    (void) args;
    return EXIT;
}