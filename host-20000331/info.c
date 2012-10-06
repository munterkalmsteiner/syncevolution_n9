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
static char Version[] = "@(#)info.c	e07@nikhef.nl (Eric Wassenaar) 991527";
#endif

#include "host.h"
#include "glob.h"

/*
** GET_HOSTINFO -- Principal routine to query about given name
** -----------------------------------------------------------
**
**	Returns:
**		TRUE if requested info was obtained successfully.
**		FALSE otherwise.
**
**	This is the equivalent of the resolver module res_search().
**
**	In this program RES_DEFNAMES is always on, and RES_DNSRCH
**	is off by default. This means that single names without dot
**	are always, and only, tried within the own default domain,
**	and compound names are assumed to be already fully qualified.
**
**	The default BIND behavior can be simulated by turning on
**	RES_DNSRCH with -R. The given name, whether or not compound,
**	is then	first tried within the possible search domains.
**
**	Note. In the latter case, the search terminates in case the
**	specified name exists but does not have the desired type.
**	The BIND behavior is to continue the search. This can be
**	simulated with the undocumented option -B.
*/

bool
get_hostinfo(name, qualified)
input char *name;			/* name to query about */
input bool qualified;			/* assume fully qualified if set */
{
	register char **domain;
	register char *cp;
	int dot;			/* number of dots in query name */
	bool result;			/* result status of action taken */
	char oldnamebuf[2*MAXDNAME+2];
	char *oldname;			/* saved actual name when NO_DATA */
	int nodata = 0;			/* NO_DATA status during DNSRCH */
	int nquery = 0;			/* number of extra search queries */

/*
 * Single dot means root zone.
 */
	if (sameword(name, "."))
		qualified = TRUE;

/*
 * Names known to be fully qualified are just tried ``as is''.
 */
	if (qualified)
	{
		result = get_domaininfo(name, (char *)NULL);
		return(result);
	}

/*
 * Count number of dots. Move to the end of the name.
 */
	for (dot = 0, cp = name; *cp != '\0'; cp++)
		if (*cp == '.')
			dot++;

/*
 * Check for aliases of single name.
 * Note that the alias is supposed to be fully qualified.
 */
	if (dot == 0 && (cp = (char *)hostalias(name)) != NULL)
	{
		if (verbose)
			printf("Aliased %s to %s\n", name, cp);

		result = get_domaininfo(cp, (char *)NULL);
		return(result);
	}

/*
 * Trailing dot means absolute (fully qualified) address.
 */
	if (dot != 0 && cp[-1] == '.')
	{
		cp[-1] = '\0';
		result = get_domaininfo(name, (char *)NULL);
		cp[-1] = '.';
		return(result);
	}

/*
 * Append own default domain and other search domains if appropriate.
 */
	if ((dot == 0 && bitset(RES_DEFNAMES, _res.options)) ||
	    (dot != 0 && bitset(RES_DNSRCH, _res.options)))
	{
		for (domain = _res.dnsrch; *domain; domain++)
		{
			result = get_domaininfo(name, *domain);
			if (result)
				return(result);

			/* keep count of extra search queries */
			nquery++;

			/* in case nameserver not present */
			if (errno == ECONNREFUSED)
				return(FALSE);

			/* if no further search desired (single name) */
			if (!bitset(RES_DNSRCH, _res.options))
				break;

			/* if name exists but has not requested type */
			if (h_errno == NO_DATA || h_errno == NO_RREC)
			{
				if (bindcompat)
				{
					/* remember status and search up */
					oldname = strcpy(oldnamebuf, realname);
					nodata = h_errno;
					continue;
				}

				return(FALSE);
			}

			/* retry only if name does not exist at all */
			if (h_errno != HOST_NOT_FOUND && h_errno != NO_HOST)
				break;
		}
	}

/*
 * Single name lookup failed.
 */
	if (dot == 0)
	{
		/* unclear what actual name should be */
		if (nquery != 1)
			realname = NULL;

		/* restore nodata status from search */
		if (bindcompat && nodata)
		{
			realname = strcpy(realnamebuf, oldname);
			seth_errno(nodata);
		}

		/* set status in case we never queried */
		if (!bitset(RES_DEFNAMES, _res.options))
			seth_errno(HOST_NOT_FOUND);

		return(FALSE);
	}

/*
 * Rest means fully qualified.
 */
	result = get_domaininfo(name, (char *)NULL);

	/* restore nodata status from search */
	if (!result && bindcompat && nodata)
	{
		realname = strcpy(realnamebuf, oldname);
		seth_errno(nodata);
	}

	return(result);
}

/*
** GET_DOMAININFO -- Fetch and print desired info about name in domain
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if requested info was obtained successfully.
**		FALSE otherwise.
**
**	Side effects:
**		Sets global variable ``realname'' to actual name queried.
**
**	This is the equivalent of the resolver module res_querydomain().
**
**	Things get a little complicated in case RES_DNSRCH is on.
**	If we get an answer but the data is corrupted, an error will be
**	returned and NO_RECOVERY will be set. This will terminate the
**	extra search loop, but a compound name will still be tried as-is.
**	The same holds if the query times out or we have a server failure,
**	in which case an error will be returned and TRY_AGAIN will be set.
**	For now we take this for granted. Normally RES_DNSRCH is disabled.
**	In this default case we do only one query and we have no problem.
*/

