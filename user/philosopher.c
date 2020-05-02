#include "libc.h"
#include "hash_table.h"

typedef enum
{
    DIRTY,
    CLEAN,
    USING
} forkState_t;

typedef struct
{
    int pid;
    forkState_t fork_state;
    sem_t *mutex;
    bool is_requested;
    int requested_by;
} fork_t;

typedef struct
{
    fork_t *left;
    fork_t *right;
    int pid;
    int eat_count;
} philosopher_t;

void namePs(char *name, int pid)
{
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

void composeMsg(char *s, int pid, char *action)
{
    namePs(s, pid);
    s[5] = ' ';
    strcpy(&s[6], action);
}

void eat(philosopher_t *p)
{
    sem_wait(p->left->mutex);
    p->left->fork_state = USING;
    sem_post(p->left->mutex);
    sem_wait(p->right->mutex);
    p->right->fork_state = USING;
    sem_post(p->right->mutex);
    // log eating
    char msg[15];
    composeMsg(msg, p->pid, "eats");
    write(STDOUT_FILENO, msg, 11);
    sleep(3000);
    // finish eating, mark fork as dirty
    p->eat_count++;
    sem_wait(p->left->mutex);
    p->left->fork_state = DIRTY;
    if (p->left->is_requested == true)
    {
        p->left->is_requested = false;
        p->left->pid = p->left->requested_by;
        p->left->fork_state = CLEAN;
    }
    sem_post(p->left->mutex);
    sem_wait(p->right->mutex);
    p->right->fork_state = DIRTY;
    if (p->right->is_requested == true)
    {
        p->right->is_requested = false;
        p->right->pid = p->right->requested_by;
        p->right->fork_state = CLEAN;
    }
    sem_post(p->right->mutex);
    // logs finish eating
    composeMsg(msg, p->pid, "thinks");
    write(STDOUT_FILENO, msg, 13);
    yield();
    return;
}

bool haveBothForks(philosopher_t *p)
{
    bool r;
    sem_wait(p->left->mutex);
    sem_wait(p->right->mutex);
    r = p->left->pid == p->pid && p->right->pid == p->pid;
    sem_post(p->left->mutex);
    sem_post(p->right->mutex);
    return r;
}

void getFork(philosopher_t *p, fork_t *fork)
{
    sem_wait(fork->mutex);
    if (fork->pid != p->pid)
    {
        if (fork->fork_state == DIRTY)
        {
            fork->fork_state = CLEAN;
            fork->pid = p->pid;
            fork->is_requested = false;
        }
        else
        {
            fork->is_requested = true;
            fork->requested_by = p->pid;
        }
    }
    sem_post(fork->mutex);
    return;
}

void philosopher(int pid)
{
    char pName[10];
    namePs(pName, pid);
    int fd = shm_open(pName);
    philosopher_t *p = (philosopher_t *)mmap(fd);
    while (true)
    {
        while (haveBothForks(p) == false)
        {
            // write(STDOUT_FILENO, "y", 1);
            getFork(p, p->left);
            getFork(p, p->right);
        }
        eat(p);
        // burps... then thinks.
    }
    return;
}
fork_t forks[20];
sem_t forkMutex[20];

void main_Ps()
{
    int n = 4;
    // place some forks on the table, or else how would the guests eat?
    // e.g. ph_0 uses forks fork[0] and fork[16]
    for (int i = 0; i < n; i++)
    {
        memset(&forks[i], 0, sizeof(fork_t));
        // initially, all forks are dirty
        forks[i].fork_state = DIRTY;
        forks[i].is_requested = false;
        sem_init(&forkMutex[i], 1);
        forks[i].mutex = &forkMutex[i];
        // forks go to philosopher with lower ID
        // e.g. f_(n-1) => p_0 <= f_(0)
        if (i == n - 1)
        {
            forks[i].pid = 0;
        }
        else
        {
            forks[i].pid = i;
        }
    }
    // welcome onboard, philosophers.
    char pName[10];
    // int pFd[20] = {0};
    philosopher_t *philosophers[20];
    // philosophers have names ph_0 ... ph_15
    for (int i = 0; i < n; i++)
    {
        namePs(pName, i);
        int fd = shm_open(pName);
        ftruncate(fd, sizeof(philosopher_t));
        philosophers[i] = mmap(fd);
        philosopher_t *x = philosophers[i];
        memset(x, 0, sizeof(philosopher_t));
        x->eat_count = 0;
        x->pid = i;
        if (i == 0)
        {
            x->left = &forks[n - 1];
        }
        else
        {
            x->left = &forks[i - 1];
        }
        x->right = &forks[i];
    }
    for (int i = 0; i < n; i++)
    {
        int c = fork();
        if (c == 0)
        {
            philosopher(i);
            break;
        }
    }
    exit(EXIT_SUCCESS);
}