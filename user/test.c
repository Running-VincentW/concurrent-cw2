#include "test.h"

extern void main_Tests();

/* Test if the kernel blocks access to other page frame*/
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

/* This test if the kernel if able to handle stack memory that overflows a page.
 * 
 * Testing procedures:
 * - `watch stack_process`     set a watch point to see which page frame is allocated to which process
 * - `break testStackOverflow` break the current test function to inspect variables
 * - `execute tests`           run the test
 * - `p/x &a`                  inspect the value of &a lies in page 0x721XXXXX,
 * - `p/x executing->bos`      should reveal bottom of stack is now 0x72100000 (was 0x72200000).
 */
void testStackOverflow()
{
  write(STDOUT_FILENO, "\ntest stack memory overflow a page ", 35);
  char a[0xFFFE9];
  strcpy(a, "abc");
  if (strcmp(a, "abc") == 0)
  {
    write(STDOUT_FILENO, "[OK]\n", 6);
  }
  else
  {
    write(STDOUT_FILENO, "[FAIL]\n", 8);
  }
  exit(EXIT_SUCCESS);
}

void testSimpleFork()
{
  int a = 0;
  a++;
  pid_t pid = fork();

  if (pid > 0)
  {
    write(STDOUT_FILENO, "parent process [OK]\n", 21);
  }
  else if (pid == 0)
  {
    write(STDOUT_FILENO, "child process ", 15);
    if (a == 1)
    {
      write(STDOUT_FILENO, "[OK]\n", 6);
    }
    else
    {
      write(STDOUT_FILENO, "[FAIL]\n", 8);
    }
  }
  sleep(3000);
  exit(EXIT_SUCCESS);
}

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
    testStackOverflow();
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
