#include <stdio.h>
#include "schedulers.h"

void run_test_case() {
    Task tasks[] = {
        {1, 6, 0, 2, 10, 6}, // Task 1: burst_time=6, arrival_time=0, priority=2, deadline=10
        {2, 8, 1, 1, 15, 8}, // Task 2: burst_time=8, arrival_time=1, priority=1, deadline=15
        {3, 7, 2, 3, 12, 7}, // Task 3: burst_time=7, arrival_time=2, priority=3, deadline=12
        {4, 3, 3, 2, 9, 3}   // Task 4: burst_time=3, arrival_time=3, priority=2, deadline=9
    };
    int n = sizeof(tasks) / sizeof(tasks[0]);

    printf("\n--- Running Test Case ---\n");

    // Test Round Robin
    printf("\nTesting Round Robin:\n");
    rr(tasks, n, 4); // Time quantum = 4

    // Reset remaining_time for next test
    for (int i = 0; i < n; i++) tasks[i].remaining_time = tasks[i].burst_time;

    // Test Preemptive Priority
    printf("\nTesting Preemptive Priority:\n");
    priority_schedule(tasks, n);

    // Reset remaining_time for next test
    for (int i = 0; i < n; i++) tasks[i].remaining_time = tasks[i].burst_time;

    // Test Shortest Job First
    printf("\nTesting Shortest Job First:\n");
    sjf(tasks, n);

    // Reset remaining_time for next test
    for (int i = 0; i < n; i++) tasks[i].remaining_time = tasks[i].burst_time;

    // Test Shortest Time Remaining Next
    printf("\nTesting Shortest Time Remaining Next:\n");
    strn(tasks, n);

    // Reset remaining_time for next test
    for (int i = 0; i < n; i++) tasks[i].remaining_time = tasks[i].burst_time;

    // Test First Come First Serve
    printf("\nTesting First Come First Serve:\n");
    fcfs(tasks, n);

    // Reset remaining_time for next test
    for (int i = 0; i < n; i++) tasks[i].remaining_time = tasks[i].burst_time;

    // Test Earliest Deadline First
    printf("\nTesting Earliest Deadline First:\n");
    edf(tasks, n);

    printf("\n--- Test Case Completed ---\n");
}

int main() {
    run_test_case();
    return 0;
}
