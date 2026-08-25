#ifndef TASK_H
#define TASK_H
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include "headers/utils.h"
#include <limits.h> // needed to use INT_MAX as lowest_instance in 'find_instance_to_deactivate()'


#define MAX_NUMBER_ACTIVE_TASKS 4



typedef struct{
    int id;
    long double cpu_usage;
    long double period;
    long double deadline;
    void (*routine)(void);
}Task;

typedef struct {
    int task_id;     
    int position;     // in the tasks_active array
    int instance_id;     // Unique identifier (e.g., slot index + 1)
    int active;          // 1 if running, 0 if free slot
    int client_owner_fd; // Socket FD of the client that owns this thread
    int client_port;
    pthread_t thread_id; // Thread handle for cancellation/detach
}ActiveTask;

/* Task catalog declaration (defined in task.c) */
extern const Task TASK_CATALOG[];
extern ActiveTask tasks_active[MAX_NUMBER_ACTIVE_TASKS];
extern pthread_mutex_t active_tasks_mutex;
extern sem_t free_slots_sem;

int add_active_task(ActiveTask *new_task);
int remove_active_task(ActiveTask *at);
void init_active_tasks();
int find_free_pos_in_tasksactive();



// TASKS
/*
    most complex: 1
*/ 
static void run_task1(void);
static void run_task2(void);
static void run_task3(void);
/*
less complex: 4
*/ 
static void run_task4(void);

#endif /* TASK_H */