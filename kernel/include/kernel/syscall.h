#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

// Syscall int number
#define INT_SYSCALL 88

#define SYS_TEST 0
#define SYS_EXEC 1
#define SYS_PRINT 2
#define SYS_YIELD 3
#define SYS_FORK 4
#define SYS_EXIT 5
#define SYS_WAIT 6
#define SYS_SBRK 7
#define SYS_OPEN 8
#define SYS_CLOSE 9
#define SYS_READ 10
#define SYS_WRITE 11
#define SYS_SEEK 12
#define SYS_DUP 13
#define SYS_GETATTR_PATH 14
#define SYS_GETATTR_FD 15
#define SYS_GET_PID 16
#define SYS_CURR_DATE_TIME 17
#define SYS_UNLINK 18
#define SYS_LINK 19
#define SYS_RENAME 20
#define SYS_READDIR 21
#define SYS_CHDIR 22
#define SYS_GETCWD 23
#define SYS_TRUNCATE_PATH 24
#define SYS_TRUNCATE_FD 25
#define SYS_MKDIR 26
#define SYS_RMDIR 27
#define SYS_REFRESH_SCREEN 28
#define SYS_DRAW_PICTURE 29

#define SYS_NETWORK_RECEIVE_IPv4_PKT 30
#define SYS_PREP_IPV4_PKT 31
#define SYS_SEND_IPV4_PKT 32
#define SYS_PREP_ICMP_PKT 33
#define SYS_FINALIZE_ICMP_PKT 34


// #define SYS_SOCKET_OPEN 30
// #define SYS_SOCKET_SETOPT 31
// #define SYS_SOCKET_SENDTO 32
// #define SYS_SOCKET_RECVFROM 33

// defined in interrupt.asm
extern void int88();

// arch specific, defined in isr.h
struct trapframe;

void syscall_handler(struct trapframe* r);

#endif