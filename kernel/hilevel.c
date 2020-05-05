/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t procTab[MAX_PROCS];
pcb_t *executing = NULL;
uint32_t procCount = 0;

/* data structures for ofd
 * openFd : list of ofd stored in the kernel
 * openFdCount : number of ofd
 * ofd_ht : hashtable that maps (char*) name to (int) &ofd
 */
ofd_t openFd[OFD_MAX];
uint32_t openFdCount = 0;

// scheduler related, ticker simulates a RTC, incremented every millisecond
uint32_t ticker = 0;
processType_t schedulerProcessType;

// stack segment related, sp points to tos of next stack frame
uint32_t sp;
bool stack_used[MAX_PROCS];
extern uint32_t _stack_start;
extern uint32_t _stack_end;

void dispatch(ctx_t *ctx, pcb_t *prev, pcb_t *next)
{
  char prev_pid = '?', next_pid = '?';

  if (NULL != prev)
  {
    memcpy(&prev->ctx, ctx, sizeof(ctx_t)); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if (NULL != next)
  {
    memcpy(ctx, &next->ctx, sizeof(ctx_t)); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
    // context switch the page table for the MMU
    if( NULL != next->T_pt){
      mmu_set_ptr0( next->T );
      mmu_flush();
      mmu_enable();
    }
    else{
      mmu_unable();
    }
  }

  PL011_putc(UART0, '[', true);
  if(prev->pid < 10){
    PL011_putc(UART0, prev_pid, true);
  }
  else{
    PL011_putc(UART0, ((prev->pid % 100) / 10) + '0' , true);
    PL011_putc(UART0, ((prev->pid %  10) /  1) + '0' , true);
  }
  PL011_putc(UART0, '-', true);
  PL011_putc(UART0, '>', true);
  if(next->pid < 10){
    PL011_putc(UART0, next_pid, true);
  }
  else{
    PL011_putc(UART0, ((next->pid % 100) / 10) + '0' , true);
    PL011_putc(UART0, ((next->pid %  10) /  1) + '0' , true);
  }
  PL011_putc(UART0, ']', true);

  executing = next; // update   executing process to P_{next}

  return;
}

void schedule(ctx_t *ctx)
{
  /* A multi-level queue scheduling
   * The two queues are `IO` process, i.e. the console;
   * and `user` process, i.e. executed from the console.
   * 
   * Each queue is FCFS (First come first served),
   * where the pcb->schedule.lastExec stores the last executed time
   */
  pcb_t *prev = NULL;
  pcb_t *next = NULL;

  // find prev PCB
  for (int i = 0; i < procCount; i++)
  {
    if (procTab[i].pid == executing->pid)
    {
      prev = &procTab[i];
    }
  }
  // update scheduler info, for FCFS queue
  if(prev != NULL){
    prev->schedule.lastExec = ticker;
  }

  // find the longest waiting process for each queue
  uint32_t oldest[PROCESS_TYPES];
  pcb_t *oldestPcb[PROCESS_TYPES];
  bool processExist[PROCESS_TYPES] = {false};

  for (int i = 0; i < PROCESS_TYPES; i++)
  {
    oldest[i] = -1;
  }

  /* The longest waiting process have the smallest last executed time
   * processExist keep track whether a queue is empty (i.e. only the console process)
   * oldestPcb keep track of the longest waiting process in a queue
   */
  for (int i = 0; i < procCount; i++)
  {
    pcb_t *ith = &procTab[i];
    if (ith->status == STATUS_READY &&
        ith->schedule.lastExec < oldest[ith->schedule.type])
    {
      oldest[ith->schedule.type] = ith->schedule.lastExec;
      oldestPcb[ith->schedule.type] = ith;
      processExist[ith->schedule.type] = true;
    }
  }

  for (int offset = 0; offset < PROCESS_TYPES; offset++)
  {
    int target = (schedulerProcessType + offset) % PROCESS_TYPES;
    if (processExist[target])
    {
      next = oldestPcb[target];
      break;
    }
  }

  // context switch to another process if there exist one
  if (next != NULL)
  {
    dispatch(ctx, prev, next);
    // Mark the previous halted process if it is not a termianted/ invalid process
    if (prev != NULL && prev->status == STATUS_EXECUTING)
    {
      prev->status = STATUS_READY;
    }
    next->status = STATUS_EXECUTING;

    // the next call to scheduler will schedule a process from another queue, (process type)
    schedulerProcessType = (next->schedule.type + 1) % PROCESS_TYPES;
  }

  return;
}


extern void main_P1();
extern uint32_t tos_P1;
extern void main_console();
extern uint32_t tos_console;

void hilevel_handler_rst(ctx_t *ctx)
{
  // Invalidate all entries in the process table
  for (int i = 0; i < MAX_PROCS; i++)
  {
    procTab[i].status = STATUS_INVALID;
  }

  mmu_set_dom( 0, 0x3 ); // set domain 0 to 11_{(2)} => manager (i.e., not checked)
  mmu_set_dom( 1, 0x1 ); // set domain 1 to 01_{(2)} => client  (i.e.,     checked)

  // initialise 0-th PCB = Console
  memset(&procTab[0], 0, sizeof(pcb_t));
  for (int i = 0; i < 4096; i++)
  {
    procTab[0].T[i] = ((pte_t)(i) << 20) | 0x00C02;
  }
  procTab[0].T_pt = procTab->T;
  procTab[0].pid = 1;
  procTab[0].status = STATUS_READY;
  procTab[0].tos = (uint32_t)(&tos_console);
  procTab[0].ctx.cpsr = 0x50;
  procTab[0].ctx.pc = (uint32_t)(&main_console);
  procTab[0].ctx.sp = procTab[0].tos;
  procTab[0].schedule.type = PROCESS_IO;
  procTab[0].schedule.lastExec = 0;
  procCount++;


  schedulerProcessType = PROCESS_IO;
  schedule(ctx);

  // update pointer for stack segment
  for(int i = 0; i < MAX_PROCS; i++){
    stack_used[i] = false;
  }
  // sp = (uint32_t)&_stack_end;


  TIMER0->Timer1Load = TIMER0_INTERVAL; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl = 0x00000002;      // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040;     // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020;     // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080;     // enable          timer

  TIMER0->Timer2Load = MILLISECOND_INTERVAL; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer2Ctrl = 0x00000002;      // select 32-bit   timer
  TIMER0->Timer2Ctrl |= 0x00000040;     // select periodic timer
  TIMER0->Timer2Ctrl |= 0x00000020;     // enable          timer interrupt
  TIMER0->Timer2Ctrl |= 0x00000080;     // enable          timer

  GICC0->PMR = 0x000000F0;         // unmask all            interrupts
  GICD0->ISENABLER1 |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR = 0x00000001;        // enable GIC interface
  GICD0->CTLR = 0x00000001;        // enable GIC distributor

  int_enable_irq();

  PL011_putc(UART0, 'K', true);
  return;
}

/* Implements the exec interface*/
void exec_(ctx_t *ctx)
{
  ctx->sp = executing->tos;
  ctx->pc = ctx->gpr[0];
  // resets the file descriptor
  executing->fdCount = 0;
  memset(&executing->fdActive, 0, sizeof(executing->fdActive));
}

/* Implements the fork interface
 * creates a new PCB entry which preserve the ctx and stack memory but with a unique PID
 */
void fork_(ctx_t *ctx)
{
  // locate a free pcb entry
  pcb_t *e = NULL;
  uint32_t eIdx = 0;
  for (int i = 0; i < procCount; i++)
  {
    if (procTab[i].status == STATUS_TERMINATED)
    {
      e = &procTab[i];
      eIdx = i;
      break;
    }
  }
  if (e == NULL)
  {
    eIdx = procCount++;
    e = &procTab[eIdx];
  }
  // copy ctx from parent to child process
  memset(e, 0, sizeof(pcb_t));
  e->ctx = *ctx;
  e->ctx.gpr[0] = 0;
  e->pid = eIdx + 1;
  e->status = STATUS_READY;
  e->schedule = executing->schedule;
  e->schedule.type = PROCESS_USR;
  e->parent = executing->pid;


  // Create a new stack segment
  uint32_t pTos = 0;
  for(int i = 0; i < MAX_PROCS; i++){
    if(stack_used[i] == false){
      stack_used[i] = true;
      e->stack_pos = i;
      pTos = (uint32_t)&_stack_end - STACK_SIZE * i;
      break;
    }
  }
  // uint32_t pTos = sp;
  // sp -= STACK_SIZE;
  int pAddr = ((int)pTos - STACK_SIZE) / STACK_SIZE;

  // copy stack memory from parent to child process
  executing->T[pAddr] &= ~0x08C00; // mask domain
  executing->T[pAddr] |= 0x00C00;  // set  access =  011_{(2)} => full access
  mmu_flush();
  memcpy((uint32_t *)(pTos - STACK_SIZE), (uint32_t *)(executing->tos - STACK_SIZE), STACK_SIZE);
  executing->T[pAddr] &= ~0x08C00; // mask domain
  executing->T[pAddr] |= 0x00000;  // set  access =  000_{(2)} => no access
  mmu_flush();
  

  // set up the page table for the new process
  e->T_pt = e->T; // mark there exist a page table for this process
  // creates a identity mapping
  for (int i = 0; i < 4096; i++)
  {
    e->T[i] = ((pte_t)(i) << 20) | 0x00C02;
  }

  // set protection for the stack memory
  int from = (int)&_stack_start / STACK_SIZE;
  int to = (int)&_stack_end / STACK_SIZE;
  // restrict access to all other stack segment
  for (int i = from; i < to; i++)
  {
    e->T[i] &= ~0x001E0; // mask domain
    e->T[i] |= 0x00020;  // set  domain = 0001_{(2)} => client
    e->T[i] &= ~0x08C00; // mask access
    e->T[i] |= 0x00000;  // set  access =  000_{(2)} => no access
  }
  /*
   * All processes share the same view of their stack address,
   * where tos = _stack_end,
   * however the console does not have a virtual address space
   */
  // map virtual segment to current segment
  int vAddr = to - 0x1;
  e->T[vAddr] = e->T[pAddr];
  // grant access to current stack segment
  e->T[vAddr] &= ~0x08C00; // mask access
  e->T[vAddr] |= 0x00C00;  // set access = 011_{(2)} => full access

  e->tos = (uint32_t)&_stack_end;
  e->ctx.sp = e->tos - executing->tos + ctx->sp;

  // return child process PID to parent process
  ctx->gpr[0] = e->pid;
}

// kill process and child processes of process p
void terminateChild(pcb_t *p)
{
  // terminate child
  for (int i = 0; i < procCount; i++)
  {
    if (procTab[i].parent == p->pid)
    {
      terminateChild(&procTab[i]);
    }
  }
  // terminate self
  stack_used[p->stack_pos] = false;
  p->status = STATUS_TERMINATED;
}

void hilevel_handler_svc(ctx_t *ctx, uint32_t id)
{
  switch (id)
  {
  case 0x00:
  { // 0x00 => yield()
    schedule(ctx);
    break;
  }
  case 0x01:
  { // 0x01 => write( fd, x, n )
    int fd = (int)(ctx->gpr[0]);
    char *x = (char *)(ctx->gpr[1]);
    int n = (int)(ctx->gpr[2]);

    for (int i = 0; i < n; i++)
    {
      PL011_putc(UART0, *x++, true);
    }

    ctx->gpr[0] = n;

    break;
  }
  case 0x03:
  { // 0x03 => fork
    fork_(ctx);
    break;
  }
  case 0x04:
  { // 0x04 => exit
    executing->status = STATUS_TERMINATED;
    terminateChild(executing);
    schedule(ctx);
    break;
  }
  case 0x05:
  { // 0x05 => exec( add )
    exec_(ctx);
    break;
  }
  case 0x06:
  { // 0x06 => kill
    pid_t pid = (pid_t)ctx->gpr[0];
    for (int i = 0; i < procCount; i++)
    {
      if (procTab[i].pid == pid)
      {
        terminateChild(&procTab[i]);
      }
    }
    schedule(ctx);
    ctx->gpr[0] = 1;
    break;
  }
  case 0x08:
  { // sleep(sec)
    executing->slp_sec = ctx->gpr[0];
    executing->status = STATUS_WAITING;
    schedule(ctx);
    break;
  }
  default:
  { // 0x?? => unknown/unsupported
    break;
  }
  }

  return;
}

// decrement the seconds elapsed for sleeping process and wake up process after sleep
void resume_sleep_process()
{
  for(int i = 0; i < procCount; i++){
    pcb_t *c = &procTab[i];
    if(c->status == STATUS_WAITING){
      if(c->slp_sec <= 1){
        c->status = STATUS_READY;
      }
      else{
        c->slp_sec -= 1;
      }
    }
  }
  return;
}

void hilevel_handler_irq(ctx_t *ctx)
{
  uint32_t id = GICC0->IAR;

  if (id == GIC_SOURCE_TIMER0)
  {
    if(TIMER0->Timer1RIS == 1){
      // Timer 1 to invoke the scheduler for pre-emptive scheduling
      PL011_putc(UART0, '.', true);
      ticker ++;
      schedule(ctx);
      TIMER0->Timer1IntClr = 0x01;
    }
    else if(TIMER0->Timer2RIS == 1){
      // Timer 2 to restore sleeping processes
      resume_sleep_process();
      TIMER0->Timer2IntClr = 0x01;
    }
  }

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_pab() {
  return;
}

void hilevel_handler_dab(ctx_t *ctx) {
  PL011_putc(UART0, '?', true);
  executing->status = STATUS_INVALID;
  schedule(ctx);
  return;
}
