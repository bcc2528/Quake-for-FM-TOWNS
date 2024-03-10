/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <conio.h>

////High C Lib for FM TOWNS Keyboard scan
#include <fmcfrb.h>

#include <his.h>

#include "quakedef.h"

#define MINIMUM_FMT_MEMORY			0x800000
#define MINIMUM_FMT_MEMORY_LEVELPAK	(MINIMUM_FMT_MEMORY + 0x100000)

#define STDOUT	1

quakeparms_t parms;

static double		curtime = 0.0;
static double		lastcurtime = 0.0;
static double		oldtime = 0.0;

qboolean		isDedicated;

float				fptest_temp;

#define stackSize 1000
char EGB_stack[ stackSize ];

#define VSYNCclear 0x05ca
#define VSYNCintNumber 11
static unsigned int ticcount;

void VSYNChandler( void )
{
	ticcount++;

	/******** ‚u‚r‚x‚m‚bŠ„‚èž‚ÝŒ´ˆöƒNƒŠƒAƒŒƒWƒXƒ^‚Ö‚Ì‘‚«ž‚Ý ********/
	_outb( VSYNCclear, 0 );
}

void Sys_SetFPCW (void)
{
}

void Sys_PushFPCW_SetHigh (void)
{
}

void Sys_PopFPCW (void)
{
}

void MaskExceptions (void)
{
}

//=============================================================================

byte	scantokey[128] = {	0,K_ESCAPE,'1','2','3','4','5','6',	  /* 00`07 */
				'7','8','9','0','-','^','@',K_BACKSPACE,  /* 08`0F */
				K_TAB,'q','w','e','r','t','y','u',    /* 10`17 */
				'i','o','p',0,'[',K_ENTER,'a','s', 	  /* 18`1F */
				'd','f','g','h','j','k','l',';',	  /* 20`27 */
				':',']','z','x','c','v','b','n',	  /* 28`2F */
				'm',',','.','/','\\',' ','*','/',	/* 30`37 */
				'+','-','7','8','9','=','4','5',/* 38`3F */
				'6',0,'1','2','3',K_ENTER,'0','.',	/* 40`47 */
				0,0,0,0,0,K_UPARROW,0,K_LEFTARROW,	/* 48`4F */
				K_DOWNARROW,K_RIGHTARROW,K_CTRL,K_SHIFT,0,0,0,0,	/* 50`57 */
				0,0,0,K_F12,K_ALT,K_F1,K_F2,K_F3,		/* 58`5F */
				K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,0,	  /* 60`67 */
				K_SHIFT,K_F11,0,0,0,0,0,0,	  /* 68`6F */
				0,0,0,0,0,0,0,0,	 	  /* 70`77 */
				0,0,0,0,K_ESCAPE,0,0,0		  /* 78`7F */
};

#define SC_UPARROW              0x48
#define SC_DOWNARROW    0x50
#define SC_LEFTARROW            0x4b
#define SC_RIGHTARROW   0x4d
#define SC_LEFTSHIFT   0x2a
#define SC_RIGHTSHIFT   0x36
#define SC_RIGHTARROW   0x4d

void MaskExceptions (void);
void Sys_InitFloatTime (void);
void Sys_PushFPCW_SetHigh (void);
void Sys_PopFPCW (void);

#define LEAVE_FOR_CACHE (512*1024)		//FIXME: tune
#define LOCKED_FOR_MALLOC (128*1024)	//FIXME: tune


/*
============
Sys_FileTime

returns -1 if not present
============
*/
int	Sys_FileTime (char *path)
{
	struct	stat	buf;
	
	if (stat (path,&buf) == -1)
		return -1;
	
	return buf.st_mtime;
}

void Sys_mkdir (char *path)
{
	mkdir (path);
}


void Sys_Sleep(void)
{
}


char *Sys_ConsoleInput(void)
{
	static char	text[256];
	static int	len = 0;
	char		ch;

	if (!isDedicated)
		return NULL;

	if (! kbhit())
		return NULL;

	ch = getche();

	switch (ch)
	{
		case '\r':
			putch('\n');
			if (len)
			{
				text[len] = 0;
				len = 0;
				return text;
			}
			break;

		case '\b':
			putch(' ');
			if (len)
			{
				len--;
				putch('\b');
			}
			break;

		default:
			text[len] = ch;
			len = (len + 1) & 0xff;
			break;
	}

	return NULL;
}

void Sys_Init(void)
{

	MaskExceptions ();

	Sys_SetFPCW ();

	//Keyboard interface initialize for scan mode.
	KYB_init();
	KYB_setcode( 0x4000 );
	KYB_clic ( 1 );

	//Timer set
	ticcount = 0;
	HIS_stackArea( EGB_stack , stackSize );
	HIS_setHandler( VSYNCintNumber , VSYNChandler );
	HIS_enableInterrupt( VSYNCintNumber );

	Sys_InitFloatTime ();
}

void Sys_Shutdown(void)
{
	KYB_clrbuf();
	KYB_init();
	KYB_clic ( 1 );

	HIS_detachHandler( VSYNCintNumber );

	VID_Shutdown();

	free(parms.membase);
}


