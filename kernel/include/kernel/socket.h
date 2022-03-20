#ifndef _KERNEL_SOCKET_H
#define _KERNEL_SOCKET_H

#include <network.h>
#include <common.h>
#include <stdint.h>


// Ref: POSIX Socket Doc
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_socket.h.html


int socket(int domain, int type, int protocol);

ssize_t sendto(int socket, const void *message, size_t length,
       int flags, const struct sockaddr *dest_addr,
       socklen_t dest_len);

int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
// int getsockopt(int socket, int level, int option_name, void * option_value, socklen_t * option_len);

ssize_t recvfrom(int socket, void * buffer, size_t length,
       int flags, struct sockaddr * address,
       socklen_t * address_len);

// int bind(int socket, const struct sockaddr *address, socklen_t address_len);

// ssize_t recv(int, void *, size_t, int);
// ssize_t send(int, const void *, size_t, int);

// int listen(int, int);
// int accept(int socket, struct sockaddr * address, socklen_t * address_len);
// int connect(int, const struct sockaddr *, socklen_t);

// struct msghdr {
//     void          *msg_name;        // Optional address. 
//     socklen_t      msg_namelen;     // Size of address. 
//     struct iovec  *msg_iov;         // Scatter/gather array. 
//     int            msg_iovlen;      // Members in msg_iov. 
//     void          *msg_control;     // Ancillary data; see below. 
//     socklen_t      msg_controllen;  // Ancillary data buffer len. 
//     int            msg_flags;       // Flags on received message. 
// };
// ssize_t sendmsg(int, const struct msghdr *, int);
// ssize_t recvmsg(int, struct msghdr *, int);

// int     getpeername(int, struct sockaddr *, socklen_t *);
// int     getsockname(int, struct sockaddr *, socklen_t *);

// int     shutdown(int, int);
// int     sockatmark(int);
// int     socketpair(int, int, int, int [2]);


// System specific definitions

#define MAX_SOCKET_OPENED 32

typedef struct pkt_cache {
  void* buff;
  uint pkt_len;
  struct pkt_cache* next;
  struct sockaddr src;
  socklen_t src_len;
} pkt_cache;

typedef struct socket_opt {
  struct timeval recv_timeout;
} socket_opt;

typedef struct socket_descriptor {
  int domain;
  int type;
  int protocol;

  // struct sockaddr filter;

  void* transmit_buff;
  pkt_cache* cache;
  int ref;

  socket_opt opt;
  ipv4_opt ip_opt;
  void* protocol_opt;
} socket_descriptor;

int socket_process_pkt(int protocol, void* pkt, uint16_t pkt_len);

int close_socket(int socket);

#endif