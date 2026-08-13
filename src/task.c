#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void run_task1_heavy(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 240000010L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

void run_task2_medium(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 180000001L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

void run_task3_light(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 120000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

void run_task4_micro(void) {
    volatile double val = 1.0001;
    for (volatile long i = 0; i < 60000000L; i++) {
        val = val * 1.0000001 + 0.0000001;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <task_number 1-4>\n", argv[0]);
        return 1;
    }

    int choice = atoi(argv[1]);
    switch (choice) {
        case 1: run_task1_heavy(); break;
        case 2: run_task2_medium(); break;
        case 3: run_task3_light(); break;
        case 4: run_task4_micro(); break;
        default: printf("Invalid choice\n"); return 1;
    }

    return 0;
}