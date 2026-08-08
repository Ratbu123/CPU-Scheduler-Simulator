/*
 * Multi-Level Feedback Queue (MLFQ) CPU Scheduler Simulator
 * Operating Systems Performance Analysis Project
 * 
 * This simulator implements a 3-level Multi-Level Feedback Queue scheduler
 * with the following policies:
 *   - Queue 0 (highest priority): Round Robin, time quantum = 4
 *   - Queue 1 (medium priority): Round Robin, time quantum = 8
 *   - Queue 2 (lowest priority): FCFS
 *
 * Processes start in Queue 0. If they do not complete within their quantum,
 * they are demoted to the next lower queue. This prioritizes short/interactive
 * jobs while preventing starvation of long-running CPU-bound processes via
 * aging (optional boost every 50 time units).
 *
 * Core data structures: Process Control Block (PCB), Ready Queues (circular),
 * and a simple event-driven simulation loop.
 *
 * Author: Rodolfo C. Guce III
 * Course: CS6206 - Operating Systems, BSIT 2nd Year
 * Date: August 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_PROCESSES 32
#define NUM_QUEUES 3
#define QUANTUM_Q0 4
#define QUANTUM_Q1 8
#define AGING_INTERVAL 50

/* Process states */
typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

/* Process Control Block */
typedef struct {
    int pid;
    int arrival_time;
    int burst_time;          /* Total CPU burst */
    int remaining_time;
    int priority;            /* Initial priority / queue level */
    int queue_level;         /* Current MLFQ queue (0=highest) */
    int waiting_time;
    int turnaround_time;
    int response_time;       /* Time from arrival to first run */
    int start_time;          /* First time scheduled (-1 if never) */
    int completion_time;
    ProcessState state;
    int time_in_quantum;     /* Time used in current quantum */
    int last_boost_time;     /* For aging */
} PCB;

/* Simple circular queue for ready lists */
typedef struct {
    int data[MAX_PROCESSES];
    int front;
    int rear;
    int count;
} Queue;

/* Global simulation state */
PCB processes[MAX_PROCESSES];
int process_count = 0;
Queue ready_queues[NUM_QUEUES];
int current_time = 0;
int context_switches = 0;
int total_idle_time = 0;

/* Queue operations */
void init_queue(Queue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

bool is_empty(Queue *q) {
    return q->count == 0;
}

bool enqueue(Queue *q, int pid_idx) {
    if (q->count >= MAX_PROCESSES) return false;
    q->rear = (q->rear + 1) % MAX_PROCESSES;
    q->data[q->rear] = pid_idx;
    q->count++;
    return true;
}

int dequeue(Queue *q) {
    if (is_empty(q)) return -1;
    int idx = q->data[q->front];
    q->front = (q->front + 1) % MAX_PROCESSES;
    q->count--;
    return idx;
}

/* Initialize a process */
void init_process(int idx, int pid, int arrival, int burst, int prio) {
    processes[idx].pid = pid;
    processes[idx].arrival_time = arrival;
    processes[idx].burst_time = burst;
    processes[idx].remaining_time = burst;
    processes[idx].priority = prio;
    processes[idx].queue_level = 0; /* All start at highest */
    processes[idx].waiting_time = 0;
    processes[idx].turnaround_time = 0;
    processes[idx].response_time = -1;
    processes[idx].start_time = -1;
    processes[idx].completion_time = -1;
    processes[idx].state = NEW;
    processes[idx].time_in_quantum = 0;
    processes[idx].last_boost_time = 0;
}

/* Load sample workloads */
void load_workload(int scenario) {
    process_count = 0;
    current_time = 0;
    context_switches = 0;
    total_idle_time = 0;

    for (int i = 0; i < NUM_QUEUES; i++) {
        init_queue(&ready_queues[i]);
    }

    /* Clear processes */
    memset(processes, 0, sizeof(processes));

    switch (scenario) {
        case 1: /* High CPU-Bound: long bursts, few I/O implications */
            init_process(0, 1, 0, 20, 1);
            init_process(1, 2, 2, 15, 1);
            init_process(2, 3, 4, 25, 1);
            init_process(3, 4, 6, 18, 1);
            init_process(4, 5, 8, 22, 1);
            process_count = 5;
            break;
        case 2: /* High I/O-Bound: short CPU bursts (simulating frequent I/O) */
            init_process(0, 1, 0, 4, 1);
            init_process(1, 2, 1, 3, 1);
            init_process(2, 3, 2, 5, 1);
            init_process(3, 4, 3, 2, 1);
            init_process(4, 5, 4, 4, 1);
            init_process(5, 6, 5, 3, 1);
            init_process(6, 7, 6, 6, 1);
            process_count = 7;
            break;
        case 3: /* Mixed Workload */
            init_process(0, 1, 0, 12, 1);  /* Medium */
            init_process(1, 2, 1, 3, 1);   /* Short interactive */
            init_process(2, 3, 3, 20, 1);  /* Long CPU */
            init_process(3, 4, 5, 5, 1);   /* Short */
            init_process(4, 5, 7, 15, 1);  /* Medium-long */
            init_process(5, 6, 8, 2, 1);   /* Very short */
            init_process(6, 7, 10, 8, 1);  /* Medium */
            process_count = 7;
            break;
        default:
            fprintf(stderr, "Unknown scenario\n");
            exit(1);
    }
}

/* Admit newly arrived processes to Queue 0 */
void admit_arrivals() {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == NEW && processes[i].arrival_time <= current_time) {
            processes[i].state = READY;
            processes[i].queue_level = 0;
            enqueue(&ready_queues[0], i);
        }
    }
}

