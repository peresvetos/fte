//These are defined later in the source tree. This file should probably be moved to a later spot.
struct progfuncs_s;
struct globalvars_s;
struct texture_s;



#ifdef HALFLIFEMODELS
	#define MAX_BONE_CONTROLLERS 5
#endif

#define FST_BASE 0	//base frames
#define FS_REG 1	//regular frames
#define FS_COUNT 2	//regular frames
typedef struct {
	struct {
		int frame[2];
		float frametime[2];
		float lerpfrac;

#ifdef HALFLIFEMODELS
		float subblendfrac;	//hl models are weird
#endif

		int endbone;
	} g[2];

	float *bonestate;
	int bonecount;

#ifdef HALFLIFEMODELS
	float bonecontrols[MAX_BONE_CONTROLLERS];	//hl special bone controllers
#endif
} framestate_t;




//function prototypes

#if defined(SERVERONLY)
#define qrenderer QR_NONE
#define FNC(n) (n)			//FNC is defined as 'pointer if client build, direct if dedicated server'

#define Mod_SkinForName Mod_SkinNumForName
#define Mod_FrameForName Mod_FrameNumForName
#define Mod_GetFrameDuration Mod_FrameDuration

#else
#define FNC(n) (*n)
extern r_qrenderer_t qrenderer;
extern char *q_renderername;

extern mpic_t	*(*Draw_SafePicFromWad)				(char *name);
extern mpic_t	*(*Draw_SafeCachePic)				(char *path);
extern void	(*Draw_Init)							(void);
extern void	(*Draw_TinyCharacter)					(int x, int y, unsigned int num);
extern void	(*Draw_Crosshair)						(void);
extern void	(*Draw_ScalePic)						(int x, int y, int width, int height, mpic_t *pic);
extern void	(*Draw_SubPic)							(int x, int y, int width, int height, mpic_t *pic, int srcx, int srcy, int srcwidth, int srcheight);
extern void	(*Draw_TransPicTranslate)				(int x, int y, int width, int height, qbyte *image, qbyte *translation);
extern void	(*Draw_ConsoleBackground)				(int firstline, int lastline, qboolean forceopaque);
extern void	(*Draw_EditorBackground)				(void);
extern void	(*Draw_TileClear)						(int x, int y, int w, int h);
extern void	(*Draw_Fill)							(int x, int y, int w, int h, unsigned int c);
extern void	(*Draw_FillRGB)							(int x, int y, int w, int h, float r, float g, float b);
extern void	(*Draw_FadeScreen)						(void);
extern void	(*Draw_BeginDisc)						(void);
extern void	(*Draw_EndDisc)							(void);
extern qboolean (*Draw_IsCached)					(char *picname);	//can be null

