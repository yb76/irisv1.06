/*
**-----------------------------------------------------------------------------
** PROJECT:			iRIS
**
** FILE NAME:       security.c
**
** DATE CREATED:    14 January 2008
**
** AUTHOR:          Tareq Hafez
**
** DESCRIPTION:     This module supports security functions
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
#include <string.h>

//
// Project include files.
//
#include <auris.h>
#include <svc.h>

#ifdef _DEBUG
	#include "macro.h"
	#include "3des.h"

	#include "cryptoki.h"
	#include "ctvdef.h"
	static CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
#else
	#include <svc_sec.h>
#endif

/*
** Local include files
*/
#include "utility.h"
#include "security.h"
#include "iris.h"

/*
**-----------------------------------------------------------------------------
** Constants
**-----------------------------------------------------------------------------
*/
#define	C_SCRIPT_ID		7

// Script macro numbers
#define	M_WRITE			80
#define	M_WRITE_3DES	81
#define	M_ERASE			82
#define	M_ERASE_3DES	83
#define	M_RANDOM		84
#define	M_RANDOM_3DES	85
#define	M_KVC			86
#define	M_KVC_3DES		87

#define	M_COPY			90
#define	M_COPY_3DES		91
#define	M_MOVE			92
#define	M_MOVE_3DES		93
#define	M_XOR			94
#define	M_XOR_3DES		95

#define	M_CLRIV			100
#define	M_SETIV			101

#define	M_ENC			110
#define	M_ENC_3DES		111
#define	M_ENCV			112
#define	M_ENCV_3DES		113
#define	M_DEC			114
#define	M_DEC_3DES		115
#define	M_DECV			116
#define	M_DECV_3DES		117
#define	M_OFB			118
#define	M_OFB_3DES		119

#define	M_INJECT		130
#define	M_INJECT_3DES	131
#define	M_3INJECT_3DES	132

#define	M_OWF			140
#define	M_OWF_3DES		141
#define	M_OWFV_3DES		142
#define	M_OWF_DATA		143
#define	M_OWF_3DES_DATA	144

#define	M_RSA_WRITE		150
#define	M_RSA_CRYPT		151
#define	M_RSA_INJECT	152
#define	M_RSA_3INJECT	153
#define	M_RSA_RSAINJECT	154
#define	M_3DES_RSAINJECT 159

#define	M_PIN			160
#define	M_PIN_3DES		161
#define	M_PINV			162
#define	M_PINV_3DES		163



#define	C_NO_OF_KEYS	256
#define	C_NO_OF_RSA		6
#define	C_APPNAME_MAX	50

/*
**-----------------------------------------------------------------------------
** Module variable definitions and initialisations.
**-----------------------------------------------------------------------------
*/
uchar iris_ktk3[17] = "\x00\xFA\x14\x4D\x82\xB1\x5A\x47\xE1\xB7\xB1\x53\xA0\xFF\x55\x6C";

static const char * fileName = "s1.dat";

#ifdef _DEBUG
	static const char * fileName2 = "keys.dat";

	static FILE * fp;
	static FILE * fpKeys;

	uchar _iv[8];
	uchar block[8*256];
	uchar rsablock[256*6];
#else
	static int handle = -1;
#endif