bool
get_domaininfo(name, domain)
input char *name;			/* name to query about */
input char *domain;			/* domain to which name is relative */
{
	char namebuf[2*MAXDNAME+2];	/* buffer to store full domain name */
	querybuf answer;
	register int n;
	bool result;			/* result status of action taken */

/*
 * Show what we are about to query.
 */
	if (verbose)
	{
		if (domain == NULL || domain[0] == '\0')
			printf("Trying %s", name);
		else
			printf("Trying %s within %s", name, domain);

		if (server && (verbose > 1))
			printf(" at server %s", server);

		printf(" ...\n");
	}

/*
 * Construct the actual domain name.
 * A null domain means the given name is already fully qualified.
 * If the composite name is too long, res_mkquery() will fail.
 */
	if (domain == NULL || domain[0] == '\0')
		(void) sprintf(namebuf, "%.*s", MAXDNAME, name);
	else
		(void) sprintf(namebuf, "%.*s.%.*s",
				MAXDNAME, name, MAXDNAME, domain);
	name = namebuf;

/*
 * Fetch the desired info.
 */
	n = get_info(&answer, name, querytype, queryclass);
	result = (n < 0) ? FALSE : TRUE;

	/* may have extra information on negative answers */
	if (n < 0 && n != -1)
		n = -n;

/*
 * Print the relevant data.
 * If we got a positive answer, the data may still be corrupted.
 */
	if (n > 0 && !print_info(&answer, n, name, querytype, queryclass, TRUE))
		result = FALSE;

/*
 * Remember the actual name that was queried.
 * Must be at the end to avoid clobbering during recursive calls.
 */
	realname = strcpy(realnamebuf, name);

	return(result);
}

/*
** GET_INFO -- Basic routine to issue a nameserver query
** -----------------------------------------------------
**
**	Returns:
**		Real length of answer buffer, if obtained. Negative length if
**		query failed, or -1 if no answer (h_errno is set appropriately).
**
**	This is the equivalent of the resolver module res_query().
*/

int
get_info(answerbuf, name, type, class)
output querybuf *answerbuf;		/* location of buffer to store answer */
input char *name;			/* full name to query about */
input int type;				/* specific resource record type */
input int class;			/* specific resource record class */
{
	querybuf query;
	HEADER *bp;
	int ancount;
	register int n;

/*
 * Construct query, and send it to the nameserver.
 * res_send() will fail if no nameserver responded. In the BIND version the
 * possible values for errno are ECONNREFUSED and ETIMEDOUT. If we did get
 * an answer, errno should be reset, since res_send() may have left an errno
 * in case it has used datagrams. Our private version of res_send() will leave
 * also other error statuses, and will clear errno if an answer was obtained.
 */
	seterrno(0);	/* reset before querying nameserver */

	n = res_mkquery(QUERY, name, class, type, (qbuf_t *)NULL, 0,
			(rrec_t *)NULL, (qbuf_t *)&query, sizeof(querybuf));
	if (n < 0)
	{
		if (debug)
			printf("%sres_mkquery failed\n", dbprefix);
		seth_errno(NO_RECOVERY);
		return(-1);
	}

	n = res_send((qbuf_t *)&query, n, (qbuf_t *)answerbuf, sizeof(querybuf));
	if (n < 0)
	{
		if (debug)
			printf("%sres_send failed\n", dbprefix);
		seth_errno(TRY_AGAIN);
		return(-1);
	}

	seterrno(0);	/* reset after we got an answer */

	if (n < HFIXEDSZ)
	{
		pr_error("answer length %s too short after %s query for %s",
			dtoa(n), pr_type(type), name);
		seth_errno(NO_RECOVERY);
		return(-1);
	}

/*
 * Analyze the status of the answer from the nameserver.
 */
	if ((verbose > print_level) || debug)
		print_answer(answerbuf, n);

	bp = (HEADER *)answerbuf;
	ancount = ntohs((u_short)bp->ancount);

	if (bp->rcode != NOERROR || ancount == 0)
	{
		switch (bp->rcode)
		{
		    case NXDOMAIN:
			/* distinguish between authoritative or not */
			seth_errno(bp->aa ? HOST_NOT_FOUND : NO_HOST);
			break;

		    case NOERROR:
			/* distinguish between authoritative or not */
			seth_errno(bp->aa ? NO_DATA : NO_RREC);
			break;

		    case SERVFAIL:
			seth_errno(SERVER_FAILURE); /* instead of TRY_AGAIN */
			break;

		    case REFUSED:
			seth_errno(QUERY_REFUSED); /* instead of NO_RECOVERY */
			break;

		    default:
			seth_errno(NO_RECOVERY); /* FORMERR NOTIMP NOCHANGE */
			break;
		}
		n = querysize(n);
		return(-n);
	}

	/* valid answer received, avoid buffer overrun */
	seth_errno(0);
	n = querysize(n);
	return(n);
}

