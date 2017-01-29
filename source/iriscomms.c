//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iriscomms.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports generic communication functions
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
#include "alloc.h"
#include "comms.h"
#include "utility.h"
#include "iris.h"
#include "iriscomms.h"

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

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// COMMS FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_CommsSend
//
// DESCRIPTION:	Sends the supplied buffer via the communication port
//
// PARAMETERS:	comms	<=	A communication structure
//				retVal	=>	The result is stored here
//
// RETURNS:		Sets the return value in retVal
//-------------------------------------------------------------------------------------------
//
void IRIS_CommsSend(T_COMMS * comms, int * retVal)
{
	char * data = IRIS_StackGet(0);

	// If not connected, return an error
	if (comms->wHandle == 0xFFFF)
		*retVal = ERR_COMMS_CONNECT_FAILURE;

	else if (data && strlen(data) >= 2)
	{
		// Get the buffer length in hex bytes
		comms->wLength = strlen(data) / 2;

		// Allocate and fill up the hex buffer
		comms->pbData = my_malloc(comms->wLength);
		UtilStringToHex(data, comms->wLength * 2, comms->pbData);

/*																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Sending %s  ", comms->eConnectionType == E_CONNECTION_TYPE_IP?"TCP":"other");
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																		}

*/		if ((*retVal = Comms(E_COMMS_FUNC_SEND, comms)) != ERR_COMMS_NONE)
			Comms(E_COMMS_FUNC_DISCONNECT, comms);

/*																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Sent                    ");
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																		}
*/
		// Release the transmit buffer as it is not needed now
		UtilStrDup((char **) &comms->pbData, NULL);
	}

	// Clear the stack and return the error description
	IRIS_StackPop(1);
	IRIS_CommsErr(*retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_CommsRecv
//
// DESCRIPTION:	Receive data from the communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void IRIS_CommsRecv(T_COMMS * comms, int bufLen, int * retVal)
{
	char * string;
	char * interCharTimeout = IRIS_StackGet(0);
	char * timeout = IRIS_StackGet(1);

	// If not connected, return an error
	if (comms->wHandle == 0xFFFF)
	{
		*retVal = ERR_COMMS_CONNECT_FAILURE;

		// Clear the stack and return
		IRIS_StackPop(3);
		IRIS_StackPush(NULL);
		return;
	}

	// Override the timeout if requested
	if (timeout && timeout[0])
		comms->bResponseTimeout = atoi(timeout);

	// Update the intercharacter timeout if requested
	if (interCharTimeout && interCharTimeout[0])
		comms->dwInterCharTimeout = atol(interCharTimeout);

	// Allocate the receive buffer and set the maximum receive length
	comms->pbData = my_malloc(bufLen);
	comms->wLength = bufLen;

//																		if (debug)
//																		{
//																			char tempBuf[40];
//																			sprintf(tempBuf, "Receiving %s  ", comms->eConnectionType == E_CONNECTION_TYPE_IP?"TCP":"other");
//																			write_at(tempBuf, strlen(tempBuf), 1, 2);
//																		}

	// Go ahead..receive...
	if ((*retVal = Comms(E_COMMS_FUNC_RECEIVE, comms)) != ERR_COMMS_NONE)
	{
		if (*retVal != ERR_COMMS_RECEIVE_TIMEOUT)
			Comms(E_COMMS_FUNC_DISCONNECT, comms);
	}

/*																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Received                  ");
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																		}
*/
	// Clear the stack
	IRIS_StackPop(3);

	// Return the received data or empty if error
	if (*retVal == ERR_COMMS_NONE && comms->wLength)
	{
		string = my_malloc(comms->wLength * 2 + 1);
		UtilHexToString(comms->pbData, comms->wLength, string);
		IRIS_StackPush(string);
		my_free(string);
	}
	else IRIS_StackPush(NULL);

	// Release the receive buffer as it is not needed now
	UtilStrDup((char **) &comms->pbData, NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_CommsDisconnect
//
// DESCRIPTION:	Disconnect the communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void IRIS_CommsDisconnect(T_COMMS * comms, int retVal)
{
/*																		{
																			char keycode;
																			char tempBuf[40];
																			sprintf(tempBuf, "Disconnecting %s  ", comms->eConnectionType == E_CONNECTION_TYPE_IP?"TCP":"other");
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																			while(read(STDIN, &keycode, 1) != 1);
																		}
*/	Comms(E_COMMS_FUNC_DISCONNECT, comms);

	// Clear the stack and return the current error number
	IRIS_CommsErr(retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : IRIS_CommsError
//
// DESCRIPTION:	Returns a description of the communication error
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void IRIS_CommsErr(int retVal)
{
	// Clear the stack
	IRIS_StackPop(1);

	// Push the status onto the stack
	IRIS_StackPush(CommsErrorDesc(retVal));
}
