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
static char Version[] = "@(#)addr.c	e07@nikhef.nl (Eric Wassenaar) 990605";
#endif

#include "host.h"
#include "glob.h"

/*
** CHECK_ADDR -- Check whether reverse address mappings revert to host
** -------------------------------------------------------------------
**
**	Returns:
**		TRUE if all addresses of host map back to host.
**		FALSE otherwise.
*/

bool
check_addr(name)
input char *name;			/* host name to check addresses for */
{
	struct hostent *hp;
	register int i;
	struct in_addr inaddr[MAXADDRS];
	int naddress;
	char *hname, hnamebuf[MAXDNAME+1];
	int matched = 0;

/*
 * Look up the specified host to fetch its addresses.
 */
	hp = gethostbyname(name);
	if (hp == NULL)
	{
		ns_error(name, T_A, C_IN, server);
		return(FALSE);
	}

	hname = strncpy(hnamebuf, hp->h_name, MAXDNAME);
	hname[MAXDNAME] = '\0';

	for (i = 0; i < MAXADDRS && hp->h_addr_list[i]; i++)
		inaddr[i] = incopy(hp->h_addr_list[i]);
	naddress = i;

	if (verbose)
		printf("Found %d address%s for %s\n",
			naddress, plurale(naddress), hname);

/*
 * Map back the addresses found, and check whether they revert to host.
 */
	for (i = 0; i < naddress; i++)
		if (check_addr_name(inaddr[i], hname))
			matched++;

	return((matched == naddress) ? TRUE : FALSE);
}

/*
** CHECK_ADDR_NAME -- Check whether reverse address mappings revert to host
** ------------------------------------------------------------------------
**
**	Returns:
**		TRUE if the given address of host maps back to host.
**		FALSE otherwise.
*/

bool
check_addr_name(inaddr, name)
input struct in_addr inaddr;		/* address of host to map back */
input char *name;			/* name of host to check */
{
	struct hostent *hp;
	register int i;
	char *iname, inamebuf[MAXDNAME+1];
	int matched = 0;

/*
 * Fetch the reverse mapping of the given host address.
 */
	iname = strcpy(inamebuf, inet_ntoa(inaddr));

	if (verbose)
		printf("Checking %s address %s\n", name, iname);

	hp = gethostbyaddr((char *)&inaddr, INADDRSZ, AF_INET);
	if (hp == NULL)
	{
		ns_error(iname, T_PTR, C_IN, server);
		return(FALSE);
	}

/*
 * Check whether the ``official'' host name matches.
 * This is the name in the first (or only) PTR record encountered.
 */
	if (!sameword(hp->h_name, name))
		pr_warning("%s address %s maps to %s",
			name, iname, hp->h_name);
	else
		matched++;

/*
 * If not, a match may be found among the aliases.
 * They are available (as of BIND 4.9) in case multipe PTR records are used.
 */
	if (!matched)
	{
		for (i = 0; hp->h_aliases[i]; i++)
		{
			pr_warning("%s address %s maps to alias %s",
				name, iname, hp->h_aliases[i]);

			if (sameword(hp->h_aliases[i], name))
				matched++;
		}
	}

	return(matched ? TRUE : FALSE);
}

/*
** CHECK_NAME -- Check whether address belongs to host addresses
** -------------------------------------------------------------
**
**	Returns:
**		TRUE if given address was found among host addresses.
**		FALSE otherwise.
*/

bool
check_name(addr)
input ipaddr_t addr;			/* address of host to check */
{
	struct hostent *hp;
	register int i;
	struct in_addr inaddr;
	char *iname, inamebuf[MAXDNAME+1];
	char *hname, hnamebuf[MAXDNAME+1];
	char *aname, anamebuf[MAXALIAS][MAXDNAME+1];
	int naliases;
	int matched = 0;

/*
 * Check whether the address is registered by fetching its host name.
 */
	inaddr.s_addr = addr;
	iname = strcpy(inamebuf, inet_ntoa(inaddr));

	hp = gethostbyaddr((char *)&inaddr, INADDRSZ, AF_INET);
	if (hp == NULL)
	{
		ns_error(iname, T_PTR, C_IN, server);
		return(FALSE);
	}

	hname = strncpy(hnamebuf, hp->h_name, MAXDNAME);
	hname[MAXDNAME] = '\0';

	if (verbose)
		printf("Address %s maps to %s\n", iname, hname);

/*
 * In case of multiple PTR records, additional names are stored as aliases.
 */
	for (i = 0; i < MAXALIAS && hp->h_aliases[i]; i++)
	{
		aname = strncpy(anamebuf[i], hp->h_aliases[i], MAXDNAME);
		aname[MAXDNAME] = '\0';

		if (verbose)
			printf("Address %s maps to alias %s\n", iname, aname);
	}
	naliases = i;

/*
 * Check whether the given address belongs to the host name and the aliases.
 */
	if (check_name_addr(hname, addr))
		matched++;

	for (i = 0; i < naliases; i++)
	{
		aname = anamebuf[i];
		if (check_name_addr(aname, addr))
			matched++;
	}

	return((matched == (naliases + 1)) ? TRUE : FALSE);
}

/*
** CHECK_NAME_ADDR -- Check whether address belongs to host
** --------------------------------------------------------
**
**	Returns:
**		TRUE if given address was found among host addresses.
**		FALSE otherwise.
*/

bool
check_name_addr(name, addr)
input char *name;			/* name of host to check */
input ipaddr_t addr;			/* address should belong to host */
{
	struct hostent *hp;
	register int i;
	struct in_addr inaddr;
	char *iname, inamebuf[MAXDNAME+1];
	int matched = 0;

	inaddr.s_addr = addr;
	iname = strcpy(inamebuf, inet_ntoa(inaddr));

/*
 * Lookup the host name found to fetch its addresses.
 */
	hp = gethostbyname(name);
	if (hp == NULL)
	{
		ns_error(name, T_A, C_IN, server);
		return(FALSE);
	}

/*
 * Verify whether the mapped host name is canonical.
 */
	if (!sameword(hp->h_name, name))
		pr_warning("%s host %s is not canonical (%s)",
			iname, name, hp->h_name);

/*
 * Check whether the given address is listed among the known addresses.
 */
	for (i = 0; hp->h_addr_list[i]; i++)
	{
		inaddr = incopy(hp->h_addr_list[i]);

		if (verbose)
			printf("Checking %s address %s\n",
				name, inet_ntoa(inaddr));

		if (inaddr.s_addr == addr)
			matched++;
	}

	if (!matched)
		pr_error("address %s does not belong to %s",
			iname, name);

	return(matched ? TRUE : FALSE);
}
