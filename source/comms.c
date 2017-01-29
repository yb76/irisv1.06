/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       comms.c
**
** DATE CREATED:    10 July 2007
**
** AUTHOR:          Tareq Hafez
**
** DESCRIPTION:     This module contains the communication functions
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** Include Files
**-----------------------------------------------------------------------------
*/

#ifdef __VMAC
#include "logsys.h"
#endif

/*
** Standard include files.
*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

/*
** KEF Project include files.
*/
#include <svc.h>
#include <uclfactory.h>
#include <verixtimer.h>
#include <TCPInterface.h>
#include <SSLInit.h>
#include <netsetup.h>
#include <vsocket.h>
#include <inet.h>
#include <hostdata.h>
#include <coerrors.h>
#include <ppp.h>
#include <errno.h>


/*
** Local include files
*/
#include "auris.h"
#include "alloc.h"
#include "display.h"
#include "timer.h"
#include "comms.h"

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/
typedef struct
{
	int fromError;
	uint wToError;
} T_ERROR_TABLE;

const T_ERROR_TABLE sErrorTable[] = 
{	{E_UCL_BUSY, ERR_COMMS_ENGAGED_TONE},
	{E_UCL_NO_ANSWER, ERR_COMMS_NO_ANSWER},
	{E_UCL_NO_DIAL_TONE, ERR_COMMS_NO_DIAL_TONE},
	{E_UCL_CTS_LOW, ERR_COMMS_CTS_LOW},
	{E_UCL_NO_CARRIER, ERR_COMMS_CARRIER_LOST},
	{E_UCL_WRITE_FAILED, ERR_COMMS_SENDING_ERROR},
	{E_UCL_NOT_OPEN, ERR_COMMS_PORT_NOT_OPEN},
	{E_UCL_NOT_CONNECTED, ERR_COMMS_NOT_CONNECTED},
	{E_UCL_TIMEOUT, ERR_COMMS_TIMEOUT},
	{E_UCL_INTERCHAR_TIMEOUT, ERR_COMMS_INTERCHAR_TIMEOUT},
	{E_UCL_NO_LINE, ERR_COMMS_NO_LINE},
	{E_UCL_CANCEL, ERR_COMMS_CANCEL},
	{E_UCL_NOT_DIALED, ERR_COMMS_NOT_DIALED},
	{E_UCL_INVALID_PARM, ERR_COMMS_INVALID_PARM},
	{E_UCL_FEATURE_NOT_SUPPORTED, ERR_COMMS_FEATURE_NOT_SUPPORTED},
	{E_UCL_PORT_OCCUPIED, ERR_COMMS_PORT_USED},

#ifdef __VX670
	{E_UCL_ERROR, ERR_COMMS_SIM},
#else
	{E_UCL_ERROR, ERR_COMMS_ERROR},
#endif

	{E_UCL_NOSIM_INVALIDPINPLMN, ERR_COMMS_SIM},
	{E_UCL_NO_SIGNAL, ERR_COMMS_NO_SIGNAL},
	{E_UCL_LOW_SIGNAL, ERR_COMMS_LOW_SIGNAL},
	{E_UCL_LOW_BATTERY, ERR_COMMS_LOW_BATTERY},
	{E_UCL_NO_COVERAGE, ERR_COMMS_NO_COVERAGE},
	{E_UCL_NO_PDPCONTEXT, ERR_COMMS_NO_PDPCONTEXT},
	{E_UCL_NW_LOST, ERR_COMMS_NETWORK_LOST},
	{E_UCL_NW_DIE, ERR_COMMS_NETWORK_LOST},

	{ELOGONFAILURE, ERR_COMMS_PPP_PAP_CHAP_FAILED},
	{ELCPFAILURE, ERR_COMMS_PPP_LCP_FAILED},
	{EIPCPFAILURE, ERR_COMMS_ETH_IPCP_FAILED},
	{ECONNABORTED, ERR_COMMS_ETH_CONNECTION_ABORTED},
	{EPIPE, ERR_COMMS_ETH_TCP_SHUTDOWN},
	{ENOTCONN, ERR_COMMS_ETH_NOT_CONNECTED},
	{ETIMEDOUT, ERR_COMMS_RECEIVE_TIMEOUT},
	{E_CO_ETH_CMD_SEND, ERR_COMMS_ETH_CMD_SEND},
	{E_CO_ETH_NO_RESP, ERR_COMMS_ETH_NO_RESP},
	{E_CO_ETH_INVALID_RESP, ERR_COMMS_ETH_INVALID_RESP},
	{E_CO_ETH_INVALID_PARAM, ERR_COMMS_INVALID_PARM},
	{E_CO_ETH_NO_IPADDR, ERR_COMMS_ETH_NO_IPADDR},
	{UCL_SUCCESS, ERR_COMMS_GENERAL}
};

typedef struct
{
	uint wError;
	char * ptDesc;
	bool fEFB;
	bool fRetry;
} T_ERROR_DESC;

#ifdef __SSL
	int sslerrno;
#endif

/*
**-----------------------------------------------------------------------------
** Module variable definitions and initialisations.
**-----------------------------------------------------------------------------
*/
#ifdef __VX670
	static unsigned char hangUpStringGprs[MAX_HANGUP_STR_LEN] = "ATH";
	static unsigned char initStringGprs[MAX_INIT_STR_LEN] = "ATQ0&D1";
	static unsigned char startUpStringGprs[MAX_STARTUP_STR_LEN] =  "ATE0V0";
	static long escapeGuardTimeGprs = 1500;
	static char baudRateGprs = Rt_115200; // for GSM always Rt_9600
	static char formatGprs = Fmt_A8N1|Fmt_DTR|Fmt_RTS; // for Gprs it must be always Fmt_A8N1
	static char parameterGprs = 0x00;
	static char protocolGprs = P_char_mode;
	static char sdlcparmoptionGprs = P_sdlc_sec;
	static char sdlcparmaddressGprs = 0x30;
	static unsigned char phoneNoGprs[MAX_PHNO_LENGTH] = "*99**PPP*1#";
	static unsigned char modelNoGprs[MAX_MODEL_SIZE] =  "GPRS_SIEMENS";
	static short minBatteryStrengthGprs = 5;
	static short minSignalStrengthGprs = 4;
//	static short lcWaitTimeGprs = 2000;
//	static short nwWaitTimeGprs = 4000;
	static short lcWaitTimeGprs = 1000;
	static short nwWaitTimeGprs = 2000;
//	static unsigned char apn[MAX_APN_LENGTH] = "telstra.internet";
	unsigned char apn[MAX_APN_LENGTH] = "STGEFTPOS";	// Telstra St. George default
//	unsigned char apn[MAX_APN_LENGTH] = "TRANSACTPLUS";	// Optus St. George default
	static unsigned char packetProtocol[MAX_PACKET_PROTOCOL_SIZE] = "IP";
	static unsigned char terminalTypeGprs[MAX_TERM_TYPE_SIZE] = "Vx670";
	static char progress[22] = "";
#else
	T_COMMS * psCommsPreDial = NULL;
	uint wCommsSyncError;

	uchar lastPSTNBaud = 0xFF;
	bool lastPSTNSync = 0xFF;
	bool lastPSTNFastConnect = 0xFF;
#endif

static uint wSerialPortHandle[2] = {0xFFFF, 0xFFFF};

char model[13];
char partNo[13];

static UclFactory * ptrFactory = NULL;
static CommTimer * timer = NULL;

typedef Ucl * tUclPtr;
static tUclPtr ptrUcl = NULL;
bool modem_open = false;
bool uart_open[2] = {false, false};

int iphandle = -1;

//#define DispText2(a,b,c,d,e,f)	LOG_PRINTF((a));
#define DispText2(a,b,c,d,e,f)	;

#ifdef __VX670

static void GPRSProgress(char * data)
{
	setfont("");
	gotoxy(1, 1);
	putpixelcol(data, 12);
}

