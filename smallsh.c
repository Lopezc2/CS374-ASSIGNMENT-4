// Christopher Lopez
// Assignment 4
// Description: 

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
// Interrupt signal
void handle_SIGINT(int sig) {
    (void)sig;
}

// foreground-only for Ctrl-Z
void handle_SIGTSTP(int sig) {
    (void)sig;
    if (!fgOnlyMode) {
        fgOnlyMode = 1;
        char *msg = "\nEntering forground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, 52);
    } else {
        fgOnlyMode = 0;
        char *msg = "\nExiting foreground-only mode\n: ";
        write (STDOUT_FILENO, msg, 32);
    }
}

/*Main*/
