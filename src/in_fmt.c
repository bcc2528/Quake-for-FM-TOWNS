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
// in_fmt.c -- for FM TOWNS

#include "quakedef.h"

#include <MOS.H>

cvar_t m_filter ={"m_filter", "1"};

char	mwork[4096];
int	mx, my;
float	mouse_x, mouse_y;
float	old_mouse_x, old_mouse_y;
qboolean mouse_avail;

void IN_Init (void)
{
	mx = 0;
	my = 0;

	Cvar_RegisterVariable (&m_filter);

	mouse_avail = true;

	if (mouse_avail)
	{
		MOS_start( mwork, 4096 );
		MOS_resolution( 0, 12 );
	}
}

void IN_Shutdown (void)
{
	if (mouse_avail)
	{
		MOS_end();
	}
}

void IN_Commands (void)
{
	if (mouse_avail)
	{
		int button;
		int gx, gy;

		gx = vid.width / 2;
		gy = vid.height / 2;

		MOS_rdpos( &button, &mx, &my );
		MOS_setpos(gx,gy);
		mx -= gx;
		my -= gy;

		button &= 3;

		switch(button)
		{
			case 0:
				Key_Event (K_MOUSE1, false);
				Key_Event (K_MOUSE2, false);
				break;
			case 1:
				Key_Event (K_MOUSE1, true);
				Key_Event (K_MOUSE2, false);
				break;
			case 2:
				Key_Event (K_MOUSE1, false);
				Key_Event (K_MOUSE2, true);
				break;
			default:
				Key_Event (K_MOUSE1, true);
				Key_Event (K_MOUSE2, true);
				break;
		}
	}
}

void IN_MouseMove (usercmd_t *cmd)
{
	if (!mouse_avail)
		return;

	if (m_filter.value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}
	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	
	if (in_mlook.state & 1)
		V_StopPitchDrift ();
		
	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove(cmd);
}

