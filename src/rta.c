#include "headers/rta.h"

/* Helper structure for temporary priority sorting in RTA*/
typedef struct {
    int task_id;
    int cpu_usage;  /* C_i */
    int period;     /* P_i / D_i */
} SchedTask;



static int utilization_factor(ActiveTask new_active_task) {
    long double sum = 0.0;

    /* 1. Sum utilization over all currently active tasks */
    for (int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++) {
        ActiveTask *current_active_task = &tasks_active[i];
        if (current_active_task->active == 1) {
            /* Fix: Subtract 1 if task_id is 1-based, or adjust to match catalog mapping */
            const Task *task = &TASK_CATALOG[current_active_task->task_id - 1];
            
            /* Explicit floating-point division */
            sum += (long double)task->cpu_usage / task->period;
        }   
    }

    /* 2. Add candidate task utilization */
    const Task *new_task = &TASK_CATALOG[new_active_task.task_id - 1];
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


int compare_rms_priority(const void *a, const void *b) {
    const SchedTask *taskA = (const SchedTask *)a;
    const SchedTask *taskB = (const SchedTask *)b;
    // Smaller period = higher priority in Rate Monotonic
    return taskA->period - taskB->period;
}

/*
    called when utilization factor returns 0. 
    Returns 1 iff new_active_task is schedulable
*/
static int rta(ActiveTask new_active_task){
    /*
        I need for each active_task j: 
            1) its period_j to determine its priority
            2) Response_time_j: computed recursively
            3) cpu_usage_j
    */
    SchedTask eval_set[MAX_NUMBER_ACTIVE_TASKS];
    int total_tasks = 0; // which tasks do i have to consider: managed by total_tasks
    
    // 1. take active tasks 
    pthread_mutex_lock(&active_tasks_mutex);
    for(int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++){
        if(tasks_active[i].active == 1){
            const Task *catalog_entry = &TASK_CATALOG[tasks_active[i].task_id - 1];
            eval_set[total_tasks].task_id = catalog_entry->id;
            eval_set[total_tasks].cpu_usage = catalog_entry->cpu_usage;
            eval_set[total_tasks].period = catalog_entry->period;
            total_tasks++;
        }
    }
    pthread_mutex_unlock(&active_tasks_mutex);
    
    // 2. add newtask in the fake array
    const Task *new_catalog_entry = &TASK_CATALOG[new_active_task.task_id - 1];
    eval_set[total_tasks].task_id = new_catalog_entry->id;
    eval_set[total_tasks].cpu_usage = new_catalog_entry->cpu_usage;
    eval_set[total_tasks].period = new_catalog_entry->period;
    total_tasks++;

    /* 3. Sort tasks by Rate Monotonic Priority (Shorter Period = Higher Priority) */
    qsort(eval_set, total_tasks, sizeof(SchedTask), compare_rms_priority);

    // once sorted: RTA analysis FOR EACH ACTIVE TASK! 
    for(int i = 0; i < total_tasks; i++){
        long double C_i = eval_set[i].cpu_usage;
        long double P_i = eval_set[i].period;
        long double response_prev = 0.0;
        long double response_time = C_i; // Initial response time iteration 
                                         // R^(0) = C_i [Base Case]
    
        while(response_prev != response_time){// till they dont converge continue!                                         
            response_prev = response_time;
            // consider only hp tasks!
            long double interference = 0.0;
            for(int j = 0; j < i; j++){
                long double C_j = eval_set[j].cpu_usage;
                long double P_j = eval_set[j].period;
                
                interference += ceill(response_prev / P_j) * C_j;
            }
            response_time += interference;
            
            if(response_time > P_i)
                return -1;  // unschedulable
        }
    }

    return 1;
}



/*
Given all the current active tasks tell me if we can schedule
 or not the new task
*/
int is_schedulable(ActiveTask new_active_task){
    int u = utilization_factor(new_active_task);
    if(u == 1){
        printf("SCHEDULABLE task %d by Utilization factor\n", new_active_task.task_id);
        return 1;
    }

    else if(u == 0){
        if(rta(new_active_task)){
            printf("SCHEDULABLE task %d by RTA\n", new_active_task.task_id);
            return 1;
        }
    }

    printf("NOT SCHEDULABLE task %d\n", new_active_task.task_id);
    return -1;
}