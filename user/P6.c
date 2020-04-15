// C program to demonstrate working of Semaphores 
// #include <stdio.h> 
// #include <pthread.h> 
// #include <semaphore.h> 
#include "libc.h"

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
	// sem_init(&mutex, 0, 0); 
    sem_wait(&mutex);
	// int s = fork();
    // if(s == 0){
    //     s = fork();
    //     if(s == 0){
    //         sleep(2);
    //         thread(s);
    //     }
    //     else{
    //         thread(s);
    //     }
    // }
	// sleep(2);
	// sem_destroy(&mutex); 
	exit( EXIT_SUCCESS );
} 
