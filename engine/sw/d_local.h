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
// d_local.h:  private rasterization driver defs

#include "r_shared.h"

//
// TODO: fine-tune this; it's based on providing some overage even if there
// is a 2k-wide scan, with subdivision every 8, for 256 spans of 12 bytes each
//
#define SCANBUFFERPAD		0x1000

#define R_SKY_SMASK	0x007F0000
#define R_SKY_TMASK	0x007F0000

#define DS_SPAN_LIST_END	-128

#define SURFCACHE_SIZE_AT_320X200	600*1024

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	unsigned			width;
	unsigned			height;		// DEBUG only needed for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	int					bytesperpix;
	int fcache;
	qbyte				data[4];	// width*height elements
} surfcache_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct sspan_s
{
	int				u, v, count;
} sspan_t;

extern cvar_t	d_subdiv16;

extern float	scale_for_mip;

extern qboolean		d_roverwrapped;
extern surfcache_t	*sc_rover;
extern surfcache_t	*d_initial_rover;

extern float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
extern float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
extern float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

fixed16_t	sadjust, tadjust;
fixed16_t	bbextents, bbextentt;


void D_DrawSpans8_Smooth (espan_t *pspan);
void D_DrawSpans8 (espan_t *pspans);
void D_DrawSpans16 (espan_t *pspans);
void D_DrawSpans8SubDiv16 (espan_t *pspans);
void D_DrawSpans16From8 (espan_t *pspan);	//skybox in 16 bit renderer (fixme - load tgas with more colourdepth).
void D_DrawZSpans (espan_t *pspans);
void Turbulent8 (espan_t *pspan);
void Turbulent16 (espan_t *pspan);
void Turbulent32 (espan_t *pspan);
void D_SpriteDrawSpans (sspan_t *pspan);

void D_DrawSkyScans8 (espan_t *pspan);
void D_DrawSkyScans16 (espan_t *pspan);
void D_DrawSkyScans32 (espan_t *pspan);

void R_ShowSubDiv (void);
void (*prealspandrawer)(void);
surfcache_t	*D_CacheSurface (msurface_t *surface, int miplevel);

extern int D_MipLevelForScale (float scale);

#if id386
extern void D_PolysetAff8Start (void);
extern void D_PolysetAff8End (void);
#endif

extern short *d_pzbuffer;
extern unsigned int d_zrowbytes, d_zwidth;

extern int	*d_pscantable;
extern int	d_scantable[MAXHEIGHT];

extern int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern int	d_y_aspect_shift, d_pix_min, d_pix_max, d_pix_shift;

extern pixel_t	*d_viewbuffer;

extern short	*zspantable[MAXHEIGHT];

extern int		d_minmip;
extern float	d_scalemip[3];

extern void (*d_drawspans) (espan_t *pspan);

#ifdef PEXT_TRANS
#define PAL555_SIZE 32*32*32
#define TRANS_LEVELS 65
#define TRANS_MAX (TRANS_LEVELS - 1)

#define TRANS_UPPER_CAP (TRANS_MAX / (TRANS_LEVELS + 0.0))
#define TRANS_LOWER_CAP (1.0 / TRANS_LEVELS)

#ifdef _fastcall
#define FASTCALL _fastcall
#else
#define FASTCALL
#endif

void D_InitTrans(void);
// void Set_TransLevelI(int level);
void D_SetTransLevel(float level, blendmode_t blend);
extern qbyte FASTCALL Trans(qbyte p, qbyte p2);
extern qbyte FASTCALL AddBlend(qbyte p, qbyte p2);

extern qbyte *pal555to8;
#endif
