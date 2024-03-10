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
// vid_fmt.h: header file for FM TOWNS video stuff

typedef struct vmode_s {
	struct vmode_s	*pnext;
	char		*name;
	char		*header;
	unsigned	width;
	unsigned	height;
	float		aspect;
	unsigned	rowbytes;
	int		planar;
	int		numpages;
	int		zoom;
} vmode_t;

vmode_t	fmtvidmodes[] = {
{
	NULL,
	"320x240", "    *****    Low res mode    *****    ",
	320, 240, (240.0/320.0)*(320.0/240.0), 320, 0, 1, 2
},
{
	NULL,
	"640x480", "    *****    High res mode   *****    ",
	640, 480, (480.0/640.0)*(320.0/240.0), 640, 0, 1, 1
},
};

// vid_wait settings
#define VID_WAIT_NONE			0
#define VID_WAIT_VSYNC			1
#define VID_WAIT_DISPLAY_ENABLE	2

extern int		numvidmodes;
extern vmode_t	*pvidmodes;

extern cvar_t	vid_wait;
extern cvar_t	vid_nopageflip;
extern cvar_t	_vid_wait_override;

extern unsigned char colormap256[32][256];

