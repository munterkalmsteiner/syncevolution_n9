/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char Version[] = "@(#)util.c	e07@nikhef.nl (Eric Wassenaar) 991527";
#endif

#include "host.h"
#include "glob.h"

/*
** PARSE_TYPE -- Decode rr type from input string
** ----------------------------------------------
**
**	Returns:
**		Value of resource record type.
**		-1 if specified record name is invalid.
**
**	Note.	Several types are deprecated or obsolete, but recognized.
**		T_AXFR/T_IXFR is not allowed to be specified as query type.
*/

int
parse_type(str)
input char *str;			/* input string with record type */
{
	register int type;

		/* standard types */

	if (sameword(str, "A"))		return(T_A);
	if (sameword(str, "NS"))	return(T_NS);
	if (sameword(str, "MD"))	return(T_MD);		/* obsolete */
	if (sameword(str, "MF"))	return(T_MF);		/* obsolete */
	if (sameword(str, "CNAME"))	return(T_CNAME);
	if (sameword(str, "SOA"))	return(T_SOA);
	if (sameword(str, "MB"))	return(T_MB);		/* deprecated */
	if (sameword(str, "MG"))	return(T_MG);		/* deprecated */
	if (sameword(str, "MR"))	return(T_MR);		/* deprecated */
	if (sameword(str, "NULL"))	return(T_NULL);		/* obsolete */
	if (sameword(str, "WKS"))	return(T_WKS);
	if (sameword(str, "PTR"))	return(T_PTR);
	if (sameword(str, "HINFO"))	return(T_HINFO);
	if (sameword(str, "MINFO"))	return(T_MINFO);	/* deprecated */
	if (sameword(str, "MX"))	return(T_MX);
	if (sameword(str, "TXT"))	return(T_TXT);

		/* new types */

	if (sameword(str, "RP"))	return(T_RP);
	if (sameword(str, "AFSDB"))	return(T_AFSDB);
	if (sameword(str, "X25"))	return(T_X25);
	if (sameword(str, "ISDN"))	return(T_ISDN);
	if (sameword(str, "RT"))	return(T_RT);
	if (sameword(str, "NSAP"))	return(T_NSAP);
	if (sameword(str, "NSAP-PTR"))	return(T_NSAPPTR);
	if (sameword(str, "SIG"))	return(T_SIG);
	if (sameword(str, "KEY"))	return(T_KEY);
	if (sameword(str, "PX"))	return(T_PX);
	if (sameword(str, "GPOS"))	return(T_GPOS);		/* withdrawn */
	if (sameword(str, "AAAA"))	return(T_AAAA);
	if (sameword(str, "LOC"))	return(T_LOC);
	if (sameword(str, "NXT"))	return(T_NXT);
	if (sameword(str, "EID"))	return(T_EID);
	if (sameword(str, "NIMLOC"))	return(T_NIMLOC);
	if (sameword(str, "SRV"))	return(T_SRV);
	if (sameword(str, "ATMA"))	return(T_ATMA);
	if (sameword(str, "NAPTR"))	return(T_NAPTR);
	if (sameword(str, "KX"))	return(T_KX);
	if (sameword(str, "CERT"))	return(T_CERT);
	if (sameword(str, "A6"))	return(T_A6);
	if (sameword(str, "DNAME"))	return(T_DNAME);
	if (sameword(str, "SINK"))	return(T_SINK);
	if (sameword(str, "OPT"))	return(T_OPT);

		/* nonstandard types */

	if (sameword(str, "UINFO"))	return(T_UINFO);
	if (sameword(str, "UID"))	return(T_UID);
	if (sameword(str, "GID"))	return(T_GID);
	if (sameword(str, "UNSPEC"))	return(T_UNSPEC);

		/* special types */

	if (sameword(str, "ADDRS"))	return(T_ADDRS);
	if (sameword(str, "TKEY"))	return(T_TKEY);
	if (sameword(str, "TSIG"))	return(T_TSIG);

		/* filters */

	if (sameword(str, "IXFR"))	return(-1);		/* illegal */
	if (sameword(str, "AXFR"))	return(-1);		/* illegal */
	if (sameword(str, "MAILB"))	return(T_MAILB);
	if (sameword(str, "MAILA"))	return(T_MAILA);	/* obsolete */
	if (sameword(str, "ANY"))	return(T_ANY);
	if (sameword(str, "*"))		return(T_ANY);

		/* unknown types */

	type = atoi(str);
	if (type >= T_FIRST && type <= T_LAST)
		return(type);

	return(-1);
}

/*
** PARSE_CLASS -- Decode rr class from input string
** ------------------------------------------------
**
**	Returns:
**		Value of resource class.
**		-1 if specified class name is invalid.
**
**	Note.	C_CSNET is obsolete, but recognized.
*/

int
parse_class(str)
input char *str;			/* input string with resource class */
{
	register int class;

	if (sameword(str, "IN"))	return(C_IN);
	if (sameword(str, "INTERNET"))	return(C_IN);
	if (sameword(str, "CS"))	return(C_CSNET);	/* obsolete */
	if (sameword(str, "CSNET"))	return(C_CSNET);	/* obsolete */
	if (sameword(str, "CH"))	return(C_CHAOS);
	if (sameword(str, "CHAOS"))	return(C_CHAOS);
	if (sameword(str, "HS"))	return(C_HS);
	if (sameword(str, "HESIOD"))	return(C_HS);

	if (sameword(str, "ANY"))	return(C_ANY);
	if (sameword(str, "*"))		return(C_ANY);

	class = atoi(str);
	if (class > 0)
		return(class);

	return(-1);
}

/*
** IN_ADDR_ARPA -- Convert dotted quad string to reverse in-addr.arpa
** ------------------------------------------------------------------
**
**	Returns:
**		Pointer to appropriate reverse in-addr.arpa name
**		with trailing dot to force absolute domain name.
**		NULL in case of invalid dotted quad input string.
*/

