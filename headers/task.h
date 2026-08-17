#ifndef TASK_H
#define TASK_H
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

#define MAX_NUMBER_ACTIVE_TASKS 4



typedef struct{
    char id;
    long cpu_usage;
    long deadline;
    long period;
    void (*routine)(void);
}Task;

typedef struct {
    int task_id;     
    int position;     // in the tasks_active array
    int instance_id;     // Unique identifier (e.g., slot index + 1)
    int active;          // 1 if running, 0 if free slot
    int client_owner_fd; // Socket FD of the client that owns this thread
    Task task;       // Copy of (or pointer to) static task parameters
    pthread_t thread_id; // Thread handle for cancellation/joining
}ActiveTask;

/* Task catalog declaration (defined in task.c) */
extern const Task TASK_CATALOG[];
extern ActiveTask tasks_active[MAX_NUMBER_ACTIVE_TASKS];
extern pthread_mutex_t active_tasks_mutex;
extern sem_t free_slots_sem;

int add_active_task(ActiveTask *new_task);
void remove_active_task(int position);
void init_active_tasks();
int find_free_pos_in_tasksactive();



// TASKS
/*
    most complex
*/ 
static void run_task1(void);
static void run_task2(void);
static void run_task3(void);
/*
less complex
*/ 
static void run_task4(void);

#endif /* TASK_H */