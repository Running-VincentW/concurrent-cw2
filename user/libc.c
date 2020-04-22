/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "libc.h"

int  atoi( char* x        ) {
  char* p = x; bool s = false; int r = 0;

  if     ( *p == '-' ) {
    s =  true; p++;
  }
  else if( *p == '+' ) {
    s = false; p++;
  }

  for( int i = 0; *p != '\x00'; i++, p++ ) {
    r = s ? ( r * 10 ) - ( *p - '0' ) :
            ( r * 10 ) + ( *p - '0' ) ;
  }

  return r;
}

void itoa( char* r, int x ) {
  char* p = r; int t, n;

  if( x < 0 ) {
     p++; t = -x; n = t;
  }
  else {
          t = +x; n = t;
  }

  do {
     p++;                    n /= 10;
  } while( n );

    *p-- = '\x00';

  do {
    *p-- = '0' + ( t % 10 ); t /= 10;
  } while( t );

  if( x < 0 ) {
    *p-- = '-';
  }

  return;
}

long pow(int base, int exp)
{
    int r = 1;
    for (exp; exp > 0; exp--)
    {
        r = r * base;
    }
    return r;
}

void yield() {
  asm volatile( "svc %0     \n" // make system call SYS_YIELD
              :
              : "I" (SYS_YIELD)
              : );

  return;
}

// int writes(int fd, const void* x){
//   return write(fd, x, strlen(x));
// }

int write( int fd, const void* x, size_t n ) {
  int r;

  asm volatile( "mov r0, %2 \n" // assign r0 = fd
                "mov r1, %3 \n" // assign r1 =  x
                "mov r2, %4 \n" // assign r2 =  n
                "svc %1     \n" // make system call SYS_WRITE
                "mov %0, r0 \n" // assign r  = r0
              : "=r" (r) 
              : "I" (SYS_WRITE), "r" (fd), "r" (x), "r" (n)
              : "r0", "r1", "r2" );

  return r;
}

int  read( int fd,       void* x, size_t n ) {
  int r;

  asm volatile( "mov r0, %2 \n" // assign r0 = fd
                "mov r1, %3 \n" // assign r1 =  x
                "mov r2, %4 \n" // assign r2 =  n
                "svc %1     \n" // make system call SYS_READ
                "mov %0, r0 \n" // assign r  = r0
              : "=r" (r) 
              : "I" (SYS_READ),  "r" (fd), "r" (x), "r" (n) 
              : "r0", "r1", "r2" );

  return r;
}

int  fork() {
  int r;

  asm volatile( "svc %1     \n" // make system call SYS_FORK
                "mov %0, r0 \n" // assign r  = r0 
              : "=r" (r) 
              : "I" (SYS_FORK)
              : "r0" );

  return r;
}

void exit( int x ) {
  asm volatile( "mov r0, %1 \n" // assign r0 =  x
                "svc %0     \n" // make system call SYS_EXIT
              :
              : "I" (SYS_EXIT), "r" (x)
              : "r0" );

  return;
}

void exec( const void* x ) {
  asm volatile( "mov r0, %1 \n" // assign r0 = x
                "svc %0     \n" // make system call SYS_EXEC
              :
              : "I" (SYS_EXEC), "r" (x)
              : "r0" );

  return;
}

int  kill( int pid, int x ) {
  int r;

  asm volatile( "mov r0, %2 \n" // assign r0 =  pid
                "mov r1, %3 \n" // assign r1 =    x
                "svc %1     \n" // make system call SYS_KILL
                "mov %0, r0 \n" // assign r0 =    r
              : "=r" (r) 
              : "I" (SYS_KILL), "r" (pid), "r" (x)
              : "r0", "r1" );

  return r;
}

void nice( int pid, int x ) {
  asm volatile( "mov r0, %1 \n" // assign r0 =  pid
                "mov r1, %2 \n" // assign r1 =    x
                "svc %0     \n" // make system call SYS_NICE
              : 
              : "I" (SYS_NICE), "r" (pid), "r" (x)
              : "r0", "r1" );

  return;
}

void sem_init(sem_t *sem, unsigned value)
{
  *sem = value;
  return;
}

extern void lolevel_sem_wait(const void* sem);
void sem_wait(sem_t *sem){
  lolevel_sem_wait(sem);
  return;
}
extern void lolevel_sem_post(const void* sem);
void sem_post(sem_t *sem){
  lolevel_sem_post(sem);
  return;
}

// void sem_destroy(sem_t *sem)
// {
//   //FIXME: I doesn't work.
//   while (*sem != 0)
//   {
//     asm volatile("nop");
//   }
//   return;
// }

void sleep(int ms){
  asm volatile( "mov r0, %1 \n" // assign r0 = name
                "svc %0     \n" // make system call
              : 
              : "I" (SYS_SLEEP), "r" (ms)
              : "r0" );
};

int shm_open(const char *name){
  int r;
  asm volatile( "mov r0, %2 \n" // assign r0 = name
                "svc %1     \n" // make system call
                "mov %0, r0 \n" // assign r = r0
              : "=r" (r)
              : "I" (SYS_SHM_OPEN), "r" (name)
              : "r0" );
  return r;
};
int shm_unlink(const char *name){
  int r;
  asm volatile( "mov r0, %2 \n" // assign r0 = name
                "svc %1     \n" // make system call
                "mov %0, r0 \n" // assign r = r0
              : "=r" (r)
              : "I" (SYS_SHM_UNLINK), "r" (name)
              : "r0" );
  return r;
};
typedef uint32_t off_t; //TODO: check if type valid
int ftruncate(int fildes, off_t length){
  int r;
  asm volatile( "mov r0, %2 \n" // assign r0 = fildes
                "mov r1, %3 \n" // assign r1 = length
                "svc %1     \n" // make system call
                "mov %0, r0 \n" // assign r = r0
              : "=r" (r)
              : "I" (SYS_FTRUNCATE), "r" (fildes), "r" (length)
              : "r0", "r1" );
  return r;
};
void *mmap(int fildes){
  uint32_t *p;
  asm volatile( "mov r0, %2 \n" // assign r0 = fd
                "svc %1     \n" // make system call
                "mov %0, r0 \n" // assign r = r0
              : "=r" (p)
              : "I" (SYS_MMAP), "r" (fildes)
              : "r0");
  return p;
};
// void *mmap(void *addr, size_t len){
//   uint32_t *p;
//   asm volatile( "mov r0, %2 \n" // assign r0 = addr
//                 "mov r1, %3 \n" // assign r1 = len
//                 "svc %1     \n" // make system call
//                 "mov %0, r0 \n" // assign r = r0
//               : "=r" (p)
//               : "I" (SYS_MMAP), "r" (addr), "r" (len)
//               : "r0", "r1" );
//   return p;
// };
int munmap(void *addr, size_t len){
  int r;
  asm volatile( "mov r0, %2 \n" // assign r0 = fildes
                "mov r1, %3 \n" // assign r1 = length
                "svc %1     \n" // make system call
                "mov %0, r0 \n" // assign r = r0
              : "=r" (r)
              : "I" (SYS_FTRUNCATE), "r" (addr), "r" (len)
              : "r0", "r1" );
  return r;
};