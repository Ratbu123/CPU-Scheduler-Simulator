# MLFQ CPU Scheduler Simulator

## Overview
This project implements a Multi-Level Feedback Queue (MLFQ) CPU scheduler simulator in C as part of the Operating Systems course performance analysis task.

## Features
- 3-level feedback queues
- Queue 0: RR quantum 4
- Queue 1: RR quantum 8
- Queue 2: FCFS
- Aging to prevent starvation
- Process Control Block (PCB) based design
- Metrics: Waiting Time, Turnaround Time, Response Time, CPU Utilization, Context Switches

## Build & Run
```bash
cd src
make
./scheduler 1   # High CPU-bound
./scheduler 2   # High I/O-bound
./scheduler 3   # Mixed
```

## Workloads
- Scenario 1: 5 long CPU-bound processes
- Scenario 2: 7 short I/O-bound processes
- Scenario 3: Mixed interactive + batch

## Author
Alex Rivera  
CS 350 - Operating Systems, Section 01  
August 2026
