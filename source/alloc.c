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

/*
** Local include files
*/
#include "alloc.h"

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

void * my_calloc(unsigned int size)
{
	if (size/4*4 != size)
		size = size/4*4 + 4;
	return calloc(1, size);
}

void * my_malloc(unsigned int size)
{
	if (size/4*4 != size)
		size = size/4*4 + 4;
	return malloc(size);
}

void * my_realloc(void * ptr, unsigned int size)
{
	if (size/4*4 != size)
		size = size/4*4 + 4;
	return realloc(ptr, size);
}

