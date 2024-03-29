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
//
// vid_fmt.c: FM TOWNS-specific video routines
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <msdos.cf>
#include <EGB.H>

char work[EgbWorkSize] ;

#include "quakedef.h"
#include "d_local.h"
#include "vid_fmt.h"

int			vid_modenum;
vmode_t		*pcurrentmode = NULL;
int			vid_testingmode, vid_realmode;
double		vid_testendtime;

cvar_t		vid_mode = {"vid_mode","0", false};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","1", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2","1", true};
cvar_t		_windowed_mouse = {"_windowed_mouse","0", true};
cvar_t		vid_fullscreen_mode = {"vid_fullscreen_mode","3", true};
cvar_t		vid_windowed_mode = {"vid_windowed_mode","0", true};
cvar_t		block_switch = {"block_switch","0", true};
cvar_t		vid_window_x = {"vid_window_x", "0", true};
cvar_t		vid_window_y = {"vid_window_y", "0", true};

int	d_con_indirect = 0;

int		numvidmodes;
vmode_t	*pvidmodes;

static int	firstupdate = 1;

void VID_TestMode_f (void);
void VID_NumModes_f (void);
void VID_DescribeCurrentMode_f (void);
void VID_DescribeMode_f (void);
void VID_DescribeModes_f (void);
qboolean VID_CheckAdequateMem (int width, int height);

byte	vid_current_palette[768];	// save for mode changes

static qboolean	nomodecheck = false;

unsigned short	d_8to16table[256];	// not used in 8 bpp mode
unsigned		d_8to24table[256];	// not used in 8 bpp mode

void VID_MenuDraw (void);
void VID_MenuKey (int key);

#define NUMVIDMODES	(sizeof(fmtvidmodes) / sizeof(fmtvidmodes[0]))

static byte	backingbuf[48*24];

static byte		*vid_surfcache;
static int		vid_surfcachesize;
static int		VID_highhunkmark;


/*
================
VID_AllocBuffers
================
*/
qboolean VID_AllocBuffers (int width, int height)
{
	int		tsize, tbuffersize;

	//tbuffersize = framebuffer + z-buffer
	tbuffersize = (width * (height + 1)) +
			(width * height * sizeof (*d_pzbuffer));

	tsize = D_SurfaceCacheForRes (width, height);

	tbuffersize += tsize;

	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
	}

	VID_highhunkmark = Hunk_HighMark ();

	d_pzbuffer = Hunk_HighAllocName (tbuffersize, "video");

	vid_surfcache = (byte *)d_pzbuffer
		+ width * height * sizeof (*d_pzbuffer);
	
	vid.buffer = (void *)( (byte *)vid_surfcache + vid_surfcachesize);
	vid.conbuffer = vid.buffer;
	vid.direct = vid.conbuffer;

	return true;
}

/*
================
VID_Init
================
*/
void    VID_Init (unsigned char *palette)
{
	int		i;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&vid_fullscreen_mode);
	Cvar_RegisterVariable (&vid_windowed_mode);
	Cvar_RegisterVariable (&block_switch);

	Cmd_AddCommand ("vid_testmode", VID_TestMode_f);
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	/*if (!COM_CheckParm ("-stdvid"))
		VID_InitExtra ();*/

// link together all the FM TOWNS graphics modes
	for (i=0 ; i<(NUMVIDMODES - 1) ; i++)
	{
		fmtvidmodes[i].pnext = &fmtvidmodes[i+1];
	}

// add the FM TOWNS graphics modes at the start of the mode list
	fmtvidmodes[NUMVIDMODES-1].pnext = pvidmodes;
	pvidmodes = &fmtvidmodes[0];

	numvidmodes += NUMVIDMODES;

	vid_testingmode = 0;

	vid_modenum = vid_mode.value;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	EGB_init( work, EgbWorkSize );

	VID_SetMode (vid_modenum, palette);

	vid_realmode = vid_modenum;
	
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
}


/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	vmode_t	*pv;

	pv = pvidmodes;
	if (!pv)
		Sys_Error ("VID_GetModePtr: empty vid mode list");

	while (modenum--)
	{
		pv = pv->pnext;
		if (!pv)
			Sys_Error ("VID_GetModePtr: corrupt vid mode list");
	}

	return pv;
}

/*
================
VID_NumModes
================
*/
int VID_NumModes ()
{
	return (numvidmodes);
}


/*
================
VID_ModeInfo
================
*/
char *VID_ModeInfo (int modenum, char **ppheader)
{
	static char	*badmodestr = "Bad mode number";
	vmode_t		*pv;

	pv = VID_GetModePtr (modenum);

	if (!pv)
	{
		if (ppheader)
			*ppheader = NULL;
		return badmodestr;
	}
	else
	{
		if (ppheader)
			*ppheader = pv->header;
		return pv->name;
	}
}


