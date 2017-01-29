//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       irispstn.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports PSTN communication functions
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

//
// Local include files
//
#include "comms.h"
#include "utility.h"
#include "iris.h"
#include "iriscomms.h"
#include "irisfunc.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
#ifndef __VX670
	static T_COMMS comms;
	static int bufLen = 300;
	static int retVal = ERR_COMMS_NONE;
#endif


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// PSTN FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

void __pstn_init(void)
{
#ifndef __VX670
	comms.wHandle = 0xFFFF;
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_CONNECT
//
// DESCRIPTION:	Connect the PSTN / Modem
//
// PARAMETERS:	None
//
// RETURNS:		Sets the return value in retVal
//-------------------------------------------------------------------------------------------
//
void __pstn_connect(void)
{
#ifndef __VX670
	extern char model[];

	// Get the parameters off the iRIS stack
	char * bufSize = IRIS_StackGet(0);
	char * baud = IRIS_StackGet(1);
	char * interCharTimeout = IRIS_StackGet(2);
	char * timeout = IRIS_StackGet(3);
	char * phoneNo = IRIS_StackGet(4);
	char * pabx = IRIS_StackGet(5);
	char * fastConnect = IRIS_StackGet(6);
	char * blindDial = IRIS_StackGet(7);
	char * dialType = IRIS_StackGet(8);
	char * sync = IRIS_StackGet(9);
	char * preDial = IRIS_StackGet(10);

#ifndef _DEBUG
#if 0
	// TESTING / DEMO.....
	memset(&comms, 0, sizeof(comms));
	comms.wHandle = 0xFFFF;
	comms.eConnectionType = E_CONNECTION_TYPE_IP;
	comms.eHeader = E_HEADER_LENGTH;
	comms.bConnectionTimeout = comms.bResponseTimeout = 30;
	comms.dwInterCharTimeout = 3000;
	comms.ownIpAddress = "10.100.1.7";
	comms.ipAddress = "10.100.1.6";
	comms.wPortNumber = 4321;
	bufLen = 500;
	goto end;
	// TESTING ENDS
#endif
#endif

#ifndef _DEBUG
	// TCP disconnect
	if (strncmp(model, "VX570", 5))
		__tcp_disconnect_do();
#endif

	// If we are already connected, disconnect first
	if (comms.wHandle != 0xFFFF)
	{
		__tcp_disconnect_extend();
		Comms(E_COMMS_FUNC_DISCONNECT, &comms);
		comms.wHandle = 0xFFFF;
	}

	// Set the communication structure...
	memset(&comms, 0, sizeof(comms));
	comms.wHandle = 0xFFFF;
	comms.eConnectionType = E_CONNECTION_TYPE_PSTN;
	comms.eHeader = E_HEADER_NONE;

	// Set the connection timeout
	if (timeout && timeout[0])
		comms.bConnectionTimeout = comms.bResponseTimeout = atoi(timeout);
	else
		comms.bConnectionTimeout = comms.bResponseTimeout = 30;

	// Set the inter-character timeout
	if (interCharTimeout && interCharTimeout[0])
		comms.dwInterCharTimeout = atol(interCharTimeout);
	else
		comms.dwInterCharTimeout = 3000;

	// Set the baud rate
	if (baud)
	{
		if (strcmp(baud, "2400") == 0)
			comms.bBaudRate = COMMS_2400;
		else if (strcmp(baud, "4800") == 0)
			comms.bBaudRate = COMMS_4800;
		else if (strcmp(baud, "9600") == 0)
			comms.bBaudRate = COMMS_9600;
		else if (strcmp(baud, "19200") == 0)
			comms.bBaudRate = COMMS_19200;
		else if (strcmp(baud, "28800") == 0)
			comms.bBaudRate = COMMS_28800;
		else if (strcmp(baud, "57600") == 0)
			comms.bBaudRate = COMMS_57600;
		else if (strcmp(baud, "115200") == 0)
			comms.bBaudRate = COMMS_115200;
		else
			comms.bBaudRate = COMMS_1200;
	}
	else
		comms.bBaudRate = COMMS_1200;

	// Set the synchronous mode
	if (sync && sync[0] == '1')
		comms.fSync = true;

	// Set the dial type
	if (dialType && dialType[0] == '1')
		comms.tDialType = 'T';
	else
		comms.tDialType = 'P';

	// Set fast connect (quick dial)
	if (fastConnect && fastConnect[0] == '1')
		comms.fFastConnect = true;

	// Set blind dial
	if (blindDial && blindDial[0] == '1')
		comms.fBlindDial = true;

	// Set PABX
	comms.ptDialPrefix = pabx;

	// Set the phone number
	comms.ptPhoneNumber = phoneNo;

	// Set the predial option
	if (preDial && preDial[0] == '1')
		comms.fPreDial = true;

	// Pre-size the receiving buffer as requested by the application
	if (bufSize && bufSize[0])
		bufLen = atol(bufSize);
	if (bufLen < 300) bufLen = 300;

//end:
	// Initiate the connection
	__tcp_disconnect_extend();
	retVal = Comms(E_COMMS_FUNC_CONNECT, &comms);
#endif
	// Clear the stack and return the error description
	IRIS_StackPop(11);	// Only 11 becuase __pstn_err pops one out
	__pstn_err();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_SEND
//
// DESCRIPTION:	Sends the supplied buffer via PSTN / Modem
//
// PARAMETERS:	None
//
// RETURNS:		Sets the return value in retVal
//-------------------------------------------------------------------------------------------
//
void __pstn_send(void)
{
#ifdef __VX670
	__tcp_send();
#else
	__tcp_disconnect_extend();
	IRIS_CommsSend(&comms, &retVal);
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_RECV
//
// DESCRIPTION:	Receive data from the PSTN / Modem
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __pstn_recv(void)
{
#ifdef __VX670
	__tcp_recv();
#else
	__tcp_disconnect_extend();
	IRIS_CommsRecv(&comms, bufLen, &retVal);
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_DISCONNECT
//
// DESCRIPTION:	Disconnect the PSTN / Modem
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __pstn_disconnect(void)
{
/*		{
			char keycode;
			char tempBuf[80];
			sprintf(tempBuf, "Disconnecting PSTN %d", comms.wHandle);
			write_at(tempBuf, strlen(tempBuf), 1, 1);
			while(read(STDIN, &keycode, 1) != 1);
		}
*/
#ifdef __VX670
	__tcp_disconnect_ip_only();
#else
	if (comms.wHandle != 0xFFFF)
	{
		__tcp_disconnect_extend();
		IRIS_CommsDisconnect(&comms, retVal);
		comms.wHandle = 0xFFFF;
	}
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_ERR
//
// DESCRIPTION:	Returns a description of the current PSTN error
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void __pstn_err(void)
{
#ifdef __VX670
	__tcp_err();
#else
	IRIS_CommsErr(retVal);
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_CLR_ERR
//
// DESCRIPTION:	Clears the current PSTN error
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void __pstn_clr_err(void)
{
#ifdef __VX670
	__tcp_clr_err();
#else
	// Return the current error
	IRIS_CommsErr(retVal);

	// ...and then clear it
	retVal = ERR_COMMS_NONE;
#endif
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PSTN_WAIT
//
// DESCRIPTION:	Wait for the PSTN connection
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __pstn_wait(void)
{
#ifndef __VX670
	__tcp_disconnect_extend();
	retVal = Comms(E_COMMS_FUNC_PSTN_WAIT, &comms);
#endif
	__pstn_err();
}
