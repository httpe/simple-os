#include <stdio.h>

int syscall(int syscall_number, int arg1, int arg2, int arg3, int arg4)
{
   asm(
      "movl 8(%esp), %eax\n" // 
      "addl $8, %esp\n" // skip saved ebp/eip, kernel will skip syscall_number
      "int $88\n" // syscall int
      "subl $8, %esp\n" // restore saved ebp/eip 
   );
}

int syscall0(int arg1, int arg2, int arg3, int arg4)
{
   asm(
      "movl $0, %eax\n"
      "addl $4, %esp\n" // skip saved ebp, kernel will skip saved eip
      "int $88\n" // syscall int
      "subl $4, %esp\n" // restore saved ebp
   );
}

int main(int argc, char** argv) {
   // printf() displays the string inside quotation
   printf("Hello, Application World! argc: %u, argv[0]: %s\n", argc, argv[0]);
   printf("Calling method 1: SYS_TEST(1,2,3,4): %d\n", syscall(0, 1, 2, 3, 4));
   printf("Calling method 2: SYS_TEST(1,2,3,4): %d\n", syscall0(1, 2, 3, 4));
   return 0;
}
