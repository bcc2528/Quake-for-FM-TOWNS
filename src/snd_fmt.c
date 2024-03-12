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

#include <snd.h>

#include "quakedef.h"
#include "pdrv_lib.h"

/*
===============================================================================
FM TOWNS High res PCM SUPPORT

===============================================================================
*/

static int pdrvInitFlag=0;
static int sndPos=0;
unsigned char *pcm_buffer=0;
unsigned char *sndPtr;

#define SND_RATE 11025
#define BUFFER_SIZE 1024


static void *loadpcm(int length,void *pcmBuf)
{
	char *sndBuf = (char *)pcmBuf;
	int length2 = length*2;
	_move(sndPtr,sndBuf,length2);
	sndPos = sndPtr - pcm_buffer;
	sndPtr+=length2;
	if(sndPtr >= (pcm_buffer + (length2 * 4))) sndPtr = pcm_buffer;
	return sndBuf;
}


/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
Returns true and fills in the "shm" structure with information for the mixer.
==================
*/
qboolean SNDDMA_Init(void)
{
	int ret;
	int para;
	int mute=0;

	shm = 0;

	pcm_buffer = malloc(BUFFER_SIZE * 8);
	if(pcm_buffer == NULL)
	{
		return false;
	}
	memset(pcm_buffer, 0, sizeof(pcm_buffer));

	SND_get_elevol_mute(&mute);
	SND_elevol_mute((mute|3));		//PCM mute off

	ret=PDRV_init();
	if(ret)
	{
		pdrvInitFlag = 0;
		free(pcm_buffer);
		return false;
	}

	pdrvInitFlag=1;

	shm = &sn;

	para = 0;
	shm->channels = 2;
	PDRV_config(PDRV_INF_CHANNEL,&para);	// Stereo
	para = 1;
	shm->samplebits = 16;
	PDRV_config(PDRV_INF_BIT,&para);	// 16bit
	para = SND_RATE;
	shm->speed = SND_RATE;
	PDRV_config(PDRV_INF_FREQ,&para);	// 11.025KHz
	para=63;
	PDRV_config(PDRV_INF_VOL,&para);	// vol=63(MAX)
	para = BUFFER_SIZE;
	PDRV_config(PDRV_INF_BUFLEN,&para);	// buffer size(MAX)
	para = (int)loadpcm;
	PDRV_config(PDRV_INF_PROC,&para);	// Š„‚èž‚Ýƒ‹[ƒ`ƒ““o˜^

	shm->soundalive = true;
	shm->splitbuffer = false;

	shm->samples = (BUFFER_SIZE * 8) / (shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = pcm_buffer;
	sndPtr = pcm_buffer;

	PDRV_start(0);

	return true;
}


/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples, not stereo)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	if(pdrvInitFlag)
	{
		shm->samplepos = sndPos;
		return shm->samplepos;
	}

	return 0;
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	if(pdrvInitFlag)
	{
		free(pcm_buffer);
		PDRV_stop();
		PDRV_end();
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

