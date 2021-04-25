#include <stdio.h>

int main(int argc, char* argv[]) {
   printf("welcome to the Shell!\n");
   printf("  ARGC(%d)\n", argc); 
   for(int i=0; i<argc; i++) {
       printf("  %d: %s\n", i, argv[i]);
   }
   
   return 123;
}