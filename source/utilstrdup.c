/*
**-----------------------------------------------------------------------------
** PROJECT:			iRIS
**
** FILE NAME:       utility.c
**
** DATE CREATED:    30 January 2008
**
** AUTHOR:          Tareq Hafez
**
** DESCRIPTION:     This module has various utility functions used by other modules
**-----------------------------------------------------------------------------
*/

//
//-----------------------------------------------------------------------------
// Include Files
//-----------------------------------------------------------------------------
//

//
// Standard include files.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Project include files.
//
#include <auris.h>
#include <svc.h>

/*
** Local include files
*/
#include "alloc.h"
#include "utility.h"

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Module variable definitions and initialisations.
**-----------------------------------------------------------------------------
*/

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : UtilStrDup
**
** DESCRIPTION:	frees the existing pointer if it already exists, then performs a strdup()
**				to allocate and transfers to it the source string but only if the source string
**				exists
**
** PARAMETERS:	dest		=>	The new string to (re)allocate
**				source		<=	The string source that holds the string value
**
** RETURNS:		dest
**-------------------------------------------------------------------------------------------
*/
char * UtilStrDup(char ** dest, char * source)
{
	if (!dest) return NULL;

	if (*dest)
	{
		my_free(*dest);
		*dest = NULL;
	}

	if (source)
	{
		*dest = my_malloc(strlen(source) + 1);
		strcpy(*dest, source);
	}

	return *dest;
}
