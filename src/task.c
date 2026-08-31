
#include "headers/task.h"

ActiveTask tasks_active[MAX_NUMBER_ACTIVE_TASKS];
pthread_mutex_t active_tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t free_slots_sem;               // is there space in our 'tasks_active' array?
static int next_instance_id = 0;    // to have a 'time history' of the tasks

static void run_task1_unlocked(void) {
    if(verbose > 8) printf("task 1 activated\n");
    volatile double val = 1.0001;   // volatile so not optimizable by -O3
    for (volatile long i = 0; i < 240000010L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task2_unlocked(void) {
    if(verbose > 8) printf("task 2 activated\n");
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 180000001L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task3_unlocked(void) {
    if(verbose > 8) printf("task 3 activated\n");
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 120000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task4_unlocked(void) {
    if(verbose > 8) printf("task 4 activated\n");
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 60000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}



// taskid, cpuusage, period, deadline, routine
const Task TASK_CATALOG[] = {
    {1,  300, 1000, 900, run_task1_unlocked},
    {2,  150,  500,  350, run_task2_unlocked},
    {3,   280,  400,  300, run_task3_unlocked},
    {4,   20,  100,  80, run_task4_unlocked}
};

// Pass a local copy array to avoid holding active_tasks_mutex during stdout I/O
static void print_active_tasks(int client_port, const ActiveTask snapshot[], int max_tasks) {
    int count = 0;

    pthread_mutex_lock(&log_mutex);

    printf("[CLIENT %d] printing tasks WAITING\n", client_port);
    printf("[CLIENT %d] printing tasks ACQUIRED\n", client_port);

    printf("\n+---+------------------+---------+------------------+\n");
    printf("| Slot| Client Port      | TASK_ID | Worker Thread ID |\n");
    printf("+---+------------------+---------+------------------+\n");

    for (int i = 0; i < max_tasks; i++) {
        if (snapshot[i].active == 1) {
            count++;
            printf("|  %-2d | Port: %-10d | Task: %-1d | ThreadID: %-5d|\n",
                   i,
                   snapshot[i].client_port, 
                   snapshot[i].task_id, 
                   snapshot[i].instance_id);
        }
    }

    if (count == 0)
        printf("|       --- No Active Tasks Running ---        |\n");

    printf("+---+------------------+---------+------------------+\n");
    printf(" Total Active Tasks: %d / %d\n\n", count, max_tasks);

    printf("[CLIENT %d] printing tasks RELEASED\n\n", client_port);
    fflush(stdout);

    pthread_mutex_unlock(&log_mutex);
}




static void init_active_task(ActiveTask *t, int position){
    t->active = 0;
    t->instance_id = -1;
    t->position = position;
}

void init_active_tasks(){
    print_server("ACTIVE TASKS initialized");
    pthread_mutex_lock(&active_tasks_mutex);
    for (int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++)
        init_active_task(&tasks_active[i], i);
    
    pthread_mutex_unlock(&active_tasks_mutex);
    sem_init(&free_slots_sem, 0, MAX_NUMBER_ACTIVE_TASKS);  // why 0? to say semaphore is NOT shared
}

int find_free_pos_in_tasksactive(){

    for(int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++){
        if(tasks_active[i].active == 0)
            return i;
    }
    return -1;
}

/* 
add new task into 'active_tasks' array.
[Return] position where the task is stored.
*/
int add_active_task_and_return_position(ActiveTask *new_task) {
    if (sem_trywait(&free_slots_sem) != 0) {
        return -1;
    }

    print_server("[tasks_active MUTEX] [CLIENT %d] ADD task %d WAITING", new_task->client_port, new_task->task_id);
    
    pthread_mutex_lock(&active_tasks_mutex);
    
    print_server("[tasks_active MUTEX] [CLIENT %d] ADD task %d ACQUIRED", new_task->client_port, new_task->task_id);
    
    int free_pos = find_free_pos_in_tasksactive();
    if (free_pos == -1) {
        pthread_mutex_unlock(&active_tasks_mutex);
        print_server("[tasks_active MUTEX] [CLIENT %d] NO POS in ADD task %d RELEASED", new_task->client_port, new_task->task_id);
        sem_post(&free_slots_sem);
        return -1;
    }

    ActiveTask *task = &tasks_active[free_pos];
    task->active = 1;
    task->task_id = new_task->task_id;
    task->position = free_pos;
    task->client_owner_fd = new_task->client_owner_fd;
    task->client_port = new_task->client_port;
    task->instance_id = next_instance_id++;  

    // 1. Create a quick snapshot of active tasks while holding the lock
    ActiveTask snapshot[MAX_NUMBER_ACTIVE_TASKS];
    memcpy(snapshot, tasks_active, sizeof(tasks_active));

    int client_port = task->client_port;

    // 2. Release the state lock IMMEDIATELY
    print_server("[tasks_active MUTEX] [CLIENT %d] ADD task %d RELEASED", new_task->client_port, new_task->task_id);
    pthread_mutex_unlock(&active_tasks_mutex);
    

    // 3. Perform slow console I/O completely outside the state lock
    print_active_tasks(client_port, snapshot, MAX_NUMBER_ACTIVE_TASKS);

    return free_pos;
}

/*
    Given a task find the oldest instance's position 
*/
static int find_instance_to_deactivate(ActiveTask *at){
    int min = -1;
    int lowest_instance = INT_MAX;
    for(int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++){
        ActiveTask *current = &tasks_active[i];
        if(current->active && 
            current->task_id == at->task_id &&
            current->instance_id < lowest_instance)
            {
                // UPDATE
                min = i;
                lowest_instance = current->instance_id;
            }
    }
    if(min == -1)
        return -1;
    return min;
}


/*
must disable the thread associated to this task,
in particular the lowest instance (first one that arrived).
How? by setting that activetask in tasks_active to NOT active
*/
int find_and_remove_active_task(ActiveTask *at){
    print_server("[tasks_active MUTEX] [CLIENT %d] RMV task %d WAITING", 
        at->client_port,
        at->task_id);

    // acquire mutex
    pthread_mutex_lock(&active_tasks_mutex);
    
    print_server("[tasks_active MUTEX] [CLIENT %d] RMV task %d ACQUIRED", 
        at->client_port,
        at->task_id);

        
    // scan for all tasks_active, chose the position where the lowest
    //  'instance' of this client is active
    int pos = find_instance_to_deactivate(at);
    if (pos == -1) {
        // no istance to deactivate
        print_server("[tasks_active MUTEX] [CLIENT %d] RMV RELEASED: NO instance to deactivate task %d", at->client_port, at->task_id); 
        pthread_mutex_unlock(&active_tasks_mutex);
        return -1;
    }

    // deactivate task
    ActiveTask *task = &tasks_active[pos];
    int client_port = task->client_port;
    task->active = 0;
    task->instance_id = -1;
    task->client_owner_fd = -1;
    task->task_id = -1;
    task->thread_id = -1;  
    task->client_port= -1;  
    task->position = -1;

    
    // 1. Create a quick snapshot of active tasks while holding the lock
    ActiveTask snapshot[MAX_NUMBER_ACTIVE_TASKS];
    memcpy(snapshot, tasks_active, sizeof(tasks_active));


    
    // 2. Release the state lock IMMEDIATELY
    print_server("[tasks_active MUTEX] [CLIENT %d] RMV task %d RELEASED", at->client_port, at->task_id); 
    pthread_mutex_unlock(&active_tasks_mutex);
    
    // 3. Perform slow console I/O completely outside the state lock
    print_active_tasks(client_port, snapshot, MAX_NUMBER_ACTIVE_TASKS);
   

    // free a slot
    sem_post(&free_slots_sem);
    return 1;
}

