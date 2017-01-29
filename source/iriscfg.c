//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iristime.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports security functions
//-----------------------------------------------------------------------------
//

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
#include <svc_sec.h>

//
// Local include files
//
#include "iris.h"
#include "irisfunc.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//
#define	C_PPID_LEN			17

#define	C_PPID				0


#define	C_CFG_FILE_LEN		(C_PPID_LEN)

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
static const char * fileName = "s2.dat";
static FILE_HANDLE handle = (FILE_HANDLE) -1;

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// iRIS CONFIGURATION PARAMETER FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////



//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SEC_INIT
//
// DESCRIPTION:	Initialise the security key by loading a KLK to allow for future upgrades
//
// PARAMETERS:	None
//
// RETURNS:		PPID as a string
//-------------------------------------------------------------------------------------------
//
void __sec_init(void)
{
#ifndef _DEBUG

	iPS_LoadSysClearKey(0, "\x80\x40\x80\x40\x80\x40\x80\x40\x80\x40\x80\x40\x80\x40\x80\x40");

	// No need to pop the function name and push NULL, just keep the function name in the stack.
#endif
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : irisCfgInit
**
** DESCRIPTION:	Initialise the configuration parameter data
**
** PARAMETERS:	None
**
** RETURNS:		None
**-------------------------------------------------------------------------------------------
*/
static void irisCfgInit(void)
{
	int i;

	// Initialise if file does not exist
	handle = open(fileName, FH_RDWR);
	if (FH_ERR(handle))
	{
		handle = open(fileName, FH_NEW_RDWR);

		for (i = 0; i < C_CFG_FILE_LEN; i++)
			write(handle, "", 1);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PPID
//
// DESCRIPTION:	Returns the current ppid value
//
// PARAMETERS:	None
//
// RETURNS:		PPID as a string
//-------------------------------------------------------------------------------------------
//
void __ppid(void)
{
	char ppid[C_PPID_LEN];

	// Initialise if required
	if (handle == (FILE_HANDLE) -1)
		irisCfgInit();

	// Read the PPID
	lseek(handle, C_PPID, SEEK_SET);
	read(handle, ppid, C_PPID_LEN);

	IRIS_StackPop(1);

	IRIS_StackPush(strlen(ppid)?ppid:NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PPID_UPDATE
//
// DESCRIPTION:	Updates the current ppid value
//
// PARAMETERS:	None
//
// RETURNS:		PPID as a string
//-------------------------------------------------------------------------------------------
//
void __ppid_update(void)
{
	char * ppid = IRIS_StackGet(0);

	// Initialise if required
	if (handle == (FILE_HANDLE) -1)
		irisCfgInit();

	// Write the PPID but only by iRIS...Other applications can't change this
	if (strcmp(currentObjectGroup, irisGroup) == 0)
	{
		lseek(handle, C_PPID, SEEK_SET);
		write(handle, ppid, strlen(ppid)+1);
#ifdef _DEBUG
		fflush(handle);
#endif
	}

	IRIS_StackPop(2);

	IRIS_StackPush(ppid);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PPID_REMOVE
//
// DESCRIPTION:	Removes the current ppid value
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __ppid_remove(void)
{
	// Initialise if required
	if (handle == (FILE_HANDLE) -1)
		irisCfgInit();

	// Write the PPID but only by iRIS...Other applications can't change this
	if (strcmp(currentObjectGroup, irisGroup))
	{
		lseek(handle, C_PPID, SEEK_SET);
		write(handle, "", 1);
#ifdef _DEBUG
		fflush(handle);
#endif
	}

	IRIS_StackPop(1);

	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SERIAL_NO
//
// DESCRIPTION:	Returns the serial number of the device
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __serial_no(void)
{
	int i, j;
	char serialNumber[12];
	char serialNumber2[12];

	memset(serialNumber, 0, sizeof(serialNumber));
	memset(serialNumber2, 0, sizeof(serialNumber2));

	SVC_INFO_SERLNO(serialNumber);

	for (i = 0, j = 0; i < (int) strlen(serialNumber); i++)
	{
		if (serialNumber[i] >= '0' && serialNumber[i] <= '9')
			serialNumber2[j++] = serialNumber[i];
	}

	IRIS_StackPop(1);
	IRIS_StackPush(serialNumber2);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()MANUFACTURER
//
// DESCRIPTION:	Returns the manufacturer name
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __manufacturer(void)
{
	IRIS_StackPop(1);

	IRIS_StackPush("VERIFONE");
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()MODEL
//
// DESCRIPTION:	Returns the model name
//
// PARAMETERS:	Name
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __model(void)
{
	char model[13];
	int i = sizeof(model) - 1;

	IRIS_StackPop(1);

	SVC_INFO_MODELNO(model);

	while (model[i-1] == ' ') i--;
	model[i] = '\0';

	IRIS_StackPush(model);
}

