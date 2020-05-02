/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
#include "hash_table.h"

/* We assume there will be two user processes, stemming from execution of the 
 * two user programs P1 and P2, and can therefore
 * 
 * - allocate a fixed-size process table (of PCBs), and then maintain an index 
 *   into it to keep track of the currently executing process, and
 * - employ a fixed-case of round-robin scheduling: no more processes can be 
 *   created, and neither can be terminated, so assume both are always ready
 *   to execute.
 */

pcb_t procTab[MAX_PROCS];
ofd_t openFd[OFD_MAX];
uint32_t openFdCount = 0;
pcb_t *executing = NULL;
uint32_t procCount = 0;
uint32_t ticker = 0;
uint32_t sp;
processType_t schedulerProcessType;
ht_hash_table *ofd_ht;

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
    if( NULL != next->T_pt){
      mmu_set_ptr0( next->T );
      mmu_flush();
      mmu_enable();
    }
    else{
      mmu_unable();
      mmu_flush();
    }
  }

  PL011_putc(UART0, '[', true);
  PL011_putc(UART0, prev_pid, true);
  PL011_putc(UART0, '-', true);
  PL011_putc(UART0, '>', true);
  PL011_putc(UART0, next_pid, true);
  PL011_putc(UART0, ']', true);

  executing = next; // update   executing process to P_{next}

  return;
}

void schedule(ctx_t *ctx)
{
  pcb_t *prev = NULL;
  pcb_t *next = NULL;

  // find prev PIB
  for (int i = 0; i < procCount; i++)
  {
    if (procTab[i].pid == executing->pid)
    {
      prev = &procTab[i];
    }
  }
  if(prev != NULL){
    prev->schedule.lastExec = ticker;
  }
  // find the processes that have been waiting for longest
  uint32_t oldest[PROCESS_TYPES];
  for (int i = 0; i < PROCESS_TYPES; i++)
  {
    oldest[i] = -1;
  }
  pcb_t *oldestPcb[PROCESS_TYPES];
  bool processExist[PROCESS_TYPES] = {false};
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

  // context switch to another process if exist one
  if (next != NULL)
  {
    dispatch(ctx, prev, next);
    if (prev != NULL && prev->status == STATUS_EXECUTING)
    {
      prev->status = STATUS_READY;
    }
    next->status = STATUS_EXECUTING;
    // next->schedule.lastExec = ticker;
    schedulerProcessType = (next->schedule.type + 1) % PROCESS_TYPES;
  }

  return;
}

extern void main_P1();
extern uint32_t tos_P1;
extern void main_P2();
extern uint32_t tos_P2;
extern void main_console();
extern uint32_t tos_console;
extern uint32_t _stack_start;
extern uint32_t _stack_end;
extern uint32_t tos_svc;

