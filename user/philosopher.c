#include "libc.h"
#include "hash_table.h"

typedef enum{
    DIRTY,CLEAN
} forkState_t;
typedef struct{
    int pid;
    forkState_t fork_state;
    sem_t* mutex;
} fork_t;
typedef struct{
    fork_t* left;
    fork_t* right;
    int pid;
    int eatCount;
} philosopher_t;
void namePs(char *name, int pid){
    int d1 = (pid % 100) / 10;
    int d2 = (pid % 10) / 1;
    name[0] = 'p';
    name[1] = 'h';
    name[2] = '_';
    itoa(&name[3], d1);
    itoa(&name[4], d2);
    name[5] = 0;
    return;
}
void acquireFork(philosopher_t* p, fork_t* fork){
    // pass if fork is already on philosopher's hand
    if(fork->pid != p->pid){
        // Heyy neighbour, pass me the fork!
        sem_wait(fork->mutex);
        // receives a CLEAN fork :)
        fork->fork_state = CLEAN;
        fork->pid = p->pid;
    }
    return;
}
void pickUpForks(philosopher_t* p){
    if(p->left->fork_state == DIRTY){
        p->left->fork_state = CLEAN;
        sem_wait(p->left->mutex);
    }
    if(p->right->fork_state == DIRTY){
        p->right->fork_state = CLEAN;
        sem_wait(p->right->mutex);
    }
    return;
}
void eat(philosopher_t* p){
    pickUpForks(p);
    // yum yum yum
    sleep(3);
    // finish eating, mark fork as dirty
    p->eatCount ++;
    p->left->fork_state = DIRTY;
    p->right->fork_state = DIRTY;
    // release fork
    sem_post(p->left->mutex);
    sem_post(p->right->mutex);
    return;
}
bool haveBothForks(philosopher_t* p){
    return p->left->pid == p->pid && p->right->pid == p->pid;
}
void releaseForksInHand(philosopher_t* p){
    if(p->left->pid == p->pid){
        sem_post(p->left->mutex);
    }
    if(p->right->pid == p->pid){
        sem_post(p->right->mutex);
    }
    return;
}
void philosopher(int pid){
    char pName[10];
    namePs(pName, pid);
    int fd = shm_open(pName);
    philosopher_t* p = (philosopher_t*) mmap(fd);
    while(true){
        // wants to eat
        while(haveBothForks(p) == false){
            releaseForksInHand(p);
            acquireFork(p, p->left);
            acquireFork(p, p->right);
        }
        // eat
        eat(p);
        // burp and start thinking...
    }
    return;
}
fork_t forks[20];
sem_t forkMutex[20];

void mainPs(){
    int n = 16;
    // place some forks on the table, or else how would the guests eat?
    // e.g. ph_0 uses forks fork[0] and fork[16]
    for (int i = 0; i < n; i++){
        forks[i].fork_state = CLEAN;
        sem_init(&forkMutex[i], 1);
        forks[i].mutex = &forkMutex[i];
        if (i == n - 1){
            forks[i].pid = 0;
        }
        else{
            forks[i].pid = i;
        }
    }
    // welcome onboard, philosophers.
    char pName[10];
    int pFd[20] = {0};
    philosopher_t* philosophers[20];
    // philosophers have names ph_0 ... ph_15
    for(int i = 0; i < n; i++){
        namePs(pName, i);
        int fd = shm_open(pName);
        ftruncate(fd, sizeof(philosopher_t));
        philosophers[i] = mmap(fd);
        philosopher_t *x = philosophers[i];
        memset(x, 0, sizeof(philosopher_t));
        x->eatCount = 0;
        x->pid = i;
        if (i == 0){x->left = &forks[n-1];}
        else{x->left = &forks[i-1];}
        x->right = &forks[i];
    }
    for(int i = 0; i < n; i++){
        int c = fork();
        if(c == 0){
            philosopher(i);
            break;
        }
    }
    exit( EXIT_SUCCESS );
}