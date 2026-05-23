// Christopher Lopez
// Assignment 4
// Description: 

#define _POSIX_C_SOURCE 200809L
#include <stdio.h> // PRINT, GET FFLSUH
#include <signal.h> // SIGINT SIGTSTP
#include <stdlib.h> // EXIT, GETENV, FREE, MALLOC
#include <string.h> // STRCMP, STRTOK_R, STRDUP
#include <sys/types.h> // PID_T, SSIZE_T
#include <sys/wait.h> // WAITPID, WIFEXITED, WEXITSTATUS
#include <unistd.h> // FORK, EXECVP, GETPID, DUP2, WRITE
#include <fcntl.h> // OPEN, READ, WRITE, CREATE

/*Constants*/
#define MAX_LINE    2048
#define MAX_ARGS    512
#define MAX_BG_PIDS 512

/*Flags to toggle SIGTSTP*/
volatile sig_atomic_t fgOnlyMode = 0;

int last_fg_status = 0; // Exit status or signal of last foreground
int last_signal_status = 0; // 1 if terminated by signal, 0 if exit

/*Track background process */
pid_t bg_pids[MAX_BG_PIDS];
int bg_pid_count = 0;

/*Parse command*/
typedef struct {
    char *args[MAX_ARGS + 1]; // Command arguments
    int argc;                 // Number of arguments
    char *inputFile;          // Input redirection
    char *outputFile;         // Output redirection
    int background;           // Run in background
} Command;

/*Forward declarations */

void setup_signals(void);
void handle_SIGTSTP(int sig);
void parse_command(char *line, Command *cmd);
void reap_background_children(void);
void run_builtin_exit(void);
void run_builtin_cd(Command *cmd);
void run_builtin_status(void);
void run_external(Command *cmd);

/*Signals*/
/*Signal setup*/
void setup_signals(void) {
    struct sigaction sa_int = {0};
    struct sigaction sa_tstp = {0};
    
    // Parent ignores SIGINT
    sa_int.sa_handler = SIG_IGN;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    // Parent catches SIGTSTP to toggle foreground mode
    sa_tstp.sa_handler = handle_SIGTSTP;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
}

// Interrupt signal
void handle_SIGINT(int sig) {
    (void)sig;
}

// foreground-only for Ctrl-Z
void handle_SIGTSTP(int sig) {
    (void)sig;
    if (fgOnlyMode == 0) {
        fgOnlyMode = 1;
        const char *msg = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    } else {
        fgOnlyMode = 0;
        const char *msg = "\nExiting foreground-only mode\n";
        write (STDOUT_FILENO, msg, strlen(msg));
    }
}

/*Parse*/
void parse_command(char *line, Command *cmd) {
    char *saveptr;
    char *token = strtok_r(line, " \t", &saveptr);

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " \t", &saveptr);
            if (token) cmd->inputFile = token;
        } else if (strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " \t", &saveptr);
            if (token) cmd-> outputFile = token;
        } else {
            cmd->args[cmd->argc++] = token;
        }
        token = strtok_r(NULL, " \t", &saveptr);
    }
    cmd->args[cmd->argc] = NULL;

    if (cmd->argc > 0 && strcmp(cmd->args[cmd->argc - 1], "&") == 0) {
        cmd-> background = 1;
        cmd->args[--cmd->argc] = NULL;
    }

    if (fgOnlyMode) {
        cmd->background = 0;
    }
}

/*Background child reaping*/
void reap_background_children(void) {
    int wstatus;
    pid_t pid;

    while ((pid = waitpid(-1, &wstatus, WNOHANG)) > 0) {
        if (WIFEXITED(wstatus)) {
            printf("background pid %d is done: exit value %d\n", pid, WEXITSTATUS(wstatus));
        } else if (WIFSIGNALED(wstatus)) {
            printf("background pid %d is done: terminated by signal %d\n", pid, WTERMSIG(wstatus));
        }
        fflush(stdout);
    }
}

/*BUILT-IN COMMANDS*/
void run_builtin_exit(void) {
    // Kill all background children before exit
    for (int i = 0; i < bg_pid_count; i ++) {
        kill(bg_pids[i], SIGTERM);
    }
    exit(0);
}

void run_builtin_cd(Command *cmd) {
    const char *path = (cmd->argc >= 2) ? cmd->args[1] : getenv("HOME");
    if (chdir(path) != 0) {
        perror("cd");
    }
}

void run_builtin_status(void) {
    if (last_signal_status) {
        printf("terminated by signal %d\n", last_fg_status);
    } else {
        printf("exit value %d\n", last_fg_status);
    }
    fflush(stdout);
}

/*External commands*/
void run_external(Command *cmd) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        if (!cmd->background) {
            struct sigaction sa = {0};
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGINT, &sa, NULL);
        }

        struct sigaction sa_tstp = {0};
        sa_tstp.sa_handler = SIG_IGN;
        sigemptyset(&sa_tstp.sa_mask);
        sigaction(SIGTSTP, &sa_tstp, NULL);

        // INput redirection
        const char *Infile = cmd->inputFile;
        if (!Infile && cmd->background) Infile = "/dev/null";
        if (Infile) {
            int fd = open(Infile, O_RDONLY);
            if(fd < 0) {
                fprintf(stderr, "cannot open %s for input\n", Infile);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Output redirection
        const char *OutFile = cmd->outputFile;
        if (!OutFile && cmd->background) OutFile = "/dev/null";
        if (OutFile) {
            int fd = open(OutFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd <0) {
                fprintf(stderr, "cannot open %s for output\n", OutFile);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execvp(cmd->args[0], cmd->args);

        fprintf(stderr, "%s: no such file or directory\n", cmd->args[0]);
        exit(1);
    }

    // Parent process
    if (cmd->background) {
        printf("background pid is %d\n", pid);
        fflush(stdout);
        // Track PID
        if (bg_pid_count < MAX_BG_PIDS) {
            bg_pids[bg_pid_count++] = pid;
        }
    } else {
        int wstatus;
        waitpid(pid, &wstatus, 0);

        if (WIFEXITED(wstatus)) {
            last_fg_status = WEXITSTATUS(wstatus);
            last_signal_status = 0;
        } else if (WIFSIGNALED(wstatus)) {
            last_fg_status = WTERMSIG(wstatus);
            last_signal_status  = 1;
            printf("terminated by signal %d\n", last_fg_status);
            fflush(stdout);
        }
    }
}

/*Main*/
int main(void) {
    setup_signals();

    char *line = NULL;
    size_t line_cap = 0;

    while (1) {
        reap_background_children();

        printf(": ");
        fflush(stdout);

        ssize_t nread = getline(&line, &line_cap, stdin);
        if (nread == -1) {
            clearerr(stdin);
            printf("\n");
            fflush(stdout);
            continue;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        
        Command cmd;
        memset(&cmd, 0, sizeof(cmd));
        parse_command(line, &cmd);

        if (cmd.argc == 0){
            continue;
        }

        if (strcmp(cmd.args[0], "exit") == 0) {
            run_builtin_exit();
        } else if (strcmp(cmd.args[0], "cd") == 0) {
            run_builtin_cd(&cmd);
        } else if (strcmp(cmd.args[0], "status") == 0) {
            run_builtin_status();
        } else {
            run_external(&cmd);
        }
    }

    free(line);
    return 0;
}