/*
** PRINT_INFO -- Check resource records in answer and print relevant data
** ----------------------------------------------------------------------
**
**	Returns:
**		TRUE if answer buffer was processed successfully.
**		FALSE otherwise.
**
**	Side effects:
**		Will recurse on MAILB records if appropriate.
**		See also side effects of the print_rrec() routine.
*/

bool
print_info(answerbuf, answerlen, name, type, class, regular)
input querybuf *answerbuf;		/* location of answer buffer */
input int answerlen;			/* length of answer buffer */
input char *name;			/* full name we are querying about */
input int type;				/* record type we are querying about */
input int class;			/* record class we are querying about */
input bool regular;			/* set if this is a regular lookup */
{
	HEADER *bp;
	int qdcount, ancount, nscount, arcount;
	u_char *msg, *eom;
	register u_char *cp;

	bp = (HEADER *)answerbuf;
	qdcount = ntohs((u_short)bp->qdcount);
	ancount = ntohs((u_short)bp->ancount);
	nscount = ntohs((u_short)bp->nscount);
	arcount = ntohs((u_short)bp->arcount);

	msg = (u_char *)answerbuf;
	eom = (u_char *)answerbuf + answerlen;
	cp  = (u_char *)answerbuf + HFIXEDSZ;

/*
 * Skip the query section in the response (present only in normal queries).
 */
	if (qdcount)
	{
		while (qdcount > 0 && cp < eom)	/* process all records */
		{
			cp = skip_qrec(name, type, class, cp, msg, eom);
			if (cp == NULL)
				return(FALSE);
			qdcount--;
		}

		if (qdcount)
		{
			pr_error("invalid qdcount after %s query for %s",
				pr_type(type), name);
			seth_errno(NO_RECOVERY);
			return(FALSE);
		}
	}

/*
 * Process the actual answer section in the response.
 * During zone transfers, this is the only section available.
 */
	if (ancount)
	{
		if ((type != T_AXFR) && verbose && !bp->aa)
			printf("The following answer is not authoritative:\n");

		while (ancount > 0 && cp < eom)
		{
			/* reset for each record during zone listings */
			soaname = NULL, subname = NULL, adrname = NULL, address = 0;

			print_level++;
			cp = print_rrec(name, type, class, cp, msg, eom, regular);
			print_level--;
			if (cp == NULL)
				return(FALSE);
			ancount--;

			/* update zone information during zone listings */
			if (type == T_AXFR)
				update_zone(name);

			/* we trace down CNAME chains ourselves */
			if (regular && !verbose && cname)
				return(TRUE);

			/* recursively expand MR/MG records into MB records */
			if (regular && mailmode && mname)
				(void) get_recursive(&mname);
		}

		if (ancount)
		{
			pr_error("invalid ancount after %s query for %s",
				pr_type(type), name);
			seth_errno(NO_RECOVERY);
			return(FALSE);
		}
	}

/*
 * The nameserver and additional info section are normally not processed.
 * Both sections shouldn't exist in zone transfers.
 */
	if (!verbose || exclusive)
		return(TRUE);

	if (nscount)
	{
		printf("Authority information:\n");

		while (nscount > 0 && cp < eom)
		{
			print_level++;
			cp = print_rrec(name, type, class, cp, msg, eom, FALSE);
			print_level--;
			if (cp == NULL)
				return(FALSE);
			nscount--;
		}

		if (nscount)
		{
			pr_error("invalid nscount after %s query for %s",
				pr_type(type), name);
			seth_errno(NO_RECOVERY);
			return(FALSE);
		}
	}

	if (arcount)
	{
		printf("Additional information:\n");

		while (arcount > 0 && cp < eom)
		{
			print_level++;
			cp = print_rrec(name, type, class, cp, msg, eom, FALSE);
			print_level--;
			if (cp == NULL)
				return(FALSE);
			arcount--;
		}

		if (arcount)
		{
			pr_error("invalid arcount after %s query for %s",
				pr_type(type), name);
			seth_errno(NO_RECOVERY);
			return(FALSE);
		}
	}

	/* all sections were processed successfully */
	return(TRUE);
}

/*
** PRINT_DATA -- Output resource record data if this record is wanted
** ------------------------------------------------------------------
**
**	Returns:
**		None.
**
**	Inputs:
**		The global variable ``doprint'' is set by print_rrec()
**		if we need to print the data.
*/

static bool doprint;		/* indicates whether or not to print */

void /*VARARGS1*/
print_data(fmt, a, b, c, d)
input char *fmt;			/* format of message */
input char *a, *b, *c, *d;		/* optional arguments */
{
	/* if (doprint) */
	{
		if (!suppress)
			printf(fmt, a, b, c, d);

		if (logfile != NULL)
			(void) fprintf(logfile, fmt, a, b, c, d);
	}
}

#define doprintf(x)\
{\
	if (doprint)\
	{\
		print_data x ;\
	}\
}

