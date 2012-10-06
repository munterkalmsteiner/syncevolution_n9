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
static char Version[] = "@(#)misc.c	e07@nikhef.nl (Eric Wassenaar) 991529";
#endif

#include "host.h"
#include "glob.h"

/*
** XALLOC -- Allocate or reallocate additional memory
** --------------------------------------------------
**
**	Returns:
**		Pointer to (re)allocated buffer space.
**		Aborts if the requested memory could not be obtained.
*/

ptr_t *
xalloc(buf, size)
register ptr_t *buf;			/* current start of buffer space */
input siz_t size;			/* number of bytes to allocate */
{
	if (buf == NULL)
		buf = malloc(size);
	else
		buf = realloc(buf, size);

	if (buf == NULL)
	{
		errmsg("Out of memory");
		exit(EX_OSERR);
	}

	return(buf);
}

/*
** DTOA -- Convert value to decimal integer ascii string
** -----------------------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
dtoa(n)
input int n;				/* value to convert */
{
	static char buf[30];		/* sufficient for 64-bit values */

	(void) sprintf(buf, "%d", n);
	return(buf);
}

/*
** UTOA -- Convert value to unsigned decimal ascii string
** ------------------------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
utoa(n)
input int n;				/* value to convert */
{
	static char buf[30];		/* sufficient for 64-bit values */

	(void) sprintf(buf, "%u", (unsigned)n);
	return(buf);
}

/*
** XTOA -- Convert value to hexadecimal ascii string
** -------------------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
xtoa(n)
input int n;				/* value to convert */
{
	static char buf[17];		/* sufficient for 64-bit values */

	(void) sprintf(buf, "%X", (unsigned)n);
	return(buf);
}

/*
** STOA -- Extract partial ascii string, escape if necessary
** ---------------------------------------------------------
**
**	Returns:
**		Pointer to string.
*/

char *
stoa(cp, size, escape)
input u_char *cp;			/* current position in answer buf */
input int size;				/* number of bytes to extract */
input bool escape;			/* escape special characters if set */
{
	static char buf[2*MAXDLEN+1];
	register char *p;
	register char c;
	register int i;

	if (size > MAXDLEN)
		size = MAXDLEN;

#ifdef obsolete
	if (size > 0)
		(void) sprintf(buf, "%.*s", size, (char *)cp);
	else
		(void) sprintf(buf, "%s", "");
#endif

	for (p = buf, i = 0; i < size; i++)
	{
		c = *cp++;
		if (escape && (c == '\n' || c == '\\' || c == '"'))
			*p++ = '\\';
		*p++ = c;
	}
	*p = '\0';

	return(buf);
}

/*
** BASE_NTOA -- Convert binary data to base64 ascii
** ------------------------------------------------
**
**	Returns:
**		Pointer to string.
**
**	This routine is used to convert encoded keys, signatures,
**	and certificates in T_KEY, T_SIG, and T_CERT resource records.
*/

