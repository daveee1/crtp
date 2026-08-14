
#include "headers/task.h"


const Task TASK_CATALOG[] = {
    {"task1",  300, 1000, 1000, run_task1},
    {"task2", 150,  500,  500, run_task2},
    {"task3",   80,  400,  400, run_task3},
    {"task4",   20,  100,  100, run_task4}
};

static void run_task1(void) {
    volatile double val = 1.0001;   // volatile so not optimizable by -O3
    for (volatile long i = 0; i < 240000010L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task2(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 180000001L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task3(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 120000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

static void run_task4(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 60000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
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