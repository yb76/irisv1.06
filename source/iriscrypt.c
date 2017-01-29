//
//-----------------------------------------------------------------------------
// PROJECT:			iRIS
//
// FILE NAME:       iriscrypt.c
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

//
// Local include files
//
#include "alloc.h"
#include "security.h"
#include "utility.h"
#include "iris.h"
#include "irisfunc.h"

//
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
//
static const char * trueResult = "TRUE";

//
//-----------------------------------------------------------------------------
// Module variable definitions and initialisations.
//-----------------------------------------------------------------------------
//
uchar iris_ktk2[17] = "\xDF\x45\xC7\x34\x8F\x51\x76\x6D\x9D\x20\xE5\x2C\x0D\x67\x40\xA7";


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// CRYPTOGRAPHY FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DES_STORE
//
// DESCRIPTION:	Stores a DES or 3DES key depending on the key size
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __des_store(void)
{
	uchar * hex;
	bool result = false;

	char * data = IRIS_StackGet(0);
	char * keySize = IRIS_StackGet(1);
	char * key = IRIS_StackGet(2);

	if (data && key && keySize)
	{
		int size = strlen(data) / 2 + 1;

		// Change the ASCII data to hex digits.
		hex = my_malloc(size);
		size = UtilStringToHex(data, strlen(data), hex);

		// Perform the key storage
		result = SecurityWriteKey(currentObjectGroup, (uchar) atoi(key), (uchar) atoi(keySize), hex);

		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(4);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DES_ERASE
//
// DESCRIPTION:	Erases a DES or 3DES key depending on the key size
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __des_erase(void)
{
	bool result = false;

	char * keySize = IRIS_StackGet(0);
	char * key = IRIS_StackGet(1);

	if (key && keySize)
		result = SecurityEraseKey(currentObjectGroup, (uchar) atoi(key), (uchar) atoi(keySize));

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(3);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DES_RANDOM
//
// DESCRIPTION:	Generate a random DES or 3DES key depending on the key size
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __des_random(void)
{
	bool result = false;

	char * keySize = IRIS_StackGet(0);
	char * key = IRIS_StackGet(1);

	if (key && keySize)
		result = SecurityGenerateKey(currentObjectGroup, (uchar) atoi(key), (uchar) atoi(keySize));

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(3);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DES_COPY, ()DES_MOVE
//
// DESCRIPTION:	Move or copy a DES or 3DES key depending on the key size from one location to another
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __des_transfer(void)
{
	bool result = false;

	char * to = IRIS_StackGet(0);
	char * keySize = IRIS_StackGet(1);
	char * key = IRIS_StackGet(2);
	char * func = IRIS_StackGet(3);

	if (to && keySize && key && func)
	{
		if (strcmp(func, "()DES_COPY") == 0)
			result = SecurityCopyKey(currentObjectGroup, (uchar) atoi(key), (uchar) atoi(keySize), (uchar) atoi(to));
		else
			result = SecurityMoveKey(currentObjectGroup, (uchar) atoi(key), (uchar) atoi(keySize), (uchar) atoi(to));
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(4);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DES_XOR
//
// DESCRIPTION:	XOR a DES or 3DES key depending on the key size with another key of the same size
//				then stores the result in another location
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __des_xor(void)
{
	bool result = false;

	char * to = IRIS_StackGet(0);
	char * with = IRIS_StackGet(1);
	char * keySize = IRIS_StackGet(2);
	char * key = IRIS_StackGet(3);

	if (to && with && keySize && key)
		result = SecurityXorKey(currentObjectGroup, (uchar) atoi(key), (uchar) atoi(keySize), (uchar) atoi(with), (uchar) atoi(to));

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(5);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()IV_SET
//
// DESCRIPTION:	Sets the IV for the next Crypt or MAC operation
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __iv_set(void)
{
	bool result = false;

	uchar * hex;
	char * iv = IRIS_StackGet(0);

	if (iv)
	{
		int size = strlen(iv) / 2 + 1;

		// Change the ASCII data to hex digits.
		hex = my_malloc(size);
		size = UtilStringToHex(iv, strlen(iv), hex);

		result = SecuritySetIV(hex);

		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(2);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()IV_CLR
//
// DESCRIPTION:	Clears the IV for the next Crypt or MAC operation
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __iv_clr(void)
{
	bool result = SecuritySetIV(NULL);

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(1);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()ENC / ()DEC / )OFB
//
// DESCRIPTION:	Perform encryption
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __crypt(void)
{
	uchar * hex;
	char * string = NULL;

	char * data = IRIS_StackGet(0);
	char * keySize = IRIS_StackGet(1);
	char * key = IRIS_StackGet(2);
	char * variant = IRIS_StackGet(3);
	char * func = IRIS_StackGet(3);

	if (func && func[0] != '(')
		func = IRIS_StackGet(4);

	if (data && key && variant && func)
	{
		int size = strlen(data) / 2 + 1;
		if (size/8*8 != size) size = size/8*8+8;

		// Change the ASCII data to hex digits.
		hex = my_calloc(size);
		size = UtilStringToHex(data, strlen(data), hex);

		// Perform the encryption
		if (strcmp(func, "()ENCV") == 0 || strcmp(func, "()DECV") == 0)
		{
			uchar hexVariant[2];

			// Change the ASCII variant to hex digits.
			UtilStringToHex(variant, sizeof(hexVariant) * 2, hexVariant);

			SecurityCryptWithVariant(currentObjectGroup, (uchar) atoi(key), (uchar) ((keySize && keySize[0] == '8')?8:16), size, hex, strlen(variant) == 4?hexVariant:NULL, (bool) (strcmp(func,"()ENCV")?true:false));
		}
		else
			SecurityCrypt(currentObjectGroup, (uchar) atoi(key), (uchar) ((keySize && keySize[0] == '8')?8:16), size, hex, (bool) (strcmp(func,"()ENC")?true:false), (bool) (strcmp(func,"()OFB")?false:true));

		// Change back to ASCII data
		string = my_malloc(size*2 + 1);
		UtilHexToString(hex, size, string);
		my_free(hex);

		// Lose the function name and parameters since they are no longer needed
		IRIS_StackPop(4);
		if (strcmp(func, "()ENCV") == 0 || strcmp(func, "()DECV") == 0)
			IRIS_StackPop(1);

		// Push the result onto the stack
		IRIS_StackPush(string);
		my_free(string);
	}
	else
	{
		// Send the data back as is if an error occurs
		UtilStrDup(&string, data);
		IRIS_StackPop(4);
		if (func && (strcmp(func, "()ENCV") == 0 || strcmp(func, "()DECV") == 0))
			IRIS_StackPop(1);
		IRIS_StackPush(string);
		UtilStrDup(&string, NULL);
	}
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()MAC
//
// DESCRIPTION:	Perform MAC calculation
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __mac(void)
{
	uchar * hex;
	uchar hexVariant[2];
	uchar hexMAC[8];
	char mac[17];

	char * data = IRIS_StackGet(0);
	char * variant = IRIS_StackGet(1);
	char * key = IRIS_StackGet(2);

	strcpy(mac, "0000000000000000");

	if (data && variant && key)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex digits.
		hex = my_malloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Change the ASCII variant to hex digits.
		UtilStringToHex(variant, sizeof(hexVariant), hexVariant);

		// Perform the MAC calculation
		SecurityMAB(currentObjectGroup, (uchar) atoi(key), 16, size, hex, strlen(variant) == 4?hexVariant:NULL, hexMAC);
		my_free(hex);

		// Clear the last 4 bytes since this is a MAC
		hexMAC[4] = hexMAC[5] = hexMAC[6] = hexMAC[7] = 0;

		// Change back to ASCII data
		UtilHexToString(hexMAC, sizeof(hexMAC), mac);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(4);

	// Push the result onto the stack and clean up
	IRIS_StackPush(mac);
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()KVC
//
// DESCRIPTION:	Perform KVC Check
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __kvc(void)
{
	uchar hexKVC[3];
	char kvc[7];

	char * keySize = IRIS_StackGet(0);
	char * key = IRIS_StackGet(1);

	strcpy(kvc, "000000");

	if (key)
	{
		// Perform the encryption
		if (SecurityKVCKey(currentObjectGroup, (uchar) atoi(key), (uchar) ((keySize && atoi(keySize) == 8)?8:16), hexKVC))
			// Change to ASCII hex data
			UtilHexToString(hexKVC, sizeof(hexKVC), kvc);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(3);

	// Push the result onto the stack
	IRIS_StackPush(kvc);
}


//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()OWF
//
// DESCRIPTION:	Perform One Way Function
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __owf(void)
{
	bool result = false;
	char * ppasn = IRIS_StackGet(0);
	char * to = IRIS_StackGet(1);
	char * from = IRIS_StackGet(2);
	char * func = IRIS_StackGet(3);

	// Perform the encryption
	if (ppasn && to && from && func)
		result = SecurityOWFKey(currentObjectGroup, (uchar) atoi(from), 16, (uchar) atoi(to), (uchar) atoi(ppasn), (bool) (strcmp(func,"()OWF") == 0?false:true));

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(4);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()OWF_WITH_DATA
//
// DESCRIPTION:	Perform One Way Function with the supplied data.
//				This works with DEK and DDK keys only.
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __owf_with_data(void)
{
	bool result = false;
	char * variant = IRIS_StackGet(0);
	char * keySize = IRIS_StackGet(1);
	char * to = IRIS_StackGet(2);
	char * from = IRIS_StackGet(3);

	// Perform the encryption
	if (variant && keySize && to && from)
	{
		uchar * hex;
		int size = strlen(variant) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(variant, strlen(variant), hex);

		result = SecurityOWFKeyWithData(currentObjectGroup, (uchar) atoi(from), (uchar) (keySize[0] == '8'?8:16), (uchar) atoi(to), hex);
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(5);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DERIVE_3DES, ()DERIVE_DES
//
// DESCRIPTION:	Perform 3DES key derivation
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __derive_key(void)
{
	uchar * hex;
	uchar hexVariant[2];
	bool result = false;

	char * data = IRIS_StackGet(0);
	char * variant = IRIS_StackGet(1);
	char * key = IRIS_StackGet(2);
	char * appName = IRIS_StackGet(3);
	char * kek = IRIS_StackGet(4);
	char * func = IRIS_StackGet(5);

/*	{
		char keycode;
		char tempBuf[200];

		sprintf(tempBuf, "DERIVE: %X %X %X === ", func, kek, appName);
		write_at(tempBuf, strlen(tempBuf), 1, 2);
		while(read(STDIN, &keycode, 1) != 1);

		sprintf(tempBuf, "DERIVE: %X %X %X === ", key, variant, data);
		write_at(tempBuf, strlen(tempBuf), 1, 2);
		while(read(STDIN, &keycode, 1) != 1);
	}
*/
	if (data && variant && key  && appName && kek && func)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Change the ASCII variant to hex digits.
		UtilStringToHex(variant, sizeof(hexVariant) * 2, hexVariant);

		// The application name of the new key must match the current group, or the current group must be iRIS
		if (appName[0] == '\0' || strcmp(appName, currentObjectGroup) == 0 || strcmp(irisGroup, currentObjectGroup) == 0)
			result = SecurityInjectKey(currentObjectGroup, (uchar) atoi(kek), 16, appName[0]?appName:currentObjectGroup, (uchar) atoi(key), (uchar) (strcmp(func,"()DERIVE_DES") == 0?8:16), hex, strlen(variant) == 4?hexVariant:NULL);

		// Clean up
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(6);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()3DES_RSA_STORE
//
// DESCRIPTION:	Perform RSA Storage
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __3des_rsa_store(void)
{
	uchar * hex;
	bool result = false;

	char * data = IRIS_StackGet(0);		// Format:	FIRST BYTE =  modulus length in bytes (i.e. 2 = 16 bits, 0 = 2096 bits, 128 = 1024 bits).
										//			SECOND BYTE = exponent length in bytes (i.e. 2 = 16 bits, 0 = 2096 bits, 128 = 1024 bits).
										//			then..........modulus bytes.
										//				..........exponent bytes.
										// If RSA location (ie rsa) is 0 to 3, then modulus length has maximum value of 252 bytes (= 2016 bits) and exponent length has maximum of 3 bytes (= 24 bits).
	char * rsa = IRIS_StackGet(1);
	char * appName = IRIS_StackGet(2);
	char * kek = IRIS_StackGet(3);

	if (kek && appName && data && rsa)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Perform RSA storage
		// The application name of the new key must match the current group, or the current group must be iRIS
		if (appName[0] == '\0' || strcmp(appName, currentObjectGroup) == 0 || strcmp(irisGroup, currentObjectGroup) == 0)
			result = Security3DESWriteRSA(currentObjectGroup, (uchar) atoi(kek), appName[0]?appName:currentObjectGroup, (uchar) atoi(rsa), size, hex);

		// Clean up
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(5);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()RSA_STORE
//
// DESCRIPTION:	Perform RSA Storage
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __rsa_store(void)
{
	uchar * hex;
	bool result = false;

	char * data = IRIS_StackGet(0);		// Format:	FIRST BYTE =  modulus length in bytes (i.e. 2 = 16 bits, 0 = 2096 bits, 128 = 1024 bits).
										//			SECOND BYTE = exponent length in bytes (i.e. 2 = 16 bits, 0 = 2096 bits, 128 = 1024 bits).
										//			then..........modulus bytes.
										//				..........exponent bytes.
										// If RSA location (ie rsa) is 0 to 3, then modulus length has maximum value of 252 bytes (= 2016 bits) and exponent length has maximum of 3 bytes (= 24 bits).
	char * rsa = IRIS_StackGet(1);

	if (data && rsa)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Perform RSA storage
		result = SecurityWriteRSA(currentObjectGroup, (uchar) atoi(rsa), size, hex);

		// Clean up
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(3);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()RSA_STORE
//
// DESCRIPTION:	Perform RSA Storage
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __rsa_clear(void)
{
	bool result = false;

	char * rsa = IRIS_StackGet(0);

	// Perform RSA Clear
	if (rsa)
		result = SecurityClearRSA(currentObjectGroup, (uchar) atoi(rsa));

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(2);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()RSA_CRYPT
//
// DESCRIPTION:	Perform an RSA cryption
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __rsa_crypt(void)
{
	uchar * hex;
	char * string = NULL;

	char * data = IRIS_StackGet(0);
	char * rsa = IRIS_StackGet(1);

	if (data && rsa)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_calloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Perform RSA storage
		SecurityCryptRSA(currentObjectGroup, (uchar) atoi(rsa), size, hex);

		// Change back to ASCII data
		string = my_malloc(size*2 + 1);
		UtilHexToString(hex, size, string);
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(3);

	// Push the result onto the stack
	IRIS_StackPush(string);
	UtilStrDup(&string, NULL);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()RSA_DERIVE_RSA
//
// DESCRIPTION:	Perform RSA 3DES key derivation
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __rsa_derive_rsa(void)
{
	uchar * hex;
	bool result = false;

	char * data = IRIS_StackGet(0);
	char * rsa2 = IRIS_StackGet(1);
	char * appName = IRIS_StackGet(2);
	char * rsa = IRIS_StackGet(3);

	if (data && rsa2 && appName && rsa)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// The application name of the new key must match the current group, or the current grop must be iRIS
		if (appName[0] == '\0' || strcmp(appName, currentObjectGroup) == 0 || strcmp(irisGroup, currentObjectGroup) == 0)
			result = SecurityRSAInjectRSA(currentObjectGroup, (uchar) atoi(rsa), appName[0]?appName:currentObjectGroup, (uchar) atoi(rsa2), size, hex);

		// Clean up
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(5);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()RSA_DERIVE_3DES
//
// DESCRIPTION:	Perform RSA 3DES key derivation
//
// PARAMETERS:	None
//
// RETURNS:		
//-------------------------------------------------------------------------------------------
//
void __rsa_derive_3des(void)
{
	uchar * hex;
	bool result = false;

	char * data = IRIS_StackGet(0);
	char * key = IRIS_StackGet(1);
	char * appName = IRIS_StackGet(2);
	char * rsa = IRIS_StackGet(3);

	if (data && key && appName && rsa)
	{
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_malloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// The application name of the new key must match the current group, or the current group must be iRIS
		if (appName[0] == '\0' || strcmp(appName, currentObjectGroup) == 0 || strcmp(irisGroup, currentObjectGroup) == 0)
			result = SecurityRSAInjectKey(currentObjectGroup, (uchar) atoi(rsa), appName[0]?appName:currentObjectGroup, (uchar) atoi(key), 16, size, hex);

		// Clean up
		my_free(hex);
	}

	// Lose the function name and parameters since they are no longer needed
	IRIS_StackPop(5);

	IRIS_StackPush((char *) (result?trueResult:NULL));
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()PINBLOCK
//
// DESCRIPTION:	Returns the pinblock of the last pin entered
//
// PARAMETERS:	None
//
// RETURNS:		The number formatted as an amount
//-------------------------------------------------------------------------------------------
//
void __pinblock(void)
{
	uchar result[8];
	char string[17];
	char * key = IRIS_StackGet(0);
	char * track2 = IRIS_StackGet(1);
	char * pan = IRIS_StackGet(2);
	char * amount = IRIS_StackGet(3);
	char * stan = IRIS_StackGet(4);

	char myPan[30];

	strcpy(string, "0000000000000000");

	if (key && (track2 || pan) && amount && stan)
	{
		// Get the PAN
		if (track2 && track2[0])
		{
			IRIS_StackPush(NULL);		// Dummy name...It could have been ()SUBSTRING to be elegant.
			IRIS_StackPush("=|-1|");
			IRIS_StackPush(track2);
			__substring();
			strcpy(myPan, IRIS_StackGet(0));
			IRIS_StackPop(1);
		}
		else if (pan)
			strcpy(myPan, pan);

		SecurityPINBlockWithVariant(currentObjectGroup, (uchar) atoi(key), 16, myPan, stan, amount, result);

		UtilHexToString(result, sizeof(result), string);
	}

	IRIS_StackPop(6);
	IRIS_StackPush(string);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()DFORMAT1
//
// DESCRIPTION:	Decode a payload received from DataWire
//
// PARAMETERS:	None
//
// RETURNS:		The decoded payload
//-------------------------------------------------------------------------------------------
//
void __dformat1(void)
{
	char * data = IRIS_StackGet(0);
	char * blockSize = IRIS_StackGet(1);
	char * string = NULL;

	if (data && blockSize)
	{
		int i;
		uchar * hex;
		uchar * block;
		int int_blockSize = atol(blockSize);
		int size = strlen(data) / 2;
		int padCount;
		signed short checksum = 0;

		// Change the ASCII data to hex (binary).
		hex = my_calloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);
		padCount = int_blockSize - size - 5;

		block = my_malloc(int_blockSize);

		if (padCount)
			block[0] = 0x03;			// Block format 3 and padding included.
		else
			block[0] = 0x02;
		block[1] = 0x00;			// Always ZERO.
		block[2] = int_blockSize/8;	// Number of 8-byte blocks of the modulus or block size.
									// Skip the checksum bytes data[3] and data[4]
		memcpy(&block[int_blockSize - padCount - size], hex, size);
		my_free(hex);

		if (padCount)
		{
			for (i = int_blockSize - padCount; i < int_blockSize; i++)
				block[i] = 0x55;
			block[i-1] = padCount;
		}

		for (i = 5; i < int_blockSize; i++)
			checksum = (checksum << 1) + ((checksum & 0x8000)?1:0) + block[i];

		block[3] = checksum >> 8;
		block[4] = checksum & 0xFF;

		// Change back to ASCII data
		string = my_malloc(int_blockSize*2 + 1);
		UtilHexToString(block, int_blockSize, string);
		my_free(block);
	}

	IRIS_StackPop(3);
	IRIS_StackPush(string);
	my_free(string);
}

//
//-------------------------------------------------------------------------------------------
// FUNCTION   : ()ADJ_ODD_PARITY
//
// DESCRIPTION:	Adjust the buffer by setting bytes to have odd parity.
//
// PARAMETERS:	None
//
// RETURNS:		The decoded payload
//-------------------------------------------------------------------------------------------
//
void __adj_odd_parity(void)
{
	char * data = IRIS_StackGet(0);
	char * string = NULL;

	if (data)
	{
		int i;
		uchar * hex;
		int size = strlen(data) / 2;

		// Change the ASCII data to hex (binary).
		hex = my_calloc(size + 1);
		size = UtilStringToHex(data, strlen(data), hex);

		// Fill the block with data
		for (i = 0; i < size; i++)
		{
			int j;
			int bitSet = 0;

			for (j = 0x80; j; j >>= 1)
			{
				if (hex[i] & j)
					bitSet++;
			}

			if ((bitSet & 0x01) == 0)
				hex[i] ^= 0x01;
		}

		// Change back to ASCII data
		string = my_malloc(size*2 + 1);
		UtilHexToString(hex, size, string);
		my_free(hex);
	}

	IRIS_StackPop(2);
	IRIS_StackPush(string);
	my_free(string);
}
