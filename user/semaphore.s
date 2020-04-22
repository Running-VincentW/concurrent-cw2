.global lolevel_sem_post
.global lolevel_sem_wait

lolevel_sem_post: ldrex     r1, [r0]     @ assign r1 = x
                  add   r1, r1, #1       @ r1 = r1 + 1
                  strex r2, r1, [r0]     @ r2 <= x == r1
                  cmp   r2, #0           @    r ?= 0
                  bne   lolevel_sem_post @ if r != 0, retry
                  dmb                    @ memory barrier
                  bx    lr               @ return

lolevel_sem_wait: ldrex     r1, [r0]     @ assign r1 = sem
                  cmp   r1, #0           @    r1 ?= 0
                  beq   lolevel_sem_wait @ if r1 == 0, retry
                  sub   r1, r1, #1       @ r1 = r1 - 1
                  strex r2, r1, [r0]     @ r <= sem == r1
                  cmp   r2, #0           @    r ?= 0
                  bne   lolevel_sem_wait @ if r != 0, retry
                  dmb                    @ memory barrier
                  bx    lr               @ return
