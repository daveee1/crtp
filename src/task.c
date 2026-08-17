
#include "headers/task.h"

/* REAL DEFINITIONS: Allocated exactly once here */
ActiveTask tasks_active[MAX_NUMBER_ACTIVE_TASKS];
pthread_mutex_t active_tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t free_slots_sem;

static void run_task1_unlocked(void) {
    printf("task 1 activated\n");
    volatile double val = 1.0001;   // volatile so not optimizable by -O3
    for (volatile long i = 0; i < 240000010L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task2_unlocked(void) {
    printf("task 2 activated\n");
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 180000001L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task3_unlocked(void) {
    printf("task 3 activated\n");
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 120000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task4_unlocked(void) {
    printf("task 4 activated\n");
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 60000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}




const Task TASK_CATALOG[] = {
    {1,  300, 1000, 1000, run_task1_unlocked},
    {2,  150,  500,  500, run_task2_unlocked},
    {3,   80,  400,  400, run_task3_unlocked},
    {4,   20,  100,  100, run_task4_unlocked}
};

static void init_active_task(ActiveTask *t, int position){
    t->active = -1;
    t->instance_id = -1;
    t->position = position;
}

void init_active_tasks(){
    pthread_mutex_lock(&active_tasks_mutex);
    for (int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++)
        init_active_task(&tasks_active[i], i);
    
    pthread_mutex_unlock(&active_tasks_mutex);
    sem_init(&free_slots_sem, 0, MAX_NUMBER_ACTIVE_TASKS - 1);  // why 0? to say semaphore is NOT shared
}

int find_free_pos_in_tasksactive(){
    for(int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++){
        if(tasks_active[i].active == -1){
            return i;
        }
    }
    return -1;
}

int add_active_task(ActiveTask *new_task){
    sem_wait(&free_slots_sem);  // is there space for a new task?
    pthread_mutex_lock(&active_tasks_mutex);
    
    int free_pos = find_free_pos_in_tasksactive(); // there must be since free_slots_sem > 1
    if (free_pos == -1) {
        // TASKS ACTIVE FULL! must not be possible
        pthread_mutex_unlock(&active_tasks_mutex);
        sem_post(&free_slots_sem);
        return -1;
    }


    ActiveTask *task = &tasks_active[free_pos];
    task->active = 1;
    task->task_id = new_task->task_id;
    task->position = free_pos;
    task->client_owner_fd = new_task->client_owner_fd;
    task->instance_id += 1;  
    
    pthread_mutex_unlock(&active_tasks_mutex);
    return free_pos;
}

void remove_active_task(int position){
    pthread_mutex_lock(&active_tasks_mutex);
    
    ActiveTask *task = &tasks_active[position];
    task->active = -1;
    task->instance_id = -1;
    task->client_owner_fd = -1;
    task->task_id = -1;
    task->thread_id = -1;  
    
    pthread_mutex_unlock(&active_tasks_mutex);
    sem_post(&free_slots_sem);
}



// int main(int argc, char **argv) {
//     if (argc < 2) {
//         printf("Usage: %s <task_number 1-4>\n", argv[0]);
//         return 1;
//     }

//     int choice = atoi(argv[1]);
//     switch (choice) {
//         case 1: task1(); break;
//         case 2: task2(); break;
//         case 3: task3(); break;
//         case 4: task4(); break;
//         default: printf("Invalid choice\n"); return 1;
//     }

//     return 0;
// }