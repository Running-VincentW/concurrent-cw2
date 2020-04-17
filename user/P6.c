// C program to demonstrate working of Semaphores 
// #include <stdio.h> 
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


void main_P6() 
{ 
    ht_hash_table* ht  = ht_new();
    // ht_del_hash_table(ht);
    ht_insert(ht, "test", 123);
    int a = ht_search(ht, "test");
	exit( EXIT_SUCCESS );
} 