/*
** PRINT_RREC -- Decode single resource record and output relevant data
** --------------------------------------------------------------------
**
**	Returns:
**		Pointer to position in answer buffer after current record.
**		NULL if there was a format error in the current record.
**
**	Outputs:
**		Sets ``doprint'' appropriately for use by print_data().
**
**	Side effects:
**		Updates resource record statistics in record_stats[].
**		Sets ``soaname'' if this is an SOA record.
**		Sets ``subname'' if this is an NS record.
**		Sets ``adrname'' if this is an A record.
**		Sets ``address'' if this is an A record.
**		Sets ``cname'' if this is a valid CNAME record.
**		Sets ``mname'' if this is a valid MAILB record.
**		These variables must have been cleared before calling
**		print_info() and may be checked afterwards.
*/

/* print domain names after certain conversions */
#define pr_name(x)	pr_domain(x, listing)

/* check the LHS record name of these records for invalid characters */
#define test_valid(t)	(((t == T_A) && !reverse) || t == T_MX || t == T_AAAA)

/* check the RHS domain name of these records for canonical host names */
#define test_canon(t)	(t == T_NS || t == T_MX)

/* an ordinary PTR record in a reverse zone */
#define test_ptr(t,s)	(((t == T_PTR) && reverse) && !zeroname(s))

/* an ordinary A record in a forward zone */
#define test_adr(t,a)	(((t == T_A) && !reverse) && !fakeaddr(a))

u_char *
print_rrec(name, qtype, qclass, cp, msg, eom, regular)
input char *name;			/* full name we are querying about */
input int qtype;			/* record type we are querying about */
input int qclass;			/* record class we are querying about */
register u_char *cp;			/* current position in answer buf */
input u_char *msg, *eom;		/* begin and end of answer buf */
input bool regular;			/* set if this is a regular lookup */
{
	char rname[MAXDNAME+1];		/* record name in LHS */
	char dname[MAXDNAME+1];		/* domain name in RHS */
	int type, class, ttl, dlen;	/* fixed values in every record */
	u_char *eor;			/* predicted position of next record */
	bool classmatch;		/* set if we want to see this class */
	bool listing;			/* set if this is a zone listing */
	char *host = listhost;		/* contacted host for zone listings */
	char *dumpmsg = NULL;		/* set if data should be dumped */
	register int n, c;
	struct in_addr inaddr;
	struct protoent *protocol;
	struct servent *service;

/*
 * Pickup the standard values present in each resource record.
 */
	n = expand_name(name, T_NONE, cp, msg, eom, rname);
	if (n < 0)
		return(NULL);
	cp += n;

	n = 3*INT16SZ + INT32SZ;
	if (check_size(rname, T_NONE, cp, msg, eom, n) < 0)
		return(NULL);

	type = _getshort(cp);
	cp += INT16SZ;

	class = _getshort(cp);
	cp += INT16SZ;

	ttl = _getlong(cp);
	cp += INT32SZ;

	dlen = _getshort(cp);
	cp += INT16SZ;

	if (check_size(rname, type, cp, msg, eom, dlen) < 0)
		return(NULL);
	eor = cp + dlen;

/*
 * Decide whether or not to print this resource record.
 */
	listing = (qtype == T_AXFR || qtype == T_IXFR) ? TRUE : FALSE;

	if (listing)
	{
		classmatch = want_class(class, queryclass);
		doprint = classmatch && want_type(type, querytype);
	}
	else
	{
		classmatch = want_class(class, C_ANY);
		doprint = classmatch && want_type(type, T_ANY);
	}

	if (doprint && exclusive && !indomain(rname, name, TRUE))
		doprint = FALSE;

	if (doprint && exclusive && fakename(rname))
		doprint = FALSE;

	if (doprint && wildcards && !in_string(rname, '*'))
		doprint = FALSE;
#ifdef justfun
	if (namelen && (strlength(rname) < namelen))
		doprint = FALSE;
#endif

/*
 * Print name and common values, if appropriate.
 */
	doprintf(("%-20s", pr_name(rname)))

	if (verbose || ttlprint)
		doprintf(("\t%s", dtoa(ttl)))

	if (verbose || classprint || (class != qclass))
		doprintf(("\t%s", pr_class(class)))

	doprintf(("\t%s", pr_type(type)))

/*
 * Update resource record statistics for zone listing.
 */
	if (listing && classmatch)
	{
		if (type >= T_FIRST && type <= T_LAST)
			record_stats[type]++;
	}

/*
 * Save the domain name of an SOA or NS or A record for zone listing.
 */
	if (listing && classmatch)
	{
		if (type == T_A)
			adrname = strcpy(adrnamebuf, rname);

		else if (type == T_NS)
			subname = strcpy(subnamebuf, rname);

		else if (type == T_SOA)
			soaname = strcpy(soanamebuf, rname);
	}

/*
 * Print type specific data, if appropriate.
 */
	switch (type)
	{
	    case T_A:
		if (class == C_IN || class == C_HS)
		{
			if (dlen == INADDRSZ)
			{
				bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
				address = inaddr.s_addr;
				doprintf(("\t%s", inet_ntoa(inaddr)))
				cp += INADDRSZ;
				break;
			}
#ifdef obsolete
			if (dlen == INADDRSZ + 1 + INT16SZ)
			{
				bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
				address = inaddr.s_addr;
				doprintf(("\t%s", inet_ntoa(inaddr)))
				cp += INADDRSZ;

				n = *cp++;
				doprintf((" ; proto = %s", dtoa(n)))

				n = _getshort(cp);
				doprintf((", port = %s", dtoa(n)))
				cp += INT16SZ;
				break;
			}
#endif
			address = 0;
			break;
		}
		address = 0;
		cp += dlen;
		break;

	    case T_MX:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_NS:
	    case T_PTR:
	    case T_CNAME:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;
		break;

	    case T_HINFO:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t\"%s\"", stoa(cp, n, TRUE)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t\"%s\"", stoa(cp, n, TRUE)))
		cp += n;
		break;

	    case T_SOA:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;

		n = 5*INT32SZ;
		if (check_size(rname, type, cp, msg, eor, n) < 0)
			break;
		doprintf((" ("))

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", utoa(n)))
		doprintf(("\t;serial (version)"))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", dtoa(n)))
		doprintf(("\t;refresh period (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", dtoa(n)))
		doprintf(("\t;retry interval (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", dtoa(n)))
		doprintf(("\t;expire time (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", dtoa(n)))
		doprintf(("\t;default ttl (%s)", pr_time(n, FALSE)))
		cp += INT32SZ;

		doprintf(("\n\t\t\t)"))
		break;

	    case T_WKS:
		if (check_size(rname, type, cp, msg, eor, INADDRSZ) < 0)
			break;
		bcopy((char *)cp, (char *)&inaddr, INADDRSZ);
		doprintf(("\t%s", inet_ntoa(inaddr)))
		cp += INADDRSZ;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;

		protocol = getprotobynumber(n);
		if (protocol != NULL)
			doprintf((" %s", protocol->p_name))
		else
			doprintf((" %s", dtoa(n)))

		doprintf((" ("))
		n = 0;
		while (cp < eor)
		{
		    c = *cp++;
		    do
		    {
 			if (c & 0200)
			{
			    int port;

			    port = htons((u_short)n);
			    if (protocol != NULL)
				    service = getservbyport(port, protocol->p_name);
			    else
				    service = NULL;

			    if (service != NULL)
				    doprintf((" %s", service->s_name))
			    else
				    doprintf((" %s", dtoa(n)))
			}
 			c <<= 1;
		    } while (++n & 07);
		}
		doprintf((" )"))
		break;

