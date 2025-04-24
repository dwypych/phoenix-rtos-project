#include <stdio.h>
#include <sys/processchildren.h>

int main(void)
{
    process_with_counter_t processWithMostChildren;
    getProcessWithMostChildren(&processWithMostChildren);
    printf("PID of process with most children: %d Number of children: %d \n", processWithMostChildren.pid, processWithMostChildren.counter);

    return 0;
}