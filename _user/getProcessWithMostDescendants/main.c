#include <stdio.h>
#include <sys/processchildren.h>

int main(void)
{
    process_with_counter_t processWithMostDescendants;
    getProcessWithMostDescendants(&processWithMostDescendants);
    printf("PID of process with most descendants: %d Number of descendants: %d \n", processWithMostDescendants.pid, processWithMostDescendants.counter);

    return 0;
}