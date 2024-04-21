#pragma once
#include <string>
#include <winsock.h>

std::string sockaddrToString(const sockaddr_in* addr);
std::string sockaddrToString(const sockaddr* addr);
std::string socketToString(SOCKET s);
std::string wsaErrorToString(int err);

typedef int (PASCAL* PFN_WSAAsyncSelect)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);
typedef int (PASCAL* PFN_listen)(SOCKET s, int backlog);
typedef SOCKET(PASCAL* PFN_accept)(SOCKET s, struct sockaddr* addr, int* addrlen);
typedef int (PASCAL* PFN_shutdown)(SOCKET s, int how);
typedef int (PASCAL* PFN_connect)(SOCKET s, const struct sockaddr* name, int namelen);
typedef SOCKET(PASCAL* PFN_socket)(int af, int type, int protocol);
typedef int (PASCAL* PFN_closesocket)(SOCKET s);
typedef int (PASCAL* PFN_sendto)(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
typedef int (PASCAL* PFN_recvfrom)(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
typedef int (PASCAL* PFN_bind)(SOCKET s, const struct sockaddr* addr, int namelen);
typedef int (PASCAL* PFN_recv)(SOCKET s, char* buf, int len, int flags);
typedef int (PASCAL* PFN_send)(SOCKET s, const char* buf, int len, int flags);
typedef int (PASCAL* PFN_select)(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout);
typedef int (PASCAL* PFN_setsockopt)(SOCKET s, int level, int optname, const char* optval, int optlen);
typedef int (PASCAL* PFN_ioctlsocket)(SOCKET s, long cmd, u_long* argp);
