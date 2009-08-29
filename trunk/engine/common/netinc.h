
#ifdef _WIN32

	#define EWOULDBLOCK	WSAEWOULDBLOCK
	#define EMSGSIZE	WSAEMSGSIZE
	#define ECONNRESET	WSAECONNRESET
	#define ECONNABORTED	WSAECONNABORTED
	#define ECONNREFUSED	WSAECONNREFUSED
	#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
	#define EAFNOSUPPORT	WSAEAFNOSUPPORT

	#ifdef _MSC_VER
		#define USEIPX
	#endif
	#include "winquake.h"
	#ifdef USEIPX
		#include "wsipx.h"
	#endif
	#ifdef IPPROTO_IPV6
		#include <ws2tcpip.h>
	#else
		#define	IPPROTO_IPV6
		#define EAI_NONAME 8
		struct ip6_scope_id
		{
			union
			{
				struct
				{
					u_long  Zone : 28;
					u_long  Level : 4;
				};
				u_long  Value;
			};
		};
		struct in6_addr
		{
			u_char	s6_addr[16];	/* IPv6 address */
		};
		typedef struct sockaddr_in6
		{
			short  sin6_family;
			u_short  sin6_port;
			u_long  sin6_flowinfo;
			struct in6_addr  sin6_addr;
			union
			{
				u_long  sin6_scope_id;
				struct ip6_scope_id  sin6_scope_struct; 
			};
		};

		struct addrinfo
		{
		  int ai_flags;
		  int ai_family;
		  int ai_socktype;
		  int ai_protocol;
		  size_t ai_addrlen;
		  char* ai_canonname;
		  struct sockaddr * ai_addr;
		  struct addrinfo * ai_next;
		};
	#endif

#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <sys/ioctl.h>
	#include <sys/uio.h>
	#include <arpa/inet.h>
	#include <errno.h>

	#include <unistd.h>

	#ifdef sun
		#include <sys/filio.h>
	#endif

	#ifdef NeXT
		#include <libc.h>
	#endif

	#if defined(__MORPHOS__) && !defined(ixemul)
		#define closesocket CloseSocket
		#define ioctlsocket IoctlSocket
	#else
		#define closesocket close
		#define ioctlsocket ioctl
	#endif

#endif

#if defined(_WIN32)
	#define qerrno WSAGetLastError()
#elif defined(__MORPHOS__) && !defined(ixemul)
	#define qerrno Errno()
#else
	#define qerrno errno
#endif


#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif

#ifndef INADDR_LOOPBACK
	#define INADDR_LOOPBACK 0x7f000001
#endif