char *
in_addr_arpa(dottedquad)
input char *dottedquad;			/* input string with dotted quad */
{
	static char addrbuf[4*4 + sizeof(ARPA_ROOT) + 2];
	unsigned int a[4];
	register int n;

	n = sscanf(dottedquad, "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]);
	switch (n)
	{
	    case 4:
		(void) sprintf(addrbuf, "%u.%u.%u.%u.%s.",
			a[3]&0xff, a[2]&0xff, a[1]&0xff, a[0]&0xff, ARPA_ROOT);
		break;

	    case 3:
		(void) sprintf(addrbuf, "%u.%u.%u.%s.",
			a[2]&0xff, a[1]&0xff, a[0]&0xff, ARPA_ROOT);
		break;

	    case 2:
		(void) sprintf(addrbuf, "%u.%u.%s.",
			a[1]&0xff, a[0]&0xff, ARPA_ROOT);
		break;

	    case 1:
		(void) sprintf(addrbuf, "%u.%s.",
			a[0]&0xff, ARPA_ROOT);
		break;

	    default:
		return(NULL);
	}

	while (--n >= 0)
		if (a[n] > 255)
			return(NULL);

	return(addrbuf);
}

/*
** NSAP_INT -- Convert dotted nsap address string to reverse nsap.int
** ------------------------------------------------------------------
**
**	Returns:
**		Pointer to appropriate reverse nsap.int name
**		with trailing dot to force absolute domain name.
**		NULL in case of invalid nsap address input string.
*/

char *
nsap_int(name)
input char *name;			/* input string with dotted nsap */
{
	static char addrbuf[4*MAXNSAP + sizeof(NSAP_ROOT) + 2];
	register int n;
	register int i;

	/* skip optional leading hex indicator */
	if (samehead(name, "0x"))
		name += 2;

	for (n = 0, i = strlength(name)-1; i >= 0; --i)
	{
		/* skip optional interspersed separators */
		if (name[i] == '.' || name[i] == '+' || name[i] == '/')
			continue;

		/* must consist of hex digits only */
		if (!is_xdigit(name[i]))
			return(NULL);

		/* but not too many */
		if (n >= 4*MAXNSAP)
			return(NULL);

		addrbuf[n++] = name[i];
		addrbuf[n++] = '.';
	}

	/* must have an even number of hex digits */ 
	if (n == 0 || (n % 4) != 0)
		return(NULL);

	(void) sprintf(&addrbuf[n], "%s.", NSAP_ROOT);
	return(addrbuf);
}

/*
** PRINT_HOST -- Print host name and address of hostent struct
** -----------------------------------------------------------
**
**	Returns:
**		None.
*/

void
print_host(heading, hp)
input char *heading;			/* header string */
input struct hostent *hp;		/* location of hostent struct */
{
	register char **ap;

	printf("%s: %s", heading, hp->h_name);

	for (ap = hp->h_addr_list; ap && *ap; ap++)
	{
		if (ap == hp->h_addr_list)
			printf("\nAddress:");

		printf(" %s", inet_ntoa(incopy(*ap)));
	}

	for (ap = hp->h_aliases; ap && *ap && **ap; ap++)
	{
		if (ap == hp->h_aliases)
			printf("\nAliases:");

		printf(" %s", *ap);
	}

	printf("\n\n");
}

/*
** SHOW_RES -- Show resolver database information
** ----------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The resolver database _res is localized in the resolver.
*/

void
show_res()
{
	register int i;
	register char **domain;

/*
 * The default domain is defined by the "domain" entry in /etc/resolv.conf
 * if not overridden by the environment variable "LOCALDOMAIN".
 * If still not defined, gethostname() may yield a fully qualified host name.
 */
	printf("Default domain:");
	if (_res.defdname[0] != '\0')
		printf(" %s", _res.defdname);
	printf("\n");

/*
 * The search domains are extracted from the default domain label components,
 * but may be overridden by "search" directives in /etc/resolv.conf
 * since 4.8.3.
 */
	printf("Search domains:");
	for (domain = _res.dnsrch; *domain; domain++)
		printf(" %s", *domain);
	printf("\n");

/*
 * The routine res_send() will do _res.retry tries to contact each of the
 * _res.nscount nameserver addresses before giving up when using datagrams.
 * The first try will timeout after _res.retrans seconds. Each following
 * try will timeout after ((_res.retrans << try) / _res.nscount) seconds.
 * Note. When we contact an explicit server the addresses will be replaced
 * by the multiple addresses of the same server.
 * When doing a zone transfer _res.retrans is used for the connect timeout.
 */
	printf("Timeout per retry: %d secs\n", _res.retrans);
	printf("Number of retries: %d\n", _res.retry);

	printf("Number of addresses: %d\n", _res.nscount);
	for (i = 0; i < _res.nscount; i++)
		printf("%s\n", inet_ntoa(nslist(i).sin_addr));

/*
 * The resolver options are initialized by res_init() to contain the
 * defaults settings (RES_RECURSE | RES_DEFNAMES | RES_DNSRCH)
 * The various options have the following meaning:
 *
 *	RES_INIT	set after res_init() has been called
 *	RES_DEBUG	let the resolver modules print debugging info
 *	RES_AAONLY	want authoritative answers only (not implemented)
 *	RES_USEVC	use tcp virtual circuit instead of udp datagrams
 *	RES_PRIMARY	use primary nameserver only (not implemented)
 *	RES_IGNTC	ignore datagram truncation; don't switch to tcp
 *	RES_RECURSE	forward query if answer not locally available
 *	RES_DEFNAMES	add default domain to queryname without dot
 *	RES_STAYOPEN	keep tcp socket open for subsequent queries
 *	RES_DNSRCH	append search domains even to queryname with dot
 */
	printf("Options set:");
	if (bitset(RES_INIT,      _res.options)) printf(" INIT");
	if (bitset(RES_DEBUG,     _res.options)) printf(" DEBUG");
	if (bitset(RES_AAONLY,    _res.options)) printf(" AAONLY");
	if (bitset(RES_USEVC,     _res.options)) printf(" USEVC");
	if (bitset(RES_PRIMARY,   _res.options)) printf(" PRIMARY");
	if (bitset(RES_IGNTC,     _res.options)) printf(" IGNTC");
	if (bitset(RES_RECURSE,   _res.options)) printf(" RECURSE");
	if (bitset(RES_DEFNAMES,  _res.options)) printf(" DEFNAMES");
	if (bitset(RES_STAYOPEN,  _res.options)) printf(" STAYOPEN");
	if (bitset(RES_DNSRCH,    _res.options)) printf(" DNSRCH");
	printf("\n");

	printf("Options clr:");
	if (!bitset(RES_INIT,     _res.options)) printf(" INIT");
	if (!bitset(RES_DEBUG,    _res.options)) printf(" DEBUG");
	if (!bitset(RES_AAONLY,   _res.options)) printf(" AAONLY");
	if (!bitset(RES_USEVC,    _res.options)) printf(" USEVC");
	if (!bitset(RES_PRIMARY,  _res.options)) printf(" PRIMARY");
	if (!bitset(RES_IGNTC,    _res.options)) printf(" IGNTC");
	if (!bitset(RES_RECURSE,  _res.options)) printf(" RECURSE");
	if (!bitset(RES_DEFNAMES, _res.options)) printf(" DEFNAMES");
	if (!bitset(RES_STAYOPEN, _res.options)) printf(" STAYOPEN");
	if (!bitset(RES_DNSRCH,   _res.options)) printf(" DNSRCH");
	printf("\n");

/*
 * The new BIND 4.9.3 has additional features which are not (yet) used.
 */
	printf("\n");
}

/*
** PRINT_STATS -- Print resource record statistics
** -----------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The record_stats[] counts have been updated by print_rrec().
**		The total_stats[]  counts have been updated by list_zone().
*/

void
print_stats(stats, nzones, name, filter, class)
input int stats[];			/* count of resource records per type */
input int nzones;			/* number of zones processed */
input char *name;			/* name of zone we are listing */
input int filter;			/* type of records we want to see */
input int class;			/* class of records we want to see */
{
	register int type;
	int nrecords;
	int total;

	for (total = 0, type = T_FIRST; type <= T_LAST; type++)
	{
		nrecords = stats[type];
		total += nrecords;

		if (nrecords > 0 || ((filter != T_ANY) && want_type(type, filter)))
		{
			printf("Found %d %s record%s", nrecords,
				pr_type(type), plural(nrecords));

			if (class != C_IN)
				printf(" in class %s", pr_class(class));

			if (nzones > 0)
				printf(" in %d zone%s", nzones, plural(nzones));

			printf(" within %s\n", name);
		}
	}

	if (total > 0)
	{
		printf("Found %d resource record%s", total, plural(total));

		if (class != C_IN)
			printf(" in class %s", pr_class(class));

		if (nzones > 0)
			printf(" in %d zone%s", nzones, plural(nzones));

		printf(" within %s\n", name);
	}
}

/*
** CLEAR_STATS -- Clear resource record statistics
** -----------------------------------------------
**
**	Returns:
**		None.
*/

void
clear_stats(stats)
output int stats[];			/* count of resource records per type */
{
	register int type;

	for (type = T_FIRST; type <= T_LAST; type++)
		stats[type] = 0;
}

/*
** SHOW_TYPES -- Show resource record types wanted
** -----------------------------------------------
**
**	Returns:
**		None.
*/

void
show_types(name, filter, class)
input char *name;			/* name we want to query about */
input int filter;			/* type of records we want to see */
input int class;			/* class of records we want to see */
{
	register int type;

	if (filter >= T_NONE)
	{
		printf("Query about %s for record types", name);

		if (filter == T_ANY)
			printf(" %s", pr_type(T_ANY));
		else
			for (type = T_FIRST; type <= T_LAST; type++)
				if (want_type(type, filter))
					printf(" %s", pr_type(type));

		if (class != C_IN)
			printf(" in class %s", pr_class(class));

		printf("\n");
	}
}

/*
** NS_ERROR -- Print error message from errno and h_errno
** ------------------------------------------------------
**
**	Returns:
**		None.
**
** If BIND res_send() fails, it will leave errno in either of the first
** two following states when using datagrams. Note that this depends on
** the proper handling of connected datagram sockets, which is usually
** true if BSD >= 43 (see res_send.c for details; it may need a patch).
** Note. If the 4.8 version succeeds, it may leave errno as EAFNOSUPPORT
** if it has disconnected a previously connected datagram socket, since
** the dummy address used to disconnect does not have a proper family set.
** Always clear errno after getting a reply, or patch res_send().
** Our private version of res_send() will leave also other error statuses.
*/

void
ns_error(name, type, class, host) 
input char *name;			/* full name we queried about */
input int type;				/* record type we queried about */
input int class;			/* record class we queried about */
input char *host;			/* set if explicit server was used */
{
	static char *auth = "Authoritative answer";

/*
 * Print the message associated with the network related errno values.
 */
	switch (errno)
	{
	    case ECONNREFUSED:
		/*
		 * The contacted host does not have a nameserver running.
		 * The standard res_send() also returns this if none of
		 * the intended hosts could be reached via datagrams.
		 */
		if (host != NULL)
			errmsg("Nameserver %s not running", host);
		else
			errmsg("Nameserver not running");
		break;

	    case ETIMEDOUT:
		/*
		 * The contacted server did not give any reply at all
		 * within the specified time frame.
		 */
		if (host != NULL)
			errmsg("Nameserver %s not responding", host);
		else
			errmsg("Nameserver not responding");
		break;

	    case ENETDOWN:
	    case ENETUNREACH:
	    case EHOSTDOWN:
	    case EHOSTUNREACH:
		/*
		 * The host to be contacted or its network can not be reached.
		 * Our private res_send() also returns this using datagrams.
		 */
		if (host != NULL)
			errmsg("Nameserver %s not reachable", host);
		else
			errmsg("Nameserver not reachable");
		break;
	}

/*
 * Print the message associated with the particular nameserver error.
 */
	switch (h_errno)
	{
	    case HOST_NOT_FOUND:
		/*
		 * The specified name does definitely not exist at all.
		 * In this case the answer is always authoritative.
		 * Nameserver status: NXDOMAIN
		 */
		if (class != C_IN)
			errmsg("%s does not exist in class %s (%s)",
				name, pr_class(class), auth);
		else if (host != NULL)
			errmsg("%s does not exist at %s (%s)",
				name, host, auth);
		else
			errmsg("%s does not exist (%s)",
				name, auth);
		break;

	    case NO_HOST:
		/*
		 * The specified name does not exist, but the answer
		 * was not authoritative, so it is still undecided.
		 * Nameserver status: NXDOMAIN
		 */
		if (class != C_IN)
			errmsg("%s does not exist in class %s, try again",
				name, pr_class(class));
		else if (host != NULL)
			errmsg("%s does not exist at %s, try again",
				name, host);
		else
			errmsg("%s does not exist, try again",
				name);
		break;

	    case NO_DATA:
		/*
		 * The name is valid, but the specified type does not exist.
		 * This status is here returned only in case authoritative.
		 * Nameserver status: NOERROR
		 */
		if (class != C_IN)
			errmsg("%s has no %s record in class %s (%s)",
				name, pr_type(type), pr_class(class), auth);
		else if (host != NULL)
			errmsg("%s has no %s record at %s (%s)",
				name, pr_type(type), host, auth);
		else
			errmsg("%s has no %s record (%s)",
				name, pr_type(type), auth);
		break;

	    case NO_RREC:
		/*
		 * The specified type does not exist, but we don't know whether
		 * the name is valid or not. The answer was not authoritative.
		 * Perhaps recursion was off, and no data was cached locally.
		 * Nameserver status: NOERROR
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s currently not present",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record currently not present at %s",
				name, pr_type(type), host);
		else
			errmsg("%s %s record currently not present",
				name, pr_type(type));
		break;

	    case TRY_AGAIN:
		/*
		 * Some intermediate failure, e.g. connect timeout,
		 * or some local operating system transient errors.
		 * General failure to reach any appropriate servers.
		 * The status SERVFAIL now yields a separate error code.
		 * Nameserver status: (SERVFAIL)
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found, try again",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s, try again",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found, try again",
				name, pr_type(type));
		break;

	    case SERVER_FAILURE:
		/*
		 * Explicit server failure status. This will be returned upon
		 * some internal server errors, forwarding failures, or when
		 * the server is not authoritative for a specific class.
		 * Also if the zone data has expired at a secondary server.
		 * Nameserver status: SERVFAIL
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found, server failure",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s, server failure",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found, server failure",
				name, pr_type(type));
		break;

	    case NO_RECOVERY:
		/*
		 * Some irrecoverable format error, or server refusal.
		 * The status REFUSED now yields a separate error code.
		 * Nameserver status: (REFUSED) FORMERR NOTIMP NOCHANGE
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found, no recovery",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s, no recovery",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found, no recovery",
				name, pr_type(type));
		break;

	    case QUERY_REFUSED:
		/*
		 * The server explicitly refused to answer the query.
		 * Servers can be configured to disallow zone transfers.
		 * Nameserver status: REFUSED
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s query refused",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record query refused by %s",
				name, pr_type(type), host);
		else
			errmsg("%s %s record query refused",
				name, pr_type(type));
		break;

	    default:
		/*
		 * Unknown cause for server failure.
		 */
		if (class != C_IN)
			errmsg("%s %s record in class %s not found",
				name, pr_type(type), pr_class(class));
		else if (host != NULL)
			errmsg("%s %s record not found at %s",
				name, pr_type(type), host);
		else
			errmsg("%s %s record not found",
				name, pr_type(type));
		break;
	}
}

/*
** DECODE_ERROR -- Convert nameserver error code to error message
** --------------------------------------------------------------
**
**	Returns:
**		Pointer to appropriate error message.
*/

char *
decode_error(rcode)
input int rcode;			/* error code from bp->rcode */
{
	switch (rcode)
	{
	    case NOERROR: 	return("no error");
	    case FORMERR:	return("format error");
	    case SERVFAIL:	return("server failure");
	    case NXDOMAIN:	return("non-existent domain");
	    case NOTIMP:	return("not implemented");
	    case REFUSED:	return("query refused");
	    case NOCHANGE:	return("no change");
	}

	return("unknown error");
}

/*
** PRINT_ANSWER -- Print result status after nameserver query
** ----------------------------------------------------------
**
**	Returns:
**		None.
**
**	Conditions:
**		The size of the answer buffer must have been
**		checked before to be of sufficient length,
**		i.e. to contain at least the buffer header.
*/

void
print_answer(answerbuf, answerlen)
input querybuf *answerbuf;		/* location of answer buffer */
input int answerlen;			/* length of answer buffer */
{
	HEADER *bp;
	int ancount;
	bool failed;

	bp = (HEADER *)answerbuf;
	ancount = ntohs((u_short)bp->ancount);
	failed = (bp->rcode != NOERROR || ancount == 0);

	printf("%s", verbose ? "" : dbprefix);

	printf("Query %s", failed ? "failed" : "done");

	if (bp->tc || (answerlen > PACKETSZ))
		printf(", %d byte%s", answerlen, plural(answerlen));

	if (bp->tc)
	{
		if (answerlen > sizeof(querybuf))
			printf(" (truncated to %d)", sizeof(querybuf));
		else
			printf(" (truncated)");
	}

	printf(", %d answer%s", ancount, plural(ancount));

	printf(", %s", bp->aa ? "authoritative " : "");

	printf("status: %s\n", decode_error((int)bp->rcode));
}

/*
** PR_ERROR -- Print error message about encountered inconsistencies
** -----------------------------------------------------------------
**
**	We are supposed to have an error condition which is fatal
**	for normal continuation, and the message is always printed.
**
**	Returns:
**		None.
**
**	Side effects:
**		Increments the global error count.
*/

void /*VARARGS1*/
pr_error(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	(void) fprintf(stderr, " *** ");
	(void) fprintf(stderr, fmt, a, b, c, d);
	(void) fprintf(stderr, "\n");

	/* flag an error */
	errorcount++;
}

/*
** PR_WARNING -- Print warning message about encountered inconsistencies
** ---------------------------------------------------------------------
**
**	We are supposed to have an error condition which is non-fatal
**	for normal continuation, and the message is suppressed in case
**	quiet mode has been selected.
**
**	Returns:
**		None.
*/

void /*VARARGS1*/
pr_warning(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	if (!quiet)
	{
		(void) fprintf(stderr, " !!! ");
		(void) fprintf(stderr, fmt, a, b, c, d);
		(void) fprintf(stderr, "\n");
	}
}

/*
** PR_TIMESTAMP -- Print timestamps for special debugging
** ------------------------------------------------------
**
**	Returns:
**		None.
*/

void /*VARARGS1*/
pr_timestamp(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	if (timing)
	{
		time_t now = time((time_t *)NULL);

		(void) fprintf(stderr, " @@@ ");
		(void) fprintf(stderr, "%s ", pr_date((int)now));
		(void) fprintf(stderr, fmt, a, b, c, d);
		(void) fprintf(stderr, "\n");
	}
}

/*
** WANT_TYPE -- Indicate whether the rr type matches the desired filter
** --------------------------------------------------------------------
**
**	Returns:
**		TRUE if the resource record type matches the filter.
**		FALSE otherwise.
**
**	In regular mode, the querytype is used to formulate the query,
**	and the filter is set to T_ANY to filter out any response.
**	In listmode, we get everything, so the filter is set to the
**	querytype to filter out the proper responses.
**	Note that T_NONE is the default querytype in listmode.
*/

bool
want_type(type, filter)
input int type;				/* resource record type */
input int filter;			/* type of records we want to see */
{
	if (type == filter)
		return(TRUE);

	if (filter == T_ANY)
		return(TRUE);

	if (filter == T_NONE &&
	   (type == T_A || type == T_NS || type == T_PTR))
		return(TRUE);

	if (filter == T_MAILB &&
	   (type == T_MB || type == T_MR || type == T_MG || type == T_MINFO))
		return(TRUE);

	if (filter == T_MAILA &&
	   (type == T_MD || type == T_MF))
		return(TRUE);

	return(FALSE);
}

/*
** WANT_CLASS -- Indicate whether the rr class matches the desired filter
** ----------------------------------------------------------------------
**
**	Returns:
**		TRUE if the resource record class matches the filter.
**		FALSE otherwise.
**
**	In regular mode, the queryclass is used to formulate the query,
**	and the filter is set to C_ANY to filter out any response.
**	In listmode, we get everything, so the filter is set to the
**	queryclass to filter out the proper responses.
**	Note that C_IN is the default queryclass in listmode.
*/

bool
want_class(class, filter)
input int class;			/* resource record class */
input int filter;			/* class of records we want to see */
{
	if (class == filter)
		return(TRUE);

	if (filter == C_ANY)
		return(TRUE);

	return(FALSE);
}

/*
** INDOMAIN -- Check whether a name belongs to a zone
** --------------------------------------------------
**
**	Returns:
**		TRUE if the given name lies anywhere in the zone, or
**		if the given name is the same as the zone and may be so.
**		FALSE otherwise.
*/

bool
indomain(name, domain, equal)
input char *name;			/* the name under consideration */
input char *domain;			/* the name of the zone */
input bool equal;			/* set if name may be same as zone */
{
	register char *dot;

	if (sameword(name, domain))
		return(equal);

	if (sameword(domain, "."))
		return(TRUE);

	dot = index(name, '.');
	while (dot != NULL)
	{
		if (!is_quoted(dot, name))
		{
			if (sameword(dot+1, domain))
				return(TRUE);
		}

		dot = index(dot+1, '.');
	}

	return(FALSE);
}

/*
** SAMEDOMAIN -- Check whether a name belongs to a zone
** ----------------------------------------------------
**
**	Returns:
**		TRUE if the given name lies directly in the zone, or
**		if the given name is the same as the zone and may be so.
**		FALSE otherwise.
*/

bool
samedomain(name, domain, equal)
input char *name;			/* the name under consideration */
input char *domain;			/* the name of the zone */
input bool equal;			/* set if name may be same as zone */
{
	register char *dot;

	if (sameword(name, domain))
		return(equal);

	dot = index(name, '.');
	while (dot != NULL)
	{
		if (!is_quoted(dot, name))
		{
			if (sameword(dot+1, domain))
				return(TRUE);

			return(FALSE);
		}

		dot = index(dot+1, '.');
	}

	if (sameword(domain, "."))
		return(TRUE);

	return(FALSE);
}

/*
** GLUERECORD -- Check whether a name is a glue record
** ---------------------------------------------------
**
**	Returns:
**		TRUE is this is a glue record.
**		FALSE otherwise.
**
**	The name is supposed to be the name of an address record.
**	If it lies directly in the given zone, it is considered
**	an ordinary host within that zone, and not a glue record.
**	If it does not belong to the given zone at all, is it
**	here considered to be a glue record.
**	If it lies in the given zone, but not directly, it is
**	considered a glue record if it belongs to any of the known
**	delegated zones of the given zone.
**	In the root zone itself are no hosts, only glue records.
*/

bool
gluerecord(name, domain, zone, nzones)
input char *name;			/* the name under consideration */
input char *domain;			/* name of zone being processed */
input char *zone[];			/* list of known delegated zones */
input int nzones;			/* number of known delegated zones */
{
	register char *dot;
	register int n;

	if (sameword(domain, "."))
		return(TRUE);

	if (samedomain(name, domain, TRUE))
		return(FALSE);

	if (!indomain(name, domain, TRUE))
		return(TRUE);

	if (zone == NULL)
		return(FALSE);

#ifdef obsolete
	for (n = 0; n < nzones; n++)
		if (indomain(name, zone[n], TRUE))
			return(TRUE);
#endif

	n = zone_index(name, FALSE);
	if (n < nzones)
		return(TRUE);

	dot = index(name, '.');
	while (dot != NULL)
	{
		if (!is_quoted(dot, name))
		{
			n = zone_index(dot+1, FALSE);
			if (n < nzones)
				return(TRUE);
		}

		dot = index(dot+1, '.');
	}

	return(FALSE);
}

/*
** MATCHLABELS -- Determine number of matching domain name labels
** --------------------------------------------------------------
**
**	Returns:
**		Number of shared trailing label components in both names.
**
**	Note. This routine is currently used only to compare nameserver
**	names in the RHS of NS records, so there is no need to check
**	for embedded quoted dots.
*/

int
matchlabels(name, domain)
input char *name;			/* domain name to check */
input char *domain;			/* domain name to compare against */
{
	register int i, j;
	int matched = 0;

	i = strlength(name);
	j = strlength(domain);

	while (--i >= 0 && --j >= 0)
	{
		if (lowercase(name[i]) != lowercase(domain[j]))
			break;
		if (domain[j] == '.')
			matched++;
		else if (j == 0 && (i == 0 || name[i-1] == '.'))
			matched++;
	}

	return(matched);
}

/*
** PR_DOMAIN -- Convert domain name according to printing options
** --------------------------------------------------------------
**
**	Returns:
**		Pointer to new domain name, if conversion was done.
**		Pointer to original name, if no conversion necessary.
*/

char *
pr_domain(name, listing)
input char *name;			/* domain name to be printed */
input bool listing;			/* set if this is a zone listing */
{
	char *newname;			/* converted domain name */

/*
 * Print reverse nsap.int name in forward notation, unless prohibited.
 */
	if (revnsap && !dotprint)
	{
		newname = pr_nsap(name);
		if (newname != name)
			return(newname);
	}

/*
 * Print domain names with trailing dot if necessary.
 */
	if (listing || dotprint)
	{
		newname = pr_dotname(name);
		if (newname != name)
			return(newname);
	}

/*
 * No conversion was required, use original name.
 */
	return(name);
}

/*
** PR_DOTNAME -- Return domain name with trailing dot
** --------------------------------------------------
**
**	Returns:
**		Pointer to new domain name, if dot was added.
**		Pointer to original name, if dot was already present.
*/

char *
pr_dotname(name)
input char *name;			/* domain name to append to */
{
	static char buf[MAXDNAME+2];	/* buffer to store new domain name */
	register int n;

	n = strlength(name);
	if (n > 0 && name[n-1] == '.')
		return(name);

	if (n > MAXDNAME)
		n = MAXDNAME;

#ifdef obsolete
	(void) sprintf(buf, "%.*s.", MAXDNAME, name);
#endif
	bcopy(name, buf, n);
	buf[n] = '.';
	buf[n+1] = '\0';
	return(buf);
}

/*
** PR_NSAP -- Convert reverse nsap.int to dotted forward notation
** --------------------------------------------------------------
**
**	Returns:
**		Pointer to new dotted nsap, if converted.
**		Pointer to original name otherwise.
*/

char *
pr_nsap(name)
input char *name;			/* potential reverse nsap.int name */
{
	static char buf[3*MAXNSAP+1];
	register char *p;
	register int n;
	register int i;

	/* must begin with single hex digits separated by dots */
	for (i = 0; is_xdigit(name[i]) && name[i+1] == '.'; i += 2)
		continue;

	/* must have an even number of hex digits */ 
	if (i == 0 || (i % 4) != 0)
		return(name);

	/* but not too many */
	if (i > 4*MAXNSAP)
		return(name);

	/* must end in the appropriate root domain */
	if (!sameword(&name[i], NSAP_ROOT))
		return(name);

	for (p = buf, n = 0; i >= 4; i -= 4, n++)
	{
		*p++ = name[i-2];
		*p++ = name[i-4];

		/* add dots for readability */
		if ((n % 2) == 0 && (i - 4) > 0)
			*p++ = '.';
	}
	*p = '\0';

	return(buf);
}

/*
** PR_TYPE -- Return name of resource record type
** ----------------------------------------------
**
**	Returns:
**		Pointer to name of resource record type.
**
**	Note.	All possible (even obsolete) types are recognized.
*/

char *
pr_type(type)
input int type;				/* resource record type */
{
	static char buf[30];		/* sufficient for 64-bit values */

	switch (type)
	{
		/* standard types */

	    case T_A:       return("A");	/* internet address */
	    case T_NS:      return("NS");	/* authoritative server */
	    case T_MD:      return("MD");	/* mail destination */
	    case T_MF:      return("MF");	/* mail forwarder */
	    case T_CNAME:   return("CNAME");	/* canonical name */
	    case T_SOA:     return("SOA");	/* start of auth zone */
	    case T_MB:      return("MB");	/* mailbox domain name */
	    case T_MG:      return("MG");	/* mail group member */
	    case T_MR:      return("MR");	/* mail rename name */
	    case T_NULL:    return("NULL");	/* null resource record */
	    case T_WKS:     return("WKS");	/* well known service */
	    case T_PTR:     return("PTR");	/* domain name pointer */
	    case T_HINFO:   return("HINFO");	/* host information */
	    case T_MINFO:   return("MINFO");	/* mailbox information */
	    case T_MX:      return("MX");	/* mail routing info */
	    case T_TXT:     return("TXT");	/* descriptive text */

		/* new types */

	    case T_RP:      return("RP");	/* responsible person */
	    case T_AFSDB:   return("AFSDB");	/* afs database location */
	    case T_X25:     return("X25");	/* x25 address */
	    case T_ISDN:    return("ISDN");	/* isdn address */
	    case T_RT:      return("RT");	/* route through host */
	    case T_NSAP:    return("NSAP");	/* nsap address */
	    case T_NSAPPTR: return("NSAP-PTR");	/* nsap pointer */
	    case T_SIG:     return("SIG");	/* security signature */
	    case T_KEY:     return("KEY");	/* security key */
	    case T_PX:      return("PX");	/* rfc822 - x400 mapping */
	    case T_GPOS:    return("GPOS");	/* geographical position */
	    case T_AAAA:    return("AAAA");	/* ip v6 address */
	    case T_LOC:     return("LOC");	/* geographical location */
	    case T_NXT:     return("NXT");	/* next valid name */
	    case T_EID:     return("EID");	/* endpoint identifier */
	    case T_NIMLOC:  return("NIMLOC");	/* nimrod locator */
	    case T_SRV:     return("SRV");	/* service info */
	    case T_ATMA:    return("ATMA");	/* atm address */
	    case T_NAPTR:   return("NAPTR");	/* naming authority urn */
	    case T_KX:      return("KX");	/* key exchange info */
	    case T_CERT:    return("CERT");	/* security certificate */
	    case T_A6:      return("A6");
	    case T_DNAME:   return("DNAME");
	    case T_SINK:    return("SINK");
	    case T_OPT:     return("OPT");

		/* nonstandard types */

	    case T_UINFO:   return("UINFO");	/* user information */
	    case T_UID:     return("UID");	/* user ident */
	    case T_GID:     return("GID");	/* group ident */
	    case T_UNSPEC:  return("UNSPEC");	/* unspecified binary data */

		/* special types */

	    case T_ADDRS:   return("ADDRS");
	    case T_TKEY:    return("TKEY");	/* transaction key */
	    case T_TSIG:    return("TSIG");	/* transaction signature */

		/* filters */

	    case T_IXFR:    return("IXFR");	/* incremental zone transfer */
	    case T_AXFR:    return("AXFR");	/* zone transfer */
	    case T_MAILB:   return("MAILB");	/* matches MB/MR/MG/MINFO */
	    case T_MAILA:   return("MAILA");	/* matches MD/MF */
	    case T_ANY:     return("ANY");	/* matches any type */

	    case T_NONE:    return("resource");	/* not yet determined */
	}

	/* unknown type */
	(void) sprintf(buf, "%d", type);
	return(buf);
}

/*
** PR_CLASS -- Return name of resource record class
** ------------------------------------------------
**
**	Returns:
**		Pointer to name of resource record class.
*/

char *
pr_class(class)
input int class;			/* resource record class */
{
	static char buf[30];		/* sufficient for 64-bit values */

	switch (class)
	{
	    case C_IN:      return("IN");	/* internet */
	    case C_CSNET:   return("CS");	/* csnet */
	    case C_CHAOS:   return("CH");	/* chaosnet */
	    case C_HS:      return("HS");	/* hesiod */
	    case C_ANY:     return("ANY");	/* any class */
	}

	/* unknown class */
	(void) sprintf(buf, "%d", class);
	return(buf);
}

/*
** EXPAND_NAME -- Expand compressed domain name in a resource record
** -----------------------------------------------------------------
**
**	Returns:
**		Number of bytes advanced in answer buffer.
**		-1 if there was a format error.
**
**	It is assumed that the specified buffer is of a fixed size
**	MAXDNAME+1 that should be sufficient to store the data.
*/

int
expand_name(name, type, cp, msg, eom, namebuf)
input char *name;			/* name of resource record */
input int type;				/* type of resource record */
input u_char *cp;			/* current position in answer buf */
input u_char *msg, *eom;		/* begin and end of answer buf */
output char *namebuf;			/* location of buf to expand name in */
{
	register int n;

	n = dn_expand(msg, eom, cp, (nbuf_t *)namebuf, MAXDNAME);
	if (n < 0)
	{
		pr_error("expand error in %s record for %s, offset %s",
			pr_type(type), name, dtoa(cp - msg));
		seth_errno(NO_RECOVERY);
		return(-1);
	}

	/* should not be necessary, but who knows */
	namebuf[MAXDNAME] = '\0';

	/* change root to single dot */
	if (namebuf[0] == '\0')
	{
		namebuf[0] = '.';
		namebuf[1] = '\0';
	}

	return(n);
}

/*
** CHECK_SIZE -- Check whether resource record is of sufficient length
** -------------------------------------------------------------------
**
**	Returns:
**		Requested size if current record is long enough.
**		-1 if current record does not have this many bytes.
**
**	Note that HINFO records are very often incomplete since only
**	one of the two data fields has been filled in and the second
**	field is missing. So we generate only a warning message.
*/

int
check_size(name, type, cp, msg, eor, size)
input char *name;			/* name of resource record */
input int type;				/* type of resource record */
input u_char *cp;			/* current position in answer buf */
input u_char *msg;			/* begin of answer buf */
input u_char *eor;			/* predicted position of next record */
input int size;				/* required record size remaining */
{
	if (cp + size > eor)
	{
		if (type != T_HINFO)
			pr_error("incomplete %s record for %s, offset %s",
				pr_type(type), name, dtoa(cp - msg));
		else
			pr_warning("incomplete %s record for %s",
				pr_type(type), name);
		seth_errno(NO_RECOVERY);
		return(-1);
	}

	return(size);
}

/*
** VALID_NAME -- Check whether domain name contains invalid characters
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if the name is valid.
**		FALSE otherwise.
**
**	The total size of a compound name should not exceed MAXDNAME.
**	We assume that this is true. Its individual components between
**	dots should not be longer than 64. This is not checked here.
**
**	Only alphanumeric characters and dash '-' may be used (dash
**	only in the middle). We only check the individual characters.
**	Strictly speaking, this restriction is only for ``host names''.
**	The underscore is illegal, at least not recommended, but is
**	so abundant that it requires special processing.
**
**	If the domain name represents a mailbox specification, the
**	first label up to the first (unquoted) dot is the local part
**	of a mail address, which should adhere to the RFC 822 specs.
**	This first dot takes the place of the RFC 822 '@' sign.
**
**	The label '*' can in principle be used anywhere to indicate
**	wildcarding. It is valid only in the LHS resource record name,
**	in definitions in zone files only as the first component.
**	Used primarily in wildcard MX record definitions.
**
**	Note. This routine is much too liberal.
*/

char *specials = ".()<>@,;:\\\"[]";	/* RFC 822 specials */

bool
valid_name(name, wildcard, localpart, underscore)
input char *name;			/* domain name to check */
input bool wildcard;			/* set if wildcard is allowed */
input bool localpart;			/* set if this is a mailbox spec */
input bool underscore;			/* set if underscores are allowed */
{
	bool backslash = FALSE;
	bool quoting = FALSE;
	register char *p;
	register char c;

	for (p = name; (c = *p) != '\0'; p++)
	{
		/* special check for local part in mailbox */
		if (localpart)
		{
			if (backslash)
				backslash = FALSE;	/* escape this char */
			else if (c == '\\')
				backslash = TRUE;	/* escape next char */
			else if (c == '"')
				quoting = !quoting;	/* start/stop quoting */
			else if (quoting)
				continue;		/* allow quoted chars */
			else if (c == '.')
				localpart = FALSE;	/* instead of '@' */
			else if (c == '@')
				return(FALSE);		/* should be '.' */
			else if (in_string(specials, c))
				return(FALSE);		/* must be escaped */
			else if (is_space(c))
				return(FALSE);		/* must be escaped */
			continue;
		}

		/* basic character set */
		if (is_alnum(c) || ((c == '-') && in_label(p, name)))
			continue;

		/* start of a new label component */
		if ((c == '.') && in_label(p, name))
			continue;

		/* allow '*' for use in wildcard names */
		if ((c == '*') && (p == name && p[1] == '.') && wildcard)
			continue;

		/* ignore underscore in certain circumstances */
		if ((c == '_') && underscore && !illegal)
			continue;

		/* silently allowed widespread exceptions */
		if (illegal && in_string(illegal, c))
			continue;

		return(FALSE);
	}

	/* must be beyond the local part in a mailbox */
	if (localpart)
		return(FALSE);

	/* cannot be a dotted quad */
	if (inet_addr(name) != NOT_DOTTED_QUAD)
		return(FALSE);

	/* all tests passed */
	return(TRUE);
}

/* 
** CANONICAL -- Check whether domain name is a canonical host name
** ---------------------------------------------------------------
**
**	Returns:
**		Nonzero if the name is definitely not canonical.
**		0 if it is canonical, or if it remains undecided.
*/

int
canonical(name)
input char *name;			/* the domain name to check */
{
	struct hostent *hp;
	int status;
	int save_errno;
	int save_herrno;
	
/*
 * Preserve state when querying, to avoid clobbering current values.
 */
	save_errno = errno;
	save_herrno = h_errno;

	hp = geth_byname(name);
	status = h_errno;

	seterrno(save_errno);
	seth_errno(save_herrno);

/*
 * Indicate negative result only after definitive lookup failures.
 */
	if (hp == NULL)
	{
		/* authoritative denial -- not existing or no A record */
		if (status == NO_DATA || status == HOST_NOT_FOUND)
			return(status);

		/* nameserver failure -- still undecided, assume ok */
		return(0);
	}

/*
 * The given name exists and there is an associated A record.
 * The name of this A record should be the name we queried about.
 * If this is not the case we probably supplied a CNAME.
 */
	status = sameword(hp->h_name, name) ? 0 : HOST_NOT_CANON;
	return(status);
}

/* 
** MAPREVERSE -- Check whether address maps back to given domain
** -------------------------------------------------------------
**
**	Returns:
**		NULL if address could definitively not be mapped.
**		Given name if the address maps back properly, or
**		in case of transient nameserver failures.
**		Reverse name if it differs from the given name.
*/

char *
mapreverse(name, inaddr)
input char *name;			/* domain name of A record */
input struct in_addr inaddr;		/* address of A record to check */
{
	struct hostent *hp;
	int status;
	int save_errno;
	int save_herrno;
	register int i;
	
/*
 * Preserve state when querying, to avoid clobbering current values.
 */
	save_errno = errno;
	save_herrno = h_errno;

	hp = geth_byaddr((char *)&inaddr, INADDRSZ, AF_INET);
	status = h_errno;

	seterrno(save_errno);
	seth_errno(save_herrno);

/*
 * Indicate negative result only after definitive lookup failures.
 */
	if (hp == NULL)
	{
		/* authoritative denial -- not existing or no PTR record */
		if (status == NO_DATA || status == HOST_NOT_FOUND)
			return(NULL);

		/* nameserver failure -- still undecided, assume ok */
		return(name);
	}

/*
 * Check whether the ``official'' host name matches.
 * This is the name in the first (or only) PTR record encountered.
 */
	if (sameword(hp->h_name, name))
		return(name);

/*
 * If not, a match may be found among the aliases.
 * They are available (as of BIND 4.9) in case multipe PTR records are used.
 */
	for (i = 0; hp->h_aliases[i]; i++)
	{
		if (sameword(hp->h_aliases[i], name))
			return(name);
	}

/*
 * The reverse mapping did not yield the given name.
 */
	return((char *)hp->h_name);
}

/* 
** ANYRECORD -- Check whether domain name has any resource records
** ---------------------------------------------------------------
**
**	Returns:
**		Nonzero if there are no resource records available.
**		0 if there are, or if it remains undecided.
*/

int
anyrecord(name)
input char *name;			/* the domain name to check */
{
	querybuf answer;
	register int n;
	int status;
	int save_errno;
	int save_herrno;
	
/*
 * Preserve state when querying, to avoid clobbering current values.
 */
	save_errno = errno;
	save_herrno = h_errno;

	n = get_info(&answer, name, T_ANY, queryclass);
	status = h_errno;

	seterrno(save_errno);
	seth_errno(save_herrno);

/*
 * Indicate negative result only after definitive lookup failures.
 */
	if (n < 0)
	{
		/* authoritative denial -- not existing or no ANY record */
		if (status == NO_DATA || status == HOST_NOT_FOUND)
			return(status);

		/* nameserver failure -- still undecided, assume ok */
		return(0);
	}

/*
 * The domain name exists, and there are resource records available.
 */
	return(0);
}

/* 
** COMPARE_NAME -- Compare two names wrt alphabetical order
** --------------------------------------------------------
**
**	Returns:
**		Value of case-insensitive comparison.
*/

int
compare_name(a, b)
input const ptr_t *a;			/* first name */
input const ptr_t *b;			/* second name */
{
	return(strcasecmp(*(char **)a, *(char **)b));
}
