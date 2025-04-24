#include <stdio.h>
#include <stdlib.h>
#include <sys/getsetvariable.h>
int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    int value = 0;
    value = atoi(argv[1]);
    setVariable(value);
    printf("Variable set to value: %d\n", value);
    return 0;
}
