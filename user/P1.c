/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "P1.h"

extern void main_P2(); 

void main_P1() {
  // while( 1 ) {
  //   write( STDOUT_FILENO, "+", 1 );
  // }
  pid_t pid = fork();

  if (pid > 0){
    write(STDOUT_FILENO, "parent", 6);
  }
  else if (pid == 0){
    write(STDOUT_FILENO, "child", 5);
    // exec(&main_P2);
  }

  exit( EXIT_SUCCESS );
}