/*
================
VID_SetMode 
================
*/
int VID_SetMode (int modenum, unsigned char *palette)
{
	int		stat = 1;
	vmode_t	*pnewmode, *poldmode;

	if ((modenum >= numvidmodes) || (modenum < 0))
	{
		Cvar_SetValue ("vid_mode", (float)vid_modenum);

		nomodecheck = true;
		Con_Printf ("No such video mode: %d\n", modenum);
		nomodecheck = false;

		if (pcurrentmode == NULL)
		{
			modenum = 0;	// mode hasn't been set yet, so initialize to base
					//  mode since they gave us an invalid initial mode
		}
		else
		{
			return 0;
		}
	}

	pnewmode = VID_GetModePtr (modenum);

	if (pnewmode == pcurrentmode)
		return 1;	// already in the desired mode

// initialize the new mode
	poldmode = pcurrentmode;
	pcurrentmode = pnewmode;

	vid.width = pcurrentmode->width;
	vid.height = pcurrentmode->height;
	vid.conwidth = pcurrentmode->width;
	vid.conheight = pcurrentmode->height;
	vid.aspect = pcurrentmode->aspect;
	vid.numpages = pcurrentmode->numpages;
	vid.rowbytes = pcurrentmode->rowbytes;
	vid.conrowbytes = pcurrentmode->rowbytes;

	//Screen set Mode12(640*480,256 Colors, 1 Page only)
	EGB_resolution( work, 0, 12 );

	//Zoom setting(320*240=2, 640*480=1)
	EGB_displayStart( work, 2, pcurrentmode->zoom, pcurrentmode->zoom );
	EGB_displayStart( work, 0, 0, 0 );

	EGB_displayPage( work, 0, 3 );
	EGB_writePage( work, 0 );

	EGB_color( work, 0, 0x00 );
	EGB_clearScreen( work );

	if (stat < 1)
	{
		if (stat == 0)
		{
		// real, hard failure that requires resetting the mode
			if (!VID_SetMode (vid_modenum, palette))	// restore prior mode
				Sys_Error ("VID_SetMode: Unable to set any mode, probably "
						   "because there's not enough memory available");
			Con_Printf ("Failed to set mode %d\n", modenum);
			return 0;
		}
		else if (stat == -1)
		{
		// not enough memory; just put things back the way they were
			pcurrentmode = poldmode;
			vid.width = pcurrentmode->width;
			vid.height = pcurrentmode->height;
			vid.conwidth = pcurrentmode->width;
			vid.conheight = pcurrentmode->height;
			vid.aspect = pcurrentmode->aspect;
			vid.rowbytes = pcurrentmode->rowbytes;
			vid.conrowbytes = pcurrentmode->rowbytes;
			return 0;
		}
		else
		{
			Sys_Error ("VID_SetMode: invalid setmode return code %d");
		}
	}

	VID_SetPalette (palette);

	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	if(!VID_AllocBuffers (vid.width, vid.height))
	{
		return false;
	}

	D_InitCaches (vid_surfcache, vid_surfcachesize);

	nomodecheck = true;
	Con_Printf ("%s\n", VID_ModeInfo (vid_modenum, NULL));
	nomodecheck = false;

	vid.recalc_refdef = 1;

	return 1;
}


/*
================
VID_SetPalette
================
*/
void    VID_SetPalette (unsigned char *palette)
{
	if(vid_current_palette != palette)
	{
		memcpy(vid_current_palette, palette, 768);
	}

	for (int i=0;i<256;i++)
	{
		_outb( 0xfd90, i );
		_outb( 0xfd94, *palette++ );
		_outb( 0xfd96, *palette++ );
		_outb( 0xfd92, *palette++ );
	}
}


/*
================
VID_ShiftPalette
================
*/
void    VID_ShiftPalette (unsigned char *palette)
{

	VID_SetPalette (palette);
}


/*
================
VID_Shutdown
================
*/
void VID_Shutdown (void)
{
	EGB_init( work, EgbWorkSize );
}