int cryptoHandle = -2;

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityInit
**
** DESCRIPTION:	Initialise the security application data
**
** PARAMETERS:	None
**
** RETURNS:		None
**-------------------------------------------------------------------------------------------
*/
void SecurityInit(void)
{
#ifdef _DEBUG
    CK_RV rv;
	rv = C_Initialize(NULL);
	rv = C_OpenSession(1, CKF_RW_SESSION, NULL, NULL, &hSession);
	rv = C_Login(hSession, CKU_USER, (CK_CHAR_PTR)"1111", (CK_SIZE) 4);


	if ((fp = fopen(fileName, "r+b")) == NULL)
	{
		int i;
		char appName[C_APPNAME_MAX];

		fp = fopen(fileName, "w+b");

		memset(appName, 0, sizeof(appName));

		// Write an empty application name for all DES and RSA keys
		for (i = 0; i < (C_NO_OF_KEYS + C_NO_OF_RSA); i++)
			fwrite(appName, sizeof(appName), 1, fp);
		fclose(fp);
		fp = fopen(fileName, "r+b");
	}

	if ((fpKeys = fopen(fileName2, "r+b")) == NULL)
	{
		int i;

		fpKeys = fopen(fileName2, "w+b");

		// Write an empty application name for all DES and RSA keys
		for (i = 0; i < (8*256 + 256*6); i++)
			fwrite("\0", 1, 1, fpKeys);
		fclose(fpKeys);
		fpKeys = fopen(fileName2, "r+b");
	}

#else
	// Open the crypto device
	cryptoHandle = open("/dev/crypto", 0);

	// Install the iRIS security script
	iPS_InstallScript("f:irissec.vso");

	// Open the application name key associations file
	// Initialise if file does not exist
	if ((handle = open(fileName, O_RDWR)) == -1)
	{
		int i;
		char appName[C_APPNAME_MAX];

		handle = open(fileName, O_CREAT | O_TRUNC | O_RDWR);

		memset(appName, 0, sizeof(appName));

		// Write an empty application name for all DES and RSA keys
		for (i = 0; i < (C_NO_OF_KEYS + C_NO_OF_RSA); i++)
			write(handle, appName, sizeof(appName));
	}
#endif
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityAppName
**
** DESCRIPTION:	Examines if the key is owned by application.
**				If empty, it optionally claims ownership of the key for the application.
**				The thin client internal manager has access to any key but does not modify ownership
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				claim		<=	If TRUE, claim ownership if empty
**
** RETURNS:		TRUE if it is owned by the application or was just claimed by the application
**				TRUE if this is an internal thin client manager call
**				FALSE if it is not owned by the application and cannot be claimed
**-------------------------------------------------------------------------------------------
*/
static bool SecurityAppName(char * appName, int location, uchar keySize, bool claim)
{
	char currentAppName[C_APPNAME_MAX*2];

	// If it is an internal thin client application manager call, then allow it.
	// An application must claim it later if the name is empty.
	if (strcmp(appName, irisGroup) == 0 && claim == false)
		return true;

	// Read the current application name
#ifdef _DEBUG
	fseek(fp, C_APPNAME_MAX * location, SEEK_SET);
	fread(currentAppName, keySize == 8? C_APPNAME_MAX:C_APPNAME_MAX*2, 1, fp);
#else
	lseek(handle, C_APPNAME_MAX * location, SEEK_SET);
	read(handle, currentAppName, keySize == 8? C_APPNAME_MAX:C_APPNAME_MAX*2);
#endif

	// If empty, then optionally claim it
	if (claim)
	{
		if ((currentAppName[0] == '\0' || strcmp(appName, currentAppName) == 0) &&
			(keySize == 8 || currentAppName[C_APPNAME_MAX] == '\0' || strcmp(appName, &currentAppName[C_APPNAME_MAX]) == 0))
		{
			// Claim the first key for the application
#ifdef _DEBUG
			fseek(fp, C_APPNAME_MAX * location, SEEK_SET);
			fwrite(appName, strlen(appName) + 1, 1, fp);
			fflush(fp);
#else
			lseek(handle, C_APPNAME_MAX * location, SEEK_SET);
			write(handle, appName, strlen(appName) + 1);
#endif

			// Claim the following key if a double length key
			if (keySize != 8)
			{
#ifdef _DEBUG
				fseek(fp, C_APPNAME_MAX * (location+1), SEEK_SET);
				fwrite(appName, strlen(appName) + 1, 1, fp);
				fflush(fp);
#else
				lseek(handle, C_APPNAME_MAX * (location+1), SEEK_SET);
				write(handle, appName, strlen(appName) + 1);
#endif
			}
			return true;
		}
	}
	else
	{
		// If already owned by the application
		if (strcmp(appName, currentAppName) == 0 && (keySize == 8 || strcmp(appName, &currentAppName[C_APPNAME_MAX]) == 0))
			return true;
	}

	// It is not owned by the application
	return false;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityWriteKey
**
** DESCRIPTION:	Write a DES / 3-DES key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				key			<=	The key value
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityWriteKey(char * appName, uchar location, uchar keySize, uchar * key)
{
	uchar data[17];
	unsigned short outLen;

	// Make sure the application owns or can claim the key
	if (SecurityAppName(appName, location, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	memcpy(&data[1], key, keySize);

	// Perform the instruction
#ifdef _DEBUG
	{
		(void) outLen;
		fseek(fpKeys, location*8, SEEK_SET);
		fwrite(&data[1], 1, keySize, fpKeys);
		fflush(fpKeys);
	}
#else
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_WRITE:M_WRITE_3DES, keySize + 1, data, 0, &outLen, data))
		return false;
#endif

	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityEraseKey
**
** DESCRIPTION:	Erase a DES / 3-DES key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityEraseKey(char * appName, uchar location, uchar keySize)
{
	uchar data[1];
	unsigned short outLen;

	// Make sure the application owns or can claim the key
	if (strcmp(appName, irisGroup) && SecurityAppName(appName, location, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;

	// Perform the instruction
#ifdef _DEBUG
	{
		int i;

		(void) outLen;
		fseek(fpKeys, location*8, SEEK_SET);
		for (i = 0; i < keySize; i++)
			fwrite("\0", 1, 1, fpKeys);
		fflush(fpKeys);
	}
#else
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_ERASE:M_ERASE_3DES, sizeof(data), data, 0, &outLen, data))
		return false;
#endif

	// Release the first key from the application
#ifdef _DEBUG
	fseek(fp, C_APPNAME_MAX * location, SEEK_SET);
	fwrite("", 1, 1, fp);
	fflush(fp);
#else
	lseek(handle, C_APPNAME_MAX * location, SEEK_SET);
	write(handle, "", 1);
#endif

	// Release the following key if a double length key
	if (keySize != 8)
	{
#ifdef _DEBUG
		fseek(fp, C_APPNAME_MAX * (location+1), SEEK_SET);
		fwrite("", 1, 1, fp);
		fflush(fp);
#else
		lseek(handle, C_APPNAME_MAX * (location+1), SEEK_SET);
		write(handle, "", 1);
#endif
	}

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityGenerateKey
**
** DESCRIPTION:	Generate a random DES / 3-DES key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityGenerateKey(char * appName, uchar location, uchar keySize)
{
	uchar data;
	unsigned short outLen;

	// Make sure the application owns or can claim the key
	if (SecurityAppName(appName, location, keySize, true) == false)
		return false;

	// Setup the input parameters
	data = location;

#ifdef _DEBUG
	{
		int i;

		(void) outLen;
		fseek(fpKeys, location*8, SEEK_SET);
		for (i = 0; i < keySize; i++)
			fwrite("\x38", 1, 1, fpKeys);
		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_RANDOM:M_RANDOM_3DES, sizeof(data), &data, 0, &outLen, &data))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityKVCKey
**
** DESCRIPTION:	Get the KVC (Key Verification Check) of a DES / 3-DES key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				kvc			<=	Place to store the KVC
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityKVCKey(char * appName, uchar location, uchar keySize, uchar * kvc)
{
	uchar data;
	unsigned short outLen;

	// Make sure the application owns or can claim the key
	if (SecurityAppName(appName, location, keySize, false) == false)
		return false;

	// Setup the input parameters
	data = location;

#ifdef _DEBUG
	(void) outLen;
	{
		uchar block[16];
		uchar zero[8];
		memset(zero, 0, sizeof(zero));

		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		if (keySize == 16)
			Des3Encrypt(block, zero, 8);
		else
			DesEncrypt(block, zero);
		memcpy(kvc, zero, 3);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_KVC:M_KVC_3DES, sizeof(data), &data, 3, &outLen, kvc))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityCopyKey
**
** DESCRIPTION:	Copy a KEK type DES / 3-DES key into another KEK type key.
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				to			<=	The destination location index
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityCopyKey(char * appName, uchar location, uchar keySize, uchar to)
{
	uchar data[2];
	unsigned short outLen;

	// Only proceed if both the location and destination are of KEK or PPASN key type
	// Note that even though the security script enforces this key separation, the key ownership is enforced by this function
	if ((location > (keySize==8?50:49) && location < 211) || (to > (keySize==8?50:49) && to < 211))
		return false;

	// Make sure the application owns the location and owns or can claim the destination location
	if (SecurityAppName(appName, location, keySize, false) == false || SecurityAppName(appName, to, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = to;

#if _DEBUG
	{
		uchar block[16];

		(void) outLen;
		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(block, 1, keySize, fpKeys);
		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_COPY:M_COPY_3DES, sizeof(data), data, 0, &outLen, data))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityMoveKey
**
** DESCRIPTION:	Move a KEK type DES / 3-DES key into another KEK type key.
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				to			<=	The destination location index
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityMoveKey(char * appName, uchar location, uchar keySize, uchar to)
{
	uchar data[2];
	unsigned short outLen;

	// Only proceed if both the location and destination are of KEK or PPASN key type
	// Note that even though the security script enforces this key separation, the key ownership is enforced by this function
	if ((location > (keySize==8?50:49) && location < 211) || (to > (keySize==8?50:49) && to < 211))
		return false;

	// Make sure the application owns the location and owns or can claim the destination location
	if (SecurityAppName(appName, location, keySize, false) == false || SecurityAppName(appName, to, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = to;

#if _DEBUG
	{
		uchar block[16];

		(void) outLen;
		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(block, 1, keySize, fpKeys);

		fseek(fpKeys, location * 8, SEEK_SET);
		memset(block, 0, 16);
		fwrite(block, 1, keySize, fpKeys);
		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_MOVE:M_MOVE_3DES, sizeof(data), data, 0, &outLen, data))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityXorKey
**
** DESCRIPTION:	XOR a KEK type DES / 3-DES key with another of the same type and store the result into another KEK type key.
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				with		<=	The other location index to XOR it with
**				to			<=	The destination location index
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityXorKey(char * appName, uchar location, uchar keySize, uchar with, uchar to)
{
	uchar data[3];
	unsigned short outLen;

	// Only proceed if both the location and destination are of KEK or PPASN key type
	// Note that even though the security script enforces this key separation, the key ownership is enforced by this function
	if ((location > (keySize==8?50:49) && location < 211) || (with > (keySize==8?50:49) && with < 211) || (to > (keySize==8?50:49) && to < 211))
		return false;

	// Make sure the application owns both initial locations and owns or can claim the destination location
	if (SecurityAppName(appName, location, keySize, false) == false || SecurityAppName(appName, with, keySize, false) == false || SecurityAppName(appName, to, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = with;
	data[2] = to;

#ifdef _DEBUG
	{
		int i;
		uchar block[16];
		uchar block2[16];

		(void) outLen;

		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		fseek(fpKeys, with * 8, SEEK_SET);
		fread(block2, 1, keySize, fpKeys);

		for (i = 0; i < keySize; i++)
			block[i] ^= block2[i];

		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(block, 1, keySize, fpKeys);

		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript(	C_SCRIPT_ID, keySize == 8?M_XOR:M_XOR_3DES, sizeof(data), data, 0, &outLen, data))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecuritySetIV
**
** DESCRIPTION:	Clears/Sets the Initial Vector used for subsequent CBC encryption/decryption operations
**
** PARAMETERS:	iv			<=	The Initial Vector 8-byte array. If NULL, then clear the IV.
**
** RETURNS:		None
**-------------------------------------------------------------------------------------------
*/
bool SecuritySetIV(uchar * iv)
{
	unsigned short outLen;

	// Clear the IV if the parameter is NULL
	if (iv == NULL)
	{
#ifdef _DEBUG
		(void) outLen;
		memset(_iv, 0, sizeof(_iv));
#else
		if (iPS_ExecuteScript(	C_SCRIPT_ID, M_CLRIV, 0, NULL, 0, &outLen, NULL))
			return false;
#endif
	}

	// Set the IV otherwise
	else
	{
#ifdef _DEBUG
		(void) outLen;
		memcpy(_iv, iv, sizeof(_iv));
#else
		if (iPS_ExecuteScript(	C_SCRIPT_ID, M_SETIV, 8, iv, 0, &outLen, NULL))
			return false;
#endif
	}

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityCrypt
**
** DESCRIPTION:	CBC or OFB Encrypt/Decrypt a data block.
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				eDataSize	<=	The data block size.
**				eData		<=	The data.
**				decrypt		<=	If true, decrypt. Otherwise, encrypt.
**				ofb			<=	If true, OFB encryption/decryption. Otherwise, CBC encryption/decryption.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityCrypt(char * appName, uchar location, uchar keySize, int eDataSize, uchar * eData, bool decrypt, bool ofb)
{
	int i;
	uchar data[9];
	unsigned short outLen;

	// Make sure the application owns the key. Note that the key type is checked by the script itself
	if (SecurityAppName(appName, location, keySize, false) == false)
		return false;

	// Setup the input parameters
	data[0] = location;

	for (i = 0; i < eDataSize; i += 8)
	{
		// Place the encrypted data in the input parameters buffer
		memset(&data[1], 0, 8);
		memcpy(&data[1], &eData[i], (eDataSize-i) < 8? (eDataSize-i):8);

#ifdef _DEBUG
		(void) outLen;
		{
			int j;
			uchar block[16];

			fseek(fpKeys, location * 8, SEEK_SET);
			fread(block, 1, keySize, fpKeys);

			if (ofb)
			{
				uchar zeros[8];

				memcpy(zeros, _iv, sizeof(zeros));

				if (keySize == 8)
					DesEncrypt(block, zeros);
				else
					Des3Encrypt(block, zeros, 8);

				for (j = 0; j < sizeof(_iv); j++)
				{
					data[1+j] ^= zeros[j];
					_iv[j] = zeros[j];
				}
			}
			else if (decrypt)
			{
				uchar temp[8];

				memcpy(temp, &data[1], 8);

				if (keySize == 8)
					DesDecrypt(block, &data[1]);
				else
					Des3Decrypt(block, &data[1]);

				for (j = 0; j < sizeof(_iv); j++)
				{
					data[1+j] ^= _iv[j];
					_iv[j] = temp[j];
				}
			}
			else
			{
				for (j = 0; j < sizeof(_iv); j++)
					data[1+j] ^= _iv[j];

				if (keySize == 8)
					DesEncrypt(block, &data[1]);
				else
					Des3Encrypt(block, &data[1], 8);

				memcpy(_iv, &data[1], 8);
			}
		}
#else
		// Perform the instruction
		if (iPS_ExecuteScript( C_SCRIPT_ID, ofb?(keySize == 8?M_OFB:M_OFB_3DES):(decrypt?(keySize == 8?M_DEC:M_DEC_3DES):(keySize == 8?M_ENC:M_ENC_3DES)), 9, data, 8, &outLen, &data[1]))
			return false;
#endif

		// Store the encrypted result back
		memcpy(&eData[i], &data[1], (eDataSize-i) < 8? (eDataSize-i):8);
	}

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : _getVariant
**
** DESCRIPTION:	Transforms a single variant byte into an array of bytes. The number of bytes
**				is equal to the key size
**
** PARAMETERS:	data		<=	Buffer where the full variant is stored
**				index		<=	Starting offset within the buffer
**				keySize		<=	The key size (8 for single DES keys or 16 for double length 3-DES keys)
**				variant		<=	Points to the initial 2 variant bytes used to vary the keys
**
** RETURNS:		An index pointing to a location within "data" just after the full variant.
**-------------------------------------------------------------------------------------------
*/
static int _getVariant(uchar * data, uchar keySize, uchar * variant)
{
	int i = 0;

	while (i < keySize)
	{
		data[i++] = variant[0];
		data[i++] = variant[1];
	}

	return i;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityCryptWithVariant
**
** DESCRIPTION:	CBC Encrypt/Decrypt a data block with a variant
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				dataSize	<=	The data block size.
**				data		<=	The data. This must be right filled with ZEROs if the dataSize is not a multiple of 8 bytes.
**				variant		<=	The variant initial 2 bytes.
**				decrypt		<=	If true, decrypt. Otherwise, encrypt.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityCryptWithVariant(char * appName, uchar location, uchar keySize, int eDataSize, uchar * eData, uchar * variant, bool decrypt)
{
	int i;
	uchar data[25];
	int inLen;
	unsigned short outLen;

	// Make sure the application owns the key. Note that the key type is checked by the script itself
	if (SecurityAppName(appName, location, keySize, false) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	inLen = 9 + _getVariant(&data[9], keySize, variant?variant:"\x00\x00");

	for (i = 0; i < eDataSize; i += 8)
	{
		// Place the encrypted data in the input parameters buffer
		memset(&data[1], 0, 8);
		memcpy(&data[1], &eData[i], (eDataSize-i) < 8? (eDataSize-i):8);

#ifdef _DEBUG
		(void) outLen;
		{
			int j;
			uchar block[16];

			fseek(fpKeys, location * 8, SEEK_SET);
			fread(block, 1, keySize, fpKeys);

			for (j = 0; variant && j < keySize;)
			{
				block[j++] ^= variant[0];
				block[j++] ^= variant[1];
			}

			if (decrypt)
			{
				uchar temp[8];

				memcpy(temp, &data[1], 8);

				if (keySize == 8)
					DesDecrypt(block, &data[1]);
				else
					Des3Decrypt(block, &data[1]);

				for (j = 0; j < sizeof(_iv); j++)
				{
					data[1+j] ^= _iv[j];
					_iv[j] = temp[j];
				}
			}
			else
			{
				for (j = 0; j < sizeof(_iv); j++)
					data[1+j] ^= _iv[j];

				if (keySize == 8)
					DesEncrypt(block, &data[1]);
				else
					Des3Encrypt(block, &data[1], 8);

				memcpy(_iv, &data[1], 8);
			}
		}
#else
		// Perform the instruction
		if (iPS_ExecuteScript( C_SCRIPT_ID, decrypt?(keySize == 8?M_DECV:M_DECV_3DES):(keySize == 8?M_ENCV:M_ENCV_3DES), inLen, data, 8, &outLen, &data[1]))
			return false;
#endif

		// Store the encrypted result back
		memcpy(&eData[i], &data[1], (eDataSize-i) < 8? (eDataSize-i):8);
	}

	return true;
}




/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityMAB
**
** DESCRIPTION:	Calculate a MAB block
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				eDataSize	<=	The data block size. This does not have to be a multiple of 8 bytes
**				eData		<=	The data.
**				mab			=>	An 8-byte array buffer containing the MAB of the data block on output if successful
**
** RETURNS:		TRUE if successful and mab will contain the MAB of the data block
**				FALSE if not. MAB array remains untouched.
**-------------------------------------------------------------------------------------------
*/
bool SecurityMAB(char * appName, uchar location, uchar keySize, int eDataSize, uchar * eData, uchar * variant, uchar * mab)
{
	int i;
	uchar data[25];
	uint inLen = 9;
	unsigned short outLen;

	// Make sure the application owns the key. Note that the key type is checked by the script itself
	if (SecurityAppName(appName, location, keySize, false) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	if (variant)
		inLen += _getVariant(&data[9], keySize, variant?variant:"\x00\x00");

	for (i = 0; i < eDataSize; i += 8)
	{
		// Place the encrypted data in the input parameters buffer
		memset(&data[1], 0, 8);
		memcpy(&data[1], &eData[i], (eDataSize-i) < 8? (eDataSize-i):8);

#ifdef _DEBUG
		{
			int j;
			uchar block[16];

			(void) outLen;
			fseek(fpKeys, location * 8, SEEK_SET);
			fread(block, 1, keySize, fpKeys);

			for (j = 0; variant && j < keySize;)
			{
				block[j++] ^= variant[0];
				block[j++] ^= variant[1];
			}

			for (j = 0; j < sizeof(_iv); j++)
				data[1+j] ^= _iv[j];

			if (keySize == 8)
				DesEncrypt(block, &data[1]);
			else
				Des3Encrypt(block, &data[1], 8);

			memcpy(_iv, &data[1], 8);
		}
#else
		// Perform the instruction
		if (iPS_ExecuteScript( C_SCRIPT_ID, keySize == 8?M_ENC:M_ENC_3DES, inLen, data, 8, &outLen, &data[1]))
			return false;
#endif
	}

	// Return the MAB value
	memcpy(mab, &data[1], 8);

	return true;
}



/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityInjectKey
**
** DESCRIPTION:	Inject a KEK encrypted key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				to			<=	The injected key location
**				eKeySize	<=	The encrypted key size (8 = single DES, 16 = double length 3-DES key)
**				eKey		<=	Encrypted key
**				variant		<=	The variant initial 2 bytes.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityInjectKey(char * appName, uchar location, uchar keySize, char * appName2, uchar to, uchar eKeySize, uchar * eKey, uchar * variant)
{
	uchar data[34];
	unsigned short outLen;

	// Only proceed if the location is of the KEK key type
	// Note that even though the security script enforces this key separation, the key ownership is enforced by this function
	if (location > (keySize==8?40:39) && location < 211)
		return false;

	// Make sure the application owns the location and owns or can claim the destination location
	if (SecurityAppName(appName, location, keySize, false) == false || SecurityAppName(appName2, to, eKeySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = to;
	memcpy(&data[2], eKey, eKeySize);
	outLen = eKeySize + 2 + _getVariant(&data[eKeySize+2], keySize, variant?variant:"\x00\x00");

/*	{
		char keycode;
		char tempBuf[200];
		sprintf(tempBuf, "Injecting %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ",
							data[0], data[1],
							data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9],
							data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17]);

		write_at(tempBuf, strlen(tempBuf), 1, 2);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
#ifdef _DEBUG
	{
		int i;
		uchar block[16];

		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		for (i = 0; variant && i < keySize;)
		{
			block[i++] ^= variant[0];
			block[i++] ^= variant[1];
		}

		if (keySize == 8)
			DesDecrypt(block, &data[2]);
		else
		{
			if (eKeySize == 8)
				Des3Decrypt(block, &data[2]);
			else
			{
				Des3Decrypt(block, &data[10]);
				for (i = 0; i < 8; i++)
					data[10+i] ^= data[2+i];
				Des3Decrypt(block, &data[2]);
			}
		}

		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(&data[2], 1, eKeySize, fpKeys);
		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, keySize == 8?M_INJECT:(eKeySize == 8?M_INJECT_3DES:M_3INJECT_3DES), outLen, data, 0, &outLen, NULL))
		return false;
#endif

	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityOWFKey
**
** DESCRIPTION:	OWF a KEK encrypted key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				eKeySize	<=	The encrypted key size (8 = single DES, 16 = double length 3-DES key)
**				eKey		<=	Encrypted key
**				variant		<=	If TRUE, then the key is first varied using the PPASN key value.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityOWFKey(char * appName, uchar location, uchar keySize, uchar to, uchar ppasn, bool variant)
{
	uchar data[3];
	unsigned short outLen;

	// Only proceed if the location and destination are of the KEK key type and ppasn is from a PPASN key type.
	// Note that even though the security script enforces this key separation, the key ownership is enforced by this function
	if ((location > (keySize==8?40:39) && location < 211) || (to > (keySize==8?40:39) && to < 211))
		return false;

	// Make sure the application owns the location and owns or can claim the destination location
	if (SecurityAppName(appName, location, keySize, false) == false || SecurityAppName(appName, to, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = to;
	data[2] = ppasn;

#ifdef _DEBUG
	{
		int i;
		uchar ppasn_16[16];
		uchar ppasn_16_k[16];
		uchar ppasn_16_k2[16];

		uchar block[16];
		uchar block2[16];

		(void) outLen;

		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		fseek(fpKeys, ppasn * 8, SEEK_SET);
		fread(block2, 1, 8, fpKeys);

		// Vary location using PPASN
		for (i = 0; i < 8; i++)
		{
			if (variant)
				block[i] ^= block2[i];
			ppasn_16[i] = block2[i];
			ppasn_16_k[i] = block2[i];
			ppasn_16_k2[i] = block2[i];

			if (variant)
				block[i+8] ^= block2[i];
			ppasn_16[i+8] = block2[i];
			ppasn_16_k[i+8] = block2[i];
			ppasn_16_k2[i+8] = block2[i];
		}

		// OWF, find the MAC (IV set to ZEROS)
		Des3Encrypt(block, ppasn_16, 16);

		for (i = 0; i < 8; i++)
			ppasn_16_k[i] ^= ppasn_16[i+8];

		Des3Encrypt(block, ppasn_16_k, 16);

		for (i = 0; i < 16; i++)
			ppasn_16_k[i] ^= ppasn_16_k2[i];
		
		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(ppasn_16_k, 1, keySize, fpKeys);
		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, keySize == 8?M_OWF:(variant?M_OWFV_3DES:M_OWF_3DES), 3, data, 0, &outLen, NULL))
		return false;
#endif

	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityOWFKeyWithData
**
** DESCRIPTION:	OWF a KEK encrypted key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				to			<=	Encrypted key
**				variant		<=	If TRUE, then the key is first varied using the PPASN key value.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityOWFKeyWithData(char * appName, uchar location, uchar keySize, uchar to, uchar * variant)
{
	uchar data[18];
	unsigned short outLen;

	// Only proceed if the location and destination are of the KEK key type and ppasn is from a PPASN key type.
	// Note that even though the security script enforces this key separation, the key ownership is enforced by this function
	if (location > (keySize==8?200:199) || to > (keySize==8?200:199) || location < 51)
		return false;

	// Make sure the application owns the location and owns or can claim the destination location
	if (SecurityAppName(appName, location, keySize, false) == false || SecurityAppName(appName, to, keySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = to;
	if (keySize == 8)
		memcpy(&data[2], variant, 8);
	else
		memcpy(&data[2], variant, 16);

#ifdef _DEBUG
	{
		int i;
		uchar ppasn_16[16];
		uchar ppasn_16_k[16];

		uchar block[16];

		(void) outLen;

		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		// Vary location using PPASN
		for (i = 0; i < 16; i++)
		{
			ppasn_16[i] = variant[i];
			ppasn_16_k[i] = variant[i];
		}

		// OWF, find the MAC (IV set to ZEROS)
		Des3Encrypt(block, ppasn_16, 16);

		for (i = 0; i < 8; i++)
			ppasn_16_k[i] ^= ppasn_16[i+8];

		Des3Encrypt(block, ppasn_16_k, 16);

		for (i = 0; i < 16; i++)
			ppasn_16_k[i] ^= variant[i];
		
		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(ppasn_16_k, 1, keySize, fpKeys);
		fflush(fpKeys);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, keySize == 8?M_OWF_DATA:M_OWF_3DES_DATA, keySize == 8?10:18, data, 0, &outLen, NULL))
		return false;
#endif

	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : Security3DESWriteRSA
**
** DESCRIPTION:	Inject a 3DES encrypted RSA key
**
** PARAMETERS:	appName		<=	The application name				
**				location	<=	The 3DES key location index
**				appName2	<=	Owner of the stored RSA key
**				to			<=	RSA key location
**				blockLen	<=	Length of the RSA block to inject
**				block		<=	The encrypted RSA block
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool Security3DESWriteRSA(char * appName, uchar location, char * appName2, uchar to, int blockLen, uchar * block)
{
	uchar data[521];
	unsigned short outLen;

	// Make sure the application owns or can claim the location
	if (SecurityAppName(appName, location, 16, false) == false || SecurityAppName(appName2, to + C_NO_OF_KEYS, 8, true) == false)
		return false;

	// Setup the input parameters
	memset(data, 0, sizeof(data));	// Set all to ZERO including the first bytes [2-8] used for EMV.
	data[0] = location;
	data[1] = to;
	memcpy(&data[9], block, blockLen);

#ifdef _DEBUG
	{
		int i, j;
		uchar key[16];
		uchar temp[8];

		for (i = 0; i < blockLen; i += 8)
		{
			// Place the encrypted data in the input parameters buffer
			memset(&data[9], 0, 8);
			memcpy(&data[9], &block[i], (blockLen-i) < 8? (blockLen-i):8);


			fseek(fpKeys, location * 8, SEEK_SET);
			fread(key, 1, 16, fpKeys);


			memcpy(temp, &data[9], 8);
			Des3Decrypt(key, &data[9]);
			for (j = 0; j < sizeof(_iv); j++)
			{
				data[9+j] ^= _iv[j];
				_iv[j] = temp[j];
			}

			// Store the encrypted result back
			memcpy(&block[i], &data[9], (blockLen-i) < 8? (blockLen-i):8);
		}

		memcpy(&data[9], block, blockLen);

		{
			/*
			 * Attribute template for the public key.
			 */
			CK_RV rv;
			CK_BBOOL bTrue = TRUE;
			CK_BBOOL bFalse = FALSE;
			CK_KEY_TYPE keyType = CKK_RSA;
			CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
			CK_OBJECT_CLASS keyClassPrivate = CKO_PRIVATE_KEY;

			CK_ATTRIBUTE publicKeyTpl[] = 
			{
				{CKA_CLASS,				&keyClass,	sizeof(keyClass)},
				{CKA_KEY_TYPE,			&keyType,	sizeof(keyType)},

				{CKA_TOKEN,             &bTrue,    sizeof(CK_BBOOL)},
				{CKA_PRIVATE,           &bFalse,   sizeof(CK_BBOOL)},
				{CKA_LABEL,             "PKsp",    strlen("PKsp")},
				{CKA_ENCRYPT,           &bTrue,    sizeof(CK_BBOOL)},
				{CKA_VERIFY,            &bTrue,    sizeof(CK_BBOOL)},
				{CKA_WRAP,              &bTrue,    sizeof(CK_BBOOL)},
				{CKA_MODIFIABLE,        &bTrue,    sizeof(CK_BBOOL)},
				{CKA_DERIVE,            &bTrue,    sizeof(CK_BBOOL)},

				{CKA_MODULUS,			&block[2],			block[0]},
				{CKA_PUBLIC_EXPONENT,   &block[2+block[0]],	block[1]}
			};
			CK_COUNT publicKeyTplSize = sizeof(publicKeyTpl)/sizeof(CK_ATTRIBUTE);

			CK_ATTRIBUTE privateKeyTpl[] = 
			{
				{CKA_CLASS,				&keyClassPrivate,	sizeof(keyClass)},
				{CKA_KEY_TYPE,			&keyType,	sizeof(keyType)},

				{CKA_TOKEN,             &bTrue,    sizeof(CK_BBOOL)},
				{CKA_PRIVATE,           &bFalse,   sizeof(CK_BBOOL)},
				{CKA_LABEL,             "SKtcu",    strlen("SKtcu")},
				{CKA_SENSITIVE,         &bFalse,   sizeof(CK_BBOOL)},
				{CKA_DECRYPT,           &bTrue,    sizeof(CK_BBOOL)},
				{CKA_SIGN,              &bTrue,    sizeof(CK_BBOOL)},
				{CKA_IMPORT,            &bFalse,   sizeof(CK_BBOOL)},
				{CKA_UNWRAP,            &bTrue,    sizeof(CK_BBOOL)},
				{CKA_MODIFIABLE,        &bTrue,    sizeof(CK_BBOOL)},
				{CKA_EXTRACTABLE,       &bTrue,    sizeof(CK_BBOOL)},
				{CKA_DERIVE,            &bTrue,    sizeof(CK_BBOOL)},
				{CKA_EXPORTABLE,        &bTrue,    sizeof(CK_BBOOL)},
				{CKA_SIGN_LOCAL_CERT,   &bTrue,    sizeof(CK_BBOOL)},

				{CKA_MODULUS,			&block[2],			block[0]},
				{CKA_PRIVATE_EXPONENT,  &block[2+block[0]],	block[1]}
			};
			CK_COUNT privateKeyTplSize = sizeof(privateKeyTpl)/sizeof(CK_ATTRIBUTE);

			CK_ATTRIBUTE templatePKsp[] =	{ {CKA_LABEL,				"PKsp",	strlen("PKsp")} };
			CK_ATTRIBUTE templateSKtcu[] =	{ {CKA_LABEL,				"SKtcu", strlen("SKtcu")} };

			CK_COUNT objectCount;
			CK_OBJECT_HANDLE hKey;

			if (to == 5)
			{
				// Delete any existing PKsp
				do
				{
					objectCount = 0;
					rv = C_FindObjectsInit(hSession, templatePKsp, 1);
					rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
					rv = C_FindObjectsFinal(hSession);
					if (objectCount)
						rv = C_DestroyObject(hSession, hKey);
				} while (objectCount == 1);


				//
				// Generate the new PKsp
				//
				rv = C_CreateObject(hSession, publicKeyTpl, publicKeyTplSize, &hKey);
			}
			else if (to == 4)
			{
				// Delete any existing PKsp
				do
				{
					objectCount = 0;
					rv = C_FindObjectsInit(hSession, templateSKtcu, 1);
					rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
					rv = C_FindObjectsFinal(hSession);
					if (objectCount)
						rv = C_DestroyObject(hSession, hKey);
				} while (objectCount == 1);


				//
				// Generate the new PKsp
				//
				rv = C_CreateObject(hSession, privateKeyTpl, privateKeyTplSize, &hKey);
			}
		}
	}

	(void) outLen;
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, M_3DES_RSAINJECT, 521, data, 0, &outLen, NULL))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityWriteRSA
**
** DESCRIPTION:	Write an RSA key
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				modLen		<=	Modulus length in bytes. 0 = 2048 bits.
**				modulus		<=	Modulus data
**				expLen		<=	Exponent length in bytes. For location 0-3, a maximum of 3 bytes apply.
**				exponent	<=	Exponent data
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityWriteRSA(char * appName, uchar location, int blockLen, uchar * block)
{
	uchar data[512];
	unsigned short outLen;

	// Make sure the application owns or can claim the location
	if (SecurityAppName(appName, location + C_NO_OF_KEYS, 8, true) == false)
		return false;

	// Setup the input parameters
	memset(data, 0, sizeof(data));	// Set all to ZERO including the first 7 bytes used for EMV.
	data[0] = location;
	memcpy(&data[8], block, blockLen);

#ifdef _DEBUG
	{
		/*
		 * Attribute template for the public key.
		 */
	    CK_RV rv;
		CK_BBOOL bTrue = TRUE;
		CK_BBOOL bFalse = FALSE;
		CK_KEY_TYPE keyType = CKK_RSA;
		CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
		CK_OBJECT_CLASS keyClassPrivate = CKO_PRIVATE_KEY;

		CK_ATTRIBUTE sessionKeyTpl[] = 
		{
			{CKA_CLASS,				&keyClass,	sizeof(keyClass)},
			{CKA_KEY_TYPE,			&keyType,	sizeof(keyType)},

			{CKA_TOKEN,             &bTrue,    sizeof(CK_BBOOL)},
			{CKA_PRIVATE,           &bFalse,   sizeof(CK_BBOOL)},
			{CKA_LABEL,             "PKsession",strlen("PKsession")},
			{CKA_ENCRYPT,           &bTrue,    sizeof(CK_BBOOL)},
			{CKA_VERIFY,            &bTrue,    sizeof(CK_BBOOL)},
			{CKA_WRAP,              &bTrue,    sizeof(CK_BBOOL)},
			{CKA_MODIFIABLE,        &bTrue,    sizeof(CK_BBOOL)},
			{CKA_DERIVE,            &bTrue,    sizeof(CK_BBOOL)},

			{CKA_MODULUS,			&block[2],		256},		// 256 is hard coded since length = 00 for 256 bytes modulue
			{CKA_PUBLIC_EXPONENT,   &block[2+256],	block[1]}
		};
		CK_COUNT sessionKeyTplSize = sizeof(sessionKeyTpl)/sizeof(CK_ATTRIBUTE);

		CK_ATTRIBUTE publicKeyTpl[] = 
		{
			{CKA_CLASS,				&keyClass,	sizeof(keyClass)},
			{CKA_KEY_TYPE,			&keyType,	sizeof(keyType)},

			{CKA_TOKEN,             &bTrue,    sizeof(CK_BBOOL)},
			{CKA_PRIVATE,           &bFalse,   sizeof(CK_BBOOL)},
			{CKA_LABEL,             "PKsp",    strlen("PKsp")},
			{CKA_ENCRYPT,           &bTrue,    sizeof(CK_BBOOL)},
			{CKA_VERIFY,            &bTrue,    sizeof(CK_BBOOL)},
			{CKA_WRAP,              &bTrue,    sizeof(CK_BBOOL)},
			{CKA_MODIFIABLE,        &bTrue,    sizeof(CK_BBOOL)},
			{CKA_DERIVE,            &bTrue,    sizeof(CK_BBOOL)},

			{CKA_MODULUS,			&block[2],			block[0]},
			{CKA_PUBLIC_EXPONENT,   &block[2+block[0]],	block[1]}
		};
		CK_COUNT publicKeyTplSize = sizeof(publicKeyTpl)/sizeof(CK_ATTRIBUTE);

		CK_ATTRIBUTE privateKeyTpl[] = 
		{
			{CKA_CLASS,				&keyClassPrivate,	sizeof(keyClass)},
			{CKA_KEY_TYPE,			&keyType,	sizeof(keyType)},

			{CKA_TOKEN,             &bTrue,    sizeof(CK_BBOOL)},
			{CKA_PRIVATE,           &bFalse,   sizeof(CK_BBOOL)},
			{CKA_LABEL,             "SKtcu",    strlen("SKtcu")},
			{CKA_SENSITIVE,         &bFalse,   sizeof(CK_BBOOL)},
			{CKA_DECRYPT,           &bTrue,    sizeof(CK_BBOOL)},
			{CKA_SIGN,              &bTrue,    sizeof(CK_BBOOL)},
			{CKA_IMPORT,            &bFalse,   sizeof(CK_BBOOL)},
			{CKA_UNWRAP,            &bTrue,    sizeof(CK_BBOOL)},
			{CKA_MODIFIABLE,        &bTrue,    sizeof(CK_BBOOL)},
			{CKA_EXTRACTABLE,       &bTrue,    sizeof(CK_BBOOL)},
			{CKA_DERIVE,            &bTrue,    sizeof(CK_BBOOL)},
			{CKA_EXPORTABLE,        &bTrue,    sizeof(CK_BBOOL)},
			{CKA_SIGN_LOCAL_CERT,   &bTrue,    sizeof(CK_BBOOL)},

			{CKA_MODULUS,			&block[2],			block[0]},
			{CKA_PRIVATE_EXPONENT,  &block[2+block[0]],	block[1]}
		};
		CK_COUNT privateKeyTplSize = sizeof(privateKeyTpl)/sizeof(CK_ATTRIBUTE);

		CK_ATTRIBUTE templatePKsession[] =	{ {CKA_LABEL,	"PKsession",	strlen("PKsession")} };
		CK_ATTRIBUTE templatePKsp[] =		{ {CKA_LABEL,	"PKsp",			strlen("PKsp")} };
		CK_ATTRIBUTE templateSKtcu[] =		{ {CKA_LABEL,	"SKtcu",		strlen("SKtcu")} };

		CK_COUNT objectCount;
		CK_OBJECT_HANDLE hKey;

		if (location == 5 && strcmp(appName, irisGroup))
		{
			// Delete any existing PKsp
			do
			{
				objectCount = 0;
				rv = C_FindObjectsInit(hSession, templatePKsp, 1);
				rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
				rv = C_FindObjectsFinal(hSession);
				if (objectCount)
					rv = C_DestroyObject(hSession, hKey);
			} while (objectCount == 1);


			//
			// Generate the new PKsp
			//
			rv = C_CreateObject(hSession, publicKeyTpl, publicKeyTplSize, &hKey);
		}
		else if (location == 4)
		{
			// Delete any existing PKsp
			do
			{
				objectCount = 0;
				rv = C_FindObjectsInit(hSession, templateSKtcu, 1);
				rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
				rv = C_FindObjectsFinal(hSession);
				if (objectCount)
					rv = C_DestroyObject(hSession, hKey);
			} while (objectCount == 1);


			//
			// Generate the new PKsp
			//
			rv = C_CreateObject(hSession, privateKeyTpl, privateKeyTplSize, &hKey);
		}
		else
		{
			// Delete any existing PKsession
			do
			{
				objectCount = 0;
				rv = C_FindObjectsInit(hSession, templatePKsession, 1);
				rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
				rv = C_FindObjectsFinal(hSession);
				if (objectCount)
					rv = C_DestroyObject(hSession, hKey);
			} while (objectCount == 1);


			//
			// Generate the new PKsession
			//
			rv = C_CreateObject(hSession, sessionKeyTpl, sessionKeyTplSize, &hKey);
		}
	}

	(void) outLen;
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, M_RSA_WRITE, 512, data, 0, &outLen, NULL))
		return false;
#endif

	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityClearRSA
**
** DESCRIPTION:	Clears the ownership of the RSA block for the owner or iRIS
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityClearRSA(char * appName, uchar location)
{
	// Make sure the application owns or can claim the key
	if (strcmp(appName, irisGroup) && SecurityAppName(appName, location + C_NO_OF_KEYS, 8, true) == false)
		return false;

	// Read the current application name
#ifdef _DEBUG
	fseek(fp, C_APPNAME_MAX * (location + C_NO_OF_KEYS), SEEK_SET);
	fwrite("", 1, 1, fp);
	fflush(fp);
#else
	lseek(handle, C_APPNAME_MAX * (location + C_NO_OF_KEYS), SEEK_SET);
	write(handle, "", 1);
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityCryptRSA
**
** DESCRIPTION:	Sign/Decrypt an RSA block
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				eDataSize	<=	Size of the data block to crypt
**				eData		<=	Encrypted/Clear data block
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityCryptRSA(char * appName, uchar location, int eDataSize, uchar * eData)
{
	uchar data[257];
	unsigned short outLen;

	// Make sure the application owns the location
	if (eDataSize > 256 || SecurityAppName(appName, location + C_NO_OF_KEYS, 8, false) == false)
		return false;

	// Setup the input parameters
	memset(data, 0, sizeof(data));	// Set all to ZEROs
	data[0] = location;
	memcpy(&data[1], eData, eDataSize);

#ifdef _DEBUG
	{
	    CK_RV rv;
		CK_ULONG outLen;
		CK_MECHANISM mech = {CKM_RSA_X_509, NULL, 0};
		CK_ATTRIBUTE templateSKtcu[] =
		{
			{CKA_LABEL,				"SKtcu",	strlen("SKtcu")}
		};
		CK_ATTRIBUTE templatePKsp[] =
		{
			{CKA_LABEL,				"PKsp",	strlen("PKsp")}
		};
		CK_ATTRIBUTE templatePKsession[] =
		{
			{CKA_LABEL,				"PKsession",	strlen("PKsession")}
		};
		CK_COUNT objectCount = 0;
		CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;

		if (location == 5 && strcmp(appName, irisGroup))
		{
			rv = C_FindObjectsInit(hSession, templatePKsp, 1);
			rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
			rv = C_FindObjectsFinal(hSession);

			rv = C_EncryptInit(	hSession, &mech, hKey);
			rv = C_Encrypt(hSession, &data[1], eDataSize, data, &outLen);
		}
		else if (location == 4)
		{
			rv = C_FindObjectsInit(hSession, templateSKtcu, 1);
			rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
			rv = C_FindObjectsFinal(hSession);

			rv = C_DecryptInit(	hSession, &mech, hKey);
			rv = C_Decrypt(hSession, &data[1], eDataSize, data, &outLen);
		}
		else
		{
			rv = C_FindObjectsInit(hSession, templatePKsession, 1);
			rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
			rv = C_FindObjectsFinal(hSession);

			rv = C_EncryptInit(	hSession, &mech, hKey);
			rv = C_Encrypt(hSession, &data[1], eDataSize, data, &outLen);
		}
	}
	(void) outLen;
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, M_RSA_CRYPT, sizeof(data), data, sizeof(data), &outLen, data))
		return false;
#endif

	memcpy(eData, data, eDataSize);
	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityRSAInjectKey
**
** DESCRIPTION:	Inject a DES / 3DES key stored within an RSA encrypted block
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				to			<=	The injected key location
**				eKeySize	<=	The encrypted key size (8 = single DES, 16 = double length 3-DES key)
**				eDataSize	<=	Size of the data block to decrypt
**				eData		<=	Encrypted data block
**
** RETURNS:		TRUE if successful. The keys will not be returned but the rest of the block will
**				in case the application wants to check a signature, etc. The application can alway
**				erase the key if it does not like the signature.
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityRSAInjectKey(char * appName, uchar location, char * appName2, uchar to, uchar eKeySize, int eDataSize, uchar * eData)
{
	uchar data[258];
	unsigned short outLen;

	// Make sure the application owns the RSA location and owns or can claim the destination location
	if (eDataSize > 256 || SecurityAppName(appName, location + C_NO_OF_KEYS, 8, false) == false || SecurityAppName(appName2, to, eKeySize, true) == false)
		return false;

	// Setup the input parameters
	data[0] = location;
	data[1] = to;
	memcpy(&data[2], eData, eDataSize);

#ifdef _DEBUG
	{
	    CK_RV rv;
		CK_ULONG outLen;
		CK_MECHANISM mech = {CKM_RSA_X_509, NULL, 0};
		CK_ATTRIBUTE templatePKsession[] =
		{
			{CKA_LABEL,				"PKsession",	strlen("PKsession")}
		};
		CK_COUNT objectCount = 0;
		CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;

		rv = C_FindObjectsInit(hSession, templatePKsession, 1);
		rv = C_FindObjects(hSession, &hKey, 1, &objectCount);
		rv = C_FindObjectsFinal(hSession);

		rv = C_EncryptInit(	hSession, &mech, hKey);
		rv = C_Encrypt(hSession, &data[2], eDataSize, data, &outLen);

		fseek(fpKeys, to * 8, SEEK_SET);
		fwrite(&data[240], 1, eKeySize, fpKeys);
		fflush(fpKeys);
	}
	(void) outLen;
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, eKeySize == 8?M_RSA_INJECT:M_RSA_3INJECT, eDataSize+2, data, eDataSize, &outLen, eData))
		return false;
#endif

	return true;
}

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityRSAInjectRSA
**
** DESCRIPTION:	Inject a RSA key stored within an RSA encrypted block
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The RSA key location index
**				to			<=	The injected RSA key location
**				eDataSize	<=	Size of the data block to decrypt
**				eData		<=	Encrypted data block
**
** RETURNS:		TRUE if successful.
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityRSAInjectRSA(char * appName, uchar location, char * appName2, uchar to, int eDataSize, uchar * eData)
{
	uchar data[512];
	unsigned short outLen;

	// Make sure the application owns the RSA location and owns or can claim the destination location
	if (eDataSize > 510 || SecurityAppName(appName, location + C_NO_OF_KEYS, 8, false) == false || SecurityAppName(appName2, to + C_NO_OF_KEYS, 8, true) == false)
		return false;

	// Setup the input parameters
	memset(data, 0, sizeof(data));	// Set all to ZERO including the first 7 bytes (ie. 2->8) used for EMV.
	data[0] = location;
	data[1] = to;
	memcpy(&data[9], eData, eDataSize);

#ifdef _DEBUG
	(void) outLen;
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, M_RSA_RSAINJECT, eDataSize+9, data, 0, &outLen, NULL))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityPINBlock
**
** DESCRIPTION:	Get the PIN Block
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				pan			<=	The PAN as an ASCII string
**				ePinBlock	=>	The PIN block will be stored here if successful.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityPINBlock(char * appName, uchar location, uchar keySize, char * pan, uchar * ePinBlock)
{
	uchar data[9];
	unsigned short outLen;
	char myPan[20];
	int index, j;

	// Make sure the application owns the key. Note that the key type is checked by the script itself
	if (SecurityAppName(appName, location, keySize, false) == false)
		return false;

	// Only get the last 12 digits of the pan excluding the last check digit.
	for (j = 0, index = strlen(pan) - 13; index < (int) (strlen(pan)-1); j++, index++)
	{
		if (index < 0) myPan[j] = '0';
		else myPan[j] = pan[index];
	}
	myPan[j] = '\0';

	// Setup the input parameters
	data[0] = location;
	UtilStringToHex(myPan, 16, &data[1]);

#ifdef _DEBUG
	(void) outLen;
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, keySize == 8?M_PIN:M_PIN_3DES, sizeof(data), data, 8, &outLen, ePinBlock))
		return false;
#endif

	return true;
}


/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : SecurityPINBlockWithVariant
**
** DESCRIPTION:	Get the PIN Block after varying KPP to a KPE
**
** PARAMETERS:	appName		<=	The application name
**				location	<=	The key location index
**				keySize		<=	The key size (8 = single DES, 16 = double length 3-DES key)
**				pan			<=	The PAN as an ASCII string
**				stan		<=	The transaction stan number - First part of the variant.
**				amount		<=	The transaction amount - Second part of the variant.
**				ePinBlock	=>	The PIN block will be stored here if successful.
**
** RETURNS:		TRUE if successful
**				FALSE if not
**-------------------------------------------------------------------------------------------
*/
bool SecurityPINBlockWithVariant(char * appName, uchar location, uchar keySize, char * pan, char * stan, char * amount, uchar * ePinBlock)
{
	uchar data[25];
	unsigned short outLen;
	char myPan[20];
	int index, j;

	// Make sure the application owns the key. Note that the key type is checked by the script itself
	if (SecurityAppName(appName, location, keySize, false) == false)
		return false;

	// Only get the last 12 digits of the pan excluding the last check digit.
	for (j = 0, index = strlen(pan) - 13; index < (int) (strlen(pan)-1); j++, index++)
	{
		if (index < 0) myPan[j] = '0';
		else myPan[j] = pan[index];
	}
	myPan[j] = '\0';

	// Setup the input parameters
	memset(data, 0, sizeof(data));
	data[0] = location;
	UtilStringToHex(myPan, 16, &data[1]);
	UtilStringToHex(stan, 6, &data[9]);
	UtilStringToHex(amount, 16, &data[17]);

#ifdef _DEBUG
	(void) outLen;
	{
		int i;
		uchar block[16];
		uchar stanamount_block_a[16];
		uchar stanamount_block_b[16];
		uchar * pin = "\x04\x12\x34\xFF\xFF\xFF\xFF\xFF";

		fseek(fpKeys, location * 8, SEEK_SET);
		fread(block, 1, keySize, fpKeys);

		memcpy(stanamount_block_a, &data[9], 16);
		memcpy(stanamount_block_b, &data[9], 16);

		// OWF, find the MAC (IV set to ZEROS)
		Des3Encrypt(block, stanamount_block_a, 16);
		for (i = 0; i < 8; i++)
			stanamount_block_b[i] ^= stanamount_block_a[i+8];
		Des3Encrypt(block, stanamount_block_b, 16);

		for (i = 0; i < 16; i++)
			stanamount_block_b[i] ^= data[9+i];

		// Vary the PIN with the PAN
		for (i = 0; i < 8; i++)
			ePinBlock[i] = data[i+1] ^ pin[i];

		// Now encrypt to get the pin block
		Des3Encrypt(stanamount_block_b, ePinBlock, 8);
	}
#else
	// Perform the instruction
	if (iPS_ExecuteScript( C_SCRIPT_ID, keySize == 8?M_PINV:M_PINV_3DES, sizeof(data), data, 8, &outLen, ePinBlock))
		return false;
#endif

	return true;
}
