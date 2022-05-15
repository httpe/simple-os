#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern char** __environ;

int main(int argc, char** argv)
{
  printf("Hello, World from SmallerC!\n");
  printf("argc: %d, argv[0]: %s\n", argc, argv[0]);
  printf("envp[\"SHELL\"]: %s\n", getenv("SHELL"));
  time_t rawtime;
  struct tm *info;
  time( &rawtime );
  info = localtime( &rawtime );
  printf("Current UTC time and date: %s\n", asctime(info));
  return 0;
}
