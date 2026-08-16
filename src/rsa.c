#include "headers/rsa.h"
/*
Given all the current active tasks tell me if we can schedule
 or not the new task
*/
int utilization_factor(ActiveTask new_active_task) {
    long double sum = 0.0;

    /* 1. Sum utilization over all currently active tasks */
    for (int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++) {
        ActiveTask *current_active_task = &tasks_active[i];
        if (current_active_task->active == 1) {
            /* Fix: Subtract 1 if task_id is 1-based, or adjust to match catalog mapping */
            Task *task = &TASK_CATALOG[current_active_task->task_id - 1];
            
            /* Explicit floating-point division */
            sum += (long double)task->cpu_usage / task->period;
        }
    }

    /* 2. Add candidate task utilization */
    Task *new_task = &TASK_CATALOG[new_active_task.task_id - 1];
    sum += (long double)new_task->cpu_usage / new_task->period;

    /* 3. Check hard physical limit (CPU Over-utilization) */
    if (sum > 1.0) {
        return -1; // Physically impossible to schedule
    }

    /* 4. Sufficient Bound Test */
    if (sum <= 0.6931471806) {
        // Guaranteed schedulable!
        return 1;
    }

    /* 5. If 0.693 < sum <= 1.0, return a status indicating exact RTA is needed */
    return 0; // Inconclusive: Must perform iterative RTA (R_i calculation)
}

/*
    called when utilization factor returns 0
*/
int rta(ActiveTask new_active_task){
    
}