extern void	(*Draw_Image)							(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic
extern void	(*Draw_ImageColours)					(float r, float g, float b, float a);
void R2D_FillBlock(int x, int y, int w, int h);
#define Draw_FillBlock R2D_FillBlock

extern void	(*R_Init)								(void);
extern void	(*R_DeInit)								(void);
extern void	(*R_RenderView)							(void);		// must set r_refdef first

extern void	(*R_NewMap)								(void);
extern void	(*R_PreNewMap)							(void);
extern int	(*R_LightPoint)							(vec3_t point);

extern void	(*R_PushDlights)						(void);
extern void	(*R_AddStain)							(vec3_t org, float red, float green, float blue, float radius);
extern void	(*R_LessenStains)						(void);

extern qboolean	(*VID_Init)							(rendererstate_t *info, unsigned char *palette);
extern void	(*VID_DeInit)							(void);
extern void	(*VID_SetPalette)						(unsigned char *palette);
extern void	(*VID_ShiftPalette)						(unsigned char *palette);
extern char *(*VID_GetRGBInfo)						(int prepad, int *truevidwidth, int *truevidheight);
extern void	(*VID_SetWindowCaption)					(char *msg);

extern void SCR_Init								(void);
extern void SCR_DeInit								(void);
extern void (*SCR_UpdateScreen)						(void);
extern void SCR_BeginLoadingPlaque					(void);
extern void SCR_EndLoadingPlaque					(void);
extern void SCR_DrawConsole							(qboolean noback);
extern void SCR_SetUpToDrawConsole					(void);
extern void SCR_EraseCenterString					(void);
extern void SCR_CenterPrint							(int pnum, char *str, qboolean skipgamecode);

void R_DrawTextField(int x, int y, int w, int h, char *text, unsigned int defaultmask, unsigned int fieldflags);
#define CPRINT_BALIGN		(1<<0)	//B
#define CPRINT_TALIGN		(1<<1)	//T
#define CPRINT_LALIGN		(1<<2)	//L
#define CPRINT_RALIGN		(1<<3)	//R
#define CPRINT_BACKGROUND	(1<<4)	//P

#define CPRINT_OBITUARTY	(1<<16)	//O (show at 2/3rds from top)
#define CPRINT_PERSIST		(1<<17)	//P (doesn't time out)
#define CPRINT_TYPEWRITER	(1<<18)	//  (char at a time)

#endif

extern void	FNC(Mod_Init)							(void);
extern void	FNC(Mod_ClearAll)						(void);
extern struct model_s *FNC(Mod_ForName)				(char *name, qboolean crash);
extern struct model_s *FNC(Mod_FindName)			(char *name);
extern void	*FNC(Mod_Extradata)						(struct model_s *mod);	// handles caching
extern void	FNC(Mod_TouchModel)						(char *name);

extern void	FNC(Mod_NowLoadExternal)				(void);

extern void	FNC(Mod_Think)							(void);
extern int FNC(Mod_SkinForName)						(struct model_s *model, char *name);
extern int FNC(Mod_FrameForName)					(struct model_s *model, char *name);
extern float FNC(Mod_GetFrameDuration)				(struct model_s *model, int framenum);

#undef FNC

extern qboolean	Mod_GetTag						(struct model_s *model, int tagnum, framestate_t *framestate, float *transforms);
extern int Mod_TagNumForName					(struct model_s *model, char *name);

int Mod_GetNumBones(struct model_s *model, qboolean allowtags);
int Mod_GetBoneRelations(struct model_s *model, int firstbone, int lastbone, framestate_t *fstate, float *result);
int Mod_GetBoneParent(struct model_s *model, int bonenum);
char *Mod_GetBoneName(struct model_s *model, int bonenum);

void Draw_FunString(int x, int y, const void *str);
void Draw_AltFunString(int x, int y, const void *str);
void Draw_FunStringWidth(int x, int y, const void *str, int width);


#ifdef SERVERONLY
#define Mod_Q1LeafPVS Mod_LeafPVS
// qbyte *Mod_LeafPVS (struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
#endif



typedef struct rendererinfo_s {
	char *description;
	char *name[4];
	r_qrenderer_t rtype;

	mpic_t	*(*Draw_SafePicFromWad)			(char *name);
	mpic_t	*(*Draw_SafeCachePic)		(char *path);
	void	(*Draw_Init)				(void);
	void	(*Draw_Shutdown)			(void);
	void	(*Draw_Crosshair)			(void);
	void	(*Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic);
	void	(*Draw_SubPic)				(int x, int y, int width, int height, mpic_t *pic, int srcx, int srcy, int srcwidth, int srcheight);
	void	(*Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *pic, qbyte *translation);
	void	(*Draw_ConsoleBackground)	(int firstline, int lastline, qboolean forceopaque);
	void	(*Draw_EditorBackground)	(void);
	void	(*Draw_TileClear)			(int x, int y, int w, int h);
	void	(*Draw_Fill)				(int x, int y, int w, int h, unsigned int c);
	void	(*Draw_FillRGB)				(int x, int y, int w, int h, float r, float g, float b);
	void	(*Draw_FadeScreen)			(void);
	void	(*Draw_BeginDisc)			(void);
	void	(*Draw_EndDisc)				(void);

	void	(*Draw_Image)				(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic
	void	(*Draw_ImageColours)		(float r, float g, float b, float a);

	void	(*R_Init)					(void);
	void	(*R_DeInit)					(void);
	void	(*R_RenderView)				(void);		// must set r_refdef first

	void	(*R_NewMap)					(void);
	void	(*R_PreNewMap)				(void);
	int		(*R_LightPoint)				(vec3_t point);

	void	(*R_PushDlights)			(void);
	void	(*R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
	void	(*R_LessenStains)			(void);

	void	(*Mod_Init)					(void);
	void	(*Mod_ClearAll)				(void);
	struct model_s *(*Mod_ForName)		(char *name, qboolean crash);
	struct model_s *(*Mod_FindName)		(char *name);
	void	*(*Mod_Extradata)			(struct model_s *mod);	// handles caching
	void	(*Mod_TouchModel)			(char *name);

	void	(*Mod_NowLoadExternal)		(void);
	void	(*Mod_Think)				(void);
	qboolean (*Mod_GetTag)				(struct model_s *model, int tagnum, framestate_t *fstate, float *result);
	int (*Mod_TagNumForName)			(struct model_s *model, char *name);
	int (*Mod_SkinForName)				(struct model_s *model, char *name);
	int (*Mod_FrameForName)				(struct model_s *model, char *name);
	float (*Mod_GetFrameDuration)		(struct model_s *model, int frame);


	qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
	void	 (*VID_DeInit)				(void);
	void	(*VID_SetPalette)			(unsigned char *palette);
	void	(*VID_ShiftPalette)			(unsigned char *palette);
	char	*(*VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
	void	(*VID_SetWindowCaption)		(char *msg);

	void	(*SCR_UpdateScreen)			(void);

	char *alignment;
} rendererinfo_t;