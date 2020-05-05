/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#ifndef __HILEVEL_H
#define __HILEVEL_H

// Include functionality relating to newlib (the standard C library).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>
// #include <time.h>

// Include functionality relating to the platform.

#include   "GIC.h"
#include "PL011.h"
#include "SP804.h"
#include   "MMU.h"

// Include functionality relating to the   kernel.

#include "lolevel.h"
#include     "int.h"

/* The kernel source code is made simpler and more consistent by using 
 * some human-readable type definitions:
 *
 * - a type that captures a Process IDentifier (PID), which is really
 *   just an integer,
 * - an enumerated type that captures the status of a process, e.g.,
 *   whether it is currently executing,
 * - a type that captures each component of an execution context (i.e.,
 *   processor state) in a compatible order wrt. the low-level handler
 *   preservation and restoration prologue and epilogue, and
 * - a type that captures a process PCB.
 */

#define MAX_PROCS 30
#define TIMER0_INTERVAL 0x00043D28
#define MILLISECOND_INTERVAL 0x0000039E
#define STACK_SIZE 0x00100000
#define OPEN_MAX 10
#define OFD_MAX 25

typedef int pid_t;

#define PROCESS_TYPES 2
typedef enum{
  PROCESS_USR,
  PROCESS_IO
} processType_t;

typedef enum { 
  STATUS_INVALID,

  STATUS_CREATED,
  STATUS_TERMINATED,

  STATUS_READY,
  STATUS_EXECUTING,
  STATUS_WAITING
} status_t;

typedef struct {
  processType_t  type;
       uint32_t lastExec;
}scheduler_t;

typedef struct {
  uint32_t cpsr, pc, gpr[ 13 ], sp, lr;
} ctx_t;

// open file descriptors
// the ofd only support type shared memory object
typedef struct{
  bool active;
  char name[10];
  uint32_t *object;
  uint32_t bytes;
  // file offset, file status, file access modes
} ofd_t;

/* T is a page table, which, for the 1MB pages (or sections) we use,
 * has 4096 entries: note the need to align T to a multiple of 16kB.
 */

typedef uint32_t pte_t;

typedef struct {
        pid_t                pid; // Process IDentifier (PID)
     status_t             status; // current status
     uint32_t                tos; // address of Top of Stack (ToS)
     uint32_t          stack_pos;
        ctx_t                ctx; // execution context
  scheduler_t           schedule; // scheduler info
        ofd_t      *fd[OPEN_MAX];
         bool fdActive[OPEN_MAX];
      uint8_t            fdCount;
     uint32_t            slp_sec;
        pte_t T[ 4096 ] __attribute__ ((aligned (1 << 14)));
        pte_t              *T_pt;
        pid_t             parent;
} pcb_t;

#endif
