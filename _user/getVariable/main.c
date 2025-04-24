#include <stdio.h>
#include <sys/getsetvariable.h>
int main(void)
{
    int value = 0;
    value = getVariable();
    printf("Read value: %d\n", value);
    return 0;
}