#ifdef obsolete
	    case T_TXT:
		/* if (dlen > 0) */
		{
			doprintf(("\t\"%s\"", stoa(cp, dlen, TRUE)))
			cp += dlen;
		}
		break;
#endif

	    case T_TXT:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t\"%s\"", stoa(cp, n, TRUE)))
		cp += n;

		while (cp < eor)
		{
			if (check_size(rname, type, cp, msg, eor, 1) < 0)
				break;
			n = *cp++;
			doprintf((" \"%s\"", stoa(cp, n, TRUE)))
			cp += n;
		}
		break;

	    case T_MINFO:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_MB:
	    case T_MG:
	    case T_MR:
	    case T_MD:
	    case T_MF:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;
		break;

	    case T_UID:
	    case T_GID:
		if (dlen == INT32SZ)
		{
			n = _getlong(cp);
			doprintf(("\t%s", dtoa(n)))
			cp += INT32SZ;
		}
		break;

	    case T_UINFO:
		doprintf(("\t\"%s\"", stoa(cp, dlen, TRUE)))
		cp += dlen;
		break;

	    case T_RP:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_RT:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_AFSDB:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_X25:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n, FALSE)))
		cp += n;
		break;

	    case T_ISDN:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n, FALSE)))
		cp += n;

		if (cp < eor)
		{
			if (check_size(rname, type, cp, msg, eor, 1) < 0)
				break;
			n = *cp++;
			doprintf((" %s", stoa(cp, n, FALSE)))
			cp += n;
		}
		break;

	    case T_NSAP:
		doprintf(("\t0x%s", nsap_ntoa(cp, dlen)))
		cp += dlen;
		break;

	    case T_NSAPPTR:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;
		break;

	    case T_PX:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_GPOS:
		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n, FALSE)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n, FALSE)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf(("\t%s", stoa(cp, n, FALSE)))
		cp += n;
		break;

	    case T_LOC:
		if ((n = *cp) != T_LOC_VERSION)
		{
			pr_error("invalid version %s in %s record for %s",
				dtoa(n), pr_type(type), rname);
			cp += dlen;
			break;
		}

		n = INT32SZ + 3*INT32SZ;
		if (check_size(rname, type, cp, msg, eor, n) < 0)
			break;
		c = _getlong(cp);
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\t%s ", pr_spherical(n, "N", "S")))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf((" %s ", pr_spherical(n, "E", "W")))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf((" %sm ", pr_vertical(n, "", "-")))
		cp += INT32SZ;

		doprintf((" %sm", pr_precision((c >> 16) & 0xff)))
		doprintf((" %sm", pr_precision((c >>  8) & 0xff)))
		doprintf((" %sm", pr_precision((c >>  0) & 0xff)))
		break;

	    case T_UNSPEC:
	    case T_NULL:
		cp += dlen;
		break;

	    case T_AAAA:
		if (dlen == IPNGSIZE)
		{
			doprintf(("\t%s", ipng_ntoa(cp)))
			cp += IPNGSIZE;
		}
		break;

	    case T_SIG:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		if (n >= T_FIRST && n <= T_LAST)
			doprintf(("\t%s", pr_type(n)))
		else
			doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" %s", dtoa(n)))

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" %s", dtoa(n)))

		n = 3*INT32SZ + INT16SZ;
		if (check_size(rname, type, cp, msg, eor, n) < 0)
			break;
		doprintf((" ("))

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", dtoa(n)))
		doprintf(("\t\t;original ttl"))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", pr_date(n)))
		doprintf(("\t;signature expiration"))
		cp += INT32SZ;

		n = _getlong(cp);
		doprintf(("\n\t\t\t%s", pr_date(n)))
		doprintf(("\t;signature inception"))
		cp += INT32SZ;

		n = _getshort(cp);
		doprintf(("\n\t\t\t%s", dtoa(n)))
		doprintf(("\t\t;key tag"))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\n\t\t\t%s", pr_name(dname)))
		cp += n;

		if (cp < eor)
		{
			register char *buf;
			register int size;

			n = eor - cp;
			buf = base_ntoa(cp, n);
			size = strlength(buf);
			cp += n;

			while ((n = (size > 64) ? 64 : size) > 0)
			{
				doprintf(("\n\t%s", stoa((u_char *)buf, n, FALSE)))
				buf += n; size -= n;
			}
		}
		doprintf(("\n\t\t\t)"))
		break;

	    case T_KEY:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t0x%s", xtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" %s", dtoa(n)))

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" %s", dtoa(n)))

		if (cp < eor)
		{
			register char *buf;
			register int size;

			n = eor - cp;
			buf = base_ntoa(cp, n);
			size = strlength(buf);
			cp += n;

			doprintf((" ("))
			while ((n = (size > 64) ? 64 : size) > 0)
			{
				doprintf(("\n\t%s", stoa((u_char *)buf, n, FALSE)))
				buf += n; size -= n;
			}
			doprintf(("\n\t\t\t)"))
		}
		break;

	    case T_NXT:
		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf(("\t%s", pr_name(dname)))
		cp += n;

		n = 0;
		while (cp < eor)
		{
		    c = *cp++;
		    do
		    {
 			if (c & 0200)
			{
			    if (n >= T_FIRST && n <= T_LAST)
				    doprintf((" %s", pr_type(n)))
			    else
				    doprintf((" %s", dtoa(n)))
			}
 			c <<= 1;
		    } while (++n & 07);
		}
		break;

	    case T_SRV:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf((" %s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf((" %s", dtoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_NAPTR:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf((" %s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" \"%s\"", stoa(cp, n, TRUE)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" \"%s\"", stoa(cp, n, TRUE)))
		cp += n;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" \"%s\"", stoa(cp, n, TRUE)))
		cp += n;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_KX:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		n = expand_name(rname, type, cp, msg, eom, dname);
		if (n < 0)
			break;
		doprintf((" %s", pr_name(dname)))
		cp += n;
		break;

	    case T_CERT:
		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf(("\t%s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, INT16SZ) < 0)
			break;
		n = _getshort(cp);
		doprintf((" %s", dtoa(n)))
		cp += INT16SZ;

		if (check_size(rname, type, cp, msg, eor, 1) < 0)
			break;
		n = *cp++;
		doprintf((" %s", dtoa(n)))

		if (cp < eor)
		{
			register char *buf;
			register int size;

			n = eor - cp;
			buf = base_ntoa(cp, n);
			size = strlength(buf);
			cp += n;

			doprintf((" ("))
			while ((n = (size > 64) ? 64 : size) > 0)
			{
				doprintf(("\n\t%s", stoa((u_char *)buf, n, FALSE)))
				buf += n; size -= n;
			}
			doprintf(("\n\t\t\t)"))
		}
		break;

	    case T_EID:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_NIMLOC:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_ATMA:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_A6:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_DNAME:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_SINK:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_OPT:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_ADDRS:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_TKEY:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    case T_TSIG:
		dumpmsg = "not implemented";
		cp += dlen;
		break;

	    default:
		dumpmsg = "unknown type";
		cp += dlen;
		break;
	}

	/* dump the data portion if necessary */
	if ((dumpmsg != NULL) || dumpdata)
		dump_rrec(eor - dlen, dlen, dumpmsg);

/*
 * End of specific data type processing.
 * Terminate resource record printout.
 */
	doprintf(("\n"))

/*
 * Check whether we have reached the exact end of this resource record.
 * If not, we cannot be sure that the record has been decoded correctly,
 * and therefore the subsequent tests will be skipped.
 * Maybe we should hex dump the entire record.
 */
	if (cp != eor)
	{
		pr_error("size error in %s record for %s, off by %s",
			pr_type(type), rname, dtoa(cp - eor));

		/* we believe value of dlen; should perhaps return(NULL) */
		return(eor);
	}

/*
 * Save the CNAME alias for cname chain tracing.
 * Save the MR or MG alias for MB chain tracing.
 * These features can be enabled only in normal mode.
 */
	if (regular && classmatch)
	{
		if (type == T_CNAME)
			cname = strcpy(cnamebuf, dname);

		else if (type == T_MR || type == T_MG)
			mname = strcpy(mnamebuf, dname);
	}

/*
 * Suppress the subsequent checks in quiet mode.
 * This can safely be done as there are no side effects.
 * It may speedup things, and nothing would be printed anyway.
 * Also suppress the checks if explicitly requested in quick mode.
 */
	if (quiet || quick)
		return(cp);

/*
 * Check for resource records with a zero ttl value. They are not cached.
 * This may lead to problems, e.g. when retrieving MX records and there
 * exists only a zero-ttl CNAME record pointing to a zero-ttl A record.
 * Certain resource records always have a zero ttl value.
 */
	if ((ttl == 0) && (type != T_SIG))
	{
		pr_warning("%s %s record has zero ttl",
			rname, pr_type(type));
	}

/*
 * In zone listings, resource records with the same name/type/class
 * must have the same ttl value. Maintain and check list of record info.
 * This is done on a per-zone basis.
 */
	if (listing && !check_ttl(rname, type, class, ttl))
	{
		pr_warning("%s %s records have different ttl within %s from %s",
			rname, pr_type(type), name, host);
	}

/*
 * Check validity of 'host' related domain names in certain resource records.
 * These include LHS record names and RHS domain names of selected records.
 * By default underscores are not reported during deep recursive listings.
 */
	if (test_valid(type) && !valid_name(rname, TRUE, FALSE, underskip))
	{
		pr_warning("%s %s record has illegal name",
			rname, pr_type(type));
	}

	if (test_canon(type) && !valid_name(dname, FALSE, FALSE, underskip))
	{
		pr_warning("%s %s host %s has illegal name",
			rname, pr_type(type), dname);
	}

	if (test_ptr(type, rname) && !valid_name(dname, FALSE, FALSE, underskip))
	{
		pr_warning("%s %s host %s has illegal name",
			rname, pr_type(type), dname);
	}

/*
 * The RHS of various resource records should refer to a canonical host name,
 * i.e. it should exist and have an A record and not be a CNAME.
 * By default this test is suppressed during deep recursive zone listings.
 * Results are cached globally, not on a per-zone basis.
 */
	if (!canonskip && test_canon(type) && ((n = check_canon(dname)) != 0))
	{
		/* only report definitive target host failures */
		if (n == HOST_NOT_FOUND)
			pr_warning("%s %s host %s does not exist",
				rname, pr_type(type), dname);
		else if (n == NO_DATA)
			pr_warning("%s %s host %s has no A record",
				rname, pr_type(type), dname);
		else if (n == HOST_NOT_CANON)
			pr_warning("%s %s host %s is not canonical",
				rname, pr_type(type), dname);

		/* authoritative failure to find nameserver target host */
		if (type == T_NS && (n == NO_DATA || n == HOST_NOT_FOUND))
		{
			if (server == NULL)
				errmsg("%s has lame delegation to %s",
					rname, dname);
		}
	}

/*
 * On request, check the RHS of a PTR record when processing a reverse zone,
 * which should refer to a canonical host name, i.e. it should exist and
 * have an A record and not be a CNAME. Results are not cached in this case.
 * Currently this option has effect here only during zone listings.
 * Note that this does not check CIDR delegations as mentioned in RFC 2317,
 * where PTR records are replaced with CNAME records.
 * Also note that this may generate warnings for PTR records for host 0 or
 * host 255 entries, indicating network names as suggested by RFC 1101.
 */
	if (addrmode && test_ptr(type, rname) && ((n = canonical(dname)) != 0))
	{
		/* only report definitive target host failures */
		if (n == HOST_NOT_FOUND)
			pr_warning("%s %s host %s does not exist",
				rname, pr_type(type), dname);
		else if (n == NO_DATA)
			pr_warning("%s %s host %s has no A record",
				rname, pr_type(type), dname);
		else if (n == HOST_NOT_CANON)
			pr_warning("%s %s host %s is not canonical",
				rname, pr_type(type), dname);
	}

/*
 * On request, reverse map the address of an A record, and verify that
 * it is registered and maps back to the name of the A record.
 * Currently this option has effect here only during zone listings.
 * Note that in reverse zones there are usually no A records, except
 * perhaps to specify a network mask as suggested in RFC 1101.
 */
	if (addrmode && test_adr(type, address))
	{
		host = mapreverse(rname, inaddr);
		if (host == NULL)
			pr_warning("%s address %s is not registered",
				rname, inet_ntoa(inaddr));
		else if (host != rname)
			pr_warning("%s address %s maps to %s",
				rname, inet_ntoa(inaddr), host);
	}

/*
 * On request, check the target in CNAME records for existence.
 */
	if (cnamecheck && (type == T_CNAME) && ((n = anyrecord(dname)) != 0))
	{
		/* only report definitive target host failures */
		if (n == HOST_NOT_FOUND)
			pr_warning("%s %s target %s does not exist",
				rname, pr_type(type), dname);
		else if (n == NO_DATA)
			pr_warning("%s %s target %s has no ANY record",
				rname, pr_type(type), dname);
	}

/*
 * This record was processed successfully.
 */
	return(cp);
}