/* Aging: periodically boost lower-priority processes to prevent starvation */
void apply_aging() {
    if (current_time % AGING_INTERVAL != 0 || current_time == 0) return;

    for (int i = 0; i < process_count; i++) {
        if (processes[i].state == READY && processes[i].queue_level > 0) {
            /* Boost one level */
            processes[i].queue_level--;
            /* Note: actual queue move would require removing from old queue;
               for simplicity in this discrete sim we re-enqueue on next selection */
            processes[i].last_boost_time = current_time;
        }
    }
}

/* Select next process: highest non-empty queue, front of queue */
int select_next() {
    for (int q = 0; q < NUM_QUEUES; q++) {
        if (!is_empty(&ready_queues[q])) {
            return dequeue(&ready_queues[q]);
        }
    }
    return -1; /* Idle */
}

/* Main simulation loop */
void run_simulation(int scenario) {
    load_workload(scenario);

    int completed = 0;
    int running_idx = -1;
    int quantum = 0;

    printf("=== Scenario %d Simulation Start ===\n", scenario);

    while (completed < process_count) {
        admit_arrivals();
        apply_aging();

        if (running_idx == -1) {
            running_idx = select_next();
            if (running_idx != -1) {
                context_switches++;
                processes[running_idx].state = RUNNING;
                if (processes[running_idx].start_time == -1) {
                    processes[running_idx].start_time = current_time;
                    processes[running_idx].response_time =
                        current_time - processes[running_idx].arrival_time;
                }
                /* Set quantum based on current queue */
                int lvl = processes[running_idx].queue_level;
                quantum = (lvl == 0) ? QUANTUM_Q0 : (lvl == 1) ? QUANTUM_Q1 : INT_MAX;
                processes[running_idx].time_in_quantum = 0;
            } else {
                /* CPU idle */
                total_idle_time++;
                current_time++;
                continue;
            }
        }

        /* Execute one time unit */
        processes[running_idx].remaining_time--;
        processes[running_idx].time_in_quantum++;
        current_time++;

        /* Check completion */
        if (processes[running_idx].remaining_time == 0) {
            processes[running_idx].state = TERMINATED;
            processes[running_idx].completion_time = current_time;
            processes[running_idx].turnaround_time =
                current_time - processes[running_idx].arrival_time;
            processes[running_idx].waiting_time =
                processes[running_idx].turnaround_time - processes[running_idx].burst_time;
            completed++;
            running_idx = -1;
            continue;
        }

        /* Quantum expired? Demote if not lowest queue */
        if (processes[running_idx].time_in_quantum >= quantum) {
            int lvl = processes[running_idx].queue_level;
            if (lvl < NUM_QUEUES - 1) {
                processes[running_idx].queue_level = lvl + 1;
            }
            processes[running_idx].state = READY;
            enqueue(&ready_queues[processes[running_idx].queue_level], running_idx);
            running_idx = -1;
            context_switches++;
        }
    }

    printf("Simulation complete at time %d\n", current_time);
    printf("Context switches: %d\n", context_switches);
}

/* Compute and print metrics */
void print_metrics(int scenario) {
    double sum_wait = 0, sum_tat = 0, sum_resp = 0;
    int total_burst = 0;

    printf("\n--- Performance Metrics (Scenario %d) ---\n", scenario);
    printf("PID | Arrival | Burst | Wait | TAT | Resp | QueueFinal\n");
    printf("----+---------+-------+------+-----+------+-----------\n");

    for (int i = 0; i < process_count; i++) {
        PCB *p = &processes[i];
        printf("%3d | %7d | %5d | %4d | %3d | %4d | %d\n",
               p->pid, p->arrival_time, p->burst_time,
               p->waiting_time, p->turnaround_time, p->response_time,
               p->queue_level);
        sum_wait += p->waiting_time;
        sum_tat += p->turnaround_time;
        sum_resp += p->response_time;
        total_burst += p->burst_time;
    }

    double avg_wait = sum_wait / process_count;
    double avg_tat = sum_tat / process_count;
    double avg_resp = sum_resp / process_count;
    double cpu_util = (current_time > 0) ?
        ((double)(current_time - total_idle_time) / current_time) * 100.0 : 0.0;

    printf("\nAverages:\n");
    printf("  Average Waiting Time   (T_wait) : %.2f\n", avg_wait);
    printf("  Average Turnaround Time (T_tat) : %.2f\n", avg_tat);
    printf("  Average Response Time  (T_resp) : %.2f\n", avg_resp);
    printf("  CPU Utilization               : %.2f%%\n", cpu_util);
    printf("  Total Context Switches        : %d\n", context_switches);
    printf("  Total Simulation Time         : %d\n", current_time);
}

int main(int argc, char *argv[]) {
    int scenario = 1;
    if (argc > 1) {
        scenario = atoi(argv[1]);
        if (scenario < 1 || scenario > 3) scenario = 1;
    }

    printf("MLFQ CPU Scheduler Simulator\n");
    printf("Quantums: Q0=%d, Q1=%d, Q2=FCFS\n\n", QUANTUM_Q0, QUANTUM_Q1);

    run_simulation(scenario);
    print_metrics(scenario);

    return 0;
}
