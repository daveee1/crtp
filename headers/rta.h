#ifndef RTA_H
#define RTA_H
#include <math.h>

#include "headers/task.h" /* Safely pulls in ActiveTask definition */

/* Prototype for is_schedulable */
int is_schedulable(ActiveTask *new_active_task);

#endif /* RTA_H */