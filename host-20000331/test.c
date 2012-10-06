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
static char Version[] = "@(#)test.c	e07@nikhef.nl (Eric Wassenaar) 991515";
#endif

#include "host.h"
#include "glob.h"

/*
** TEST -- Facility to test new code within the host environment
** -------------------------------------------------------------
**
**	Returns:
**		TRUE if test completed successfully.
**		FALSE otherwise.
*/

bool /*ARGSUSED*/
test(name, addr)
input char *name;			/* name to query about */
input ipaddr_t addr;			/* explicit address of query */
{
	return(TRUE);
}
