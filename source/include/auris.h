#ifndef __AURIS_H
#define __AURIS_H

/*
**-----------------------------------------------------------------------------
** PROJECT:         AURIS
**
** FILE NAME:       auris.h
**
** DATE CREATED:    10 July 2007
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:     This contains general constants and type definitions used
**					throughout the project
**-----------------------------------------------------------------------------
*/

//
//-----------------------------------------------------------------------------
// Constant Definitions.
//-----------------------------------------------------------------------------
//
#define	false					0
#define	true					1

//
//-----------------------------------------------------------------------------
// Type Definitions
//-----------------------------------------------------------------------------
//
typedef	unsigned char	uchar;
typedef	unsigned int	uint;
typedef	unsigned long	ulong;
typedef	unsigned char	bool;


extern int debug;

#ifdef _DEBUG
	#define	_remove	remove
	#define STDIN 1
	write_at(char * tempBuf, int length, int q, int w);
	read(int x, char * y, int z);
	void DispInit(void);

	int dir_get_first(char * filename);
	int dir_get_next(char * filename);

	int get_env(char * var, char * value, int size);
	int put_env(char * var, char * value, int size);

	void wait_event();

	void set_backlight(int mode);
	void DispGraphics2(uchar * graphics, int width, int height);

	int SVC_SHUTDOWN(void);
	void SVC_INFO_MODELNO(char * model);
	void SVC_INFO_SERLNO(char * serialNumber);
	void SVC_WAIT(unsigned long milliseconds);

	#define	FH_RDONLY				"rb"
	#define	FH_NEW					"wb"
	#define	FH_RDWR					"r+b"
	#define FH_NEW_RDWR				"w+b"

	#define	open(fn,attr)			fopen(fn, attr)
	#define	close(a)				fclose(a)

	#define	write(h,d,s)			fwrite(d,s,1,h)
	#define	read(h,d,s)				fread(d,s,1,h)
	#define	lseek(h,p,f)			fseek(h,p,f)

	#define	FH_OK(h)				(h != NULL)
	#define	FH_ERR(h)				(h == NULL)

	#define	FILE_HANDLE				FILE *

#else

	#define	FH_RDONLY				O_RDONLY
	#define	FH_NEW					(O_CREAT | O_TRUNC | O_WRONLY | O_APPEND)
	#define	FH_RDWR					O_RDWR
	#define FH_NEW_RDWR				(O_CREAT | O_TRUNC | O_RDWR)

	#define	FH_OK(h)				(h != -1)
	#define	FH_ERR(h)				(h == -1)

	#define	_delete_(h,c,x,y,z)		delete_(h,c)
	#define	_insert(h,d,c,x,y,z)	insert(h,d,c)

	#define printf(a,...)

	#define	FILE_HANDLE				int

#endif

#endif /* __AURIS_H */
