#include "libc.h"

typedef enum
{
    DIRTY,
    CLEAN,
    USING
} forkStateAlt_t;

typedef struct
{
    int pid;
    forkStateAlt_t fork_state;
    sem_t *mutex;
    bool is_requested;
    int requested_by;
} forkAlt_t;

typedef struct
{
    forkAlt_t *left;
    forkAlt_t *right;
    int pid;
    int eat_count;
} philosopherAlt_t;

void namePsAlt(char *name, int pid)
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

void composeMsgAlt(char *s, int pid, char *action)
{
    namePsAlt(s, pid);
    s[5] = ' ';
    strcpy(&s[6], action);
}

void eatAlt(philosopherAlt_t *p)
{
    sem_wait(p->left->mutex);
    p->left->fork_state = USING;
    sem_post(p->left->mutex);
    sem_wait(p->right->mutex);
    p->right->fork_state = USING;
    sem_post(p->right->mutex);
    // log eating
    char msg[15];
    composeMsgAlt(msg, p->pid, "eats\n");
    write(STDOUT_FILENO, msg, 12);
    sleep(300);
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
    composeMsgAlt(msg, p->pid, "thinks\n");
    write(STDOUT_FILENO, msg, 14);
    // sleep(300);
    return;
}

bool haveBothForksAlt(philosopherAlt_t *p)
{
    bool r;
    sem_wait(p->left->mutex);
    sem_wait(p->right->mutex);
    r = p->left->pid == p->pid && p->right->pid == p->pid;
    sem_post(p->left->mutex);
    sem_post(p->right->mutex);
    return r;
}

void getForkAlt(philosopherAlt_t *p, forkAlt_t *fork)
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

void philosopherAlt(philosopherAlt_t* p)
{
    while (true)
    {
        getForkAlt(p, p->left);
        getForkAlt(p, p->right);
        if(haveBothForksAlt(p) == true) eatAlt(p);
        sleep(300);
    }
    return;
}


forkAlt_t forks_alt[20];
sem_t forkMutex_alt[20];
philosopherAlt_t philosophers_alt[20];

void main_PsAlt()
{
    int n = 16;

    for (int i = 0; i < n; i++)
    {
        memset(&forks_alt[i], 0, sizeof(forkAlt_t));
        forks_alt[i].fork_state = DIRTY;
        forks_alt[i].is_requested = false;

        sem_init(&forkMutex_alt[i], 1);
        forks_alt[i].mutex = &forkMutex_alt[i];

        // forks go to philosopher with lower ID
        // e.g. f_(n-1) => p_0 <= f_(0)
        if (i == n - 1)
        {
            forks_alt[i].pid = 0;
        }
        else
        {
            forks_alt[i].pid = i;
        }
    }

    // philosophers have names ph_0 ... ph_15
    for (int i = 0; i < n; i++)
    {
        philosopherAlt_t *x = &philosophers_alt[i];
        x->eat_count = 0;
        x->pid = i;
        if (i == 0)
        {
            x->left = &forks_alt[n - 1];
        }
        else
        {
            x->left = &forks_alt[i - 1];
        }
        x->right = &forks_alt[i];
    }

    for (int i = 0; i < n; i++)
    {
        int c = fork();
        if (c == 0)
        {
            philosopherAlt(&philosophers_alt[i]);
            break;
        }
    }

    // Performance count
    sleep(60000);
    int min = philosophers_alt[0].eat_count;
    int max = philosophers_alt[0].eat_count;
    int total = 0;
    for (int i = 0; i < n; i++)
    {
        int c = philosophers_alt[i].eat_count;
        if (c < min)
        {
            min = c;
        }
        if (c > max)
        {
            max = c;
        }
        total += c;
    }
    int d1, d2, d3;
    char msg[15];
    d1 = ((total % 1000) / 100) + '0';
    d2 = ((total %  100) /  10) + '0';
    d3 = ((total %   10) /   1) + '0';
    strcpy(msg, "\nsum: ");
    char sum[] = {d1, d2, d3, '\n', NULL};
    strcat(msg, sum);
    write(STDOUT_FILENO, msg, 10);

    
    exit(EXIT_SUCCESS);
}