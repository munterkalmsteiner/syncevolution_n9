/*
** Master include file of the host utility.
**
**	@(#)host.h              e07@nikhef.nl (Eric Wassenaar) 991529
*/

#if defined(apollo) && defined(lint)
#define __attribute(x)
#endif

#define justfun			/* this is only for fun */
#undef  obsolete		/* old code left as a reminder */
#undef  notyet			/* new code for possible future use */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>

#include <sys/types.h>		/* not always automatically included */
#if !defined(WINNT)
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <sys/stat.h>
#if !defined(WINNT)
#include <sys/time.h>
#endif
#if defined(_AIX)
#include <sys/select.h>		/* needed for fd_set */
#endif
#include <fcntl.h>

#include <netdb.h>
#undef NOERROR			/* in <sys/streams.h> on solaris 2.x */
#include <arpa/nameser.h>
#include <resolv.h>

#include "port.h"		/* various portability definitions */
#include "conf.h"		/* various configuration definitions */
#include "type.h"		/* types should be in <arpa/nameser.h> */
#include "exit.h"		/* exit codes come from <sysexits.h> */

#ifndef NO_DATA
#define NO_DATA	NO_ADDRESS	/* used here only in case authoritative */
#endif

#define NO_RREC		(NO_DATA + 1)	/* non-authoritative NO_DATA */
#define NO_HOST		(NO_DATA + 2)	/* non-authoritative HOST_NOT_FOUND */
#define QUERY_REFUSED	(NO_DATA + 3)	/* query explicitly refused by server */
#define SERVER_FAILURE	(NO_DATA + 4)	/* instead of TRY_AGAIN upon SERVFAIL */
#define HOST_NOT_CANON	(NO_DATA + 5)	/* host name is not canonical */
#define CACHE_ERROR	(NO_DATA + 6)	/* to signal local cache I/O errors */

#define T_NONE	0		/* yet unspecified resource record type */
#define T_FIRST	T_A		/* first possible type in resource record */
#define T_LAST	(T_IXFR - 1)	/* last  possible type in resource record */

#ifndef NOCHANGE
#define NOCHANGE 0xf		/* compatibility with older BIND versions */
#endif

#define NOT_DOTTED_QUAD	((ipaddr_t)-1)
#define BROADCAST_ADDR	((ipaddr_t)0xffffffff)
#define LOCALHOST_ADDR	((ipaddr_t)0x7f000001)

#if PACKETSZ > 8192
#define MAXPACKET PACKETSZ	/* PACKETSZ should be the max udp size (512) */
#else
#define MAXPACKET 8192		/* but tcp packets can be considerably larger */
#endif

typedef union {
	HEADER header;
	u_char packet[MAXPACKET];
} querybuf;

#ifndef HFIXEDSZ
#define HFIXEDSZ 12		/* actually sizeof(HEADER) */
#endif

#define MAXDLEN (MAXPACKET - HFIXEDSZ)	/* upper bound for dlen */

#include "rrec.h"		/* resource record structures */

#define input			/* read-only input parameter */
#define output			/* modified output parameter */

#define STDIN	0
#define STDOUT	1
#define STDERR	2

#define MAXINT8		255
#define MAXINT16	65535

#define HASHSIZE	2003	/* size of various hash tables */

#ifdef lint
#define EXTERN
#else
#define EXTERN extern
#endif

#if !defined(errno)
EXTERN int errno;
#endif

#if !defined(h_errno)
EXTERN int h_errno;		/* defined in the resolver library */
#endif

EXTERN res_state_t _res;	/* defined in res_init.c */

#include "defs.h"		/* declaration of functions */

#define plural(n)	(((n) == 1) ? "" : "s")
#define plurale(n)	(((n) == 1) ? "" : "es")

#define is_xdigit(c)	(isascii(c) && isxdigit(c))
#define is_digit(c)	(isascii(c) && isdigit(c))
#define is_print(c)	(isascii(c) && isprint(c))
#define is_space(c)	(isascii(c) && isspace(c))
#define is_alnum(c)	(isascii(c) && isalnum(c))
#define is_upper(c)	(isascii(c) && isupper(c))

#define lowercase(c)	(is_upper(c) ? tolower(c) : (c))
#define lower(c)	(((c) >= 'A' && (c) <= 'Z') ? (c) + 'a' - 'A' : (c))
#define hexdigit(c)	(((c) < 10) ? '0' + (c) : 'A' + (c) - 10);

#define bitset(a,b)	(((a) & (b)) != 0)
#define sameword(a,b)	(strcasecmp(a, b) == 0)
#define samepart(a,b)	(strncasecmp(a, b, strlen(b)) == 0)
#define samehead(a,b)	(strncasecmp(a, b, sizeof(b)-1) == 0)

#define zeroname(a)	(samehead(a, "0.") || samehead(a, "255."))
#define fakename(a)	(samehead(a, "localhost.") || samehead(a, "loopback."))
#define nulladdr(a)	(((a) == 0) || ((a) == BROADCAST_ADDR))
#define fakeaddr(a)	(nulladdr(a) || ((a) == htonl(LOCALHOST_ADDR)))
#define incopy(a)	*((struct in_addr *)(a))
#define querysize(n)	(((n) > sizeof(querybuf)) ? (int)sizeof(querybuf) : (n))

#define newlist(a,n,t)	(t *)xalloc((ptr_t *)(a), (siz_t)((n)*sizeof(t)))
#define newstruct(t)	(t *)xalloc((ptr_t *)NULL, (siz_t)(sizeof(t)))
#define newstring(s)	(char *)xalloc((ptr_t *)NULL, (siz_t)(strlen(s)+1))
#define newstr(s)	strcpy(newstring(s), s)
#define xfree(a)	(void) free((ptr_t *)(a))

#define strlength(s)	(int)strlen(s)
#define in_string(s,c)	(index(s, c) != NULL)
#define in_label(a,b)	(((a) > (b)) && ((a)[-1] != '.') && ((a)[1] != '.'))
#define is_quoted(a,b)	(((a) > (b)) && ((a)[-1] == '\\'))
#define is_empty(s)	(((s) == NULL) || ((s)[0] == '\0'))

#ifdef DEBUG
#define assert(condition)\
{\
	if (!(condition))\
	{\
		(void) fprintf(stderr, "assertion botch: ");\
		(void) fprintf(stderr, "%s(%d): ", __FILE__, __LINE__);\
		(void) fprintf(stderr, "%s\n", "condition");\
		exit(EX_SOFTWARE);\
	}\
}
#else
#define assert(condition)
#endif