/*
** DUMP_RREC -- Print data of resource record in hex and ascii
** -----------------------------------------------------------
**
**	Returns:
**		None.
*/

static char ascbuf[] = "................";
static char hexbuf[] = "XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX";

void
dump_rrec(cp, size, comment)
input u_char *cp;			/* current position in answer buf */
input int size;				/* number of bytes to extract */
input char *comment;			/* additional comment text */
{
	register int c;
	register int n;
	register int i;

	if (comment != NULL)
		doprintf(("\t# (\t; %s", comment))

	while ((n = (size > 16) ? 16 : size) > 0)
	{
		for (i = 0; i < n; i++, cp++)
		{
			ascbuf[i] = is_print(*cp) ? *cp : '.';
			c = ((int)(*cp) >> 4) & 0x0f;
			hexbuf[i*3+0] = hexdigit(c);
			c = ((int)(*cp) >> 0) & 0x0f;
			hexbuf[i*3+1] = hexdigit(c);
		}

		for (size -= n; i < 16; i++)
		{
			ascbuf[i] = ' ';
			hexbuf[i*3+0] = ' ';
			hexbuf[i*3+1] = ' ';
		}

		if (comment != NULL)
			doprintf(("\n\t%s ; %s", hexbuf, ascbuf))
		else
			doprintf(("\n%s\t%s ; %s", dbprefix, hexbuf, ascbuf))
	}

	if (comment != NULL)
		doprintf(("\n\t)"))
}

