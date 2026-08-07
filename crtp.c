/*
5. Simulation of dynamic periodic task execution. A pre-defined set of routines (with unique
assigned name) shall be defined in advance with given processor usage, period and
deadline. Every routine shall be composed of a program loop followed by a nanosleep() call.
The exact amount of CPU time and consequently of the processor utilization can be done in
advance using the time Linux command. The execution supervisor shall listen in TCP/IP for
requests for task activation/deactivation. The received message shall specify the name of
the task to be activated. A given task can be activated multiple times, starting every time a
new thread running the selected routine. Before accepting a request for a new task, a
response time analysis shall be carried out in order to assess the schedulability of the
system.
*/

/*
6. Implementation of a resource allocator with Deadlock prevention based on the banker’s
algorithm. The resource allocator will provide an initialization function:
initializeResources(int numResourceClasses, int*numResourcesPerClass, int numTasks, int
**maxResourcesPerClassPerTask) and two run-time functions: allocateResources(int
resourceClass, int numResources) and freeResources(int resourceClass, int numResources)
These function can be called by different threads and therefore must be thread-safe.
Routine initializeResources() called at the beginning of the program, provides the column
vector t and the maximum allocation matrix X describing the total number of available
resources per resource class and the maximum number of resources per resource class that
a given task will require, respectively.(cfr deadlock lecture). Routines allocateResources()
and freeResources() will be called by the program threads. allocateResources() will return
soon if the requested number of resources is available and safe, according to the
banker’salgorithm. 
Otherwise, the calling thread will be suspended until the above conditions are
met. freeResources() will always return soon and possibly trigger the check for pending
allocateResources() calls.
*/



#include "crtp.h"

int main(){
    
}