#include "test.h"

extern void main_Tests(); 

void testStackProtection(){
    // attempt access to the top of stack, assuming this is not using the top stack
    uint32_t* ptr = (uint32_t*) 0x70700000;
    write(STDOUT_FILENO, "\n[Test] data access to other stack\n", 36);
    int a = *ptr;
    write(STDOUT_FILENO, "[failed!]\n", 11);
    return;
}

void testVirtualMem(){
  for (int i = 0 ; i < 5 ; i++){
    int s = fork();
    if(s == 0){
      char x = '0' + i;
      write(STDOUT_FILENO, &x, 1);
      exit(EXIT_SUCCESS);
    }
  }
}

void main_Tests() {
  int s = fork();
  if (s==0){
      testStackProtection();
      exit( EXIT_SUCCESS );
  }
  s = fork();
  if (s==0){
      testVirtualMem();
      exit( EXIT_SUCCESS );
  }
  
  exit( EXIT_SUCCESS );
}
