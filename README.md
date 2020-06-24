# concurrent-cw2
Kernel developed for the ARM based Cortex-A8 processor, which allows concurrent execution of processes via the scheduler and IPC.
The project also includes an implementation for solving the dining philosopher's problem, which demonstrates the concurrrent execution of processes via the kernel scheduler and IPC support.

## Execution instructions
Launch 4 terminals and execute the following commands
```
make build           # build command
make launch-gdb      # debug terminal
make launch-qemu     # emulation terminal
nc 127.0.0.1 1235    # console terminal
```