char b64tab[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *
base_ntoa(cp, size)
input u_char *cp;			/* current position in answer buf */
input int size;				/* number of bytes to extract */
{
	static char buf[MAXB64SIZE+1];
	register char *p;
	int c1, c2, c3, c4;

	if (size > MAXMD5SIZE)
		size = MAXMD5SIZE;

	for (p = buf; size > 2; cp += 3, size -= 3)
	{
		c1 = (((int)cp[0] >> 2) & 0x3f);
		c2 = (((int)cp[0] & 0x03) << 4) + (((int)cp[1] >> 4) & 0x0f);
		c3 = (((int)cp[1] & 0x0f) << 2) + (((int)cp[2] >> 6) & 0x03);
		c4 =  ((int)cp[2] & 0x3f);

		*p++ = b64tab[c1];
		*p++ = b64tab[c2];
		*p++ = b64tab[c3];
		*p++ = b64tab[c4];
	}
    
	if (size == 2)
	{
		c1 = (((int)cp[0] >> 2) & 0x3f);
		c2 = (((int)cp[0] & 0x03) << 4) + (((int)cp[1] >> 4) & 0x0f);
		c3 = (((int)cp[1] & 0x0f) << 2);

		*p++ = b64tab[c1];
		*p++ = b64tab[c2];
		*p++ = b64tab[c3];
		*p++ = '=';
	}
	else if (size == 1)
	{
		c1 = (((int)cp[0] >> 2) & 0x3f);
		c2 = (((int)cp[0] & 0x03) << 4);

		*p++ = b64tab[c1];
		*p++ = b64tab[c2];
		*p++ = '=';
		*p++ = '=';
	}
	*p = '\0';

	return(buf);
}

/*
** NSAP_NTOA -- Convert binary nsap address to ascii
** -------------------------------------------------
**
**	Returns:
**		Pointer to string.
**
**	As per RFC 1637 an nsap address is encoded in binary form
**	in the resource record. It was unclear from RFC 1348 how
**	the encoding should be. RFC 1629 defines an upper bound
**	of 20 bytes to the size of a binary nsap address.
*/

char *
nsap_ntoa(cp, size)
input u_char *cp;			/* current position in answer buf */
input int size;				/* number of bytes to extract */
{
	static char buf[3*MAXNSAP+1];
	register char *p;
	register int c;
	register int i;

	if (size > MAXNSAP)
		size = MAXNSAP;

	for (p = buf, i = 0; i < size; i++, cp++)
	{
		c = ((int)(*cp) >> 4) & 0x0f;
		*p++ = hexdigit(c);
		c = ((int)(*cp) >> 0) & 0x0f;
		*p++ = hexdigit(c);

		/* add dots for readability */
		if ((i % 2) == 0 && (i + 1) < size)
			*p++ = '.';
	}
	*p = '\0';

	return(buf);
}

/*
** IPNG_NTOA -- Convert binary ip v6 address to ascii
** --------------------------------------------------
**
**	Returns:
**		Pointer to string.
**
**	As per RFC 1886 an ip v6 address is encoded in binary form
**	in the resource record. The size is fixed.
*/

char *
ipng_ntoa(cp)
input u_char *cp;			/* current position in answer buf */
{
	static char buf[5*(IPNGSIZE/2)+1];
	register char *p;
	register int n;
	register int i;

	for (p = buf, i = 0; i < IPNGSIZE/2; i++)
	{
		n = _getshort(cp);
		cp += INT16SZ;

		(void) sprintf(p, ":%X", n);
		p += strlength(p);
	}
	*p = '\0';

	return(buf + 1);
}

/*
** PR_DATE -- Produce printable version of a clock value
** -----------------------------------------------------
**
**	Returns:
**		Pointer to string.
**
**	The value is a standard absolute clock value.
*/

char *
pr_date(value)
input int value;			/* the clock value to be converted */
{
	static char buf[sizeof("YYYYMMDDHHMMSS")+1];
	time_t clocktime = value;
	struct tm *t;
	
	t = gmtime(&clocktime);
	t->tm_year += 1900;
	t->tm_mon += 1;

	(void) sprintf(buf, "%04d%02d%02d%02d%02d%02d",
		t->tm_year, t->tm_mon, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec);

	return(buf);
}

/*
** PR_TIME -- Produce printable version of a time interval
** -------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of interval.
**
**	The value is a time interval expressed in seconds.
*/

char *
pr_time(value, brief)
input int value;			/* the interval to be converted */
input bool brief;			/* use brief format if set */
{
	static char buf[256];
	register char *p = buf;
	int week, days, hour, mins, secs;

	/* special cases */
	if (value < 0)
		return("negative");
	if ((value == 0) && !brief)
		return("zero seconds");

/*
 * Decode the components.
 */
	secs = value % 60; value /= 60;
	mins = value % 60; value /= 60;
	hour = value % 24; value /= 24;
	days = value;

	if (!brief)
	{
		days = value % 7; value /= 7;
		week = value;
	}

/*
 * Now turn it into a sexy form.
 */
	if (brief)
	{
		if (days > 0)
		{
			(void) sprintf(p, "%d+", days);
			p += strlength(p);
		}

		(void) sprintf(p, "%02d:%02d:%02d", hour, mins, secs);
		return(buf);
	}

	if (week > 0)
	{
		(void) sprintf(p, ", %d week%s", week, plural(week));
		p += strlength(p);
	}

	if (days > 0)
	{
		(void) sprintf(p, ", %d day%s", days, plural(days));
		p += strlength(p);
	}

	if (hour > 0)
	{
		(void) sprintf(p, ", %d hour%s", hour, plural(hour));
		p += strlength(p);
	}

	if (mins > 0)
	{
		(void) sprintf(p, ", %d minute%s", mins, plural(mins));
		p += strlength(p);
	}

	if (secs > 0)
	{
		(void) sprintf(p, ", %d second%s", secs, plural(secs));
		/* p += strlength(p); */
	}

	return(buf + 2);
}

/*
** PR_SPHERICAL -- Produce printable version of a spherical location
** -----------------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of location.
**
**	The value is a spherical location (latitude or longitude)
**	expressed in thousandths of a second of arc.
**	The value 2^31 represents zero (equator or prime meridian).
*/

char *
pr_spherical(value, pos, neg)
input int value;			/* the location to be converted */
input char *pos;			/* suffix if value positive */
input char *neg;			/* suffix if value negative */
{
	static char buf[256];
	register char *p = buf;
	char *direction;
	int degrees, minutes, seconds, fracsec;

/*
 * Normalize.
 */
	value -= (int)((unsigned)1 << 31);

	direction = pos;
	if (value < 0)
	{
		direction = neg;
		value = -value;
	}

/*
 * Decode the components.
 */
	fracsec = value % 1000; value /= 1000;
	seconds = value % 60;   value /= 60;
	minutes = value % 60;   value /= 60;
	degrees = value;

/*
 * Construct output string.
 */
	(void) sprintf(p, "%d", degrees);
	p += strlength(p);

	if (minutes > 0 || seconds > 0 || fracsec > 0)
	{
		(void) sprintf(p, " %02d", minutes);
		p += strlength(p);
	}

	if (seconds > 0 || fracsec > 0)
	{
		(void) sprintf(p, " %02d", seconds);
		p += strlength(p);
	}

	if (fracsec > 0)
	{
		(void) sprintf(p, ".%03d", fracsec);
		p += strlength(p);
	}

	(void) sprintf(p, " %s", direction);

#ifdef obsolete
	(void) sprintf(buf, "%d %02d %02d.%03d %s",
		degrees, minutes, seconds, fracsec, direction);
#endif
	return(buf);
}

/*
** PR_VERTICAL -- Produce printable version of a vertical location
** ---------------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of location.
**
**	The value is an altitude expressed in centimeters, starting
**	from a base 100000 meters below the GPS reference spheroid.
**	This allows for the actual range [-10000000 .. 4293967296].
*/

char *
pr_vertical(value, pos, neg)
input int value;			/* the location to be converted */
input char *pos;			/* prefix if value positive */
input char *neg;			/* prefix if value negative */
{
	static char buf[256];
	register char *p = buf;
	char *direction;
	int meters, centimeters;
	unsigned int altitude;
	unsigned int reference;

/*
 * Normalize.
 */
	altitude = value;
	reference = 100000*100;

	if (altitude < reference)
	{
		direction = neg;
		altitude = reference - altitude;
	}
	else
	{
		direction = pos;
		altitude = altitude - reference;
	}

/*
 * Decode the components.
 */
	centimeters = altitude % 100; altitude /= 100;
	meters = altitude;

/*
 * Construct output string.
 */
	(void) sprintf(p, "%s%d", direction, meters);
	p += strlength(p);

	if (centimeters > 0)
		(void) sprintf(p, ".%02d", centimeters);

#ifdef obsolete
	(void) sprintf(buf, "%s%d.%02d", direction, meters, centimeters);
#endif
	return(buf);
}

/*
** PR_PRECISION -- Produce printable version of a location precision
** -----------------------------------------------------------------
**
**	Returns:
**		Pointer to a string version of precision.
**
**	The value is a precision expressed in centimeters, encoded
**	as 4-bit mantissa and 4-bit power of 10 (each ranging 0-9).
*/

unsigned int poweroften[10] =
{1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};

char *
pr_precision(value)
input int value;			/* the precision to be converted */
{
	static char buf[256];
	register char *p = buf;
	int meters, centimeters;
	unsigned int precision;
	register int mantissa;
	register int exponent;

/*
 * Normalize.
 */
	mantissa = ((value >> 4) & 0x0f) % 10;
	exponent = ((value >> 0) & 0x0f) % 10;
	precision = mantissa * poweroften[exponent];

/*
 * Decode the components.
 */
	centimeters = precision % 100; precision /= 100;
	meters = precision;

/*
 * Construct output string.
 */
	(void) sprintf(p, "%d", meters);
	p += strlength(p);

	if (centimeters > 0)
		(void) sprintf(p, ".%02d", centimeters);

#ifdef obsolete
	(void) sprintf(buf, "%d.%02d", meters, centimeters);
#endif
	return(buf);
}

/*
** CONVTIME -- Decode a time period from input string
** --------------------------------------------------
**
**	Returns:
**		Non-negative numeric value of time period (in seconds).
**		-1 in case of invalid time specification.
**
**	Only rudimentary syntax errors are checked.
**	It is easy to fool this routine to yield bizarre results.
**	If the result is negative, it should not be trusted.
*/

int
convtime(string, defunits)
input char *string;			/* time specification in ascii */
input char defunits;			/* default units if none specified */
{
	int period = 0;			/* overall result value */
	register int value;		/* intermediate value of component */
	register char units;
	register char *p = string;

	while (*p != '\0')
	{
		/* must start with numeric value */
		if (!is_digit(*p))
			return(-1);

		/* assemble numeric value */
		for (value = 0; is_digit(*p); p++)
			value = (value * 10) + (*p - '0');

		/* fetch units -- use default when omitted */
		if (*p != '\0')
			units = *p++;
		else
			units = defunits;

		switch (lowercase(units))
		{
		    case 'w':		/* weeks */
			value *= 7;
			/*FALLTHROUGH*/

		    case 'd':		/* days */
			value *= 24;
			/*FALLTHROUGH*/

		    case 'h':		/* hours */
			value *= 60;
			/*FALLTHROUGH*/

		    case 'm':		/* minutes */
			value *= 60;
			/*FALLTHROUGH*/

		    case 's':		/* seconds */
			break;

		    default:		/* unknown */
			value = -1;
			break;
		}

		/* must be reasonable */
		if (value < 0)
			return(-1);

		/* accumulate total */
		period += value;
	}

	return(period);
}
