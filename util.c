/* vi: ts=4 sw=4
 * 
 * Hifs -- Handy Information For Sysadmins
 * Copyright (C) 1996,1997 Geert Jansen 
 */

#include "hifs.h"

/* ------------------------------------------------------------------------
 * xmalloc: Allocate memory and exit if not successfull */

void * xmalloc( size_t size)
{
	void * p;

	if (!(p = malloc( size))) {
		perror( "malloc()");
		exit (1);
	}
	return (p);
}

/* ------------------------------------------------------------------------
 * xrealloc: Reallocae memory and exit if not successfull. */

void * xrealloc( void * ptr, size_t size)
{
	void * p;

	if (!(p = realloc( ptr, size))) {
		perror( "malloc()");
		exit (1);
	}
	return (p);
}

/* ------------------------------------------------------------------------
 * strnzcpy: Zero teminate strncpy. */

char * strnzcpy( char * dest, const char * src, size_t size)
{
	strncpy( dest, src, size);
	dest[size-1] = '\000';
	return (dest);
}
	
