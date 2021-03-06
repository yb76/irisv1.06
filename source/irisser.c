//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       irisser.c
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
#include <stdlib.h>
#include <string.h>

//
// Project include files.
//
#include <auris.h>

//
// Local include files
//
#include "comms.h"
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
static T_COMMS comms[2];
static int retVal = ERR_COMMS_NONE;
static int bufLen = 300;


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// SER FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

void __ser_init(void)
{
	comms[0].wHandle = 0xFFFF;
	comms[1].wHandle = 0xFFFF;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_CONNECT
//
// DESCRIPTION:	Connect the serial line
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
static void _ser_param(T_COMMS * myComms, char * baud, char * dataBits, char * parity, char * stopBits)
{
	// Set the baud rate
	if (baud)
	{
		if (strcmp(baud, "300") == 0)
			myComms->bBaudRate = COMMS_300;
		else if (strcmp(baud, "600") == 0)
			myComms->bBaudRate = COMMS_600;
		else if (strcmp(baud, "1200") == 0)
			myComms->bBaudRate = COMMS_1200;
		else if (strcmp(baud, "2400") == 0)
			myComms->bBaudRate = COMMS_2400;
		else if (strcmp(baud, "4800") == 0)
			myComms->bBaudRate = COMMS_4800;
		else if (strcmp(baud, "9600") == 0)
			myComms->bBaudRate = COMMS_9600;
		else if (strcmp(baud, "19200") == 0)
			myComms->bBaudRate = COMMS_19200;
		else if (strcmp(baud, "28800") == 0)
			myComms->bBaudRate = COMMS_28800;
		else if (strcmp(baud, "38400") == 0)
			myComms->bBaudRate = COMMS_38400;
		else if (strcmp(baud, "57600") == 0)
			myComms->bBaudRate = COMMS_57600;
		else if (strcmp(baud, "115200") == 0)
			myComms->bBaudRate = COMMS_115200;
	}
	else
		myComms->bBaudRate = COMMS_1200;

	// Set the data bits
	if (dataBits && dataBits[0])
		myComms->bDataBits = atoi(dataBits);
	else 
		myComms->bDataBits = 8;

	// Set the parity bit
	if (parity && parity[0])
		myComms->tParityBit = parity[0];
	else 
		myComms->tParityBit = 'N';

	// Set the stop bits
	if (stopBits && stopBits[0])
		myComms->bStopBits = atoi(stopBits);
	else 
		myComms->bStopBits = 1;
}

void __ser_connect(void)
{
	// Get the parameters off the iRIS stack
	char * bufSize = IRIS_StackGet(0);
	char * stopBits = IRIS_StackGet(1);
	char * parity = IRIS_StackGet(2);
	char * dataBits = IRIS_StackGet(3);
	char * baud = IRIS_StackGet(4);
	char * interCharTimeout = IRIS_StackGet(5);
	char * timeout = IRIS_StackGet(6);
	char * port = IRIS_StackGet(7);
	char * header = IRIS_StackGet(8);
	T_COMMS * myComms = &comms[0];
	int myPort = 0;

	// Get the port number
	if (port && port[0])
	{
		myPort = atoi(port);
		myComms = &comms[myPort];
	}

	// If we are already connected, disconnect first
	if (myComms->wHandle != 0xFFFF)
	{
		Comms(E_COMMS_FUNC_DISCONNECT, myComms);
		myComms->wHandle = 0xFFFF;
	}

	// Set the communication structure...
	memset(myComms, 0, sizeof(T_COMMS));
	myComms->wHandle = 0xFFFF;
	myComms->eConnectionType = myPort == 0?E_CONNECTION_TYPE_UART_1:E_CONNECTION_TYPE_UART_2;

	// Set the connection timeout
	if (timeout && timeout[0])
		myComms->bConnectionTimeout = myComms->bResponseTimeout = atoi(timeout);
	else
		myComms->bConnectionTimeout = myComms->bResponseTimeout = 15;

	// Set the inter-character timeout
	if (interCharTimeout && interCharTimeout[0])
		myComms->dwInterCharTimeout = atol(interCharTimeout);
	else
		myComms->dwInterCharTimeout = 3000;

	// Set the serial port parameters
	_ser_param(myComms, baud, dataBits, parity, stopBits);

	// Pre-size the receiving buffer as requested by the application
	if (bufSize && bufSize[0])
		bufLen = atol(bufSize);
	if (bufLen < 300) bufLen = 300;

	// Set the header if set
	if (header && header[0] >= '1' && header[0] <= '5')
		myComms->eHeader = header[0] - '0';

	// Initiate the connection
	retVal = Comms(E_COMMS_FUNC_CONNECT, myComms);

	// Return with the current result
	IRIS_StackPop(8);		// Only 8 becuase __ser_err pops one out
	if (header && header[0] >= '1' && header[0] <= '5')
		IRIS_StackPop(1);

	__ser_err();
}

int __ser_reconnect(int myPort)
{
	T_COMMS * myComms = &comms[myPort];
	
	// If we are already connected, disconnect first
	if (myComms->wHandle != 0xFFFF)
	{
		Comms(E_COMMS_FUNC_DISCONNECT, myComms);
		myComms->wHandle = 0xFFFF;
	}

	// Initiate the connection
	retVal = Comms(E_COMMS_FUNC_CONNECT, myComms);

	// Return with the current result
	return retVal;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_SET
//
// DESCRIPTION:	Set the serial line
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __ser_set(void)
{
	char * stopBits = IRIS_StackGet(0);
	char * parity = IRIS_StackGet(1);
	char * dataBits = IRIS_StackGet(2);
	char * baud = IRIS_StackGet(3);
	char * port = IRIS_StackGet(4);
	T_COMMS * myComms = &comms[0];

	// Get the port number
	if (port && port[0])
		myComms = &comms[atoi(port)];

	// Set the serial port parameters
	_ser_param(myComms, baud, dataBits, parity, stopBits);

	// Change the setting
	retVal = Comms(E_COMMS_FUNC_SET_SERIAL, myComms);

	// Return with the current result
	IRIS_StackPop(5);
	__ser_err();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_SEND
//
// DESCRIPTION:	Send the supplied buffer via the serial port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __ser_send(void)
{
	char * port = IRIS_StackGet(0);
	T_COMMS * myComms = &comms[0];

	// Get the port number
	if (port && port[0])
		myComms = &comms[atoi(port)];
	IRIS_StackPop(1);

	IRIS_CommsSend(myComms, &retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_RECV
//
// DESCRIPTION:	Receive data into  the allocated buffer from the serial port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __ser_recv(void)
{
	char * port = IRIS_StackGet(0);
	T_COMMS * myComms = &comms[0];

	// Get the port number
	if (port && port[0])
		myComms = &comms[atoi(port)];
	IRIS_StackPop(1);

	IRIS_CommsRecv(myComms, bufLen, &retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_DATA
//
// DESCRIPTION:	Checks if serial data is available to be read
//
// PARAMETERS:	None
//
// RETURNS:		"1" if yes, NULL if not
//-------------------------------------------------------------------------------------------
//
void __ser_data(void)
{
	char * port = IRIS_StackGet(0);
	T_COMMS * myComms = &comms[0];

	int myPort = 0;

	// Get the port number
	if (port && port[0])
	{
		myPort = atoi(port);
		myComms = &comms[myPort];
	}

	// Lose the port number and function name
	IRIS_StackPop(2);

	if (myComms->wHandle != 0xFFFF && Comms(myPort == 0?E_COMMS_FUNC_SERIAL_DATA_AVAILABLE:E_COMMS_FUNC_SERIAL2_DATA_AVAILABLE, myComms) == 1)
		IRIS_StackPush("TRUE");
	else
		IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_DISCONNECT
//
// DESCRIPTION:	Disconnect the serial line
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __ser_disconnect()
{
	char * port = IRIS_StackGet(0);
	T_COMMS * myComms = &comms[0];

	// Get the port number
	if (port && port[0])
		myComms = &comms[atoi(port)];
	IRIS_StackPop(1);

	if (myComms->wHandle != 0xFFFF)
	{
		IRIS_CommsDisconnect(myComms, retVal);
		myComms->wHandle = 0xFFFF;
	}
}

int ____ser_disconnect(int myPort)
{
	T_COMMS * myComms = &comms[myPort];

	if (myComms->wHandle != 0xFFFF)
	{
		Comms(E_COMMS_FUNC_DISCONNECT, myComms);
		myComms->wHandle = 0xFFFF;
		return 1;
	}

	return 0;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()SER_ERR
//
// DESCRIPTION:	Returns a desciption of the current serial line error
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __ser_err()
{
	IRIS_CommsErr(retVal);
}