/*
** SKIP_QREC -- Skip the query record in the nameserver answer buffer
** ------------------------------------------------------------------
**
**	Returns:
**		Pointer to position in answer buffer after current record.
**		NULL if there was a format error in the current record.
*/

u_char *
skip_qrec(name, qtype, qclass, cp, msg, eom)
input char *name;			/* full name we are querying about */
input int qtype;			/* record type we are querying about */
input int qclass;			/* record class we are querying about */
register u_char *cp;			/* current position in answer buf */
input u_char *msg, *eom;		/* begin and end of answer buf */
{
	char rname[MAXDNAME+1];		/* record name in LHS */
	int type, class;		/* fixed values in query record */
	register int n;

/*
 * Pickup the standard values present in the query section.
 */
	n = expand_name(name, T_NONE, cp, msg, eom, rname);
	if (n < 0)
		return(NULL);
	cp += n;

	n = 2*INT16SZ;
	if (check_size(rname, T_NONE, cp, msg, eom, n) < 0)
		return(NULL);

	type = _getshort(cp);
	cp += INT16SZ;

	class = _getshort(cp);
	cp += INT16SZ;

#ifdef lint
	if (verbose)
		printf("%-20s\t%s\t%s\n",
			rname, pr_class(class), pr_type(type));
#endif

/*
 * The values in the answer should match those in the query.
 * If there is a mismatch, we just signal an error, but don't abort.
 * For regular queries there is exactly one record in the query section.
 */
	if (!sameword(rname, name))
		pr_error("invalid answer name %s after %s query for %s",
			rname, pr_type(qtype), name);

	if (type != qtype)
		pr_error("invalid answer type %s after %s query for %s",
			pr_type(type), pr_type(qtype), name);

	if (class != qclass)
		pr_error("invalid answer class %s after %s query for %s",
			pr_class(class), pr_type(qtype), name);

	return(cp);
}

/*
** GET_RECURSIVE -- Wrapper for get_hostinfo() during recursion
** ------------------------------------------------------------
**
**	Returns:
**		TRUE if requested info was obtained successfully.
**		FALSE otherwise.
*/

bool
get_recursive(name)
input char **name;			/* name to query about */
{
	static int level = 0;		/* recursion level */
	char newnamebuf[MAXDNAME+1];
	char *newname;			/* new name to look up */
	bool result;			/* result status of action taken */
	int save_errno;
	int save_herrno;

	if (level > MAXCHAIN)
	{
		errmsg("Recursion too deep");
		return(FALSE);
	}

	/* save local copy, and reset indicator */
	newname = strcpy(newnamebuf, *name);
	*name = NULL;

	save_errno = errno;
	save_herrno = h_errno;

	level++;
	result = get_hostinfo(newname, TRUE);
	level--;

	seterrno(save_errno);
	seth_errno(save_herrno);

	return(result);
}