/*
================
VID_Update
================
*/
void    VID_Update (vrect_t *rects)
{
	char    para[16];

	if (firstupdate && _vid_default_mode.value)
	{
		if(_vid_default_mode.value >= numvidmodes)
			Cvar_SetValue ("_vid_default_mode", 0);

		firstupdate = 0;
		Cvar_SetValue ("vid_mode", _vid_default_mode.value);
	}

	DWORD( para+0  ) = vid.buffer;
	WORD( para+4  ) = getds();
	WORD( para+6  ) = 0;
	WORD( para+8  ) = 0;
	WORD( para+10 ) = rects->width - 1;
	WORD( para+12 ) = rects->height - 1;
	EGB_putBlock( work, 0, para );

	rects = rects->pnext;

	if (!nomodecheck)
	{
		if (vid_testingmode)
		{
			if (realtime >= vid_testendtime)
			{
				VID_SetMode (vid_realmode, vid_current_palette);
				vid_testingmode = 0;
			}
		}
		else
		{
			if (vid_mode.value != vid_realmode)
			{
				VID_SetMode ((int)vid_mode.value, vid_current_palette);
				Cvar_SetValue ("vid_mode", (float)vid_modenum);
									// so if mode set fails, we don't keep on
									//  trying to set that mode
				vid_realmode = vid_modenum;
			}
		}
	}
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{
	int		nummodes;

	nummodes = VID_NumModes ();
	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", VID_NumModes ());
	else
		Con_Printf ("%d video modes are available\n", VID_NumModes ());
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_ModeInfo (vid_modenum, NULL));
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int		modenum;
	
	modenum = Q_atoi (Cmd_Argv(1));

	Con_Printf ("%s\n", VID_ModeInfo (modenum, NULL));
}


/*
================
VID_CheckAdequateMem
================
*/
qboolean VID_CheckAdequateMem (int width, int height)
{
	int		tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);

	tbuffersize += D_SurfaceCacheForRes (width, height);

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
	if ((host_parms.memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
		 0x10000 * 3) < minimum_memory)
	{
		return false;		// not enough memory for mode
	}

	return true;
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, nummodes;
	char		*pinfo, *pheader;
	vmode_t		*pv;
	qboolean	na;

	na = false;

	nummodes = VID_NumModes ();
	for (i=0 ; i<nummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_ModeInfo (i, &pheader);
		if (pheader)
			Con_Printf ("\n%s\n", pheader);

		if (VID_CheckAdequateMem (pv->width, pv->height))
		{
			Con_Printf ("%2d: %s\n", i, pinfo);
		}
		else
		{
			Con_Printf ("**: %s\n", pinfo);
			na = true;
		}
	}

	if (na)
	{
		Con_Printf ("\n[**: not enough system RAM for mode]\n");
	}
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo, *pheader;
	vmode_t		*pv;

	pv = VID_GetModePtr (mode);
	pinfo = VID_ModeInfo (mode, &pheader);

	if (VID_CheckAdequateMem (pv->width, pv->height))
	{
		return pinfo;
	}
	else
	{
		return NULL;
	}
}


/*
=================
VID_TestMode_f
=================
*/
void VID_TestMode_f (void)
{
	int		modenum;
	double	testduration;

	if (!vid_testingmode)
	{
		modenum = Q_atoi (Cmd_Argv(1));

		if (VID_SetMode (modenum, vid_current_palette))
		{
			vid_testingmode = 1;
			testduration = Q_atof (Cmd_Argv(2));
			if (testduration == 0)
				testduration = 5.0;
			vid_testendtime = realtime + testduration;
		}
	}
}


/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	int		i, j;

	if (!vid.direct || !pcurrentmode)
		return;

	if ((width > 24) || (height > 24) || (width < 1) || (height < 1))
		return;

	if (width & 0x03)
		return;

	if (vid.aspect > 1.5)
	{
		for (i=0 ; i < (height << 1) ; i += 2)
		{
			for (j=0 ; j < 2 ; j++)
			{
				memcpy (&backingbuf[(i + j) * 24],
						vid.buffer + x + ((y << 1) + i + j) *
						vid.rowbytes,
						width);
				memcpy (vid.buffer + x + ((y << 1) + i + j) *
						vid.rowbytes,
						&pbitmap[(i >> 1) * width],
						width);
			}
		}
	}
	else
	{
		for (i=0 ; i < height ; i++)
		{
			memcpy (&backingbuf[i * 24],
				vid.buffer + x + (y + i) *
				vid.rowbytes,
				width);
			memcpy (vid.buffer + x + (y + i) *
				vid.rowbytes,
				&pbitmap[i * width],
				width);
		}
	}


}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
	int		i, j;

	if (!vid.direct || !pcurrentmode)
		return;

	if ((width > 24) || (height > 24) || (width < 1) || (height < 1))
		return;

	if ((width & 0x03) || (height & 0x03))
		return;

	if (vid.aspect > 1.5)
	{
		for (i=0 ; i<(height << 1) ; i += 2)
		{
			for (j=0 ; j < 2 ; j++)
			{
				memcpy (vid.buffer + x + ((y << 1) + i + j) *
						vid.rowbytes,
						&backingbuf[(i + j) * 24],
						width);
			}
		}
	}
	else
	{
		for (i=0 ; i < height ; i++)
		{
			memcpy (vid.buffer + x + (y + i) *
					vid.rowbytes,
					&backingbuf[i * 24],
					width);
		}
	}
}


