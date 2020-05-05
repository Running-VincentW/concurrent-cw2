#include "test.h"

extern void main_Tests();

void testStackProtection()
{
  // attempt access to the top of stack
  uint32_t *ptr = (uint32_t *)0x70700000;
  write(STDOUT_FILENO, "\ndata access to other stack segment [?]\n", 41);
  int a = *ptr;
  // the dab handler should terminate this process and prints a '?'
  // Therefore the following shall not execute
  write(STDOUT_FILENO, "[FAIL]\n", 8);
  exit(EXIT_SUCCESS);
}

// void testSimpleFork()
// {
//   int a = 0;
//   a++;
//   pid_t pid = fork();

//   if (pid > 0)
//   {
//     write(STDOUT_FILENO, "parent process [OK]\n", 21);
//   }
//   else if (pid == 0)
//   {
//     write(STDOUT_FILENO, "child process ", 15);
//     if (a == 1)
//     {
//       write(STDOUT_FILENO, "[OK]\n", 6);
//     }
//     else
//     {
//       write(STDOUT_FILENO, "[FAIL]\n", 8);
//     }
//   }
//   sleep(3000);
//   exit(EXIT_SUCCESS);
// }

// void testStackAfterFork()
// {
//   for (int i = 0; i < 5; i++)
//   {
//     int s = fork();
//     if (s == 0)
//     {
//       char x = '0' + i;
//       write(STDOUT_FILENO, &x, 1);
//       exit(EXIT_SUCCESS);
//     }
//   }
//   sleep(3000);
//   exit(EXIT_SUCCESS);
// }

void main_Tests()
{
  int s = fork();
  if (s == 0)
  {
    testStackProtection();
  }
  
  // sleep(500);
  // s = fork();
  // if (s == 0)
  // {
  //   testSimpleFork();
  // }
  // 
  // sleep(500);
  // s = fork();
  // if (s == 0)
  // {
  //   testStackAfterFork();
  // }
  sleep(3000);
  exit(EXIT_SUCCESS);
}
