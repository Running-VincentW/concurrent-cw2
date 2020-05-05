#include "libc.h"

typedef struct
{
    int pid;
    sem_t *first;
    sem_t *second;
    int eat_count;
} philosopher_t;

void namePs(char *name, int pid)
{
    int d1 = (pid % 100) / 10;
    int d2 = (pid %  10) / 1;
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

void philosopher(philosopher_t *p)
{
    char msg[15];
    while (true)
    {
        // hungry
        sem_wait(p->first);
        sem_wait(p->second);
        // eat
        composeMsg(msg, p->pid, "eats\n");
        write(STDOUT_FILENO, msg, 12);
        p->eat_count++;
        sleep(300);
        sem_post(p->first);
        sem_post(p->second);
        // think
        composeMsg(msg, p->pid, "thinks\n");
        write(STDOUT_FILENO, msg, 14);
        sleep(300);
    }
    return;
}

sem_t forkMutex[20];
philosopher_t philosophers[20];

void main_Ps()
{
    int n = 16;
    // assign a mutex to each resource (fork)
    for (int i = 0; i < n; i++)
    {
        sem_init(&forkMutex[i], 1);
    }

    // initialise the philosophers and assign forks
    for (int i = 0; i < n; i++)
    {
        philosopher_t *x = &philosophers[i];
        memset(x, 0, sizeof(philosopher_t));
        x->eat_count = 0;
        x->pid = i;
        /* Using the Resource Hierachy concurrent algorithm,
         * The first and second fork defines the philosopher
         * always picks up the fork with the lower id first.
         */
        if (i == n - 1)
        {
            x->first = &forkMutex[0];
            x->second = &forkMutex[i];
        }
        else
        {
            x->first = &forkMutex[i];
            x->second = &forkMutex[i + 1];
        }
    }

    // Start the child process
    for (int i = 0; i < n; i++)
    {
        int c = fork();
        if (c == 0)
        {
            philosopher(&philosophers[i]);
            break;
        }
    }

    // Performance count
    sleep(60000);
    int min = philosophers[0].eat_count;
    int max = philosophers[0].eat_count;
    int total = 0;
    for (int i = 0; i < n; i++)
    {
        int c = philosophers[i].eat_count;
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