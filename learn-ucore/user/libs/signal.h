#include "signum.h"

int signal(int sign, sighandler_t handler);
int tkill(int pid, int sign);
int sigprocmask(int how, const sigset_t * set, sigset_t * old);
int sigsuspend(uint32_t mask);