#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <direttore.h>
#include <util.h>
#include <sig_handler.h>

/* pid processo */
extern pid_t pid;

static bool signal_handler(int sig, handler_t* h){
    switch(sig){
        case SIGHUP: {
            if(Lock_Acquire(&h->check)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            *(h->stato) = INT_SIGHUP;
            if(Lock_Release(&h->check)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            return true;
        }
        case SIGQUIT: {
            if(Lock_Acquire(&h->check)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            *(h->stato) = INT_SIGQUIT;
            if(Lock_Release(&h->check)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            return true;
        }
        case SIGUSR2: {
            kill(pid, SIGKILL);
            return false;
        }
        default: {
            return false;
        }
    }
}
void* main_handler(void* arg){
    int sig;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    if(pthread_sigmask(SIG_SETMASK, &set, NULL)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    handler_t* sig_handler = (handler_t*)arg;
    while(1){
        if(sigwait(&set, &sig) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        if(!signal_handler(sig, sig_handler)) break;
    }
    return NULL;
}