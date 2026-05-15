#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "schedulers.h"

// Round Robin
void rr(Task tasks[], int n, int time_quantum) {
    int time = 0, completed = 0;
    bool executed[n];
    for (int i = 0; i < n; i++) executed[i] = false;

    while (completed < n) {
        bool any_executed = false;
        for (int i = 0; i < n; i++) {
            if (tasks[i].arrival_time <= time && !executed[i]) {
                int exec_time = tasks[i].remaining_time < time_quantum ? tasks[i].remaining_time : time_quantum;
                printf("Task %d executed for %d units\n", tasks[i].id, exec_time);
                tasks[i].remaining_time -= exec_time;
                time += exec_time;
                any_executed = true;

                if (tasks[i].remaining_time == 0) {
                    executed[i] = true;
                    completed++;
                }
            }
        }
        if (!any_executed) time++;
    }
}

// Preemptive Priority (lower number = higher priority, with aging)
void priority_schedule(Task tasks[], int n) {
    int time = 0, completed = 0;
    int wait_time[n]; // tracks how many cycles each task has been waiting
    for (int i = 0; i < n; i++) wait_time[i] = 0;

    while (completed < n) {
        int highest = -1;
        for (int i = 0; i < n; i++) {
            if (tasks[i].arrival_time <= time && tasks[i].remaining_time > 0) {
                if (highest == -1 || tasks[i].priority < tasks[highest].priority) {
                    highest = i;
                }
            }
        }

        if (highest != -1) {
            printf("Task %d executed for 1 unit (Priority: %d)\n", tasks[highest].id, tasks[highest].priority);
            tasks[highest].remaining_time--;
            time++;

            // Increment wait time for all arrived, unfinished tasks except the one running
            for (int i = 0; i < n; i++) {
                if (i != highest && tasks[i].arrival_time <= time && tasks[i].remaining_time > 0) {
                    wait_time[i]++;
                    // Every 4 cycles waiting, boost priority (lower the number, min 1)
                    if (wait_time[i] % 4 == 0 && tasks[i].priority > 1) {
                        tasks[i].priority--;
                        printf("  -> Task %d priority boosted to %d (aging)\n", tasks[i].id, tasks[i].priority);
                    }
                }
            }

            if (tasks[highest].remaining_time == 0) {
                completed++;
            }
        } else {
            time++;
        }
    }
}

// Shortest Job First
void sjf(Task tasks[], int n) {
    int time = 0, completed = 0;
    bool executed[n];
    for (int i = 0; i < n; i++) executed[i] = false;

    while (completed < n) {
        int shortest = -1;
        for (int i = 0; i < n; i++) {
            if (!executed[i] && tasks[i].arrival_time <= time) {
                if (shortest == -1 || tasks[i].burst_time < tasks[shortest].burst_time) {
                    shortest = i;
                }
            }
        }

        if (shortest != -1) {
            printf("Task %d executed for %d units\n", tasks[shortest].id, tasks[shortest].burst_time);
            time += tasks[shortest].burst_time;
            executed[shortest] = true;
            completed++;
        } else {
            time++;
        }
    }
}

// Shortest Time Remaining Next
void strn(Task tasks[], int n) {
    int time = 0, completed = 0;

    while (completed < n) {
        int shortest = -1;
        for (int i = 0; i < n; i++) {
            if (tasks[i].arrival_time <= time && tasks[i].remaining_time > 0) {
                if (shortest == -1 || tasks[i].remaining_time < tasks[shortest].remaining_time) {
                    shortest = i;
                }
            }
        }

        if (shortest != -1) {
            printf("Task %d executed for 1 unit\n", tasks[shortest].id);
            tasks[shortest].remaining_time--;
            time++;

            if (tasks[shortest].remaining_time == 0) {
                completed++;
            }
        } else {
            time++;
        }
    }
}

// First Come First Serve
void fcfs(Task tasks[], int n) {
    int time = 0;

    // Sort by arrival time
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (tasks[j].arrival_time < tasks[i].arrival_time) {
                Task temp = tasks[i];
                tasks[i] = tasks[j];
                tasks[j] = temp;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        if (time < tasks[i].arrival_time)
            time = tasks[i].arrival_time;
        printf("Task %d executed for %d units\n", tasks[i].id, tasks[i].burst_time);
        time += tasks[i].burst_time;
    }
}

// Earliest Deadline First
void edf(Task tasks[], int n) {
    int time = 0, completed = 0;
    bool executed[n];
    for (int i = 0; i < n; i++) executed[i] = false;

    while (completed < n) {
        // Among arrived tasks, pick the one with the earliest deadline
        int earliest = -1;
        for (int i = 0; i < n; i++) {
            if (!executed[i] && tasks[i].arrival_time <= time) {
                if (earliest == -1 || tasks[i].deadline < tasks[earliest].deadline) {
                    earliest = i;
                }
            }
        }

        if (earliest != -1) {
            if (time + tasks[earliest].burst_time <= tasks[earliest].deadline) {
                printf("Task %d executed for %d units\n", tasks[earliest].id, tasks[earliest].burst_time);
                time += tasks[earliest].burst_time;
            } else {
                printf("Task %d missed its deadline.\n", tasks[earliest].id);
                time += tasks[earliest].burst_time;
            }
            executed[earliest] = true;
            completed++;
        } else {
            time++; // No task available yet, advance time
        }
    }
}