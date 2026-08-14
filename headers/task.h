#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef struct{
    char name[10];
    long cpu_usage;
    long deadline;
    long period;
    void (*routine)(void);
} Task;
/* Task catalog declaration (defined in task.c) */
extern const Task TASK_CATALOG[];



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
