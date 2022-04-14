#include <stdlib.h>
#include <stdio.h>

char *getenv(const char *name)
{
    // printf("getenv(%s)\n", name);
    (void) name;
    // We don't have environment variable mechanism yet
    return 0; // NULL
}
