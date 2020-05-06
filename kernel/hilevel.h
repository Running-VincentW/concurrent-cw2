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
#define SCHEDULER_INTERVAL 0x000169B8
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

typedef uint32_t pte_t;

typedef struct {
        pid_t                pid; // Process IDentifier (PID)
     status_t             status; // current status
     uint32_t                tos; // address of Top of Stack (ToS)
     uint32_t                bos; // address of Bottom of Stack
        ctx_t                ctx; // execution context
  scheduler_t           schedule; // scheduler info
     uint32_t            slp_sec; // sleep duration upon sleep sys call
        pte_t T[ 4096 ] __attribute__ ((aligned (1 << 14))); // page table
        pte_t              *T_pt; // indicates if a page table exist
        pid_t             parent; // pid of parent
} pcb_t;

#endif