#define SC_RSHIFT       0x36 
#define SC_LSHIFT       0x2a 
void Sys_SendKeyEvents (void)
{
	int outkey;
	unsigned int encode;

// get key events

	while(1)
	{
		outkey = KYB_read( 1, &encode );

		if ( (outkey >> 8 ) == 0xff )
		{
			break;
		}
		else if( (outkey >> 7 ) == 0)
		{
			Key_Event (scantokey[outkey & 127], true);
		}
		else
		{
			Key_Event (scantokey[outkey & 127], false);
		}
	}
}


// =======================================================================
// General routines
// =======================================================================

/*
================
Sys_Printf
================
*/

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	
	va_start (argptr,fmt);
	vsprintf (text,fmt,argptr);
	va_end (argptr);

	if (cls.state == ca_dedicated)
		fprintf(stderr, "%s", text);
}

void Sys_AtExit (void)
{

// shutdown only once (so Sys_Error can call this function to shutdown, then
// print the error message, then call exit without exit calling this function
// again)
	Sys_Shutdown();
}


void Sys_Quit (void)
{
	byte	screen[80*25*2];
	byte	*d;
	char			ver[6];
	int			i;
	

// load the sell screen before shuting everything down
	if (registered.value)
		d = COM_LoadHunkFile ("end2.bin"); 
	else
		d = COM_LoadHunkFile ("end1.bin"); 
	if (d)
		memcpy (screen, d, sizeof(screen));

// write the version number directly to the end screen
	sprintf (ver, " v%4.2f", VERSION);
	for (i=0 ; i<6 ; i++)
		screen[0*80*2 + 72*2 + i*2] = ver[i];

	Host_Shutdown();

// do the text mode sell screen
	if (d)
	{
		//memcpy ((void *)real2ptr(0xb8000), screen,80*25*2); 
	
	// set text pos

	}
	else
		printf ("couldn't load endscreen.\n");

	exit(0);
}

void Sys_Error (char *error, ...)
{ 
    va_list     argptr;
    char        string[1024];
    
    va_start (argptr,error);
    vsprintf (string,error,argptr);
    va_end (argptr);

	Host_Shutdown();
	fprintf(stderr, "Error: %s\n", string);
// Sys_AtExit is called by exit to shutdown the system
	exit(0);
} 


int Sys_FileOpenRead (char *path, int *handle)
{
	int	h;

	h = open (path, O_RDONLY|O_BINARY, 0666);
	*handle = h;
	if (h == -1)
		return -1;

	return filelength(h);
}

int Sys_FileOpenWrite (char *path)
{
	int     handle;

	umask (0);
	
	handle = open(path,O_RDWR | O_BINARY | O_CREAT | O_TRUNC
	, 0666);

	if (handle == -1)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));

	return handle;
}

void Sys_FileClose (int handle)
{
	close (handle);
}

void Sys_FileSeek (int handle, int position)
{
	lseek (handle, position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
   return read (handle, dest, count);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return write (handle, data, count);
}

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	// it's always writeable
}


/*
================
Sys_FloatTime
================
*/
double Sys_FloatTime (void)
{
	double			ft, time;
	static int		sametimecount;

	Sys_PushFPCW_SetHigh ();

//{static float t = 0; t=t+0.05; return t;}	// DEBUG

	ft = (double) ticcount / 60; // 60 = Vsync
	time = ft - oldtime;
	oldtime = ft;

	if (time < 0)
	{
		if (time > -3000.0)
			time = 0.0;
		else
			time += 3600.0;
	}

	curtime += time;

	if (curtime == lastcurtime)
	{
		sametimecount++;

		if (sametimecount > 100000)
		{
			curtime += 1.0;
			sametimecount = 0;
		}
	}
	else
	{
		sametimecount = 0;
	}

	lastcurtime = curtime;

	Sys_PopFPCW ();

    return curtime;
}


/*
================
Sys_InitFloatTime
================
*/
void Sys_InitFloatTime (void)
{
	int		j;

	Sys_FloatTime ();

	oldtime = curtime;

	j = COM_CheckParm("-starttime");

	if (j)
	{
		curtime = (double) (Q_atof(com_argv[j+1]));
	}
	else
	{
		curtime = 0.0;
	}
	lastcurtime = curtime;
}


/*
================
main
================
*/
int main (int c, char **v)
{
	double			time, oldtime, newtime;
	int j;

	printf ("Quake v%4.2f\n", VERSION);

	if (fptest_temp >= 0.0)
		fptest_temp += 0.1;

	memset(&parms, 0, sizeof(parms));

	COM_InitArgv (c, v);

	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 8*1024*1024;

	j = COM_CheckParm("-mem");
	if (j)
	{
		parms.memsize = (int) (Q_atof(com_argv[j+1]) * 1024 * 1024);
	}
	parms.membase = malloc (parms.memsize);

	parms.basedir = ".";

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

	Sys_Init ();

	atexit (Sys_AtExit);	// in case we crash

	Host_Init(&parms);

	oldtime = Sys_FloatTime ();
	while (1)
	{
		newtime = Sys_FloatTime ();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated && (time<sys_ticrate.value))
			continue;

		Host_Frame (time);

//Sys_StackCheck ();

		oldtime = newtime;
	}

	return 1;
}