void hilevel_handler_rst(ctx_t *ctx)
{
  /* Invalidate all entries in the process table, so it's clear they are not
   * representing valid (i.e., active) processes.
   */

  for (int i = 0; i < MAX_PROCS; i++)
  {
    procTab[i].status = STATUS_INVALID;
  }

  /* Automatically execute the user programs P1 and P2 by setting the fields
   * in two associated PCBs.  Note in each case that
   *    
   * - the CPSR value of 0x50 means the processor is switched into USR mode, 
   *   with IRQ interrupts enabled, and
   * - the PC and SP values match the entry point and top of stack. 
   */

  memset(&procTab[0], 0, sizeof(pcb_t)); // initialise 0-th PCB = P_1
  procTab[0].pid = 1;
  procTab[0].status = STATUS_READY;
  procTab[0].tos = (uint32_t)(&tos_P1);
  procTab[0].ctx.cpsr = 0x50;
  procTab[0].ctx.pc = (uint32_t)(&main_P1);
  procTab[0].ctx.sp = procTab[0].tos;
  procTab[0].schedule.type = PROCESS_USR;
  procTab[0].schedule.lastExec = 0;
  procCount++;

  memset(&procTab[1], 0, sizeof(pcb_t)); // initialise 1-st PCB = P_2
  procTab[1].pid = 2;
  procTab[1].status = STATUS_INVALID;
  procTab[1].tos = (uint32_t)(&tos_P2);
  procTab[1].ctx.cpsr = 0x50;
  procTab[1].ctx.pc = (uint32_t)(&main_P2);
  procTab[1].ctx.sp = procTab[1].tos;
  procTab[1].schedule.type = PROCESS_USR;
  procTab[1].schedule.lastExec = 0;
  procCount++;

  memset(&procTab[2], 0, sizeof(pcb_t)); // initialise 2-nd PCB = Console
  procTab[2].pid = 3;
  procTab[2].status = STATUS_READY;
  procTab[2].tos = (uint32_t)(&tos_console);
  procTab[2].ctx.cpsr = 0x50;
  procTab[2].ctx.pc = (uint32_t)(&main_console);
  procTab[2].ctx.sp = procTab[2].tos;
  procTab[2].schedule.type = PROCESS_IO;
  procTab[2].schedule.lastExec = 0;
  procCount++;

  /* Once the PCBs are initialised, we arbitrarily select the 0-th PCB to be 
   * executed: there is no need to preserve the execution context, since it 
   * is invalid on reset (i.e., no process was previously executing).
   */

  schedulerProcessType = PROCESS_IO;
  schedule(ctx);
  ofd_ht = ht_new();
  // dispatch( ctx, NULL, &procTab[ 0 ] );

  // initialise the open file descriptors hashtable

  /* Configure the mechanism for interrupt handling by
   *
   * - configuring timer st. it raises a (periodic) interrupt for each 
   *   timer tick,
   * - configuring GIC st. the selected interrupts are forwarded to the 
   *   processor via the IRQ interrupt signal, then
   * - enabling IRQ interrupts.
   */

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
  mmu_set_dom( 0, 0x3 ); // set domain 0 to 11_{(2)} => manager (i.e., not checked)
  mmu_set_dom( 1, 0x1 ); // set domain 1 to 01_{(2)} => client  (i.e.,     checked)

  PL011_putc(UART0, 'K', true);
  
  sp = (uint32_t)&_stack_end;
  return;
}
void exec_(ctx_t *ctx)
{
  ctx->sp = executing->tos;
  ctx->pc = ctx->gpr[0];
  // resets the file descriptor
  executing->fdCount = 0;
  memset(&executing->fdActive, 0, sizeof(executing->fdActive));
}
void fork_(ctx_t *ctx)
{
  // Create a PCB entry with same ctx but unique pid
  // TODO: do something to claim sys resources back
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

  // create new stack in stack segment, and copy content in memory over
  e->tos = sp;
  sp -= STACK_SIZE;
  mmu_unable();
  memcpy((uint32_t *)(e->tos - STACK_SIZE), (uint32_t *)(executing->tos - STACK_SIZE), STACK_SIZE);
  if (NULL != executing->T_pt)
  {
    mmu_enable();
  }

  // Memory virtualisation
  // set the virtual address space to an identity map
  e->T_pt = e->T;
  for (int i = 0; i < 4096; i++)
  {
    e->T[i] = ((pte_t)(i) << 20) | 0x00C02;
  }
  int from = (int)&_stack_start / 0x100000;
  int to = (int)&_stack_end / 0x100000;
  int current = ((int)e->tos - 0x100000) / 0x100000;
  // set protection for stack space
  for (int i = from; i < to; i++)
  {
    // grant no access to other stack segments
    e->T[i] &= ~0x001E0; // mask domain
    e->T[i] |= 0x00020;  // set  domain = 0001_{(2)} => client
    e->T[i] &= ~0x08C00; // mask access
    e->T[i] |= 0x00000;  // set  access =  000_{(2)} => no access
  }
  // grant access to current stack segment
  e->T[current] &= ~0x08C00; // mask access
  e->T[current] |= 0x00C00;  // set access = 011_{(2)} => full access

  e->ctx.sp = e->tos - executing->tos + ctx->sp;

  // return child process PID to parent process
  ctx->gpr[0] = e->pid;
}

ofd_t *createOfd(const char *name){
  uint32_t c = openFdCount++;
  memset(&openFd[c], 0, sizeof(ofd_t));
  openFd[c].active = true;
  strcpy(openFd[c].name, name);
  return &openFd[c];
}

