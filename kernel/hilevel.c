/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

// process
pcb_t procTab[MAX_PROCS];
pcb_t *executing = NULL;
uint32_t procCount = 0;

// scheduler related, ticker simulates a RTC, incremented every millisecond
uint32_t ticker = 0;
processType_t schedulerProcessType;

// stack segment related, sp points to tos of next stack frame
uint32_t stack_count; // count the number of stack frames in use
bool stack_used[MAX_PROCS]; // indicates a stack frame is in use
uint32_t stack_process[MAX_PROCS]; // indicates which processes uses a stack frame
extern uint32_t _stack_start;
extern uint32_t _stack_end;
int stackC; // total number of stack frames

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
  for (int i = 0; i < MAX_PROCS; i++)
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
  for (int i = 0; i < MAX_PROCS; i++)
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
  procTab[0].bos = procTab[0].tos - STACK_SIZE;
  procTab[0].ctx.cpsr = 0x50;
  procTab[0].ctx.pc = (uint32_t)(&main_console);
  procTab[0].ctx.sp = procTab[0].tos;
  procTab[0].schedule.type = PROCESS_IO;
  procTab[0].schedule.lastExec = 0;
  procCount++;


  schedulerProcessType = PROCESS_IO;
  schedule(ctx);

  // mark stack segment as empty
  for(int i = 0; i < MAX_PROCS; i++){
    stack_used[i] = false;
  }
  stack_count = 0;
  stackC = ((int)&_stack_end - (int)&_stack_start) / STACK_SIZE -1;

  TIMER0->Timer1Load = SCHEDULER_INTERVAL; // select period = 2^20 ticks ~= 1 sec
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
}

// Creates/ expands to a new page for the stack segment, and returns the physical address of the page frame
uint32_t newStackPage(pcb_t* process)
{
  PL011_putc(UART0, '+', true);
  // locate an available physical page frame
  uint32_t frameAddr = 0;
  for(int i = 0; i < stackC; i++){
    if(stack_used[i] == false){
      stack_used[i] = true;
      stack_count ++;
      stack_process[i] = process->pid;
      // frameAddr: the address of the page frame
      frameAddr = (uint32_t)&_stack_end - STACK_SIZE * (i+1);
      break;
    }
  }
  if (frameAddr == 0){
    process->status = STATUS_INVALID;
    return -1;
  }
  // update the page table
  int pFrameIdx = frameAddr / STACK_SIZE;
  process->bos -= STACK_SIZE;
  int pageIdx = process->bos / STACK_SIZE;

  // map the virtual address of page to page frame
  process->T[pageIdx] = process->T[pFrameIdx];
  // grant access to current stack segment
  process->T[pageIdx] &= ~0x08C00; // mask access
  process->T[pageIdx] |= 0x00C00;  // set access = 011_{(2)} => full access

  return frameAddr;
}

/* Implements the fork interface
 * creates a new PCB entry which preserve the ctx and stack memory but with a unique PID
 */
void fork_(ctx_t *ctx)
{
  // locate a free pcb entry
  pcb_t *e = NULL;
  int eIdx = 0;
  for (int i = 0; i < MAX_PROCS; i++)
  {
    if (procTab[i].status == STATUS_TERMINATED || procTab[i].status == STATUS_INVALID)
    {
      e = &procTab[i];
      eIdx = i;
      break;
    }
  }

  int pageCount = (executing->tos - executing->bos) / STACK_SIZE; // pages of stack in executing process
  bool valid = stack_count + pageCount < stackC;
  /* Return -1 if fork fails, possible reasons are
   * i) insufficient free PCB entries (limited by MAX_PROCS),
   * ii) insufficient free page frames for stack pages to map to.
   */
  if (e == NULL || valid == false)
  {
    ctx->gpr[0] = -1;
    PL011_putc(UART0, 'X', true);
    return;
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
  e->tos = (uint32_t)&_stack_end;
  e->bos = (uint32_t)&_stack_end;
  e->ctx.sp = e->tos - executing->tos + ctx->sp; // can replace this with = ctx->sp
  procCount ++;


  /* Initialize the page table*/
  e->T_pt = e->T; // mark the existence of a page table
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


  /* Virtual Memory and pages*/
  // int pageCount = (executing->tos - executing->bos) / STACK_SIZE; // pages of stack in executing process
  int swpIdx = (uint32_t)&_stack_end / STACK_SIZE;
  uint32_t* swpAddr = &_stack_end;

  for (int i = 1; i <= pageCount; i++){
    // create a new page in the new processes per page in executing process
    uint32_t frameAddr = newStackPage(e);
    int pFrameIdx = frameAddr / STACK_SIZE;
    /* swpAddr will act as a temporary address for us to copy 
     * from the page frame of the executing process to the new process.
     * entry executing->T[swpIdx] will make sure the temporary address 
     * maps to the physical page frame corresponding to the new process.
     */
    pte_t swp = executing->T[swpIdx];
    executing->T[swpIdx] = executing->T[pFrameIdx];
    executing->T[swpIdx] &= ~0x08C00; // mask domain
    executing->T[swpIdx] |= 0x00C00;  // set  access =  011_{(2)} => full access
    mmu_flush();
    memcpy(swpAddr, (uint32_t *)(executing->tos - STACK_SIZE * i), STACK_SIZE);
    executing->T[swpIdx] = swp;
    mmu_flush();
  }

  // return child process PID to parent process
  ctx->gpr[0] = e->pid;
}

// kill process and child processes of process p
void terminateChild(pcb_t *p)
{
  // terminate child
  for (int i = 0; i < MAX_PROCS; i++)
  {
    if (procTab[i].parent == p->pid)
    {
      terminateChild(&procTab[i]);
    }
  }
  // terminate self
  for (int i = 0; i < stackC; i++)
  {
    if (stack_process[i] == p->pid)
    {
      stack_process[i] = 0;
      stack_used[i] = false;
      stack_count --;
    }
  }
  procCount --;
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
    for (int i = 0; i < MAX_PROCS; i++)
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
  for(int i = 0; i < MAX_PROCS; i++){
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
  uint32_t* addr = (uint32_t*)ctx->gpr[3];
  // allocate a new page frame if the stack overflow the existing pages
  if (ctx->sp < executing->bos){
    PL011_putc(UART0, '*', true);
    uint32_t r = newStackPage(executing);
    // terminate process if there is insufficient stack space for the executing process
    if(r == -1){
      terminateChild(executing);
      schedule(ctx);
    }
  }
  else{
    PL011_putc(UART0, '?', true);
    executing->status = STATUS_INVALID;
    schedule(ctx);
  }
  return;
}
