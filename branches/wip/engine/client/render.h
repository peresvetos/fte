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

// refresh.h -- public interface to refresh functions

// default soldier colors
#define TOP_DEFAULT		1
#define BOTTOM_DEFAULT	6

#define	TOP_RANGE		(TOP_DEFAULT<<4)
#define	BOTTOM_RANGE	(BOTTOM_DEFAULT<<4)

extern int		r_framecount;

struct msurface_s;

typedef union {
	int num;
#ifdef D3DQUAKE
	void *ptr;
#endif
} texid_t;
static const texid_t r_nulltex = {0};
#define TEXVALID(t) (t.num!=0)


#ifdef D3DQUAKE
	#define sizeof_index_t 2
#endif
#if sizeof_index_t == 2
	#define GL_INDEX_TYPE GL_UNSIGNED_SHORT
	#define D3DFMT_QINDEX D3DFMT_INDEX16
	typedef unsigned short index_t;
#else
	#define GL_INDEX_TYPE GL_UNSIGNED_INT
	#define D3DFMT_QINDEX D3DFMT_INDEX32
	typedef unsigned int index_t;
#endif

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct entity_s
{
	int						keynum;			// for matching entities in different frames
	vec3_t					origin;
	vec3_t					angles;
	vec3_t					axis[3];

	vec4_t					shaderRGBAf;
	float					shaderTime;

	vec3_t					oldorigin;
	vec3_t					oldangles;
	
	struct model_s			*model;			// NULL = no model
	int						skinnum;		// for Alias models

	struct player_info_s	*scoreboard;	// identify player

	struct efrag_s			*efrag;			// linked list of efrags (FIXME)
	int						visframe;		// last frame this entity was
											// found in an active leaf
											// only used for static objects
											
	int						dlightframe;	// dynamic lighting
	int						dlightbits;
	
// FIXME: could turn these into a union
	int						trivial_accept;
	struct mnode_s			*topnode;		// for bmodels, first world node
											//  that splits bmodel, or NULL if
											//  not split

	framestate_t			framestate;

	int flags;

	refEntityType_t rtype;
	float rotation;

	struct shader_s *forcedshader;

#ifdef PEXT_SCALE
	float scale;
#endif
#ifdef PEXT_FATNESS
	float fatness;
#endif
#ifdef PEXT_HEXEN2
	int drawflags;
	int abslight;
#endif
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible 
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably always be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	float		fov_x, fov_y;
	
	int			ambientlight;

	int			flags;

	int			currentplayernum;

	float		time;

	qboolean	useperspective;
} refdef_t;

extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

extern	entity_t	r_worldentity;


//gl_alias.c
void R_DrawGAliasModel (entity_t *e, unsigned int rmode);

