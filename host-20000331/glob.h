/*
** Global variable definitions.
**
**	@(#)glob.h              e07@nikhef.nl (Eric Wassenaar) 991529
*/

#ifdef _DEFINE
#define GLOBAL
#else
#define GLOBAL extern
#endif

extern char *dbprefix;		/* prefix for debug messages (send.c) */
extern char *version;		/* program version number (vers.c) */

extern ipaddr_t srcaddr;	/* explicit source address (send.c) */
extern int minport;		/* first source port in range (send.c) */
extern int maxport;		/* last  source port in range (send.c) */

extern time_t cachetime;	/* time cache was last modified (file.c) */
GLOBAL time_t loadtime;		/* force cache load if younger than this */

GLOBAL int errorcount;		/* global error count */

GLOBAL int record_stats[T_ANY+1]; /* count of resource records per type */

GLOBAL char cnamebuf[MAXDNAME+1];
GLOBAL char *cname;		/* RHS name to which CNAME is aliased */
GLOBAL char mnamebuf[MAXDNAME+1];
GLOBAL char *mname;		/* RHS name to which MR or MG is aliased */
GLOBAL char soanamebuf[MAXDNAME+1];
GLOBAL char *soaname;		/* LHS domain name of SOA record */
GLOBAL char subnamebuf[MAXDNAME+1];
GLOBAL char *subname;		/* LHS domain name of NS record */
GLOBAL char adrnamebuf[MAXDNAME+1];
GLOBAL char *adrname;		/* LHS domain name of A record */

GLOBAL ipaddr_t address;	/* internet address of A record */

GLOBAL char *listhost;		/* actual host queried during zone listing */

GLOBAL char serverbuf[MAXDNAME+1];
GLOBAL char *server;		/* name of explicit server to query */

GLOBAL char realnamebuf[2*MAXDNAME+2];
GLOBAL char *realname;		/* the actual name that was queried */

GLOBAL FILE *logfile;		/* default is stdout only */
GLOBAL bool logexchange;	/* exchange role of log file and stdout */

GLOBAL char *illegal;		/* give warning about illegal domain names */
GLOBAL char *skipzone;		/* zone(s) for which to skip zone transfer */
GLOBAL char *prefserver;	/* preferred server(s) for zone listing */

GLOBAL char *queryname;		/* the name about which to query */
GLOBAL int querytype;		/* the type of the query */
GLOBAL int queryclass;		/* the class of the query */
GLOBAL ipaddr_t queryaddr;	/* set if name to query is dotted quad */

GLOBAL int debug;		/* print resolver debugging output */
GLOBAL int verbose;		/* verbose mode for extra output */
GLOBAL bool timing;		/* print timestamps (for debugging) */

#ifdef justfun
GLOBAL int namelen;		/* select records exceeding this length */
#endif

GLOBAL int recursive;		/* recursive listmode maximum level */
GLOBAL int recursion_level;	/* current recursion level */
GLOBAL int skip_level;		/* level beyond which to skip checks */
GLOBAL int print_level;		/* level below which to skip verbose output */

GLOBAL bool quiet;		/* suppress non-fatal warning messages */
GLOBAL bool quick;		/* disable time consuming special checks */
GLOBAL bool reverse;		/* generate reverse in-addr.arpa queries */
GLOBAL bool revnsap;		/* generate reverse nsap.int queries */
GLOBAL bool primary;		/* use primary server for zone transfers */
GLOBAL bool compare;		/* compare serial numbers before transfer */
GLOBAL bool loading;		/* really load zone data from local cache */
GLOBAL bool dumping;		/* really dump zone data to local cache */
GLOBAL bool loadzone;		/* should load zone data from local cache */
GLOBAL bool dumpzone;		/* should dump zone data to local cache */
GLOBAL bool suppress;		/* suppress resource record output */
GLOBAL bool dotprint;		/* print trailing dot in non-listing mode */
GLOBAL bool ttlprint;		/* print ttl value in non-verbose mode */
GLOBAL bool dumpdata;		/* dump record data in hex and ascii */
GLOBAL bool waitmode;		/* wait until server becomes available */
GLOBAL bool mailmode;		/* trace MG and MR into MB records */
GLOBAL bool testmode;		/* special mode to test code separately */
GLOBAL bool addrmode;		/* check reverse mappings of addresses */
GLOBAL bool listmode;		/* generate zone listing of a zone */
GLOBAL bool hostmode;		/* count real hosts residing within zone */
GLOBAL bool duplmode;		/* list duplicate hosts within zone */
GLOBAL bool extrmode;		/* list extrazone hosts within zone */
GLOBAL bool gatemode;		/* list gateway hosts within zone */
GLOBAL bool checkmode;		/* check SOA records at each nameserver */
GLOBAL bool mxdomains;		/* list MX records for each delegated zone */
GLOBAL bool wildcards;		/* list only wildcard records in a zone */
GLOBAL bool listzones;		/* list only delegated zones in a zone */
GLOBAL bool exclusive;		/* exclude records that are not in zone */
GLOBAL bool canonskip;		/* skip canonical check during recursion */
GLOBAL bool underskip;		/* skip underscore check during recursion */
GLOBAL bool canoncheck;		/* enable canonical check during recursion */
GLOBAL bool undercheck;		/* enable underscore check during recursion */
GLOBAL bool cnamecheck;		/* check cname target for existence */
GLOBAL bool statistics;		/* print resource record statistics */
GLOBAL bool bindcompat;		/* enforce full BIND DNSRCH compatibility */
GLOBAL bool classprint;		/* print class value in non-verbose mode */