//===========================================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes, vid_column_size;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE	11

#define MAX_MODEDESCS	(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			nummodes, i, j, column, row, dup;
	char		temp[100];

	vid_wmodes = 0;
	nummodes = VID_NumModes ();
	
	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i=0 ; i<nummodes ; i++)
	{
		if (vid_wmodes < MAX_MODEDESCS)
		{
			if (i != 1)
			{
				ptr = VID_GetModeDescription (i);

				if (ptr)
				{
					dup = 0;

					for (j=0 ; j<vid_wmodes ; j++)
					{
						if (!strcmp (modedescs[j].desc, ptr))
						{
							if (modedescs[j].modenum != 0)
							{
								modedescs[j].modenum = i;
								dup = 1;

								if (i == vid_modenum)
									modedescs[j].iscur = 1;
							}
							else
							{
								dup = 1;
							}

							break;
						}
					}

					if (!dup)
					{
						modedescs[vid_wmodes].modenum = i;
						modedescs[vid_wmodes].desc = ptr;
						modedescs[vid_wmodes].iscur = 0;

						if (i == vid_modenum)
							modedescs[vid_wmodes].iscur = 1;

						vid_wmodes++;
					}
				}
			}
		}
	}

	vid_column_size = (vid_wmodes + 2) / 3;

	column = 16;
	row = 36;

	for (i=0 ; i<vid_wmodes ; i++)
	{
		if (modedescs[i].iscur)
			M_PrintWhite (column, row, modedescs[i].desc);
		else
			M_Print (column, row, modedescs[i].desc);

		row += 8;

		if ((i % vid_column_size) == (vid_column_size - 1))
		{
			column += 13*8;
			row = 36;
		}
	}

// line cursor
	if (vid_testingmode)
	{
		sprintf (temp, "TESTING %s",
				modedescs[vid_line].desc);
		M_Print (13*8, 36 + MAX_COLUMN_SIZE * 8 + 8*4, temp);
		M_Print (9*8, 36 + MAX_COLUMN_SIZE * 8 + 8*6,
				"Please wait 5 seconds...");
	}
	else
	{
		M_Print (9*8, 36 + MAX_COLUMN_SIZE * 8 + 8,
				"Press Enter to set mode");
		M_Print (6*8, 36 + MAX_COLUMN_SIZE * 8 + 8*3,
				"T to test mode for 5 seconds");
		ptr = VID_GetModeDescription (vid_modenum);
		sprintf (temp, "D to make %s the default", ptr);
		M_Print (6*8, 36 + MAX_COLUMN_SIZE * 8 + 8*5, temp);
		ptr = VID_GetModeDescription ((int)_vid_default_mode.value);

		if (ptr)
		{
			sprintf (temp, "Current default is %s", ptr);
			M_Print (7*8, 36 + MAX_COLUMN_SIZE * 8 + 8*6, temp);
		}

		M_Print (15*8, 36 + MAX_COLUMN_SIZE * 8 + 8*8,
				"Esc to exit");

		row = 36 + (vid_line % vid_column_size) * 8;
		column = 8 + (vid_line / vid_column_size) * 13*8;

		M_DrawCharacter (column, row, 12+((int)(realtime*4)&1));
	}
}


/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	if (vid_testingmode)
		return;

	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line--;

		if (vid_line < 0)
			vid_line = vid_wmodes - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line++;

		if (vid_line >= vid_wmodes)
			vid_line = 0;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line -= vid_column_size;

		if (vid_line < 0)
		{
			vid_line += ((vid_wmodes + (vid_column_size - 1)) /
					vid_column_size) * vid_column_size;

			while (vid_line >= vid_wmodes)
				vid_line -= vid_column_size;
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line += vid_column_size;

		if (vid_line >= vid_wmodes)
		{
			vid_line -= ((vid_wmodes + (vid_column_size - 1)) /
					vid_column_size) * vid_column_size;

			while (vid_line < 0)
				vid_line += vid_column_size;
		}
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu1.wav");
		VID_SetMode (modedescs[vid_line].modenum, vid_current_palette);
		break;

	case 'T':
	case 't':
		S_LocalSound ("misc/menu1.wav");
		if (VID_SetMode (modedescs[vid_line].modenum, vid_current_palette))
		{
			vid_testingmode = 1;
			vid_testendtime = realtime + 5.0;
		}
		break;

	case 'D':
	case 'd':
		S_LocalSound ("misc/menu1.wav");
		firstupdate = 0;
		Cvar_SetValue ("_vid_default_mode", vid_modenum);
		break;

	default:
		break;
	}
}
