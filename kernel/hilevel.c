/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */


#include "hilevel.h"

/* We assume there will be two user processes, stemming from execution of the 
 * two user programs P1 and P2, and can therefore
 * 
 * - allocate a fixed-size process table (of PCBs), and then maintain an index 
 *   into it to keep track of the currently executing process, and
 * - employ a fixed-case of round-robin scheduling: no more processes can be 
 *   created, and neither can be terminated, so assume both are always ready
 *   to execute.
 */

pcb_t procTab[ MAX_PROCS ]; pcb_t* executing = NULL; uint32_t procCount = 0;

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );

    executing = next;                           // update   executing process to P_{next}

  return;
}
void schedule( ctx_t* ctx ) {
  // schedule the process with the higest priority score that is ready
  pcb_t* prev = NULL; pcb_t* next = NULL;
  uint8_t prevIdx, nextIdx;
  uint8_t highest = 0;
  for (int i = 0; i < procCount; i++){
    if(procTab[i].pid == executing->pid){
      prev = &procTab[i];
      prevIdx = i;
    }
    if(procTab[i].status == STATUS_READY && procTab[i].priority > highest){
      highest = procTab[i].priority;
      next = &procTab[i];
      nextIdx = i;
    }
  }
  // context switch to another process if exist one
  if (next != NULL){
    dispatch(ctx, prev, next);
    if(prev != NULL && prev->status != STATUS_TERMINATED){
      procTab[prevIdx].status = STATUS_READY;
    }
    procTab[nextIdx].status = STATUS_EXECUTING;
  }

  // if     ( executing->pid == procTab[ 0 ].pid ) {
  //   dispatch( ctx, &procTab[ 0 ], &procTab[ 1 ] );  // context switch P_1 -> P_2

  //   procTab[ 0 ].status = STATUS_READY;             // update   execution status  of P_1 
  //   procTab[ 1 ].status = STATUS_EXECUTING;         // update   execution status  of P_2
  // }
  // else if( executing->pid == procTab[ 1 ].pid ) {
  //   dispatch( ctx, &procTab[ 1 ], &procTab[ 0 ] );  // context switch P_2 -> P_1

  //   procTab[ 1 ].status = STATUS_READY;             // update   execution status  of P_2
  //   procTab[ 0 ].status = STATUS_EXECUTING;         // update   execution status  of P_1
  // }

  return;
}

extern void     main_P1(); 
extern uint32_t tos_P1;
extern void     main_P2(); 
extern uint32_t tos_P2;
extern void     main_console(); 
extern uint32_t tos_console;

void hilevel_handler_rst( ctx_t* ctx              ) { 
  /* Invalidate all entries in the process table, so it's clear they are not
   * representing valid (i.e., active) processes.
   */

  for( int i = 0; i < MAX_PROCS; i++ ) {
    procTab[ i ].status = STATUS_INVALID;
  }

  /* Automatically execute the user programs P1 and P2 by setting the fields
   * in two associated PCBs.  Note in each case that
   *    
   * - the CPSR value of 0x50 means the processor is switched into USR mode, 
   *   with IRQ interrupts enabled, and
   * - the PC and SP values match the entry point and top of stack. 
   */

  memset( &procTab[ 0 ], 0, sizeof( pcb_t ) ); // initialise 0-th PCB = P_1
  procTab[ 0 ].pid      = 1;
  procTab[ 0 ].status   = STATUS_READY;
  procTab[ 0 ].tos      = ( uint32_t )( &tos_P1  );
  procTab[ 0 ].ctx.cpsr = 0x50;
  procTab[ 0 ].ctx.pc   = ( uint32_t )( &main_P1 );
  procTab[ 0 ].ctx.sp   = procTab[ 0 ].tos;
  procTab[ 0 ].priority = 2;
  procCount ++;

  memset( &procTab[ 1 ], 0, sizeof( pcb_t ) ); // initialise 1-st PCB = P_2
  procTab[ 1 ].pid      = 2;
  procTab[ 1 ].status   = STATUS_INVALID;
  procTab[ 1 ].tos      = ( uint32_t )( &tos_P2  );
  procTab[ 1 ].ctx.cpsr = 0x50;
  procTab[ 1 ].ctx.pc   = ( uint32_t )( &main_P2 );
  procTab[ 1 ].ctx.sp   = procTab[ 1 ].tos;
  procTab[ 1 ].priority = 1;
  procCount ++;

  memset( &procTab[ 2 ], 0, sizeof( pcb_t ) ); // initialise 2-nd PCB = Console
  procTab[ 2 ].pid      = 3;
  procTab[ 2 ].status   = STATUS_READY;
  procTab[ 2 ].tos      = ( uint32_t )( &tos_console  );
  procTab[ 2 ].ctx.cpsr = 0x50;
  procTab[ 2 ].ctx.pc   = ( uint32_t )( &main_console );
  procTab[ 2 ].ctx.sp   = procTab[ 2 ].tos;
  procTab[ 2 ].priority = -1;
  procCount ++;

  /* Once the PCBs are initialised, we arbitrarily select the 0-th PCB to be 
   * executed: there is no need to preserve the execution context, since it 
   * is invalid on reset (i.e., no process was previously executing).
   */

  schedule(ctx);
  // dispatch( ctx, NULL, &procTab[ 0 ] );

  /* Configure the mechanism for interrupt handling by
   *
   * - configuring timer st. it raises a (periodic) interrupt for each 
   *   timer tick,
   * - configuring GIC st. the selected interrupts are forwarded to the 
   *   processor via the IRQ interrupt signal, then
   * - enabling IRQ interrupts.
   */

  TIMER0->Timer1Load  = TIMER0_INTERVAL; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();
  PL011_putc(UART0, 'K', true);

  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) { 
  /* Based on the identifier (i.e., the immediate operand) extracted from the
   * svc instruction, 
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    // case 0x00 : { // 0x00 => yield()
    //   schedule( ctx );

    //   break;
    // }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );  
      char*  x = ( char* )( ctx->gpr[ 1 ] );  
      int    n = ( int   )( ctx->gpr[ 2 ] ); 

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }
      
      ctx->gpr[ 0 ] = n;

      break;
    }
    case 0x03 : { // 0x03 => fork
      uint32_t c = procCount ++;
      if(c == 0){
        ctx->gpr[ 0 ] = 0; //todo: do something to claim sys resources back
      }
      else{
        memset( &procTab[ c ], 0, sizeof( pcb_t ) );
        procTab[c].ctx = *ctx;
        procTab[c].ctx.gpr[ 0 ] = 0;
        procTab[c].pid      = c + 1;
        procTab[c].tos = executing->tos;
        procTab[c].priority = executing->priority;
        procTab[c].status = STATUS_READY;

        ctx->gpr[ 0 ] = procTab[c].pid;
      }
      break;
    }
    case 0x04: { // 0x04 => exit
      executing->status = STATUS_TERMINATED;
      schedule( ctx );
      break;
    }
    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}

void hilevel_handler_irq( ctx_t* ctx ){
  PL011_putc( UART0, '!',      true );
  // Step 2: read the interrupt identifier so we know the source.
  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt to perform a context switch, then reset the timer.
  if( id == GIC_SOURCE_TIMER0 ){
    schedule( ctx );
    TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}