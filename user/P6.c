// C program to demonstrate working of Semaphores 
// #include <pthread.h> 
// #include <semaphore.h> 
#include "libc.h"
#include "hash_table.h"

sem_t mutex = 0; 

// void thread(int pid) 
// { 
// 	//wait 
// 	sem_wait(&mutex); 
//     write( STDOUT_FILENO, "Entered", 7 );

// 	//critical section 
// 	sleep(4); 
	
// 	//signal 
//     write( STDOUT_FILENO, "Exiting", 7 );
// 	sem_post(&mutex);
//     exit( EXIT_SUCCESS );
// } 

void testeq(int a, int b){
    if (a == b){
        write(STDOUT_FILENO, "Y", 1);
    }
    else{
        write(STDOUT_FILENO, "N", 1);
    }
}
void testneq(int a, int b){
    if (a != b){
        write(STDOUT_FILENO, "Y", 1);
    }
    else{
        write(STDOUT_FILENO, "N", 1);
    }
}
typedef struct{
    int a;
    int b[5];
} dummy_t;
void main_P6() 
{
    ht_hash_table* ht  = ht_new();
    write(STDOUT_FILENO, "HASHTABLE TEST", 15);
    ht_insert(ht, "valid", 123);
    testeq(123, ht_search(ht, "valid"));
    testeq(0, ht_search(ht, "invalid"));
    ht_delete(ht, "valid");
    int a = ht_search(ht, "valid");
    testneq(0, a);
    ht_insert(ht, "valid", 123);
    testeq(123, ht_search(ht, "valid"));
    ht_insert(ht, "valid", 1234);
    testeq(1234, ht_search(ht, "valid"));
    write(STDOUT_FILENO, "ALL TEST PASSED", 16);
    
    // test shared mem
    int shm_fd = shm_open("test");
    testneq(shm_fd, 0);
    ftruncate(shm_fd, sizeof(dummy_t));
    dummy_t* shm = mmap(shm_fd);
    testneq((int) shm, 0);
    memset(shm, 0, sizeof(dummy_t));
    shm->a = 1;
    shm->b[0]=1;
    shm->b[1]=2;
    shm->b[2]=3;
    shm->b[3]=4;
    shm->b[4]=5;
    
    // fork a new process to see if the shared memory is read;
    int s = fork();
    if(s == 0){
        // I am a child!
        int fd = shm_open("test");
        dummy_t* shm2 = mmap(shm_fd);
        testeq(shm->a, shm2->a);
        testeq(shm->b[0], shm->b[0]);
        testeq(shm->b[4], shm->b[4]);
    }

    exit( EXIT_SUCCESS );
} 
