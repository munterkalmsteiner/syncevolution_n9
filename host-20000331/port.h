/*
** Various portability definitions.
**
**	@(#)port.h              e07@nikhef.nl (Eric Wassenaar) 991328
*/

#if defined(__SVR4) || defined(__svr4__)
#define SVR4
#endif

#if defined(SYSV) || defined(SVR4)
#define SYSV_MALLOC
#define SYSV_MEMSET
#define SYSV_STRCHR
#define SYSV_SETVBUF
#endif

#if defined(WINNT)
#define SYSV_MALLOC
#define SYSV_STRCHR
#define SYSV_SETVBUF
#endif

#if defined(__hpux) || defined(hpux)
#define SYSV_MALLOC
#define SYSV_SETVBUF
#endif

#if defined(sgi)
#define SYSV_MALLOC
#endif

#if defined(linux)
#define SYSV_MALLOC
#endif

#if defined(bsdi) || defined(__bsdi__)
#define SYSV_MALLOC
#endif

#if defined(NeXT)
#define SYSV_MALLOC
#endif

/*
** Distinguish between various BIND releases.
*/

#if defined(RES_PRF_STATS)
#define BIND_49
#else
#define BIND_48
#endif

#if defined(BIND_49) && defined(__BIND)
#define BIND_493
#endif

/*
** Define constants for fixed sizes.
*/

#ifndef INT16SZ
#define	INT16SZ		2	/* for systems without 16-bit ints */
#endif
#ifndef INT32SZ
#define	INT32SZ		4	/* for systems without 32-bit ints */
#endif
#ifndef INADDRSZ
#define	INADDRSZ	4	/* for sizeof(struct inaddr) != 4 */
#endif

#ifndef	IPNGSIZE
#define	IPNGSIZE	16	/* 128 bit ip v6 address size */
#endif

/*
** The following should depend on existing definitions.
*/

typedef int	bool;		/* boolean type */
#define TRUE	1
#define FALSE	0

#if defined(BIND_48) || defined(OLD_RES_STATE)
typedef struct state		res_state_t;
#else
typedef struct __res_state	res_state_t;
#endif

#if defined(BIND_48)
typedef struct rrec	rrec_t;
#else
#if defined(BIND_493)
typedef u_char		rrec_t;
#else
typedef char		rrec_t;
#endif
#endif

#if defined(BIND_493)
typedef u_char	qbuf_t;
#else
typedef char	qbuf_t;
#endif

#if defined(BIND_493)
typedef char	nbuf_t;
#else
typedef u_char	nbuf_t;
#endif

#ifndef _IPADDR_T
#if defined(__alpha) || defined(BIND_49)
typedef u_int	ipaddr_t;
#else
typedef u_long	ipaddr_t;
#endif
#endif

#if defined(apollo) || defined(_BSD_SIGNALS)
typedef int	sigtype_t;
#else
typedef void	sigtype_t;
#endif

#ifdef SYSV_MALLOC
typedef void	ptr_t;		/* generic pointer type */
typedef u_int	siz_t;		/* general size type */
typedef void	free_t;
#else
typedef char	ptr_t;		/* generic pointer type */
typedef u_int	siz_t;		/* general size type */
typedef int	free_t;
#endif

#ifdef SYSV_MEMSET
#define bzero(a,n)	(void) memset(a,'\0',n)
#define bcopy(a,b,n)	(void) memcpy(b,a,n)
#endif

#ifdef SYSV_STRCHR
#define index		strchr
#define rindex		strrchr
#endif

#ifdef SYSV_SETVBUF
#define linebufmode(a)	(void) setvbuf(a, (char *)NULL, _IOLBF, BUFSIZ)
#else
#define linebufmode(a)	(void) setlinebuf(a)
#endif

#ifdef ULTRIX_RESOLV
#define nslist(i)	_res.ns_list[i].addr
#else
#define nslist(i)	_res.nsaddr_list[i]
#endif

#ifdef fp_nquery
#define pr_query(a,n,f)	fp_nquery(a,n,f)
#else
#define pr_query(a,n,f)	fp_query(a,f)
#endif

#if defined(sun) && defined(NO_YP_LOOKUP)
#define gethostbyname	(struct hostent *)res_gethostbyname
#define gethostbyaddr	(struct hostent *)res_gethostbyaddr
#endif

#if defined(SVR4)
#define jmp_buf		sigjmp_buf
#define setjmp(e)	sigsetjmp(e,1)
#define longjmp(e,n)	siglongjmp(e,n)
#endif

/*
** Very specific definitions for certain platforms.
*/

#if defined(WINNT)
#define NO_CONNECTED_DGRAM
#endif

#if defined(WINNT)
#undef  linebufmode
#define linebufmode(a)	(void) setvbuf(a, (char *)NULL, _IONBF, 0)
#endif

#if defined(WINNT)
#ifndef strcasecmp
#define strcasecmp	_stricmp
#endif
#ifndef strncasecmp
#define strncasecmp	_strnicmp
#endif
#endif /*WINNT*/

#if defined(WINNT)
#define setalarm(n)
#define setsignal(s,f)
#else
#define setalarm(n)	(void) alarm((unsigned int)(n))
#define setsignal(s,f)	(void) signal(s,f)
#endif

#if defined(WINNT)
#ifndef errno
#define errno		WSAGetLastError()
#endif
#ifndef h_errno
#define h_errno		WSAGetLastError()
#endif
#endif /*WINNT*/

#if defined(WINNT)
#define seterrno(n)	WSASetLastError(n)
#define seth_errno(n)	WSASetLastError(n)
#else
#define seterrno(n)	errno = (n)
#define seth_errno(n)	h_errno = (n)
#endif

#if defined(WINNT)
#undef  EINTR
#define EINTR		WSAEINTR
#undef  EWOULDBLOCK
#define EWOULDBLOCK	WSAEWOULDBLOCK
#undef  ETIMEDOUT
#define ETIMEDOUT	WSAETIMEDOUT
#undef  ECONNRESET
#define ECONNRESET	WSAECONNRESET
#undef  ECONNREFUSED
#define ECONNREFUSED	WSAECONNREFUSED
#undef  ENETDOWN
#define ENETDOWN	WSAENETDOWN
#undef  ENETUNREACH
#define ENETUNREACH	WSAENETUNREACH
#undef  EHOSTDOWN
#define EHOSTDOWN	WSAEHOSTDOWN
#undef  EHOSTUNREACH
#define EHOSTUNREACH	WSAEHOSTUNREACH
#undef  EADDRINUSE
#define EADDRINUSE	WSAEADDRINUSE
#endif /*WINNT*/

#if defined(WINNT)
HANDLE hReadWriteEvent;
#endif

#if defined(WINNT) && !defined(__STDC__)
#define __STDC__
#endif

/*
** No prototypes yet.
*/

#define PROTO(TYPES)	()

#if !defined(__STDC__) || defined(apollo)
#define Proto(TYPES)	()
#else
#define Proto(TYPES)	TYPES
#endif

#if !defined(__STDC__) || defined(apollo)
#define const
#endif

#if defined(__STDC__) && defined(BIND_49)
#define CONST	const
#else
#define CONST
#endif