/////////////////////////////////////////////////////////////////////////////
// FUNCTION NAME:	GetParamGprs()
//
// DESCRIPTION:		This method is called by library APIs to get the user specified values
//					of the communication parameters.
//
// PARAMETERS:		paramId	<=	ID of the parameter required
//					buffer	=>	buffer to hold parameter value 
//					size	<=	size of the buffer
//
// RETURNS:			required ID value is updated and no. of bytes/errNo is returned 
//
/////////////////////////////////////////////////////////////////////////////
static short GetParamGprs(short paramId, unsigned char *buffer, short size)
{
	if(buffer == NULL || size <= 0)
		return E_UCL_INVALID_PARM;

	// Initialisation
	memset(buffer,0,size);

/*	{
		char temp[40];
		char key;
		sprintf(temp, "Param ID: %d ", paramId);
		write_at(temp, strlen(temp), 1, 0);
		while(read(STDIN, &key, 1) != 1);
	}
*/

	switch (paramId)
	{
		case ESCAPE_GUARD_TIME:
				sprintf((char*)buffer, "%ld",escapeGuardTimeGprs);
				return strlen((char*)buffer);

		case NWCHECK:
				if(get_env("#NWCHECK", (char*) buffer, 2) <= 0)
					buffer[0] = 0;
				else
					buffer[1] = 1;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;

		case BAUDRATE:
				buffer[0] = baudRateGprs;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;
		
		case FORMAT:
				buffer[0] = formatGprs;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;

		case PARAMETER:
				buffer[0] = parameterGprs;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;

		case PROTOCOL:
				buffer[0] = protocolGprs;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;

		case SDLC_PARM_OPTION:
				buffer[0] = sdlcparmoptionGprs;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;

		case SDLC_PARM_ADDRESS:
				buffer[0] = sdlcparmaddressGprs;
				buffer[1] = '\0';
				return SINGLE_CHAR_SIZE;

		case PHONE_NUMBER:
				if(get_env("#PHONE", (char*) buffer,MAX_PHNO_LENGTH) <= 0)
					strcpy((char*) buffer,(char *) phoneNoGprs);
				return strlen((char *) buffer);

		case APN:
				if(get_env("#APN", (char *) buffer,MAX_APN_LENGTH) <= 0)
					strcpy((char *) buffer, (char *) apn);
				return strlen((char *) buffer);

		case INIT_STRING:
				if(get_env("#INITGPRS", (char *) buffer, MAX_INIT_STR_LEN) <= 0)
					memcpy(buffer, initStringGprs,MAX_INIT_STR_LEN);
				return MAX_INIT_STR_LEN;

		case STARTUP_STRING:
				if(get_env("#STARTUPGPRS", (char *) buffer, MAX_STARTUP_STR_LEN) <= 0)
					memcpy(buffer, startUpStringGprs,MAX_STARTUP_STR_LEN);
				return MAX_STARTUP_STR_LEN;

		case HANGUP_STRING:
				if(get_env("#HANGUPGPRS", (char *) buffer, MAX_HANGUP_STR_LEN) <= 0)
					memcpy(buffer, hangUpStringGprs,MAX_HANGUP_STR_LEN);
				return MAX_HANGUP_STR_LEN;

		case MIN_BATTERY_STRENGTH:
				if(get_env("#MINBATTERYSTRENGTH", (char *) buffer, size) <= 0)
					sprintf((char *) buffer, "%d",minBatteryStrengthGprs);
				return strlen((char *) buffer);

		case MIN_SIGNAL_STRENGTH:
				if(get_env("#MINSIGNALSTRENGTH", (char *) buffer, size) <= 0)
					sprintf((char *) buffer, "%d",minSignalStrengthGprs);
				return strlen((char *) buffer);

		case LOCAL_CMD_WAIT_TIME:
				if(get_env("#LCWAITTIME", (char *) buffer, 10) <= 0)
					sprintf((char *) buffer, "%d", lcWaitTimeGprs);
				return strlen((char *) buffer);

		case NW_CMD_WAIT_TIME:
				if(get_env("#NWWAITTIME", (char *) buffer, 10) <= 0)
					sprintf((char *) buffer, "%d", nwWaitTimeGprs);
				return strlen((char *) buffer);

		case PACKET_PROTOCOL:
				strncpy((char *) buffer, (const char *) packetProtocol, MAX_PACKET_PROTOCOL_SIZE-1);
				return MAX_PACKET_PROTOCOL_SIZE;

		case TERMINAL_TYPE :
				strcpy((char *)buffer,(const char *)terminalTypeGprs);
				return (strlen((const char *) buffer));

		case MODEL_NO:
				memcpy(buffer, modelNoGprs, MAX_MODEL_SIZE);
				return MAX_MODEL_SIZE;

		default:
			break;
	}
	return UCL_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////
// FUNCTION   : NotifyGprs()
//
// DESCRIPTION: This method is called by library APIs to inform the user about device status
//              and waits for the user response.
//
// PARAMETERS:	state	<=	ID of the parameter required
//
// RETURNS:		return the terminal status
//
/////////////////////////////////////////////////////////////////////////////
static short NotifyGprs(short state)
{
	switch(state)
	{
		case STATUS_INITRADIO:
			{
				char data[12] = {0x18, 0x00, 0x24, 0x18, 0x42, 0x24, 0x99, 0x42, 0x3C, 0x00, 0x00, 0x00};
				GPRSProgress(data);
			}
//			DispText("\x87", 0, 0, false, false, false);	// stage 4
			return UCL_SUCCESS;

		case STATUS_DIAL:
			{
				char data[12] = {0x18, 0x00, 0x24, 0x18, 0x42, 0x24, 0x99, 0x42, 0x3C, 0x81, 0x7E, 0x00};
				GPRSProgress(data);
			}
//			DispText("\x86", 0, 0, false, false, false);	// stage 6
			return UCL_SUCCESS;

		case STATUS_RECV:
		case STATUS_REG:
		case STATUS_ATTACH:
		default:
			break;
	}

	return 0;
}

static ApplicationObj appObjGprs = {GetParamGprs, NotifyGprs};

#else

static IPParams ipparams = {"", "", "", "", ""};

void CommsReInitPSTN(void)
{
	lastPSTNSync = 0xFF;
	lastPSTNFastConnect = 0xFF;
	lastPSTNBaud = 0xFF;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : GetParameter
**
** DESCRIPTION: Get the various parameter set for this connection.
** 
** PARAMETERS:	paramId	<=	Parameter ID
**		buffer	=>	Contains the value of the parameter requested
**		size	<=	Maximum size of the buffer
**
** RETURNS:	Number of bytes filled in the buffer --OR-- Error number
**
**-----------------------------------------------------------------------------
*/
short GetParameter(short paramId, unsigned char *buffer, short size)
{
	if((buffer == NULL) || (size <= 0))
		return E_UCL_INVALID_PARM;

	memset(buffer,0,size);

	switch (paramId)
	{
	case ESCAPE_GUARD_TIME:
			sprintf((char*)buffer, "%ld", 1300);	// Default value.
			return strlen((char*)buffer);
	case BAUDRATE:
			buffer[0] = Rt_9600;
//			buffer[0] = Rt_19200;
//			buffer[0] = Rt_115200;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case FORMAT:
			buffer[0] = Fmt_A8N1|Fmt_DTR|Fmt_RTS;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case PARAMETER:
			buffer[0] = 0;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case PROTOCOL:
			buffer[0] = P_char_mode;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case SDLC_PARM_OPTION:
			buffer[0] = P_sdlc_sec;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case SDLC_PARM_ADDRESS:
			buffer[0] = 0x30;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case MDM_TYPE:
			buffer[0] = SINGLE_PORT;
			buffer[1] = '\0';
			return SINGLE_CHAR_SIZE;
	case INIT_STRING:	// Possibly fast connect needs to go here....
			strcpy((char*)buffer, "AT+iE0");
			return MAX_INIT_STR_LEN;
	case HANGUP_STRING:
			strcpy((char*)buffer, "ATH");
			return MAX_HANGUP_STR_LEN;
	case STARTUP_STRING:	// Possibly fast connect needs to go here....
			strcpy((char*)buffer, "AT+i");
			return MAX_STARTUP_STR_LEN;
	case MODEL_NO:
			if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
				strcpy((char*)buffer, "ETHERNET_ASIX");
			else
				strcpy((char*)buffer, "ETHERNET_ICHIP");
			return MAX_MODEL_SIZE;
	case TERMINAL_TYPE:
			if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
				strcpy((char*)buffer, "Vx570");
			else
				strcpy((char*)buffer, "Vx510");
			return 6;
	default:
		break;
	}
	return UCL_FAILURE;
}

short Notify(short state)
{
	if (state == STATUS_RECV)	// If negative value returned, IP connection is disrupted
		return 0;
	return state;
}

static ApplicationObj appObj = {GetParameter, Notify};

#endif

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsSerialDataAvailable
**
** DESCRIPTION: Check if receive data is available
** 
** PARAMETERS:	None
**
** RETURNS:	RECEIVE_EMPTY		if receive buffer is empty
**			RECEIVE_READY		if receive buffer has data
**			ERR_COMMS_CONNECT_FAILURE	if connection is not up.
**
**-----------------------------------------------------------------------------
*/
static uint CommsSerialDataAvailable(int port)
{
	char four[4];

	if (wSerialPortHandle[port] == 0xFFFF || get_port_status(wSerialPortHandle[port], four) < 0)
		return 0;

	if (four[0])
		return 1;

	return 2;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsTranslateError
**
** DESCRIPTION: Translate the local error to an FPOZ error
**
** PARAMETERS:	NONE
**
** RETURNS:		NONE
**
**-----------------------------------------------------------------------------
*/
static uint CommsTranslateError(int eError)
{
	T_ERROR_TABLE * errorTable = (T_ERROR_TABLE *) sErrorTable;

	for(;errorTable->fromError != UCL_SUCCESS; errorTable++)
		if (errorTable->fromError == eError)
			return errorTable->wToError;

	return errorTable->wToError;
}

#ifndef __VX670
// Write AT string to modem
int WriteATCmd(uint wHandle, char * ATString, int OK, char * ATResponse)
{
    int     k;
    int     status;
    char    rbuf[100];
	char	searchChar;
    
	errno = 0;
	status = write(wHandle, ATString, strlen(ATString)); 
	k = 0;
	rbuf[0] = '\0';

    // While in terse mode (ATV0) all result codes are terminated 
    // with a <CR>.  If the modem was in verbose mode (ATV1 which 
    // is default) the termination would be <CR><LF>.
	searchChar = (OK == 2)? 0x0A:0x0D;

	// Look for the search character
   	while (OK > 0) {
       	do {
        	SVC_WAIT(10);
	        status = read(wHandle, &rbuf[k], 1);
       		if (status > 0) {
        		k = k + status;
	        	rbuf[k] = '\0';
       		}	
        } while (rbuf[k-1] != searchChar);
        OK--;
   	} 

	// Compare the response against what is expected...
	if (ATResponse)
	{
		if (strstr(rbuf, ATResponse) != NULL)
			return true;
	}

	return false;
}

int GetDialResponse(T_COMMS * psCommsModem)
{
	TIMER_TYPE myTimer;
	int     k;
	int     status;
	char    rbuf[300];

	// Initialisation
	memset(rbuf, 0, sizeof(rbuf));

	// Arm the connection timer
	TimerArm(&myTimer, psCommsModem->bConnectionTimeout * 10000L);

	// Wait for a response during the connection timer period
	for(k = 0;TimerExpired(&myTimer) == false;)
	{
	    SVC_WAIT(10);
		status = read(psCommsModem->wHandle, &rbuf[k], sizeof(rbuf)-k);
		if (status > 0)
		{
			if (strstr(rbuf, "CONNECT") != NULL)
				return UCL_SUCCESS;
			if (strstr(rbuf, "BUSY") != NULL)
				return E_UCL_BUSY;
			if (strstr(rbuf, "ERROR") != NULL)
				return E_UCL_ERROR;
			if (strstr(rbuf, "NO DIALTONE") != NULL)
				return E_UCL_NO_DIAL_TONE;
			if (strstr(rbuf, "NO ANSWER") != NULL)
				return E_UCL_NO_ANSWER;
			if (strstr(rbuf, "NO CARRIER") != NULL)
				return E_UCL_NO_CARRIER;
			if (strstr(rbuf, "NO LINE") != NULL)
				return E_UCL_NO_LINE;

			k += status;
		}
	}

	return E_UCL_TIMEOUT;
}
#endif

uchar TranslateBaudRate(uchar bBaudRate)
{
	int i;
	static struct
	{
		uchar fromBaudRate;
		uchar toBaudRate;
	} baudTable[] = {	{COMMS_300, Rt_300},	{COMMS_600, Rt_600},	{COMMS_1200, Rt_1200},	{COMMS_2400, Rt_2400},
						{COMMS_4800, Rt_4800},	{COMMS_9600, Rt_9600},	{COMMS_12000, Rt_12000},{COMMS_14400, Rt_14400},
						{COMMS_19200, Rt_19200},{COMMS_28800, Rt_28800},{COMMS_38400, Rt_38400},{COMMS_57600, Rt_57600},
						{COMMS_115200, Rt_115200}
					};

	// Find the true baud rate number for Verifone first
	for (i = 0; i < COMMS_BAUDRATE_MAX_COUNT; i++)
	{
		if (bBaudRate == baudTable[i].fromBaudRate)
			return baudTable[i].toBaudRate;
	}

	return Rt_115200;
}

#ifndef __VX670
/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsAsyncToSync
**
** DESCRIPTION: Change to Synchronoous connection
** 
** PARAMETERS:	None
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
static void CommsAsyncToSync(uint handle)
{
	struct Opn_Blk Com3ob;

  	Com3ob.rate = Rt_115200;
    Com3ob.format = Fmt_SDLC | Fmt_AFC | Fmt_DTR;
	Com3ob.protocol = P_sdlc_mode;
	Com3ob.trailer.sdlc_parms.address = 0x30;
    Com3ob.trailer.sdlc_parms.option = P_sdlc_sec;
	set_opn_blk(handle, &Com3ob);
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsSyncSwitch
**
** DESCRIPTION: Check if receive data is available
** 
** PARAMETERS:	None
**
** RETURNS:	RECEIVE_EMPTY		if receive buffer is empty
**			RECEIVE_READY		if receive buffer has data
**			ERR_COMMS_CONNECT_FAILURE	if connection is not up.
**
**-----------------------------------------------------------------------------
*/
static uint CommsSyncSwitch(void)
{
	int retVal;

	if (psCommsPreDial)
	{
		if (peek_event() & EVT_COM3)
		{
			wCommsSyncError = ERR_COMMS_NONE;

			if ((retVal = GetDialResponse(psCommsPreDial)) < UCL_SUCCESS)
				wCommsSyncError = CommsTranslateError(retVal);
			else if (psCommsPreDial->fSync)
			{
				wCommsSyncError = ERR_COMMS_NONE;
				CommsAsyncToSync(psCommsPreDial->wHandle);
			}

			psCommsPreDial = NULL;
		}
		else
			wCommsSyncError = ERR_COMMS_NONE;
	}

	return ERR_COMMS_NONE;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsPstnWait
**
** DESCRIPTION: Wait for the connection to be established
** 
** PARAMETERS:	None
**
** RETURNS:		None
**
**-----------------------------------------------------------------------------
*/
static uint CommsPstnWait(T_COMMS * psComms)
{
	int retVal;

	if (psCommsPreDial)
	{
		psCommsPreDial = NULL;

		if ((retVal = GetDialResponse(psComms)) < UCL_SUCCESS)
		{
			wCommsSyncError = CommsTranslateError(retVal);
			return wCommsSyncError;
		}
		else if (psComms->fSync)
		{
			wCommsSyncError = ERR_COMMS_NONE;
			CommsAsyncToSync(psComms->wHandle);
		}
	}

	return wCommsSyncError;
}
#endif

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsSetSerial
**
** DESCRIPTION: Changes parameters of an open communication port
**
** PARAMETERS:	T_COMMS * psComms
**
** RETURNS:		NONE
**
**-----------------------------------------------------------------------------
*/
uint CommsSetSerial(T_COMMS * psComms)
{
	uchar format;
	struct Opn_Blk Com1ob;

	// Set the correct data bits, parity bits and stop bits
	if (psComms->bDataBits == 7)
	{
		if (psComms->tParityBit == 'E')
			format = Fmt_A7E1;
		else if (psComms->tParityBit == 'O')
			format = Fmt_A7O1;
		else /*if (psComms->tParityBit == 'N')*/
			format = Fmt_A7N1;
	}
	else
	{
		if (psComms->tParityBit == 'E')
			format = Fmt_A8E1;
		else if (psComms->tParityBit == 'O')
			format = Fmt_A8O1;
		else /*if (psComms->tParityBit == 'N')*/
			format = Fmt_A8N1;
	}

	Com1ob.rate = TranslateBaudRate(psComms->bBaudRate);
    Com1ob.format = format;
	Com1ob.protocol = P_char_mode;
	Com1ob.parameter = 0;
	set_opn_blk(psComms->wHandle, &Com1ob);

	return ERR_COMMS_NONE;
}


#ifdef __VX670

static int initUCL(void)
{
	short retVal = UCL_SUCCESS;

	if (ptrFactory == NULL)
		ptrFactory = CreateUclFactory();

	if (timer == NULL)
		timer = (CommTimer *) CreateVerixCommTimer();

	if (ptrUcl == NULL)
		ptrUcl = ptrFactory->Create(COM2, &retVal, &appObjGprs, GPRS);

	return retVal;
}

//
//-----------------------------------------------------------------------------
// FUNCTION   : commsGPRS
//
// DESCRIPTION: Establish a GPRS connection
//
// PARAMETERS:	connectionTimeout	<=	TCP connection timeout
//				responseTimeout		<=	TCP response timeout
//
// RETURNS:		UCL_SUCCESS or Communication Error
//
//-----------------------------------------------------------------------------
//
int commsGPRS(T_COMMS * psComms)
{
	short retVal;
	short waitTime;
	short tcpTimout;
	char connectionTimeout[30];
	int length;
	netparams net_param;

	// If GPRS already established, return OK
	if (iphandle >= 0)
	{
		if (netstatus() != 1)
			CommsDisconnect(psComms);
		else
			return UCL_SUCCESS;
	}

	// Setup the APN & progress display
	{
		char data[12] = {0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		GPRSProgress(data);
	}
//	DispText("\x8A", 0, 0, false, false, false);	// Stage 1
	strcpy((char *)apn, psComms->ownIpAddress);
	strcpy(progress, psComms->gateway);

	if ((retVal = initUCL()) < UCL_SUCCESS)
		return retVal;

	//Doing InitRadio much before netconnect() results in netconnect() function taking much less time for GPRS
	//Beacuse lcWaitTimeGprs and nwWaitTimeGprs value can be reduced by the amount this function is called 
	// before calling netconnect.
	if(ptrUcl != NULL) 
	{
		timer->SetTimer(5000);
		ptrUcl->InitRadio(ptrUcl, timer);
	}

	{
		char data[12] = {0x18, 0x00, 0x24, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		GPRSProgress(data);
	}
//	DispText("\x89", 0, 0, false, false, false);	// Stage 2

	// Get the timeout variables
	if ((length = get_env("#CONNECTIONTIMEOUT", connectionTimeout, sizeof(connectionTimeout)-1)) > 0)
		connectionTimeout[length] = '\0';		
	else strcpy(connectionTimeout, "30000");

	// Assign the timeouts and datalink layer establishment to the TCP/IP library
	AssignUcltoTCP(	ptrUcl, timer,
					atoi(connectionTimeout), /* for open() */
					atoi(connectionTimeout), /* for connect() */
					atoi(connectionTimeout), /* for disconnect() */
					atoi(connectionTimeout), /* for close() */
					6000, /* for send() */
					atoi(connectionTimeout) /* for receive() */);

	// Configure DHCP
	memset(&net_param,0,sizeof(net_param));
	strcpy(net_param.ipSubnetMask, "0.0.0.0");	// Obtain address using DHCP
	strcpy(net_param.ipSubnetMask, "0.0.0.0");	// Obtain address using DHCP

	net_param.datalinkProtocol = DL_PPP_APP_DEFINED;
	strcpy(net_param.linkparams.appparams.ispPassword, "");
	strcpy(net_param.linkparams.appparams.ispUserid, "");
	retVal = SetTimeout(SEND_TIMEOUT,50);
	retVal = SetTimeout(RECEIVE_TIMEOUT,80);

	if ((retVal= netconfig(&net_param)) < 0)
		return errno;

#ifdef __SSL
	if ((retVal = vSSL_Init(NULL, NULL)) < 0)
		return retVal;
#endif

	{
		char data[12] = {0x18, 0x00, 0x24, 0x18, 0x42, 0x24, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00};
		GPRSProgress(data);
	}
//	DispText("\x88", 0, 0, false, false, false);	// Stage 3

	// Setting the time to wait after sending AT+iDOWN command. If not set default value is 2750 msec. 
	waitTime = 1000;	// time is in milli-seconds
	setparam(WAIT_ICHIPRESET,(char*)&waitTime , sizeof(waitTime));

	// This parameter will make iChip commands to respond within the time set.
	tcpTimout = 7;	// time is in seconds
	setparam(TCP_TIMEOUT,(char*)&tcpTimout , sizeof(tcpTimout));

	// Establish a connection and obtain an IP address from the DHCP server
	if ((retVal = netconnect(1)) != UCL_SUCCESS)
	{
		DispText("  ", 0, 0, false, false, false);	// Disconnect Stage
		DispText("X", 0, 0, false, false, true);

		if (retVal > 0 || errno < 0)
			return errno;
		else return retVal;
	}

	DispText("  ", 0, 0, false, false, false);	// Disconnect Stage
//	DispText("\x85", 0, 0, false, false, false);	// Stage 6

	// Indicate that GPRS has been established
	iphandle = 0;

	return UCL_SUCCESS;
}
#else

#ifdef __SSL
int app_verify_cb(int ok, X509_STORE_CTX * ctx)
{
	if (!ok)
	{
		int errCode = X509_STORE_CTX_get_error(ctx);

		if (errCode == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
			sslerrno = ERR_COMMS_SSL_SELF_SIGNED;
		else if (errCode == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)
			sslerrno = ERR_COMMS_SSL_NO_ROOT_CA;
		else if (errCode == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY)
			sslerrno = ERR_COMMS_SSL_NO_ISSUER_CA;
		else if (errCode == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE)
			sslerrno = ERR_COMMS_SSL_UNTRUSTED_SGL_CERT;
		else if (errCode == X509_V_ERR_INVALID_CA)
			sslerrno = ERR_COMMS_SSL_INVALID_CA;
		else if (errCode == X509_V_ERR_INVALID_PURPOSE)
			sslerrno = ERR_COMMS_SSL_WRONG_PURPOSE_CA;
		else if (errCode == X509_V_ERR_CERT_REJECTED)
			sslerrno = ERR_COMMS_SSL_CERT_REJECTED;
		else if (errCode == X509_V_ERR_CERT_UNTRUSTED)
			sslerrno = ERR_COMMS_SSL_UNTRUSTED_CERT;
		else if (errCode == X509_V_ERR_CERT_LENGTH)
			sslerrno = ERR_COMMS_SSL_WEAK_KEY;
		else if (errCode == X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY)
			sslerrno = ERR_COMMS_SSL_DECODING_PUBLIC_KEY;
		else if (errCode == X509_V_ERR_CERT_SIGNATURE_FAILURE)
			sslerrno = ERR_COMMS_SSL_SIGNATURE_FAILURE;
		else if (errCode == X509_V_ERR_CERT_NOT_YET_VALID || errCode == X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD)
			sslerrno = ERR_COMMS_SSL_CERT_NOT_YET_VALID;
		else if (errCode == X509_V_ERR_CERT_HAS_EXPIRED || errCode == X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD)
			sslerrno = ERR_COMMS_SSL_CERT_EXPIRED;
		else
			sslerrno = ERR_COMMS_SSL_GENERAL;

		return 0;
	}

//	{
//		char temp[40];
//		char key;
//		sprintf(temp, "OK: %d %d ", ok, errCode);
//		write_at(temp, strlen(temp), 1, 0);
//		while(read(STDIN, &key, 1) != 1);
//	}

	return 1;
}
#endif

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsIPCreate
**
** DESCRIPTION: Sets up the IP UCL but only for a VX570 since VX510 can have
**				a problem with the shared port for the modem
**
** PARAMETERS:	NONE
**
** RETURNS:		NONE
**
**-----------------------------------------------------------------------------
*/
static short CommsIPCreate(T_COMMS * psComms)
{
	short retVal = 0;
	short waitTime;
	short tcpTimeout;
	short retransmit = 0;
	netparams net_param;

	if (ptrUcl == NULL)
	{
		if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
			ptrUcl = ptrFactory->Create(COM_ETH1, &retVal, &appObj, ETHERNET);
		else
			ptrUcl = ptrFactory->Create(COM3, &retVal, &appObj, ETHERNET);

		// Assign the timeouts and datalink layer establishment to the TCP/IP library
		if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
			AssignUcltoTCP(ptrUcl, timer,
								DEFAULT_USB_ETH_OPEN_TIMEOUT,		// open() timer
								DEFAULT_USB_ETH_CONNECT_TIMEOUT,	// connect() timer
								DEFAULT_USB_ETH_DISCONNECT_TIMEOUT, // disconnect() timer
								DEFAULT_USB_ETH_CLOSE_TIMEOUT,		// close() timer
								DEFAULT_USB_ETH_SEND_TIMEOUT,		// send() timer
								DEFAULT_USB_ETH_RECV_TIMEOUT);		// receive() timer
		else
			AssignUcltoTCP(ptrUcl, timer,
								psComms->bConnectionTimeout * 1000, /* open() timer */
								psComms->bConnectionTimeout * 1000, /* connect() timer */
								psComms->bConnectionTimeout * 1000, /* disconnect() timer */
								psComms->bConnectionTimeout * 1000, /* close() timer */
								6000, /* send() timer */
								psComms->bResponseTimeout * 1000 /* receive() timer */);
	}

	if (iphandle < 0)
	{
		// Configure DHCP
		memset(&net_param,0,sizeof(net_param));
		if (ipparams.myIP[0] == '\0' || strcmp(psComms->ownIpAddress, "0.0.0.0"))
		{
			strcpy(net_param.ipAddress, psComms->ownIpAddress);	// Obtain address using DHCP
			if (strcmp(net_param.ipAddress, "0.0.0.0") == 0)
				strcpy(net_param.ipSubnetMask, "0.0.0.0");	// Obtain address using DHCP
			else
			{
				strcpy(net_param.ipSubnetMask, "255.255.255.0");
				setparam(GATEWAY, psComms->gateway, strlen(psComms->gateway));
				setparam(DNSPRI, psComms->pdns, strlen(psComms->pdns));
				setparam(DNSSEC, psComms->sdns, strlen(psComms->sdns));
			}
		}
		else
		{
			strcpy(net_param.ipAddress, ipparams.myIP);
			strcpy(net_param.ipSubnetMask, ipparams.subnetIP);
			setparam(GATEWAY, ipparams.gateWayIP, strlen(ipparams.gateWayIP));
			setparam(DNSPRI, ipparams.DNSPriIP, strlen(ipparams.DNSPriIP));
			setparam(DNSSEC, ipparams.DNSSecIP, strlen(ipparams.DNSSecIP));
		}

		if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
			net_param.datalinkProtocol = DL_USB_ASIX_ETH;
		else
			net_param.datalinkProtocol = DL_PPP_CO_ETH;

		if ((retVal = netconfig(&net_param)) < 0)
			return retVal;

#ifdef __SSL
		if ((retVal = vSSL_SetCAFile("ca.pem")) < 0)
			return retVal;

		if ((retVal = vSSL_Init(app_verify_cb, NULL)) < 0)
			return retVal;
#endif

		// Setting the time to wait after sending AT+iDOWN command. If not set default value is 2750 msec. 
		// Not required for Vx570
		if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
		{
			waitTime = 1000;	// time is in milli-seconds
			retVal = setparam(WAIT_ICHIPRESET,(char*)&waitTime , sizeof(waitTime));
		}

		// This parameter will make iChip commands to respond within the time set.
		tcpTimeout = 7;	// time is in seconds
		setparam(TCP_TIMEOUT,(char*)&tcpTimeout , sizeof(tcpTimeout));

		if (strncmp(model, "VX570", 5) == 0 || strncmp(model, "VX810", 5) == 0 || (strncmp(partNo, "M251", 4) == 0 && partNo[5] == '6'))
		{
			retransmit = 0;	// Number of TCP retransmission retries
			retVal = setparam(TCP_MAXTXRETRY,(char*)&retransmit, sizeof(retransmit));
		}
	}

	return retVal;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsIPSetup
**
** DESCRIPTION: Sets up the IP UCL but only for a VX570 since VX510 can have
**				a problem with the shared port for the modem
**
** PARAMETERS:	NONE
**
** RETURNS:		NONE
**
**-----------------------------------------------------------------------------
*/
static short CommsIPSetup(T_COMMS * psComms)
{
	short retVal = 0;

	// Delay before any previous netdisconnect as per Verifone recommendation.
	// Obviously we will not call netdisconnect unless we get errors....
	// Possibly the sections up to SSL_init should be called only once but at the expense of
	//  inability to change ip address and DNS settings. This is also as per Verifone recommendations.
	SVC_WAIT(1000);

	// Establish a connection and obtain an IP address from the DHCP server
	retVal = netconnect(1);

	// If already connected to the network, then set the result to be OK
	if (retVal == -2)
		retVal = 0;

	if (retVal >= 0)
		getparam(ALLPARAM, (char *) &ipparams, sizeof(ipparams));

	return retVal;
}
#endif

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsIPConnect
**
** DESCRIPTION: Connect to the host using a TCP/IP socket
**
** PARAMETERS:	NONE
**
** RETURNS:		NONE
**
**-----------------------------------------------------------------------------
*/
static uint CommsIPConnect(T_COMMS * psComms)
{
	short retVal = 0;
	struct timeval timeout;

	// Structure needed to get resolve a host name to a host address
	struct sockaddr_in socket_host;
	struct hostent hostEnt;
	char h_name[50];		
	char *h_aliases[2];
	char h_aliases1[50]={'\0'};
	char h_aliases2[50]={'\0'};
	char *h_addr_list[2];
	char h_addr_list1[50]={'\0'};
	char h_addr_list2[50]={'\0'};

	// Initialise the TCP connection parameters
	yield();
	SVC_WAIT(10);
	memset( &socket_host, 0, sizeof(socket_host));
	socket_host.sin_family = AF_INET;
	memset(socket_host.sin_zero,0,8);

	// Set up the structure
	hostEnt.h_name = h_name;
	hostEnt.h_aliases = h_aliases;
	h_aliases[0] = h_aliases1;
	h_aliases[1] = h_aliases2;
	hostEnt.h_addr_list = h_addr_list;
	h_addr_list[0] = h_addr_list1;
	h_addr_list[1] = h_addr_list2;

#ifdef __VX670
	// Update the progress if available
	strcpy(progress, psComms->gateway);
	DispText(progress, 0, 0, false, false, false);
#endif

	if (gethostbyname(psComms->ipAddress, &hostEnt) < 0)
	{
		if (h_errno == EBADARGUMENT && inet_addr(psComms->ipAddress) != 0xFFFFFFFF)
			socket_host.sin_addr.s_addr = htonl(inet_addr(psComms->ipAddress));
		else
		{
			iphandle = -1;
			netdisconnect(1);
			yield();
			return CommsTranslateError(h_errno);
		}
	}
	else
	{
		addhost(psComms->ipAddress, hostEnt.h_addr, 4, AF_INET);
		socket_host.sin_addr.s_addr = htonl(*((ulong *)hostEnt.h_addr));
	}

//		socket_host.sin_addr.s_addr = htonl(inet_addr(psComms->ipAddress));
	socket_host.sin_port = htons(psComms->wPortNumber);

		// TESTING **********************
/*		{
			char keycode;
			char tempBuf[200];

			sprintf(tempBuf, "IP Address: %ld %d      ",	socket_host.sin_addr.s_addr, socket_host.sin_port);
			write_at(tempBuf, strlen(tempBuf), 1, 2);
			while(read(STDIN, &keycode, 1) != 1);
		}
*/
	// Establish a TCP connection
#ifdef __SSL
	if ((iphandle = socket(AF_INET, SOCK_STREAM | (psComms->eHeader >= E_HEADER_SSL?SOCK_SSL:0), 0)) >= 0)
#else
	if ((iphandle = socket(AF_INET, SOCK_STREAM, 0)) >= 0)
#endif
	{
		struct linger myLinger;

		psComms->wHandle = iphandle;

		timeout.tv_sec = 5;
		timeout.tv_usec = 100;
		retVal = setsockopt(psComms->wHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		timeout.tv_sec = 2;
		timeout.tv_usec = 100;
		retVal = setsockopt(psComms->wHandle, SOL_SOCKET, SO_PKTRCVTIMEO, (char*)&timeout, sizeof(timeout));

		myLinger.l_onoff = false;
		myLinger.l_linger = 0;
		setsockopt(psComms->wHandle, SOL_SOCKET, SO_LINGER, (char*)&myLinger, sizeof(myLinger));

		if ((retVal = connect(psComms->wHandle, (struct sockaddr *) &socket_host, sizeof(struct sockaddr_in))) < 0)
		{
			closesocket(psComms->wHandle);
			psComms->wHandle = 0xFFFF;
		}
		yield();
	}

		// TESTING **********************
/*		{
			char keycode;
			char tempBuf[200];

			sprintf(tempBuf, "IP Handle: %d %d      ",	iphandle, retVal);
			write_at(tempBuf, strlen(tempBuf), 1, 2);
			while(read(STDIN, &keycode, 1) != 1);
		}
*/

	// Disconnect if the TCP fails
	if (iphandle < 0 || retVal < 0)
	{
		if (iphandle > 0)
			closesocket(iphandle);

		if (psComms->ipAddress[0] < '0' || psComms->ipAddress[0] > '9')
			deletehost(psComms->ipAddress);

		iphandle = -1;
		netdisconnect(1);
		yield();
		return CommsTranslateError(errno);
	}

	return ERR_COMMS_NONE;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsConnect
**
** DESCRIPTION: Start a new file session
**				Assumes a connection has already been established with the host
**
** PARAMETERS:	NONE
**
** RETURNS:		NONE
**
**-----------------------------------------------------------------------------
*/
uint CommsConnect(T_COMMS * psComms)
{
	short retVal = 0;
	int serialPort = -1;

	if (ptrFactory == NULL)
		ptrFactory = CreateUclFactory();
	if (timer == NULL)
		timer = (CommTimer *) CreateVerixCommTimer();

	switch (psComms->eConnectionType)
	{
		case E_CONNECTION_TYPE_UART_1:
			serialPort = 0;
			wSerialPortHandle[0] = 0xFFFF;

			if (uart_open[0])
				return ERR_COMMS_NONE;
			psComms->wHandle = open((strncmp(model, "VX810", 5) == 0)?DEV_COM2:DEV_COM1, 0);
			uart_open[0] = true;

			CommsSetSerial(psComms);
			break;

		case E_CONNECTION_TYPE_UART_2:
			serialPort = 1;
			wSerialPortHandle[1] = 0xFFFF;

			if (uart_open[1])
				return ERR_COMMS_NONE;
#ifdef __VX670
			psComms->wHandle = open(DEV_COM6, 0);
#else
			psComms->wHandle = open((strncmp(model, "VX810", 5) == 0)?DEV_COM1:DEV_COM2, 0);
#endif
			uart_open[1] = true;

			CommsSetSerial(psComms);
			break;

		case E_CONNECTION_TYPE_IP_SETUP:
		case E_CONNECTION_TYPE_IP:

#ifdef __SSL
			sslerrno = 0;
#endif

#ifdef __VX670
			if ((retVal = commsGPRS(psComms)) < UCL_SUCCESS)
				return CommsTranslateError(retVal);

			if (psComms->eConnectionType == E_CONNECTION_TYPE_IP_SETUP)
				return ERR_COMMS_NONE;
#else
			if ((retVal = CommsIPCreate(psComms)) < UCL_SUCCESS)
				return CommsTranslateError(retVal);

			if (iphandle < 0 && CommsIPSetup(psComms) < UCL_SUCCESS)
				return CommsTranslateError(errno);

			if (psComms->eConnectionType == E_CONNECTION_TYPE_IP_SETUP)
			{
				if (iphandle < 0) iphandle = 0;
				return ERR_COMMS_NONE;
			}
#endif
			break;

#ifndef __VX670
		case E_CONNECTION_TYPE_PSTN:

#ifdef __SSL
			sslerrno = 0;
#endif
			{
				struct Opn_Blk Com3ob;
				char buffer[300];

				if (modem_open)
					return ERR_COMMS_NONE;
				psComms->wHandle = open(DEV_COM3, 0);
				modem_open = true;

				Com3ob.rate = TranslateBaudRate(psComms->bBaudRate);
				if (psComms->fSync || Com3ob.rate >= Rt_9600)
					Com3ob.rate = Rt_115200;

				if (psComms->fSync)
					Com3ob.format = Fmt_A7E1 | Fmt_AFC;
				else
					Com3ob.format = Fmt_A8N1 | Fmt_DTR | Fmt_RTS;

				Com3ob.protocol = P_char_mode;
				Com3ob.parameter = 0;
				set_opn_blk(psComms->wHandle, &Com3ob);

				// No need to initiate the second time round
				if (lastPSTNBaud == psComms->bBaudRate && lastPSTNSync == psComms->fSync && lastPSTNFastConnect == psComms->fFastConnect)
					break;
				lastPSTNBaud = psComms->bBaudRate;
				lastPSTNSync = psComms->fSync;
				lastPSTNFastConnect = psComms->fFastConnect;

				// Write the startup strings to the modem...
			    WriteATCmd(psComms->wHandle, "AT&F\r", 2, "OK");
				WriteATCmd(psComms->wHandle, "ATE0\r", 2, "OK");

				// Initialise the modem for a synchronous communication
				if (psComms->fSync)
				{
					strcpy(buffer, "ATW2X4S25=1&D2%C0\\N0+A8E=,,,0;\r");
				    WriteATCmd(psComms->wHandle, buffer, 2, "OK");

					if (psComms->fFastConnect)
					{
						strcpy(buffer, "AT$F2\r");
						WriteATCmd(psComms->wHandle, buffer, 2, "OK");
					}

					strcpy(buffer,"ATS17=15+ES=6,,8;+ESA=,,,,1\r");
					WriteATCmd(psComms->wHandle, buffer, 2, "OK");

					if (psComms->bBaudRate == COMMS_1200)
						strcpy(buffer, "AT+MS=v22,0,1200,1200,1200,1200");
					else if (psComms->bBaudRate == COMMS_2400)
						strcpy(buffer, "AT+MS=v22b,0,2400,2400,2400,2400");
					else if (psComms->bBaudRate == COMMS_4800)
						strcpy(buffer, "AT+MS=v32,0,4800,4800,4800,4800");
					else if (psComms->bBaudRate == COMMS_9600)
						strcpy((char*)buffer, "AT+MS=v32,0,9600,9600,9600,9600");
				}
				else
					strcpy(buffer, "AT&D2Q0S0=1");

				strcat(buffer, "\r");
			    WriteATCmd(psComms->wHandle, buffer, 2, "OK");
			}
			break;
#endif
		default:
			return ERR_COMMS_CONNECT_NOT_SUPPORTED;
	}

	if (psComms->eConnectionType == E_CONNECTION_TYPE_IP)
		return CommsIPConnect(psComms);

#ifndef __VX670
	else if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
	{
		char rbuf[200];
		rbuf[0] = 0x01;
		set_serial_lines(psComms->wHandle, rbuf);

		if (psComms->ptPhoneNumber == NULL || psComms->ptPhoneNumber[0] == '\0')
		{
			psCommsPreDial = NULL;
			wCommsSyncError = ERR_COMMS_NO_PHONE_NO;
			return ERR_COMMS_NO_PHONE_NO;
		}

		sprintf(rbuf, "ATD%c%s%s%s\r", psComms->tDialType, psComms->fBlindDial?"":"W", psComms->ptDialPrefix?psComms->ptDialPrefix:"", psComms->ptPhoneNumber?psComms->ptPhoneNumber:"");
		write(psComms->wHandle, rbuf, strlen(rbuf)); 
	}

	// Initialise synchronous PSTN switch needed if pre-dial ON to false
	psCommsPreDial = NULL;
#endif

	// Update the serial handle for backgound monitoring...
	if (serialPort != -1)
		wSerialPortHandle[serialPort] = psComms->wHandle;

#ifndef __VX670
	// If pre-dial is enabled, return now
	if (psComms->fPreDial)
	{
		if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
		{
			read_event();
			psCommsPreDial = psComms;
		}

		return ERR_COMMS_NONE;
	}

	if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
	{
		if ((retVal = GetDialResponse(psComms)) < UCL_SUCCESS)
		{
			wCommsSyncError = CommsTranslateError(retVal);
			return wCommsSyncError;
		}
		else if (psComms->fSync)
		{
			wCommsSyncError = ERR_COMMS_NONE;
			CommsAsyncToSync(psComms->wHandle);
		}
	}
#endif

	return ERR_COMMS_NONE;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsSend
**
** DESCRIPTION: Send data via the modem
** 
** PARAMETERS:	None
**
** RETURNS:	ERR_MDM_CONNECTION_FAILED if the modem connection is lost.
**		ERR_MDM_SEND_FAILED if transmiting the data failed.
**
**-----------------------------------------------------------------------------
*/
uint CommsSend( T_COMMS * psComms )
{
#ifndef __VX670
	short retVal;
#endif
	char four[4];

	// If length = ZERO, just return with SUCCESS
	if (psComms->wLength == 0) return ERR_COMMS_NONE;

	if (psComms->eConnectionType == E_CONNECTION_TYPE_IP)
	{
		char * data = (char *) psComms->pbData;
		unsigned length = psComms->wLength;

		DispText2("SEND:SVC_WAIT", 1, 0, true, false, true);
//		SVC_WAIT(150);

#ifdef __VX670
		DispText(progress, 0, 0, false, false, true);
#endif
		DispText2("SEND:send", 1, 0, true, false, true);

		if (psComms->eHeader == E_HEADER_HTTP || psComms->eHeader == E_HEADER_HTTPS)
		{
			unsigned i;

			for (i = 0; i < psComms->wLength; i++)
			{
				if (psComms->pbData[i-3] == '\r' && psComms->pbData[i-2] == '\n' && psComms->pbData[i-1] == '\r' && psComms->pbData[i] == '\n')
				{
					unsigned j, k;
					unsigned bodyLength = psComms->wLength - i - 1;

					data = my_malloc(strlen((char *) psComms->pbData) + 10);	// Allow for the Content-Length:%d

					for (k = 0, j = 0; k < psComms->wLength; k++, j++)
					{
						if (psComms->pbData[k] == '%' && psComms->pbData[k+1] == 'd')
						{
							ltoa(bodyLength, &data[j], 10);
							j += (strlen(&data[j]) - 1);
							k++;
						}
						else data[j] = psComms->pbData[k];
					}

					data[length = j] = '\0';
					
					break;
				}
			}
			if (i == psComms->wLength)
				return ERR_COMMS_INVALID_PARM;
		}

		if (send(psComms->wHandle, data, length, 0) < 0)
		{
			if (data != (char *) psComms->pbData) my_free(data);
			DispText2("SEND:error", 1, 0, true, false, true);
//			SVC_WAIT(150);	// Delay before disconnecting after an error

			if (closesocket(psComms->wHandle))
			{
				SVC_WAIT(50);
				shutdown(psComms->wHandle, 3);
			}

			iphandle = -1;
			SVC_WAIT(100);
			netdisconnect(1);
//			SVC_WAIT(100);
			yield();

			return CommsTranslateError(errno);
		}
		if (data != (char *) psComms->pbData) my_free(data);

		DispText2("SEND:Done", 1, 0, true, false, true);
		return ERR_COMMS_NONE;
	}
#ifndef __VX670
	else if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN && psComms->fPreDial)
	{
		if (wCommsSyncError != ERR_COMMS_NONE)
		{
			psComms->wLength = 0;	// Indicate nothing sent
			return wCommsSyncError;
		}
		
		if (psCommsPreDial != NULL)
		{
			psCommsPreDial = NULL;

			if ((retVal = GetDialResponse(psComms)) < UCL_SUCCESS)
				return CommsTranslateError(retVal);
			else if (psComms->fSync)
				CommsAsyncToSync(psComms->wHandle);
		}
	}

	// Make sure we are still connected if this is a PSTN connection
	if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
	{
		get_port_status(psComms->wHandle, four);
		if ((four[3] & 0x08) == 0)
		{
			psComms->wLength = 0;
			return CommsTranslateError(E_UCL_NO_CARRIER);
		}
	}
#endif

	// Display the 0800 request data
/*	if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
	{
		int i;
		for (i = 0; i < psComms->wLength; i+= 4)
		{
			char keycode;
			char tempBuf[100];

			sprintf(tempBuf, "Tx:%02X %02X %02X %02X  ", psComms->pbData[i], psComms->pbData[i+1], psComms->pbData[i+2], psComms->pbData[i+3]);
			write_at(tempBuf, strlen(tempBuf), 1, 2);
			while(read(STDIN, &keycode, 1) != 1);
		}
	}
*/

	// Write the data
	write(psComms->wHandle, (char *) psComms->pbData, psComms->wLength);

	// Wait for it to transmit completely
	while(get_port_status(psComms->wHandle, four) > 0)
	{
#ifndef __VX670
		// If the line disconnected abruptly, report an error
		if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN && (four[3] & 0x08) == 0)
			return CommsTranslateError(E_UCL_NO_CARRIER);
#endif
		SVC_WAIT(10);
	}

    return ERR_COMMS_NONE;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsReceive
**
** DESCRIPTION: Receive data via the modem
** 
** PARAMETERS:	None
**
** RETURNS:	ERR_MDM_CONNECTION_FAILED if the modem connection is lost.
**		ERR_MDM_RECEIVE_TIMEOUT	if the response does not arrive in time.
**		ERR_MDM_RECEIVE_FAILED if data reception fails
**
**-----------------------------------------------------------------------------
*/
uint CommsReceive( T_COMMS * psComms, bool fFirstChar )
{
	short retVal;
	TIMER_TYPE myTimer;

	// If the length = 0, return with no error
	if (psComms->wLength == 0)
		return ERR_COMMS_NONE;

	if (psComms->eConnectionType == E_CONNECTION_TYPE_IP)
	{
		struct timeval timeout;

/*																		if (debug)
																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Receiving- %d     ", fFirstChar);
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																		}
*/

//		yield();
//		SVC_WAIT(fFirstChar?100:10);
//		SVC_WAIT(10);

/*																		if (debug)
																		{
																			char tempBuf[40];
																			sprintf(tempBuf, "Receiving %d     ", fFirstChar);
																			write_at(tempBuf, strlen(tempBuf), 1, 2);
																		}
*/
/*		if (fFirstChar)
			TimerArm(&myTimer, fFirstChar?psComms->bResponseTimeout * 10000L:psComms->dwInterCharTimeout * 10);

		for (;;)
		{
			int numOfBytes = 0;

#ifndef __VX670
			if (strncmp(model, "VX570", 5) == 0)
#endif
			{
				DispText2("RECV:ioctlsocket", 1, 0, true, false, true);
				ioctlsocket(psComms->wHandle, FIONREAD, (int *) &numOfBytes);
				DispText2("RECV:yield 0", 1, 0, true, false, true);
				yield();
				if (numOfBytes == 0) numOfBytes = -1;
			}

			DispText2("RECV:SVC_WAIT", 1, 0, true, false, true);
//			SVC_WAIT(100);

			if (numOfBytes >= 0)
			{
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
				setsockopt(psComms->wHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
				setsockopt(psComms->wHandle, SOL_SOCKET, SO_PKTRCVTIMEO, (char*)&timeout, sizeof(timeout));

				DispText2("RECV:yield 1", 1, 0, true, false, true);
				yield();
				DispText2("RECV:recv", 1, 0, true, false, true);
				if ((retVal = recv(psComms->wHandle, (char *) psComms->pbData, numOfBytes, 0)) <= 0  && errno != ETIMEDOUT)
				{
					if (errno != ETIMEDOUT)
					{
						if (closesocket(psComms->wHandle))
						{
							SVC_WAIT(50);
							shutdown(psComms->wHandle, 3);
						}

						iphandle = -1;
						SVC_WAIT(100);
						netdisconnect(1);
//						SVC_WAIT(100);
						yield();

						DispText2("RECV:error", 1, 0, true, false, true);
						return CommsTranslateError(errno);
					}
				}
				else
				{
					if (errno != ETIMEDOUT)
					{
						DispText2("RECV:length", 1, 0, true, false, true);
						psComms->wLength = retVal;
						return ERR_COMMS_NONE;
					}
				}
			}

			// Check that the timer has not expired
			if (TimerExpired(&myTimer) == true)
			{
				psComms->wLength = 0;
				DispText2("RECV:TIMEOUT", 1, 0, true, false, true);
				return ERR_COMMS_RECEIVE_TIMEOUT;
			}
		}
*/
		if (fFirstChar)
		{
			timeout.tv_sec = psComms->bResponseTimeout;
			timeout.tv_usec = 100;
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_PKTRCVTIMEO, (char*)&timeout, sizeof(timeout));
		}
		else
		{
			timeout.tv_sec = psComms->dwInterCharTimeout / 1000;
			timeout.tv_usec = psComms->dwInterCharTimeout % 1000 * 1000;
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_PKTRCVTIMEO, (char*)&timeout, sizeof(timeout));
		}
		

		if ((retVal = recv(psComms->wHandle, (char *) psComms->pbData, 1, 0)) < 0)
		{
			if (CommsTranslateError(errno) == ERR_COMMS_TIMEOUT)
				return ERR_COMMS_RECEIVE_TIMEOUT;
			return CommsTranslateError(errno);
		}

		if (psComms->wLength != 1)
		{
//			yield();
//			SVC_WAIT(10);

			timeout.tv_sec = 0;
			timeout.tv_usec = 1;
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_PKTRCVTIMEO, (char*)&timeout, sizeof(timeout));
			if ((retVal = recv(psComms->wHandle, (char *) &psComms->pbData[1], psComms->wLength - 1, 0)) < 0 && errno != ETIMEDOUT)
			{
				if (CommsTranslateError(errno) == ERR_COMMS_TIMEOUT)
					return ERR_COMMS_RECEIVE_TIMEOUT;
				return CommsTranslateError(errno);
			}
			retVal++;
		}

		psComms->wLength = retVal;
		return ERR_COMMS_NONE;
	}
	else
	{
		char four[4];

		// Arm the timers
		TimerArm(&myTimer, fFirstChar?psComms->bResponseTimeout * 10000L:psComms->dwInterCharTimeout * 10);

		for (;;)
		{
			if (get_port_status(psComms->wHandle, four) < 0)
			{
				return ERR_COMMS_RECEIVE_FAILURE;
			}

#ifndef __VX670
			// If a PSTN communication error detected, report it
			if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN && (four[3] & 0x08) == 0)
				return ERR_COMMS_CONNECT_FAILURE;
#endif

			// Check if there are some data available
			if (four[0] > 0)
			{
				uint length;

				length = read(psComms->wHandle, (char *) psComms->pbData, psComms->wLength);
				if (length <= 0)
					return ERR_COMMS_RECEIVE_FAILURE;
				else psComms->wLength = length;


		// Display the data from the host
/*		if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
		{
			int i;
			for (i = 0; i < psComms->wLength; i+=8)
			{
				char keycode;
				char tempBuf[200];

				sprintf(tempBuf, "Rx: %02X %02X %02X %02X %02X %02X %02X %02X ",	psComms->pbData[i], psComms->pbData[i+1], psComms->pbData[i+2], psComms->pbData[i+3],
																					psComms->pbData[i+4], psComms->pbData[i+5], psComms->pbData[i+6], psComms->pbData[i+7]);
				write_at(tempBuf, strlen(tempBuf), 1, 2);
				while(read(STDIN, &keycode, 1) != 1);
			}
		}
*/

				return ERR_COMMS_NONE;
			}

			// Check that the timer has not expired
			if (TimerExpired(&myTimer) == true)
			{
				psComms->wLength = 0;
				return ERR_COMMS_RECEIVE_TIMEOUT;
			}

//			SVC_WAIT(10);
		}
	}
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsDisconnect
**
** DESCRIPTION: Disconnect. If it does not disconnect within 3 seconds, it
**				tries to disconnect again forever until the line becomes idle
** 
** PARAMETERS:	None
**
** RETURNS:		ERR_DISCONNECTION_FAILURE if the disconnection attempt fails.
**
**-----------------------------------------------------------------------------
*/
uint CommsDisconnect(T_COMMS * psComms)
{
	TIMER_TYPE myTimer;

	if (psComms->eConnectionType == E_CONNECTION_TYPE_IP || psComms->eConnectionType == E_CONNECTION_TYPE_IP_SETUP)
	{
		if (psComms->wHandle != 0xFFFF)
		{
			char buf[100];
			struct timeval timeout;

#ifdef __VX670
			DispText("X ", 0, 0, false, false, false);
#endif
			DispText2("DISC:Shutdown", 1, 0, true, false, true);
			SVC_WAIT(50);
			shutdown(psComms->wHandle, SHUT_WR);
			SVC_WAIT(50);
			DispText2("DISC:RECV", 1, 0, true, false, true);

			timeout.tv_sec = 5;
			timeout.tv_usec = 100;
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
			setsockopt(psComms->wHandle, SOL_SOCKET, SO_PKTRCVTIMEO, (char*)&timeout, sizeof(timeout));
			while (recv(psComms->wHandle, buf, 100, 0) > 0);

			DispText2("DISC:SVC_WAIT", 1, 0, true, false, true);
			SVC_WAIT(100);
			DispText2("DISC:closesocket", 1, 0, true, false, true);
			if (closesocket(psComms->wHandle))
			{
				DispText2("DISC:shutdown 2", 1, 0, true, false, true);
				SVC_WAIT(50);
				shutdown(psComms->wHandle, 3);
			}
			DispText2("DISC:TCP/IP Done", 1, 0, true, false, true);
			psComms->wHandle = 0xFFFF;
		}

//		if (iphandle >= 0 && strncmp(model, "VX570", 5))
#ifdef __VX670
		if (iphandle >= 0 && psComms->fFastConnect == false)
		{
			DispText("  ", 0, 0, false, false, false);
			DispText("X", 0, 0, false, false, true);
#else
//		if (iphandle >= 0)
		if (iphandle >= 0 && psComms->fSync)
		{
#endif
			iphandle = -1;
			DispText2("DISC:netdisconnect", 1, 0, true, false, true);
			SVC_WAIT(50);
			netdisconnect(1);
			yield();
			DispText2("DISC:UCL Done", 1, 0, true, false, true);
		}
	}
	else
	{
#ifndef __VX670
		if (psComms->eConnectionType == E_CONNECTION_TYPE_PSTN)
		{
			char rbuf[10];
			rbuf[0] = 0x00;
			set_serial_lines(psComms->wHandle, rbuf);
			modem_open = false;
		}
#endif

		close(psComms->wHandle);

		if (psComms->eConnectionType == E_CONNECTION_TYPE_UART_1)
			uart_open[0] = false;
		else if (psComms->eConnectionType == E_CONNECTION_TYPE_UART_2)
			uart_open[1] = false;
		else
		{
			// Wait 1/2 a second after issuing the disconnection
			for (TimerArm(&myTimer, 5000L);	TimerExpired(&myTimer) == false;);
		}
	}

	if (psComms->wHandle == wSerialPortHandle[0])
		wSerialPortHandle[0] = 0xFFFF;
	else if (psComms->wHandle == wSerialPortHandle[1])
		wSerialPortHandle[1] = 0xFFFF;

	return ERR_COMMS_NONE;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : Comms
**
** DESCRIPTION: Main Handler external communication functions
** 
** PARAMETERS:	None
**
** RETURNS:		Error code
**
**-----------------------------------------------------------------------------
*/
uint Comms(E_COMMS_FUNC eFunc, T_COMMS * psComms, ...)
{
	uint retCode = ERR_COMMS_NONE;
	uchar header[2];
	uchar headerIndex;
	uchar headerLength;
	uchar * data;
	uint dataLength;
	uint msgLength = 0;
//	int row = 1;

	if (model[0] == '\0')
	{
		SVC_INFO_MODELNO(model);
		SVC_INFO_PARTNO(partNo);
	}

	switch (eFunc)
	{
		case E_COMMS_FUNC_CONNECT:
			return CommsConnect(psComms);

		case E_COMMS_FUNC_SEND:
			// Check if a header must be sent first
			switch (psComms->eHeader)
			{
				default:
				case E_HEADER_NONE:
				case E_HEADER_SSL:
				case E_HEADER_HTTP:
				case E_HEADER_HTTPS:
					headerLength = 0;
					break;
				case E_HEADER_LENGTH:
				case E_HEADER_SSL_LENGTH:
					header[0] = psComms->wLength / 256;
					header[1] = psComms->wLength % 256;
					headerLength = 2;
					break;
			}

			// If a header must be sent first, send it
			if (headerLength)
			{
				int i;

				// Shift the main data and add the header data
				psComms->wLength += headerLength;
				psComms->pbData = my_realloc(psComms->pbData, psComms->wLength);

				for (i = psComms->wLength-1; i >= headerLength; i--)
					psComms->pbData[i] = psComms->pbData[i-headerLength];
				memcpy(psComms->pbData, header, headerLength);
				return CommsSend(psComms);
			}
			else
				return CommsSend(psComms);

		case E_COMMS_FUNC_RECEIVE:
			// Preserve the maximum number of bytes expected
			dataLength = psComms->wLength;

			// If HTTP protocol used, then clear the buffer as we will be searching for strings...not needed otherwise (ie. assumed binary).
			if (psComms->eHeader == E_HEADER_HTTP || psComms->eHeader == E_HEADER_HTTPS)
				memset(psComms->pbData, 0, sizeof(psComms->pbData));

			// Preserve the start of the receive buffer
			data = psComms->pbData;

			// Set the header length according to the header type
			if (psComms->eHeader == E_HEADER_LENGTH || psComms->eHeader == E_HEADER_SSL_LENGTH)
				headerLength = 2;
			else
				headerLength = 0;

			// Initialisation. The buffer size must be big enough to receive the header as well
			headerIndex = 0;
			psComms->wLength = dataLength + headerLength;

/*	{
		char temp[200];
		setfont("");
		sprintf(temp, "%X,%d,%d,%ld  ", psComms->pbData, psComms->wLength, psComms->bResponseTimeout, psComms->dwInterCharTimeout);
		write_at(temp, strlen(temp), 1, row++);
	}
*/			// Start receiving characters
			for(retCode = CommsReceive(psComms, true); psComms->wLength && retCode == ERR_COMMS_NONE; retCode = CommsReceive(psComms, false))
			{
				// Are we still expecting header data?
				if (headerLength)
				{
					// Is the header data complete?
					if (psComms->wLength >= headerLength)
					{
						msgLength = psComms->wLength - headerLength;
						memcpy(&header[headerIndex], psComms->pbData, headerLength);
						memmove(data, &psComms->pbData[headerLength], msgLength);
						headerIndex = headerLength = 0;

						// Reduce the maximum length if the header has some length information
						if ((psComms->eHeader == E_HEADER_LENGTH || psComms->eHeader == E_HEADER_SSL_LENGTH) && dataLength > (header[0] * 256 + header[1]))
							dataLength = header[0] * 256 + header[1];
						if (dataLength < msgLength) dataLength = msgLength;
					}
					// Header data not complete yet, update the header and continue...
					else
					{
						memcpy(&header[headerIndex], psComms->pbData, psComms->wLength);
						headerIndex += psComms->wLength;
						headerLength -= psComms->wLength;
						psComms->pbData = &data[headerIndex];
					}
				}
				else
					msgLength += psComms->wLength;

				// Adjust the data length for HTTP responses
				if (psComms->eHeader == E_HEADER_HTTP || psComms->eHeader == E_HEADER_HTTPS)
				{
					unsigned bodyLength = 0;
					char * bodyStart = strstr((char *) psComms->pbData, "\r\n\r\n");
					if (bodyStart)
					{
						char * contentLength = strstr((char *) psComms->pbData, "Content-Length:");
						if (contentLength)
							bodyLength = atoi(contentLength + strlen("Content-Length:"));
						dataLength = bodyStart - (char *) psComms->pbData + 4 + bodyLength;

/*						{
							char temp[200];
							setfont("");
							sprintf(temp, "==> %d,%d  ", dataLength, bodyLength);
							write_at(temp, strlen(temp), 1, row++);
						}
*/					}
				}

				// Adjust the receiving length and receive pointer
				psComms->pbData = &data[headerIndex + msgLength];
				psComms->wLength = dataLength + headerLength - msgLength;

/*				{
					char temp[200];
					setfont("");
					sprintf(temp, "%X,%d,%d  ", psComms->pbData, psComms->wLength, msgLength);
					write_at(temp, strlen(temp), 1, row++);
				}
*/			}


			psComms->pbData = data;
			psComms->wLength = msgLength;

/*			{
				char temp[200];
				char key;
				setfont("");
				sprintf(temp, "%X,%d  ", psComms->pbData, psComms->wLength);
				write_at(temp, strlen(temp), 1, row++);
				while(read(STDIN, &key, 1) != 1);
			}
*/
//			SVC_WAIT(200);

			if (msgLength && retCode == ERR_COMMS_RECEIVE_TIMEOUT)
				retCode = ERR_COMMS_NONE;

			// Limit the data if the length of data > header length
			if ((psComms->eHeader == E_HEADER_LENGTH || psComms->eHeader == E_HEADER_SSL_LENGTH) && psComms->wLength > (header[0] * 256 + header[1]))
				psComms->wLength = header[0] * 256 + header[1];

			return retCode;

		case E_COMMS_FUNC_DISCONNECT:
			return CommsDisconnect(psComms);

		case E_COMMS_FUNC_SERIAL_DATA_AVAILABLE:
			return CommsSerialDataAvailable(0);

		case E_COMMS_FUNC_SERIAL2_DATA_AVAILABLE:
			return CommsSerialDataAvailable(1);

		case E_COMMS_FUNC_SYNC_SWITCH:
			#ifndef __VX670
				return CommsSyncSwitch();
			#else
				return ERR_COMMS_NONE;
			#endif

		case E_COMMS_FUNC_SET_SERIAL:
			return CommsSetSerial(psComms);

		case E_COMMS_FUNC_PSTN_WAIT:
			#ifndef __VX670
				return CommsPstnWait(psComms);
			#else
				return ERR_COMMS_NONE;
			#endif
		case E_COMMS_FUNC_DISP_GPRS_STS:
#ifdef __VX670
			DispText("  ", 0, 0, false, false, false);
			DispText(iphandle >= 0?progress:"X", 0, 0, false, false, true);
#endif
			return ERR_COMMS_NONE;
		case E_COMMS_FUNC_SIGNAL_STRENGTH:
#ifdef __VX670
			if (ptrUcl && iphandle >= 0)
			{
				timer->SetTimer(5000);
				return ptrUcl->GetStatus(ptrUcl, ABS_SIGNAL, timer);
			}
#endif
			return 0;
		case E_COMMS_FUNC_PING:
			return ping(psComms->ipAddress);
	}

	return ERR_COMMS_FUNC_NOT_SUPPORTED;
}

/*
**-----------------------------------------------------------------------------
** FUNCTION   : CommsErrorDesc
**
** DESCRIPTION: Returns the comms error description
** 
** PARAMETERS:	None
**
** RETURNS:		Error code
**
**-----------------------------------------------------------------------------
*/
char * CommsErrorDesc(uint wError)
{
	uchar i;

#ifdef __SSL
	static const T_ERROR_DESC sslErrorDesc[] = 
						{
							{ERR_COMMS_NONE,					"NOERROR"},							
							{ERR_COMMS_SSL_NO_ROOT_CA,			"NO_ROOT_CA"},
							{ERR_COMMS_SSL_NO_ISSUER_CA,		"NO_ISSUER_CA"},
							{ERR_COMMS_SSL_UNTRUSTED_SGL_CERT,	"UNTRUSTED_SGL_CA"},
							{ERR_COMMS_SSL_INVALID_CA,			"INVALID_CA"},
							{ERR_COMMS_SSL_WRONG_PURPOSE_CA,	"WRONG_PURPOSE_CA"},
							{ERR_COMMS_SSL_CERT_REJECTED,		"CERT_REJECTED"},
							{ERR_COMMS_SSL_UNTRUSTED_CERT,		"UNTRUSTED_CERT"},
							{ERR_COMMS_SSL_WEAK_KEY,			"WEAK_KEY"},
							{ERR_COMMS_SSL_DECODING_PUBLIC_KEY,	"DECODING_PUBLIC_KEY"},
							{ERR_COMMS_SSL_SIGNATURE_FAILURE,	"CERT_SIG_FAILURE"},
							{ERR_COMMS_SSL_CERT_NOT_YET_VALID,	"CERT_NOT_YET_VALID"},
							{ERR_COMMS_SSL_CERT_EXPIRED,		"CERT_EXPIRED"},
							{ERR_COMMS_SSL_GENERAL,				"SSL_GENERAL"},
							{0,									NULL}
						};
#endif

	static const T_ERROR_DESC errorDesc[] = 
						{
							{ERR_COMMS_NONE,			"NOERROR"},
							{ERR_COMMS_ENGAGED_TONE,	"BUSY"},
							{ERR_COMMS_NO_ANSWER,		"ANSWER"},
							{ERR_COMMS_NO_LINE,			"LINE"},
							{ERR_COMMS_NOT_DIALED,		"NOT_DIALED"},
							{ERR_COMMS_NO_DIAL_TONE,	"LINE"},
							{ERR_COMMS_NO_PHONE_NO,		"NOPHONENUM"},
							{ERR_COMMS_CTS_LOW,			"CTS_LOW"},
							{ERR_COMMS_CANCEL,			"USER_CANCEL"},
							{ERR_COMMS_CARRIER_LOST,	"CARRIER"},
							{ERR_COMMS_SENDING_ERROR,	"SND_FAIL"},
							{ERR_COMMS_PORT_NOT_OPEN,	"PORT_CLOSED"},
							{ERR_COMMS_NOT_CONNECTED,	"IDLE"},
							{ERR_COMMS_TIMEOUT,			"TIMEOUT"},
							{ERR_COMMS_INTERCHAR_TIMEOUT,"ITIMEOUT"},
							{ERR_COMMS_INVALID_PARM,	"PARAM"},
							{ERR_COMMS_FEATURE_NOT_SUPPORTED,"NOT_SUPP"},
							{ERR_COMMS_PORT_USED,		"PORT_USED"},
							{ERR_COMMS_GENERAL,			"GENERAL"},
							{ERR_COMMS_ERROR,			"ERROR"},

							{ERR_COMMS_SIM,				"SIM"},
							{ERR_COMMS_NO_SIGNAL,		"SIGNAL"},
							{ERR_COMMS_LOW_SIGNAL,		"SIGNAL"},
							{ERR_COMMS_LOW_BATTERY,		"BATTERY"},
							{ERR_COMMS_NO_COVERAGE,		"COVERAGE"},
							{ERR_COMMS_NO_PDPCONTEXT,	"PDP"},
							{ERR_COMMS_NETWORK_LOST,	"NETWORK"},

							{ERR_COMMS_PPP_PAP_CHAP_FAILED,	"PPP_AUTH"},
							{ERR_COMMS_PPP_LCP_FAILED,	"PPP_LCP"},
							{ERR_COMMS_ETH_IPCP_FAILED,	"IPCP"},
							{ERR_COMMS_ETH_CONNECTION_ABORTED,"HOST"},
							{ERR_COMMS_ETH_TCP_SHUTDOWN,"SHUTDOWN"},
							{ERR_COMMS_ETH_NOT_CONNECTED,"SOCKET"},
							{ERR_COMMS_ETH_NO_IPADDR,	"DHCP"},
							{ERR_COMMS_ETH_CMD_SEND,	"ETH_SND_FAIL"},
							{ERR_COMMS_ETH_NO_RESP,		"ETH_RCV_FAIL"},

							{ERR_COMMS_RECEIVE_FAILURE,	"RCV_FAIL"},
							{ERR_COMMS_RECEIVE_TIMEOUT,	"TIMEOUT"},

							{ERR_COMMS_CONNECT_FAILURE,	"CONNECT"},
							{ERR_COMMS_CONNECT_NOT_SUPPORTED,"MEDIUM"},
							{0,							NULL}
						};

#ifdef __SSL
	if (sslerrno)
	{
		for (i = 0; sslErrorDesc[i].ptDesc; i++)
		{
			if (sslErrorDesc[i].wError == sslerrno)
				return sslErrorDesc[i].ptDesc;
		}
	}
	else
#endif

	for (i = 0; errorDesc[i].ptDesc; i++)
	{
		if (errorDesc[i].wError == wError)
			return errorDesc[i].ptDesc;
	}

	return "???";
}
