#include <stdlib.h>
#include <string.h>

char* strdup(const char* s)
{
    char* s_dup = malloc(strlen(s)+1);
    strcpy(s_dup, s);
    return s_dup;
}