//r_surf.c
struct model_s;
struct msurface_s;
void Surf_DrawWorld(void);
void Surf_StainSurf(struct msurface_s *surf, float *parms);
void Surf_AddStain(vec3_t org, float red, float green, float blue, float radius);
void Surf_LessenStains(void);
void Surf_WipeStains(void);
void Surf_DeInit(void);
void Surf_BuildLightmaps(void);
void Surf_RenderDynamicLightmaps (struct msurface_s *fa, int shift);
int Surf_LightmapShift (struct model_s *model);
#ifndef LMBLOCK_WIDTH
#define	LMBLOCK_WIDTH		128
#define	LMBLOCK_HEIGHT		128
typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;
typedef unsigned char stmap;
struct mesh_s;
typedef struct {
	struct mesh_s		*meshchain;
	qboolean	modified;
	qboolean	deluxmodified;
	glRect_t	rectchange;
	glRect_t	deluxrectchange;
	int allocated[LMBLOCK_WIDTH];
	qbyte		lightmaps[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
	qbyte		deluxmaps[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//fixme: make seperate structure for easy disabling with less memory usage.
	stmap		stainmaps[3*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//rgb no a. added to lightmap for added (hopefully) speed.
} lightmapinfo_t;
extern lightmapinfo_t **lightmap;
extern int numlightmaps;
extern texid_t		*lightmap_textures;
extern texid_t		*deluxmap_textures;
extern int			lightmap_bytes;		// 1, 3(, or 4)
#endif





#if defined(GLQUAKE)
void GLR_Init (void);
void GLR_ReInit (void);
void GLR_InitTextures (void);
void GLR_InitEfrags (void);
void GLR_RenderView (void);		// must set r_refdef first
								// called whenever r_refdef or vid change

void GLR_AddEfrags (entity_t *ent);
void GLR_RemoveEfrags (entity_t *ent);

void GLR_PreNewMap(void);
void GLR_NewMap (void);

void GLR_PushDlights (void);
void GLR_DrawWaterSurfaces (void);

void MediaGL_ShowFrame8bit(qbyte *framedata, int inwidth, int inheight, qbyte *palette);
void MediaGL_ShowFrameRGBA_32(qbyte *framedata, int inwidth, int inheight);	//top down
void MediaGL_ShowFrameBGR_24_Flip(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...

void GLVID_DeInit (void);
void GLR_DeInit (void);
void GLSCR_DeInit (void);
void GLVID_Console_Resize(void);

int GLR_LightPoint (vec3_t p);
#endif


void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

enum imageflags
{
	/*warning: many of these flags only apply the first time it is requested*/
	IF_CLAMP = 1<<0,
	IF_NOPICMIP = 1<<1,
	IF_NOMIPMAP = 1<<2,
	IF_NOALPHA = 1<<3,
	IF_NOGAMMA = 1<<4
};

enum uploadfmt
{
	TF_INVALID,
	TF_RGBA32,		/*rgba byte order*/
	TF_BGRA32,		/*bgra byte order*/
	TF_RGBX32,		/*rgb byte order, with extra wasted byte after blue*/
	TF_RGB24,		/*bgr byte order, no alpha channel nor pad, and top down*/
	TF_BGR24_FLIP,	/*bgr byte order, no alpha channel nor pad, and bottom up*/
	TF_SOLID8,	/*8bit quake-palette image*/
	TF_TRANS8,	/*8bit quake-palette image, index 255=transparent*/
	TF_TRANS8_FULLBRIGHT,	/*fullbright 8 - fullbright texels have alpha 255, everything else 0*/
	TF_HEIGHT8,	/*image data is greyscale, convert to a normalmap and load that, uploaded alpha contains the original heights*/
	TF_HEIGHT8PAL, /*source data is palette values rather than actual heights, generate a fallback heightmap*/
	TF_H2_T7G1, /*8bit data, odd indexes give greyscale transparence*/
	TF_H2_TRANS8_0,	/*8bit data, 0 is transparent, not 255*/
	TF_H2_T4A4	/*8bit data, weird packing*/
};


#if defined(GLQUAKE) && defined(D3DQUAKE)
	#define R_LoadTexture8Pal32(skinname,width,height,data,palette,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTexture8Pal32(skinname, width, height, data, palette, usemips, alpha):GL_LoadTexture8Pal32(skinname, width, height, data, palette, usemips, alpha))
	#define R_LoadTexture8Pal24(skinname,width,height,data,palette,usemips,alpha) ((qrenderer == QR_DIRECT3D)?D3D_LoadTexture8Pal24(skinname, width, height, data, palette, usemips, alpha):GL_LoadTexture8Pal24(skinname, width, height, data, palette, usemips, alpha))
	
	#define R_FindTexture(name)  ((qrenderer == QR_DIRECT3D)?D3D_FindTexture(name):GL_FindTexture(name))
	#define R_LoadCompressed(name)  ((qrenderer == QR_DIRECT3D)?D3D_LoadCompressed(name):GL_LoadCompressed(name))
#elif defined(D3DQUAKE)
//	#define R_LoadTexture8Pal32
//	#define R_LoadTexture8Pal24

	#define R_FindTexture		D3D_FindTexture
	#define R_LoadCompressed	D3D_LoadCompressed

	#define R_AllocNewTexture	D3D_AllocNewTexture
	#define R_Upload			D3D_UploadFmt
	#define R_LoadTexture		D3D_LoadTextureFmt
	#define R_DestroyTexture(tno)	0
#elif defined(GLQUAKE)
	#define R_LoadTexture8Pal32	GL_LoadTexture8Pal32
	#define R_LoadTexture8Pal24	GL_LoadTexture8Pal24

	#define R_FindTexture		GL_FindTexture
	#define R_LoadCompressed	GL_LoadCompressed

	#define R_AllocNewTexture(w,h) GL_AllocNewTexture()
	#define R_Upload			GL_UploadFmt
	#define R_LoadTexture		GL_LoadTextureFmt
	#define R_DestroyTexture(tno)	0
#endif

#define R_LoadTexture8(id,w,h,d,f,t)		R_LoadTexture(id,w,h,t?TF_TRANS8:TF_SOLID8,d,f)
#define R_LoadTexture32(id,w,h,d,f)		R_LoadTexture(id,w,h,TF_RGBA32,d,f)
#define R_LoadTextureFB(id,w,h,d,f)		R_LoadTexture(id,w,h,TF_TRANS8_FULLBRIGHT,d,f)
#define R_LoadTexture8BumpPal(id,w,h,d,f)	R_LoadTexture(id,w,h,TF_HEIGHT8PAL,d,f)
#define R_LoadTexture8Bump(id,w,h,d,f)	R_LoadTexture(id,w,h,TF_HEIGHT8,d,f)

/*it seems a little excessive to have to include glquake (and windows headers), just to load some textures/shaders for the backend*/
#ifdef GLQUAKE
texid_t GL_AllocNewTexture(void);
void GL_UploadFmt(texid_t tex, char *name, enum uploadfmt fmt, void *data, int width, int height, unsigned int flags);
texid_t GL_LoadTextureFmt (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags);
#endif
#ifdef D3DQUAKE
texid_t D3D_AllocNewTexture(int width, int height);
void D3D_UploadFmt(texid_t tex, char *name, enum uploadfmt fmt, void *data, int width, int height, unsigned int flags);
texid_t D3D_LoadTextureFmt (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags);

texid_t D3D_LoadCompressed(char *name);
texid_t D3D_FindTexture (char *identifier);
#endif

extern int image_width, image_height;
texid_t R_LoadReplacementTexture(char *name, char *subpath, unsigned int flags);
texid_t R_LoadHiResTexture(char *name, char *subpath, unsigned int flags);
texid_t R_LoadBumpmapTexture(char *name, char *subpath);

extern	texid_t	particletexture;
extern	texid_t particlecqtexture;
extern	texid_t explosiontexture;
extern	texid_t balltexture;
extern	texid_t beamtexture;
extern	texid_t ptritexture;

extern	float	r_projection_matrix[16];
extern	float	r_view_matrix[16];
void GL_ParallelPerspective(double xmin, double xmax, double ymax, double ymin, double znear, double zfar);
void GL_InfinatePerspective(double fovx, double fovy, double zNear);

#if defined(GLQUAKE) || defined(D3DQUAKE)

void	RMod_Init (void);
int Mod_TagNumForName(struct model_s *model, char *name);
int Mod_SkinNumForName(struct model_s *model, char *name);
int Mod_FrameNumForName(struct model_s *model, char *name);
float Mod_FrameDuration(struct model_s *model, int frameno);

void	RMod_ClearAll (void);
struct model_s *RMod_ForName (char *name, qboolean crash);
struct model_s *RMod_FindName (char *name);
void	*RMod_Extradata (struct model_s *mod);	// handles caching
void	RMod_TouchModel (char *name);

struct mleaf_s *RMod_PointInLeaf (struct model_s *model, float *p);

void RMod_Think (void);
void RMod_NowLoadExternal(void);
void GLR_LoadSkys (void);
void R_BloomRegister(void);
#endif

#ifdef RUNTIMELIGHTING
void LightFace (int surfnum);
void LightLoadEntities(char *entstring);
#endif


extern struct model_s		*currentmodel;

qboolean Media_ShowFilm(void);
void Media_CaptureDemoEnd(void);
void Media_RecordFrame (void);
qboolean Media_PausedDemo (void);
double Media_TweekCaptureFrameTime(double time);

void MYgluPerspective(double fovx, double fovy, double zNear, double zFar);

qbyte *R_MarkLeaves_Q1 (void);
qbyte *R_CalcVis_Q1 (void);
qbyte *R_MarkLeaves_Q2 (void);
qbyte *R_MarkLeaves_Q3 (void);
void R_SetFrustum (float projmat[16], float viewmat[16]);
void R_SetRenderer(int wanted);
void R_AnimateLight (void);
struct texture_s *R_TextureAnimation (struct texture_s *base);
void RQ_Init(void);

void CLQ2_EntityEvent(entity_state_t *es);
void CLQ2_TeleporterParticles(entity_state_t *es);
void CLQ2_IonripperTrail(vec3_t oldorg, vec3_t neworg);
void CLQ2_TrackerTrail(vec3_t oldorg, vec3_t neworg, int flags);
void CLQ2_Tracker_Shell(vec3_t org);
void CLQ2_TagTrail(vec3_t oldorg, vec3_t neworg, int flags);
void CLQ2_FlagTrail(vec3_t oldorg, vec3_t neworg, int flags);
void CLQ2_TrapParticles(entity_t *ent);
void CLQ2_BfgParticles(entity_t *ent);
struct q2centity_s;
void CLQ2_FlyEffect(struct q2centity_s *ent, vec3_t org);
void CLQ2_DiminishingTrail(vec3_t oldorg, vec3_t neworg, struct q2centity_s *ent, unsigned int effects);
void CLQ2_BlasterTrail2(vec3_t oldorg, vec3_t neworg);

void WritePCXfile (const char *filename, qbyte *data, int width, int height, int rowbytes, qbyte *palette, qboolean upload); //data is 8bit.
qbyte *ReadPCXFile(qbyte *buf, int length, int *width, int *height);
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, int asgrey);
qbyte *ReadJPEGFile(qbyte *infile, int length, int *width, int *height);
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height, const char *name);
qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out);

void BoostGamma(qbyte *rgba, int width, int height);
void SaturateR8G8B8(qbyte *data, int size, float sat);
void AddOcranaLEDsIndexed (qbyte *image, int h, int w);

void Renderer_Init(void);
void R_ShutdownRenderer(void);
void R_RestartRenderer_f (void);//this goes here so we can save some stack when first initing the sw renderer.

//used to live in glquake.h
qbyte GetPaletteIndex(int red, int green, int blue);
extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_drawviewmodelinvis;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadow_realtime_dlight, r_shadow_realtime_dlight_shadows;
extern	cvar_t	r_shadow_realtime_world,r_shadow_realtime_world_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;

#ifdef R_XFLIP
extern cvar_t	r_xflip;
#endif

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_nohwblend;
extern	cvar_t	gl_keeptjunctions;
extern	cvar_t	gl_reporttjunctions;
extern	cvar_t	r_flashblend;
extern	cvar_t	r_lightstylesmooth;
extern	cvar_t	r_lightstylespeed;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_load24bit;
extern	cvar_t	gl_finish;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;

extern cvar_t   d_palconvwrite;
extern cvar_t	d_palremapsize;

extern  cvar_t	r_lightmap_saturation;

enum {
	RSPEED_TOTALREFRESH,
	RSPEED_LINKENTITIES,
	RSPEED_PROTOCOL,
	RSPEED_WORLDNODE,
	RSPEED_WORLD,
	RSPEED_DRAWENTITIES,
	RSPEED_STENCILSHADOWS,
	RSPEED_FULLBRIGHTS,
	RSPEED_DYNAMIC,
	RSPEED_PARTICLES,
	RSPEED_PARTICLESDRAW,
	RSPEED_PALETTEFLASHES,
	RSPEED_2D,
	RSPEED_SERVER,
	RSPEED_FINISH,

	RSPEED_MAX
};
int rspeeds[RSPEED_MAX];

enum {
	RQUANT_MSECS,	//old r_speeds
	RQUANT_EPOLYS,
	RQUANT_WPOLYS,
	RQUANT_SHADOWFACES,
	RQUANT_SHADOWEDGES,
	RQUANT_LITFACES,

	RQUANT_MAX
};
int rquant[RQUANT_MAX];

#define RQuantAdd(type,quant) rquant[type] += quant;

#define RSpeedLocals() int rsp
#define RSpeedMark() int rsp = r_speeds.ival?Sys_DoubleTime()*1000000:0
#define RSpeedRemark() rsp = r_speeds.ival?Sys_DoubleTime()*1000000:0

#if defined(_WIN32) && defined(GLQUAKE)
extern void (_stdcall *qglFinish) (void);
#define RSpeedEnd(spt) do {if(r_speeds.ival && qglFinish){qglFinish(); rspeeds[spt] += (int)(Sys_DoubleTime()*1000000) - rsp;}}while (0)
#else
#define RSpeedEnd(spt) rspeeds[spt] += r_speeds.value?Sys_DoubleTime()*1000000 - rsp:0
#endif