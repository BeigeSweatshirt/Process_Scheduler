# Process_Scheduler
A simulation of process scheduling / exectution. Depending on input, the program can be configured to use multiple threads each running their own independent scheduling algorithm. When one thread finishes it's queue, the program can dynamically reallocate processs from other, busier threads so none are sitting idly.

## How to Build
Download the repo:
git clone https://github.com/BeigeSweatshirt/Process_Scheduler

Compile:
cd Process_Scheduler
gcc -o process_sched process -lm -pthread

## How to Run:
./process_sched PCB [(SCHED PERCENTAGE)]

where

PCB: The path to a process list. The repository comes with two: PCB1.bin and PCB2.bin.

SCHED: The scheduling algorithm.
- 1 = First Come First Serve (FCFS)
- 2 = Round Robin (RR)
- 3 = Shortest Job First (SJF)
- 4 = Priority (PRI)

PERCENTAGE: The percentage of processes allocated to the previously specified scheduling algorithm.

## Examples

Import processes from the PCB1.bin process queue file. 100% of processes should be processed by a shortest-job first algorithm.
./process_sched PCB1.bin 3 1.00

Import processes from the PCB2.bin process queue file. The first 25% are processed by a FCFS scheduler, the next 25% are processed by a RR scheduler, the next 25% are processsed by a SJF scheduler and the last 25% are processed by a PRI scheduler.
./process_sched PCB2.bin 1 0.25 2 0.25 3 0.25 4 0.25