void hilevel_handler_svc(ctx_t *ctx, uint32_t id)
{
  /* Based on the identifier (i.e., the immediate operand) extracted from the
   * svc instruction, 
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  switch (id)
  {
    case 0x00 : { // 0x00 => yield()
      schedule( ctx );
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
    schedule(ctx);
    break;
  }
  case 0x05:
  { // 0x05 => exec( add )
    exec_(ctx);
    break;
  }
  case 0x08:
  { // shmopen(name)
    if(executing->fdCount == OPEN_MAX){
      // file descriptor exceeded max limit
      ctx->gpr[0] = -1;
    }
    else{
      int fdIdx;
      // create a fd for the PCB
      for(int i = 0; i < OPEN_MAX; i++){
        if(executing->fdActive[i] == false){
          fdIdx = i;
          break;
        }
      }
      bool new = false;
      ofd_t *ofd = NULL;
      // lookup the open file descriptor by name
      int val = ht_search(ofd_ht, (char*)ctx->gpr[0]);
      if(val != 0){
        ofd = (ofd_t*) val;
      }
      else if (openFdCount < OFD_MAX){
        // if not found, create a open file descriptor
        new = true;
        for(int i = 0; i < OFD_MAX; i++){
          if(openFd[i].active == false){
            ofd = &openFd[i];
            break;
          }
        }
      }
      if(ofd != NULL){
        if(new == true){
          // initialize an ofd
          memset(ofd, 0, sizeof(ofd_t));
          strcpy(ofd->name, (char*)ctx->gpr[0]);
          ofd->active = true;
          ofd->bytes = 0;
          ofd->object = NULL;
          // create entry in hashtable lookup
          ht_insert(ofd_ht, (char*)ctx->gpr[0], (uint32_t)ofd);
        }
        // update fd
        executing->fdActive[fdIdx] = true;
        executing->fd[fdIdx] = ofd;
        // return fd
        ctx->gpr[0] = fdIdx;
      }
      else{
        ctx->gpr[0] = -1;
      }
    }
    break;
  }
  case 0x09:
  { // ftruncate (fd, len)
    // find ofd of fd

    // check if valid FD
    if(executing->fdActive[ctx->gpr[0]] == false){
      ctx->gpr[0] = -1;
    }
    else{
      // check if we need to realloc or malloc a shared memory object
      ofd_t* ofd = executing->fd[ctx->gpr[0]];
      void* ptr;
      uint32_t size = ctx->gpr[1];
      if(ofd->object == NULL){ // create a memory shared object
        ptr = malloc(size);
      }
      else if(ofd->object != NULL && size != ofd->bytes){ // resize the shared memory object
        ptr = malloc(size);
        if(size < ofd->bytes){
          memcpy(ptr, ofd->object, size);
        }
        else{
          memcpy(ptr, ofd->object, ofd->bytes);
        }
      }
      else{ // keep the shared memory object
        ptr = ofd->object;
      }
      if(ptr != NULL){
        if(ptr != ofd->object){
          free(ofd->object);
        }
        ofd->object = ptr;
        ofd->bytes = size;
        ctx->gpr[0] = 0;
      }
      else{
        ctx->gpr[0] = -1;
      }
    }
    break;
  }
  case 0x0A:
  {  // mmap (fd)
  // FIXME: I need to be a proper memory mapping
    if(executing->fdActive[ctx->gpr[0]]){
      ofd_t *ofd = executing->fd[ctx->gpr[0]];
      ctx->gpr[0] = (uint32_t) ofd->object;
      //TODO: store attach info
    }
    else{
      ctx->gpr[0] = 0;
    }
    break;
  }
  case 0x0B:
  { // munmap (addr, len)
    // TODO: we need to be able to dettach the process from a shared memory object
    break;
  }
  case 0x0C:
  { // shm_unlink (name)
    uint32_t r = ht_search(ofd_ht, (char*) ctx->gpr[0]);
    if(r == 0){
      ctx->gpr[0] = -1;
    }
    else{
      // TODO: check if any process is still mapped to it...
      ofd_t *ofd = (ofd_t*) r;
      // remove the shared memory object from heap mem
      free(ofd->object);
      // set ofd to unactive so that we can reuse it
      ofd->active = false;
      // remove the hashtable entry that maps the name to the ofd
      ht_delete(ofd_ht, (char*) ctx->gpr[0]);
      ctx->gpr[0] = 0;
    }

    break;
  }
  case 0x0D: // sleep(sec)
  {
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
  // Step 2: read the interrupt identifier so we know the source.
  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt to perform a context switch, then reset the timer.
  if (id == GIC_SOURCE_TIMER0)
  {
    if(TIMER0->Timer1RIS == 1){
      PL011_putc(UART0, '!', true);
      ticker ++;
      schedule(ctx);
      TIMER0->Timer1IntClr = 0x01;
    }
    else if(TIMER0->Timer2RIS == 1){
      // PL011_putc(UART0, 'R', true);
      resume_sleep_process();
      TIMER0->Timer2IntClr = 0x01;
    }
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_pab() {
  return;
}

void hilevel_handler_dab() {
  // should abort user process
  executing->status = STATUS_INVALID;
  return;
}
