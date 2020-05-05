/* This is an alternate solution using Chandy/ Misra's algorithm
 */

#include "libc.h"

typedef enum
{
    DIRTY,
    CLEAN,
    USING
} forkStateAlt_t;

typedef struct
{
               int          pid;
    forkStateAlt_t   fork_state;
             sem_t       *mutex;
              bool is_requested;
               int requested_by;
} forkAlt_t;

typedef struct
{
    forkAlt_t     *left;
    forkAlt_t    *right;
          int       pid;
          int eat_count;
} philosopherAlt_t;

// set name to console friendly output e.g. 8 -> ph_08
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

// creates a output message for a philosopher's action, e.g. ph_08 eats
void composeMsgAlt(char *s, int pid, char *action)
{
    namePsAlt(s, pid);
    s[5] = ' ';
    strcpy(&s[6], action);
}

// let a philosopher to eat
void eatAlt(philosopherAlt_t *p)
{
    // update the state of the fork to being used
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
    p->eat_count++;
    sleep(300);
    /* After eating, the forks become dirty.
     * If another philosopher requested the fork,
     * the philosopher cleans the fork and pass it over.
     */
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
    composeMsgAlt(msg, p->pid, "thinks\n");
    write(STDOUT_FILENO, msg, 14);
    return;
}

// check if a philosopher have both forks to eat
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

// obtain a fork or send a request to obtain fork
void getForkAlt(philosopherAlt_t *p, forkAlt_t *fork)
{
    sem_wait(fork->mutex);
    /* If another philosopher have the fork,
     * the other philosopher will clean and pass the fork
     * if the fork is dirty, else kept it.
     */
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

// The philosopher instances
void philosopherAlt(philosopherAlt_t* p)
{
    while (true)
    {
        // get both forks
        getForkAlt(p, p->left);
        getForkAlt(p, p->right);
        // eat only if philosopher have both forks
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

    // Initialise the forks, and the mutex within a fork
    // At first, the forks are assigned to the philosophers with the lower ID.
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

    // Initialise the philosophers and assign forks to philosophers
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

    /* The philospher will terminate after one minute
     * set a break point on line including d1, d2, d3,
     * to inspect the values of min and max (eat count of individual philosophers)
     * to verify no philosophers suffers from resource starvation.
     */
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
    int d1, d2, d3; // break here to inspect min and max
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