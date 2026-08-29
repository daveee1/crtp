#include "headers/task.h"
#include "headers/utils.h"
#include <math.h> // ceill function for rsa

static int rta(ActiveTask *new_active_task);
static int utilization_factor(ActiveTask *new_active_task);
int is_schedulable(ActiveTask *new_active_task);
