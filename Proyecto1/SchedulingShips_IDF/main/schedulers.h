#ifndef SCHEDULERS_H
#define SCHEDULERS_H

typedef struct {
    int id;
    int burst_time;
    int arrival_time;
    int priority;
    int deadline;
    int remaining_time;
} Task;

void rr(Task tasks[], int n, int time_quantum);
void priority_schedule(Task tasks[], int n);
void sjf(Task tasks[], int n);
void strn(Task tasks[], int n);
void fcfs(Task tasks[], int n);
void edf(Task tasks[], int n);

#endif
