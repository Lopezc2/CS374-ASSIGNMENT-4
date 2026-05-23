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

/*Signals*/

// Interrupt signal
void handle_SIGINT(int sig) {
    
    (void)sig;
}

/*Main*/
