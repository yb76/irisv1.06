//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iristcp.c
//
// DATE CREATED:    5 March 2008
//
// AUTHOR:          Tareq Hafez
//
// DESCRIPTION:     This module supports TCP/IP communication functions
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
#include "my_time.h"
#include "comms.h"
#include "display.h"
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
static T_COMMS comms;
static int retVal;
static int bufLen = 300;

static time_t myTime;
static char currOwnIPAddress[50];
static char currGateway[50];
static char currPDNS[50];
static char currSDNS[50];
static char currIPAddress[50];
static unsigned int currPortNumber;
static unsigned int currHandle;

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// TCP FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void __tcp_disconnect_do(void);


void __tcp_init(void)
{
	currHandle = 0xFFFF;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()IP_CONNECT
//
// DESCRIPTION:	Establish an IP connection
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __ip_connect(void)
{
	// Get the parameters off the iRIS stack
	char * timeout = IRIS_StackGet(0);
	char * sdns = IRIS_StackGet(1);
	char * pdns = IRIS_StackGet(2);
	char * gw = IRIS_StackGet(3);
	char * oip = IRIS_StackGet(4);	// This is also "APN" for GPRS connections.
	bool change = false;

	// Set the communication structure...
	memset(&comms, 0, sizeof(comms));
	comms.wHandle = 0xFFFF;
	comms.eConnectionType = E_CONNECTION_TYPE_IP_SETUP;

	// Set the connection timeout
	if (timeout && timeout[0])
		comms.bConnectionTimeout = comms.bResponseTimeout = atoi(timeout);
	else
		comms.bConnectionTimeout = comms.bResponseTimeout = 30;

	// Set the IP Address
	comms.ownIpAddress = oip?oip:"";
	if (strcmp(comms.ownIpAddress, currOwnIPAddress)) change = true;
	comms.gateway = gw?gw:"";
	if (strcmp(comms.gateway, currGateway)) change = true;
	comms.pdns = pdns?pdns:"";
	if (strcmp(comms.pdns, currPDNS)) change = true;
	comms.sdns = sdns?sdns:"";
	if (strcmp(comms.sdns, currSDNS)) change = true;

	// If already connected...
	if (change)
	{
		if (currHandle != 0xFFFF)
		{
			// If the parameters have changed, disconnect first then connect
			comms.wHandle = currHandle;
			__tcp_disconnect_do();
		}
#ifdef __VX670
		else if (currOwnIPAddress[0] && strcmp(comms.ownIpAddress, currOwnIPAddress))
			__tcp_disconnect_do();
#endif
		retVal = Comms(E_COMMS_FUNC_CONNECT, &comms);
	}

//	if (retVal == ERR_COMMS_NONE)
	{
		strcpy(currOwnIPAddress, comms.ownIpAddress);
		strcpy(currGateway, comms.gateway);
		strcpy(currPDNS, comms.pdns);
		strcpy(currSDNS, comms.sdns);
	}

	if (retVal == ERR_COMMS_NONE)
		currHandle = comms.wHandle;

	// Clear the stack and return the error description
	IRIS_StackPop(5);	// Only 5 because IRIS_CommsErr pops one out
	IRIS_CommsErr(retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_CONNECT
//
// DESCRIPTION:	Connect the PSTN / Modem
//
// PARAMETERS:	None
//
// RETURNS:		Sets the return value in retVal
//-------------------------------------------------------------------------------------------
//
void __tcp_connect(void)
{
	// Get the parameters off the iRIS stack
	char * bufSize = IRIS_StackGet(0);
	char * interCharTimeout = IRIS_StackGet(1);
	char * timeout = IRIS_StackGet(2);
	char * port = IRIS_StackGet(3);
	char * hip = IRIS_StackGet(4);
	char * sdns = IRIS_StackGet(5);
	char * pdns = IRIS_StackGet(6);
	char * gw = IRIS_StackGet(7);	// This is also the progress display for GPRS connections
	char * oip = IRIS_StackGet(8);	// This is also "APN" for GPRS connections
	char * header = IRIS_StackGet(9);
	bool change = false;

	// Set the communication structure...
	memset(&comms, 0, sizeof(comms));
	comms.wHandle = 0xFFFF;
	comms.eConnectionType = E_CONNECTION_TYPE_IP;
	if (header && header[0] >= '1' && header[0] <= '5')
		comms.eHeader = header[0] - '0';

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

	// Set own IP Address or leave as is
	if (oip && oip[0] == '\0')
		comms.ownIpAddress = currOwnIPAddress;
	else
	{
		comms.ownIpAddress = oip?oip:"";
		if (strcmp(comms.ownIpAddress, currOwnIPAddress)) change = true;
	}

	// Set gateway or leave as is
	if (gw && gw[0] == '\0')
		comms.gateway = currGateway;
	else
	{
		comms.gateway = gw?gw:"";
		if (strcmp(comms.gateway, currGateway)) change = true;
	}

	// Set up primary DNS or leave as is
	if (pdns && pdns[0] == '\0')
		comms.pdns = currPDNS;
	else
	{
		comms.pdns = pdns?pdns:"";
		if (strcmp(comms.pdns, currPDNS)) change = true;
	}

	// Set up secondary DNS or leave as is
	if (sdns && sdns[0] == '\0')
		comms.sdns = currSDNS;
	else
	{
		comms.sdns = sdns?sdns:"";
		if (strcmp(comms.sdns, currSDNS)) change = true;
	}

	// Set up the host IP address
	comms.ipAddress = hip?hip:"";
	if (strcmp(comms.ipAddress, currIPAddress)) change = true;

	// Set the tcp port number
	if (port)
	{
		comms.wPortNumber = atoi(port);
		if (comms.wPortNumber != currPortNumber) change = true;
	}

	// Set the buffer size
	if (bufSize && bufSize[0])
		bufLen = atol(bufSize);
	if (bufLen < 300) bufLen = 300;

	// If already connected...
	if (currHandle != 0xFFFF)
	{
		// If the parameters have changed, disconnect first then connect
		if (change || (retVal != ERR_COMMS_NONE && retVal != ERR_COMMS_RECEIVE_TIMEOUT))
		{
			comms.wHandle = currHandle;
			// No need to disconnect if already in error since we have already disconnected
			if (retVal == ERR_COMMS_NONE || retVal == ERR_COMMS_RECEIVE_TIMEOUT)
				__tcp_disconnect_do();
			retVal = Comms(E_COMMS_FUNC_CONNECT, &comms);
		}

		// If no change, report all is connected OK.
		else
		{
			retVal = ERR_COMMS_NONE;
			comms.wHandle = currHandle;
		}
	}
	// Initiate the connection
	else
	{
#ifdef __VX670
		if (currOwnIPAddress[0] && strcmp(comms.ownIpAddress, currOwnIPAddress))
			__tcp_disconnect_do();
#endif
		retVal = Comms(E_COMMS_FUNC_CONNECT, &comms);
//		retVal = ERR_COMMS_RECEIVE_TIMEOUT;
	}

	// Arm the timer
	if (retVal == ERR_COMMS_NONE)
	{
		myTime = my_time(NULL) + 60;
		strcpy(currOwnIPAddress, comms.ownIpAddress);
		strcpy(currGateway, comms.gateway);
		strcpy(currPDNS, comms.pdns);
		strcpy(currSDNS, comms.sdns);
		strcpy(currIPAddress, comms.ipAddress);
		currPortNumber = comms.wPortNumber;
		currHandle = comms.wHandle;
	}

	// Clear the stack and return the error description
	IRIS_StackPop(10);	// Only 10 because __tcp_err pops one out
	__tcp_err();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_SEND
//
// DESCRIPTION:	Sends the supplied buffer via TCP
//
// PARAMETERS:	None
//
// RETURNS:		Sets the return value in retVal
//-------------------------------------------------------------------------------------------
//
void __tcp_send(void)
{
	__tcp_disconnect_extend();
	IRIS_CommsSend(&comms, &retVal);

	if (retVal != ERR_COMMS_NONE)
		__tcp_disconnect_completely();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_RECV
//
// DESCRIPTION:	Receive data from the PSTN / Modem
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __tcp_recv(void)
{
	__tcp_disconnect_extend();
	IRIS_CommsRecv(&comms, bufLen, &retVal);

	if (retVal != ERR_COMMS_NONE && retVal != ERR_COMMS_RECEIVE_TIMEOUT)
		__tcp_disconnect_completely();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_DISCONNECT
//
// DESCRIPTION:	Disconnect the TCP/IP communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect(void)
{
	// If there is an error, disconnect and house keep
//#ifdef __VMAC
//	if ((retVal != ERR_COMMS_NONE && retVal != ERR_COMMS_RECEIVE_TIMEOUT) || strncmp(model, "VX570", 5))
//#else
	if (retVal != ERR_COMMS_NONE && retVal != ERR_COMMS_RECEIVE_TIMEOUT)
//#endif
		__tcp_disconnect_do();

	// Delay the connection for one minute
	else myTime = my_time(NULL) + 60;

#ifdef __VX670
	comms.fFastConnect = true;
#endif

	IRIS_CommsErr(retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_DISCONNECT_NOW
//
// DESCRIPTION:	Disconnect the TCP/IP communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect_now(void)
{
	__tcp_disconnect_do();

	IRIS_CommsErr(retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : __tcp_disconnect_do
//
// DESCRIPTION:	Actually disconnect the TCP/IP communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect_do(void)
{
	Comms(E_COMMS_FUNC_DISCONNECT, &comms);
	myTime = 0xFFFFFFFFUL;

/*	currPortNumber = 0;
	if (comms.fFastConnect == false)
		memset(currOwnIPAddress, 0, sizeof(currOwnIPAddress));
	memset(currIPAddress, 0, sizeof(currIPAddress));
	memset(currGateway, 0, sizeof(currGateway));
	memset(currPDNS, 0, sizeof(currPDNS));
	memset(currSDNS, 0, sizeof(currSDNS));
*/
	currHandle = comms.wHandle = 0xFFFF;
}

#ifdef __VX670
//
//-------------------------------------------------------------------------------------------
// FUNCTION   : __tcp_disconnect_ip_only
//
// DESCRIPTION:	Actually disconnect the TCP/IP communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect_ip_only(void)
{
	// Indicate that we need a faster connection next time.... This will keep the GPRS connection alive but shut down the IP connection.
	comms.fFastConnect = true;

	__tcp_disconnect_do();
}
#endif

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : __tcp_disconnect_ip_only
//
// DESCRIPTION:	Actually disconnect the TCP/IP communication port
//
// PARAMETERS:	None
//
// RETURNS:		The current time in strunc tm format
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect_completely(void)
{
	// Indicate that we want to netdisconnect as well....This is for VMAC deactivation purposes....
	comms.fSync = true;

	__tcp_disconnect_do();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : __tcp_disconnect_check
//
// DESCRIPTION:	Check if we should actually disconnect the TCP/IP communication port
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect_check(void)
{
	if (my_time(NULL) > myTime && currHandle != 0xFFFF)
		__tcp_disconnect_do();
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : __tcp_disconnect_extend
//
// DESCRIPTION:	Extends the disconnection timer. Used when performing PSTN sessions in the middle
//				of a TCP session.
//
// PARAMETERS:	None
//
// RETURNS:		None
//-------------------------------------------------------------------------------------------
//
void __tcp_disconnect_extend(void)
{
	if (currHandle != 0xFFFF)
		myTime = my_time(NULL) + 60;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_ERR
//
// DESCRIPTION:	Returns a description of the current TCP error
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void __tcp_err(void)
{
	IRIS_CommsErr(retVal);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_CLR_ERR
//
// DESCRIPTION:	Clears the current TCP error
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void __tcp_clr_err(void)
{
	// Return the current error
	IRIS_CommsErr(retVal);

	// ...and then clear it
	retVal = ERR_COMMS_NONE;
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()TCP_GPRS_STS
//
// DESCRIPTION:	Returns a description of the current TCP error
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void __tcp_gprs_sts(void)
{
	Comms(E_COMMS_FUNC_DISP_GPRS_STS, &comms);

	IRIS_StackPop(1);
	IRIS_StackPush(NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PING
//
// DESCRIPTION:	Performs a ping
//
// PARAMETERS:	None
//
// RETURNS:		Error description
//-------------------------------------------------------------------------------------------
//
void __ping(void)
{
	int i;
	T_COMMS temp;
	char * host = IRIS_StackGet(0);
	char result[30];

	temp.ipAddress = IRIS_StackGet(0);
	i = Comms(E_COMMS_FUNC_PING, &temp);

	if (i >= 0)
		sprintf(result, "OK (%d msec)", i);
	else
		strcpy(result, "FAILED");

	IRIS_StackPop(2);
	IRIS_StackPush(result);
}
