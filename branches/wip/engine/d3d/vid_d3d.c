#include "quakedef.h"
#include "gl_draw.h"
#include "shader.h"

#ifdef D3DQUAKE
#include "winquake.h"
//#include "d3d9quake.h"

#include    "d3d9.h"

//#pragma comment(lib, "../libs/dxsdk9/lib/d3d9.lib")


/*Fixup outdated windows headers*/
#ifndef WM_XBUTTONDOWN
   #define WM_XBUTTONDOWN      0x020B
   #define WM_XBUTTONUP      0x020C
#endif
#ifndef MK_XBUTTON1
   #define MK_XBUTTON1         0x0020
#endif
#ifndef MK_XBUTTON2
   #define MK_XBUTTON2         0x0040
#endif
// copied from DarkPlaces in an attempt to grab more buttons
#ifndef MK_XBUTTON3
   #define MK_XBUTTON3         0x0080
#endif
#ifndef MK_XBUTTON4
   #define MK_XBUTTON4         0x0100
#endif
#ifndef MK_XBUTTON5
   #define MK_XBUTTON5         0x0200
#endif
#ifndef MK_XBUTTON6
   #define MK_XBUTTON6         0x0400
#endif
#ifndef MK_XBUTTON7
   #define MK_XBUTTON7         0x0800
#endif

#ifndef WM_INPUT
	#define WM_INPUT 255
#endif


int gl_bumpmappingpossible;

static void D3D9_GetBufferSize(int *width, int *height);
static LPDIRECT3D9 pD3D;
LPDIRECT3DDEVICE9 pD3DDev9;
//static LPDIRECTDRAWGAMMACONTROL pGammaControl;


static qboolean vid_initializing;

extern qboolean		scr_initialized;                // ready to draw
extern qboolean		scr_drawloading;
extern qboolean		scr_con_forcedraw;

cvar_t vid_hardwaregamma;


//sound/error code needs this
HWND mainwindow;

//input code needs these
int		window_center_x, window_center_y;
RECT		window_rect;
int window_x, window_y;


void BuildGammaTable (float g, float c);
static void	D3D9_VID_GenPaletteTables (unsigned char *palette)
{
	extern unsigned short		ramps[3][256];
	qbyte	*pal;
	unsigned r,g,b;
	unsigned v;
	unsigned short i;
	unsigned	*table;
	extern qbyte gammatable[256];

	if (palette)
	{
		extern cvar_t v_contrast;
		BuildGammaTable(v_gamma.value, v_contrast.value);

		//
		// 8 8 8 encoding
		//
		if (1)//vid_hardwaregamma.value)
		{
		//	don't built in the gamma table

			pal = palette;
			table = d_8to24rgbtable;
			for (i=0 ; i<256 ; i++)
			{
				r = pal[0];
				g = pal[1];
				b = pal[2];
				pal += 3;

		//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
		//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
				v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
				*table++ = v;
			}
			d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
		}
		else
		{
	//computer has no hardware gamma (poor suckers) increase table accordingly

			pal = palette;
			table = d_8to24rgbtable;
			for (i=0 ; i<256 ; i++)
			{
				r = gammatable[pal[0]];
				g = gammatable[pal[1]];
				b = gammatable[pal[2]];
				pal += 3;

		//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
		//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
				v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
				*table++ = v;
			}
			d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
		}

		if (LittleLong(1) != 1)
		{
			for (i=0 ; i<256 ; i++)
				d_8to24rgbtable[i] = LittleLong(d_8to24rgbtable[i]);
		}
	}

	if (pD3DDev9)
		IDirect3DDevice9_SetGammaRamp(pD3DDev9, 0, D3DSGR_NO_CALIBRATION, (D3DGAMMARAMP *)ramps);
}

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
static modestate_t modestate;

static qboolean D3D9AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	if (ActiveApp == fActive && Minimized == minimize)
		return false;	//so windows doesn't crash us over and over again.

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	IN_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);

	if (fActive)
	{
		Cvar_ForceCallback(&v_gamma);
	}
	if (!fActive)
	{
		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.
	}

	return true;
}





static LRESULT WINAPI D3D9_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

    switch (uMsg)
    {
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			GetWindowRect(mainwindow, &window_rect);
			//window_x = (int) LOWORD(lParam);
			//window_y = (int) HIWORD(lParam);
//			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (!vid_initializing)
				IN_TranslateKeyEvent (wParam, lParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
				IN_TranslateKeyEvent (wParam, lParam, false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			if (wParam & MK_XBUTTON3)
				temp |= 32;

			if (wParam & MK_XBUTTON4)
				temp |= 64;

			if (wParam & MK_XBUTTON5)
				temp |= 128;

			if (wParam & MK_XBUTTON6)
				temp |= 256;

			if (wParam & MK_XBUTTON7)
				temp |= 512;

			if (!vid_initializing)
				IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if (!vid_initializing)
			{
				if ((short) HIWORD(wParam) > 0)
				{
					Key_Event(K_MWHEELUP, 0, true);
					Key_Event(K_MWHEELUP, 0, false);
				}
				else
				{
					Key_Event(K_MWHEELDOWN, 0, true);
					Key_Event(K_MWHEELDOWN, 0, false);
				}
			}
			break;

		case WM_INPUT:
			// raw input handling
			IN_RawInput_MouseRead((HANDLE)lParam);
			break;

    	case WM_SIZE:
			if (!vid_initializing)
			{
				GetClientRect(mainwindow, &window_rect);
				// force width/height to be updated
				//vid.pixelwidth = window_rect.right - window_rect.left;
				//vid.pixelheight = window_rect.bottom - window_rect.top;
//				Cvar_ForceCallback(&vid_conautoscale);
//				Cvar_ForceCallback(&vid_conwidth);
			}
            break;

   	    case WM_CLOSE:
			if (!vid_initializing)
				if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				{
					Sys_Quit ();
				}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			if (!D3D9AppActivate(!(fActive == WA_INACTIVE), fMinimized))
				break;//so, urm, tell me microsoft, what changed?
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWNORMAL);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
//			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
//			if (dibwindow)
//				DestroyWindow (dibwindow);
        }
        break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

			
		case WM_MWHOOK:
			if (!vid_initializing)
				MW_Hook_Message (lParam);
			break;
		
    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}

static D3DPRESENT_PARAMETERS d3dpp;
static void resetD3D9(void)
{
	IDirect3DDevice9_Reset(pD3DDev9, &d3dpp);


	/*clear the screen to black as soon as we start up, so there's no lingering framebuffer state*/
	IDirect3DDevice9_BeginScene(pD3DDev9);
	IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	IDirect3DDevice9_EndScene(pD3DDev9);
	IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);



	
	


	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_DITHERENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_SPECULARENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_LIGHTING, FALSE);
}

#if (WINVER < 0x500) && !defined(__GNUC__)
typedef struct tagMONITORINFO
{
    DWORD   cbSize;
    RECT    rcMonitor;
    RECT    rcWork;
    DWORD   dwFlags;
} MONITORINFO, *LPMONITORINFO;
#endif

static void initD3D9(HWND hWnd, rendererstate_t *info)
{
	int i;
	int numadaptors;
	int err;
	RECT rect;
	D3DADAPTER_IDENTIFIER9 inf;
	extern cvar_t _vid_wait_override;

	static HMODULE d3d9dll;
	LPDIRECT3D9 (WINAPI *pDirect3DCreate9) (int version);

	if (!d3d9dll)
		d3d9dll = LoadLibrary("d3d9.dll");
	if (!d3d9dll)
	{
		Con_Printf("Direct3d 9 does not appear to be installed\n");
		return;
	}
	pDirect3DCreate9 = (void*)GetProcAddress(d3d9dll, "Direct3DCreate9");
	if (!pDirect3DCreate9)
	{
		Con_Printf("Direct3d 9 does not appear to be installed properly\n");
		return;
	}

	pD3D = pDirect3DCreate9(D3D_SDK_VERSION);    // create the Direct3D interface
	if (!pD3D)
		return;

	numadaptors = IDirect3D9_GetAdapterCount(pD3D);
	for (i = 0; i < numadaptors; i++)
	{	//try each adaptor in turn until we get one that actually works
		memset(&d3dpp, 0, sizeof(d3dpp));    // clear out the struct for use
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;    // discard old frames
		d3dpp.hDeviceWindow = hWnd;    // set the window to be used by Direct3D
		d3dpp.BackBufferWidth = info->width;
		d3dpp.BackBufferHeight = info->height;
		d3dpp.MultiSampleType = info->multisample;
		d3dpp.BackBufferCount = 3;
		d3dpp.FullScreen_RefreshRateInHz = info->fullscreen?info->rate:0;	//don't pass a rate if not fullscreen, d3d doesn't like it.
		d3dpp.Windowed = !info->fullscreen;

		d3dpp.EnableAutoDepthStencil = true;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		d3dpp.BackBufferFormat = info->fullscreen?D3DFMT_X8R8G8B8:D3DFMT_UNKNOWN;

		if (!*_vid_wait_override.string)
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
		else
		{
			if (_vid_wait_override.value == 1)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			else if (_vid_wait_override.value == 2)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
			else if (_vid_wait_override.value == 3)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_THREE;
			else if (_vid_wait_override.value == 4)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_FOUR;
			else
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}

		memset(&inf, 0, sizeof(inf));
		err = IDirect3D9_GetAdapterIdentifier(pD3D, i, 0, &inf);

		// create a device class using this information and information from the d3dpp stuct
		IDirect3D9_CreateDevice(pD3D, 
				i,
				D3DDEVTYPE_HAL,
				hWnd,
				D3DCREATE_HARDWARE_VERTEXPROCESSING,
				&d3dpp,
				&pD3DDev9);

		if (pD3DDev9)
		{
			HMONITOR hm;
			MONITORINFO mi;
			char *s;
			for (s = inf.Description + strlen(inf.Description)-1; s >= inf.Description && *s <= ' '; s--)
				*s = 0;
			Con_Printf("D3D9: Using device %s\n", inf.Description);

			vid.numpages = d3dpp.BackBufferCount;

			if (d3dpp.Windowed)	//fullscreen we get positioned automagically.
			{					//windowed, we get positioned at 0,0... which is often going to be on the wrong screen
								//the user can figure it out from here
				static HANDLE huser32;
				BOOL (WINAPI *pGetMonitorInfoA)(HMONITOR hMonitor, LPMONITORINFO lpmi);
				if (!huser32)
					huser32 = LoadLibrary("user32.dll");
				if (!huser32)
					return;
				pGetMonitorInfoA = (void*)GetProcAddress(huser32, "GetMonitorInfoA");
				if (!pGetMonitorInfoA)
					return;

				hm = IDirect3D9_GetAdapterMonitor(pD3D, i);
				memset(&mi, 0, sizeof(mi));
				mi.cbSize = sizeof(mi);
				pGetMonitorInfoA(hm, &mi);
				rect.left = rect.top = 0;
				rect.right = d3dpp.BackBufferWidth;
				rect.bottom = d3dpp.BackBufferHeight;
				AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
				MoveWindow(d3dpp.hDeviceWindow, mi.rcWork.left, mi.rcWork.top, rect.right-rect.left, rect.bottom-rect.top, false);
			}
			return;	//successful
		}
	}


	Con_Printf("IDirect3D9_CreateDevice failed\n");


	return;
}

static qboolean D3D9_VID_Init(rendererstate_t *info, unsigned char *palette)
{
	DWORD width = info->width;
	DWORD height = info->height;
	DWORD bpp = info->bpp;
	DWORD zbpp = 16;
	DWORD flags = 0;
	DWORD wstyle;
	RECT rect;
	MSG msg;

	extern cvar_t vid_conwidth;
	extern cvar_t vid_conheight;

	//DDGAMMARAMP gammaramp;
	//int i;

	char *CLASSNAME = "FTED3D9QUAKE";
	WNDCLASS wc = {
		0,
		&D3D9_WindowProc,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		CLASSNAME
	};

	vid_initializing = true;

	RegisterClass(&wc);

	if (info->fullscreen)
		wstyle = 0;
	else
		wstyle = WS_OVERLAPPEDWINDOW;
	
	rect.left = rect.top = 0;
	rect.right = info->width;
	rect.bottom = info->height;
	AdjustWindowRectEx(&rect, wstyle, FALSE, 0);
	mainwindow = CreateWindow(CLASSNAME, "Direct3D", wstyle, 0, 0, rect.right-rect.left, rect.bottom-rect.top, NULL, NULL, NULL, NULL);

	// Try as specified.

	initD3D9(mainwindow, info);
	if (!pD3DDev9)
		return false;



	while (PeekMessage(&msg, NULL,  0, 0, PM_REMOVE)) 
	{ 
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}

	ShowWindow(mainwindow, SW_NORMAL);
	//IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, 0xffffffff, 1, 0);
	//IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);

	IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	IDirect3DDevice9_BeginScene(pD3DDev9);
	IDirect3DDevice9_EndScene(pD3DDev9);
	IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);



//	pD3DX->lpVtbl->GetBufferSize((void*)pD3DX, &width, &height);
	vid.pixelwidth = width;
	vid.pixelheight = height;
	vid.recalc_refdef = true;

	vid.width = vid.conwidth = width;
	vid.height = vid.conheight = height;

//	pDD->lpVtbl->QueryInterface ((void*)pDD, &IID_IDirectDrawGammaControl, (void**)&pGammaControl);
/*	if (pGammaControl)
	{
		for (i = 0; i < 256; i++)
			gammaramp.red[i] = i*2;
		pGammaControl->lpVtbl->SetGammaRamp(pGammaControl, 0, &gammaramp);
	}
	else*/
		Con_Printf("Couldn't get gamma controls\n");

	vid_initializing = false;


resetD3D9();


	
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 0.666*256 );

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, TRUE );


	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_DITHERENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_SPECULARENABLE, FALSE);
	//IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_LIGHTING, FALSE);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, TRUE);

	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);


	GetWindowRect(mainwindow, &window_rect);


	D3D9_VID_GenPaletteTables(palette);

	{
		extern qboolean	mouseactive;
		mouseactive = false;
	}

	{
		extern cvar_t v_contrast;
		void GLV_Gamma_Callback(struct cvar_s *var, char *oldvalue);
		Cvar_Hook(&v_gamma, GLV_Gamma_Callback);
		Cvar_Hook(&v_contrast, GLV_Gamma_Callback);

		Cvar_ForceCallback(&v_gamma);
	}

	return true;
}

/*a new model has been loaded*/
static void	(D3D9_R_NewMap)					(void)
{
	int i;
	r_worldentity.model = cl.worldmodel;
	R_AnimateLight();
	Surf_BuildLightmaps();

	/*wipe any lingering particles*/
	P_ClearParticles();

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
 		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}

extern mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
static void	(D3D9_R_PreNewMap)				(void)
{
	r_viewleaf = NULL;
	r_oldviewleaf = NULL;
	r_viewleaf2 = NULL;
	r_oldviewleaf2 = NULL;
}
static int		(D3D9_R_LightPoint)				(vec3_t point)
{
	return 0;
}

static void	(D3D9_R_PushDlights)			(void)
{
}
static void	(D3D9_R_AddStain)				(vec3_t org, float red, float green, float blue, float radius)
{
}
static void	(D3D9_R_LessenStains)			(void)
{
}

static void	 (D3D9_VID_DeInit)				(void)
{
	/*final shutdown, kill the video stuff*/
	if (pD3DDev9)
	{
		IDirect3DDevice9_Release(pD3DDev9);
		pD3DDev9 = NULL;
	}
	if (pD3D)
	{
		IDirect3D9_Release(pD3D);
		pD3D = NULL;
	}
	if (mainwindow)
	{
		DestroyWindow(mainwindow);
		mainwindow = NULL;
	}
}
static void	(D3D9_VID_LockBuffer)			(void)
{
}
static void	(D3D9_VID_UnlockBuffer)			(void)
{
}
static void	(D3D9_D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height)
{
}
static void	(D3D9_D_EndDirectRect)			(int x, int y, int width, int height)
{
}
static void	(D3D9_VID_ForceLockState)		(int lk)
{
}
static int		(D3D9_VID_ForceUnlockedAndReturnState) (void)
{
	return 0;
}

static void	(D3D9_VID_SetPalette)			(unsigned char *palette)
{
	D3D9_VID_GenPaletteTables(palette);
}
static void	(D3D9_VID_ShiftPalette)			(unsigned char *palette)
{
	D3D9_VID_GenPaletteTables(palette);
}
static char	*(D3D9_VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight)
{
	return NULL;
}
static void	(D3D9_VID_SetWindowCaption)		(char *msg)
{
	SetWindowText(mainwindow, msg);
}

void Matrix4_OrthographicD3D(float *proj, float xmin, float xmax, float ymax, float ymin,
		     float znear, float zfar);
void d3dx_ortho(float *m);

void D3D9_Set2D (void)
{
	float m[16];
	D3DVIEWPORT9 vport;
//	IDirect3DDevice9_EndScene(pD3DDev9);

	Matrix4_OrthographicD3D(m, 0 + (0.5*vid.width/vid.pixelwidth), vid.width + (0.5*vid.width/vid.pixelwidth), 0 + (0.5*vid.height/vid.pixelheight), vid.height + (0.5*vid.height/vid.pixelheight), -100, 100);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)m);

	Matrix4_Identity(m);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_WORLD, (D3DMATRIX*)m);

	Matrix4_Identity(m);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_VIEW, (D3DMATRIX*)m);


	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CCW);

	IDirect3DDevice9_SetSamplerState(pD3DDev9, 0, D3DSAMP_MAGFILTER,  D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(pD3DDev9, 1, D3DSAMP_MAGFILTER,  D3DTEXF_LINEAR);

	IDirect3DDevice9_SetSamplerState(pD3DDev9, 0, D3DSAMP_MIPFILTER,  D3DTEXF_NONE);
	IDirect3DDevice9_SetSamplerState(pD3DDev9, 1, D3DSAMP_MIPFILTER,  D3DTEXF_NONE);

	IDirect3DDevice9_SetSamplerState(pD3DDev9, 0, D3DSAMP_MINFILTER,  D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(pD3DDev9, 1, D3DSAMP_MINFILTER,  D3DTEXF_LINEAR);

	vport.X = 0;
	vport.Y = 0;
	vport.Width = vid.pixelwidth;
	vport.Height = vid.pixelheight;
	vport.MinZ = 0;
	vport.MaxZ = 1;
	IDirect3DDevice9_SetViewport(pD3DDev9, &vport);
}

static int d3d9error(int i)
{
	if (FAILED(i))// != D3D_OK)
		Con_Printf("D3D error: %x  %i\n", i);
	return i;
}

static void	(D3D9_SCR_UpdateScreen)			(void)
{
	extern int keydown[];
	extern cvar_t vid_conheight;
	int uimenu;
#ifdef TEXTEDITOR
	extern qboolean editormodal;
#endif
	qboolean nohud, noworld;
	RSpeedMark();

	switch (IDirect3DDevice9_TestCooperativeLevel(pD3DDev9))
	{
	case D3DERR_DEVICELOST:
		//the user has task switched away from us or something
		return;
	case D3DERR_DEVICENOTRESET:
		resetD3D9();
		if (FAILED(IDirect3DDevice9_TestCooperativeLevel(pD3DDev9)))
			Sys_Error("D3D9 Device lost. Additionally restoration failed.\n");
	//	D3DSucks();
	//	scr_disabled_for_loading = false;

		VID_ShiftPalette (NULL);
		break;
	default:
		break;
	}


	if (keydown['k'])
	{
		d3d9error(IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(rand()&255, rand()&255, rand()&255), 1.0f, 0));
		d3d9error(IDirect3DDevice9_BeginScene(pD3DDev9));
		d3d9error(IDirect3DDevice9_EndScene(pD3DDev9));
		d3d9error(IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL));

		VID_ShiftPalette (NULL);
	}

	if (scr_disabled_for_loading)
	{
		extern float scr_disabled_time;
		if (Sys_DoubleTime() - scr_disabled_time > 60 || key_dest != key_game)
		{
			scr_disabled_for_loading = false;
		}
		else
		{		
			IDirect3DDevice9_BeginScene(pD3DDev9);
			scr_drawloading = true;
			SCR_DrawLoading ();
			scr_drawloading = false;
			IDirect3DDevice9_EndScene(pD3DDev9);
			IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);
			RSpeedEnd(RSPEED_TOTALREFRESH);
			return;
		}
	}

	if (!scr_initialized || !con_initialized)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;                         // not initialized yet
	}

#pragma message("Fixme: remove the code from here...")
	{
		extern cvar_t vid_conwidth, vid_conheight, vid_conautoscale;
		if (vid_conautoscale.value)
		{
			vid.conwidth = vid.pixelwidth*vid_conautoscale.value;
			vid.conheight = vid.pixelheight*vid_conautoscale.value;
		}
		else
		{
			vid.conwidth = vid_conwidth.value;
			vid.conheight = vid_conheight.value;
		}

		if (!vid.conwidth)
			vid.conwidth = vid.pixelwidth;
		if (!vid.conheight)
			vid.conheight = vid.pixelheight;

		if (vid.width != vid.conwidth || vid.height != vid.conheight)
			vid.recalc_refdef = true;
		vid.width = vid.conwidth;
		vid.height = vid.conheight;
	}


#ifdef VM_UI
	uimenu = UI_MenuState();
#else
	uimenu = 0;
#endif

	d3d9error(IDirect3DDevice9_BeginScene(pD3DDev9));
	D3D9_Set2D ();
/*
#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		GLV_UpdatePalette (false, host_frametime);
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		R2D_BrightenScreen();

		if (key_dest == key_console)
			Con_DrawConsole(vid_conheight.value/2, false);
		GL_EndRendering ();	
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}
#endif
*/
	if (Media_ShowFilm())
	{
		M_Draw(0);
//		GLV_UpdatePalette (false, host_frametime);
#if defined(_WIN32)
		Media_RecordFrame();
#endif
//		R2D_BrightenScreen();
		IDirect3DDevice9_EndScene(pD3DDev9);
		IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL);

//		pD3DDev->lpVtbl->BeginScene(pD3DDev);

		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}

	//
	// determine size of refresh window
	//
	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	noworld = false;
	nohud = false;

#ifdef VM_CG
	if (CG_Refresh())
		nohud = true;
	else
#endif
#ifdef CSQC_DAT
		if (cls.state == ca_active && CSQC_DrawView())
		nohud = true;
	else
#endif
		if (uimenu != 1)
		{
			if (r_worldentity.model && cls.state == ca_active)
 				V_RenderView ();
			else
			{
				noworld = true;
			}
		}


	D3D9_Set2D ();

	R2D_BrightenScreen();

	scr_con_forcedraw = false;
	if (noworld)
	{
		if (scr_con_current != vid.height)
			Draw_ConsoleBackground(0, vid.height, true);
		else
			scr_con_forcedraw = true;

		nohud = true;
	}
	else if (!nohud)
		SCR_TileClear ();

	SCR_DrawTwoDimensional(uimenu, nohud);

	GLV_UpdatePalette (false, host_frametime);
#if defined(_WIN32) && defined(GLQUAKE)
	Media_RecordFrame();
#endif

	RSpeedEnd(RSPEED_TOTALREFRESH);
	RSpeedShow();
#pragma message("Fixme: ... to here")


	d3d9error(IDirect3DDevice9_EndScene(pD3DDev9));
	d3d9error(IDirect3DDevice9_Present(pD3DDev9, NULL, NULL, NULL, NULL));

	window_center_x = (window_rect.left + window_rect.right)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;


	IN_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);

	VID_ShiftPalette (NULL);
}





static void	(D3D9_Draw_BeginDisc)			(void)
{
}
static void	(D3D9_Draw_EndDisc)				(void)
{
}


static void	(D3D9_Draw_Init)				(void)
{
	R2D_Init();
}
static void	(D3D9_Draw_ReInit)				(void)
{
}
static void	(D3D9_Draw_Crosshair)			(void)
{
}
static void	(D3D9_Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *pic, qbyte *translation)
{
}
static void	(D3D9_Draw_Fill)				(int x, int y, int w, int h, unsigned int c)
{
}
static void	(D3D9_Draw_FillRGB)				(int x, int y, int w, int h, float r, float g, float b)
{
}
static void	(D3D9_Draw_FadeScreen)			(void)
{
}
static void	(D3D9_Draw_BeginDisc)			(void);
static void	(D3D9_Draw_EndDisc)				(void);

static void	(D3D9_R_Init)					(void)
{
}
static void	(D3D9_R_DeInit)					(void)
{
	Surf_DeInit();
}



static void D3D9_SetupViewPort(void)
{
	extern cvar_t gl_mindist;
	float	screenaspect;
	int		x, x2, y2, y, w, h;

	float fov_x, fov_y;

	D3DVIEWPORT9 vport;

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * vid.pixelwidth/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * vid.pixelwidth/(int)vid.width;
	y = (r_refdef.vrect.y) * vid.pixelheight/(int)vid.height;
	y2 = ((int)(r_refdef.vrect.y + r_refdef.vrect.height)) * vid.pixelheight/(int)vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < vid.pixelwidth)
		x2++;
	if (y < 0)
		y--;
	if (y2 < vid.pixelheight)
		y2++;

	w = x2 - x;
	h = y2 - y;

	vport.X = x;
	vport.Y = y;
	vport.Width = w;
	vport.Height = h;
	vport.MinZ = 0;
	vport.MaxZ = 1;
	IDirect3DDevice9_SetViewport(pD3DDev9, &vport);

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if (r_waterwarp.value<0 && r_viewleaf->contents <= Q1CONTENTS_WATER)
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	GL_InfinatePerspective(fov_x, fov_y, gl_mindist.value);

	Matrix4_ModelViewMatrixFromAxis(r_view_matrix, vpn, vright, vup, r_refdef.vieworg);


	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)r_projection_matrix);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_VIEW, (D3DMATRIX*)r_view_matrix);
}

static void	(D3D9_R_RenderView)				(void)
{
	D3D9_SetupViewPort();
	d3d9error(IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1, 0));
	R_SetFrustum (r_projection_matrix, r_view_matrix);
	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
		Surf_DrawWorld();
	P_DrawParticles ();
}

void	(D3D9_R_NewMap)					(void);
void	(D3D9_R_PreNewMap)				(void);
int		(D3D9_R_LightPoint)				(vec3_t point);

void	(D3D9_R_PushDlights)			(void);
void	(D3D9_R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
void	(D3D9_R_LessenStains)			(void);

qboolean (D3D9_VID_Init)				(rendererstate_t *info, unsigned char *palette);
void	 (D3D9_VID_DeInit)				(void);
void	(D3D9_VID_LockBuffer)			(void);
void	(D3D9_VID_UnlockBuffer)			(void);
void	(D3D9_D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height);
void	(D3D9_D_EndDirectRect)			(int x, int y, int width, int height);
void	(D3D9_VID_ForceLockState)		(int lk);
int		(D3D9_VID_ForceUnlockedAndReturnState) (void);
void	(D3D9_VID_SetPalette)			(unsigned char *palette);
void	(D3D9_VID_ShiftPalette)			(unsigned char *palette);
char	*(D3D9_VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
void	(D3D9_VID_SetWindowCaption)		(char *msg);

void	(D3D9_SCR_UpdateScreen)			(void);





rendererinfo_t d3drendererinfo =
{
	"Direct3D9 Native",
	{
		"D3D",
		"Direct3d",
		"DirectX",
		"DX"
	},
	QR_DIRECT3D,

	R2D_SafePicFromWad,
	R2D_SafeCachePic,
	D3D9_Draw_Init,
	D3D9_Draw_ReInit,
	D3D9_Draw_Crosshair,
	R2D_ScalePic,
	R2D_SubPic,
	D3D9_Draw_TransPicTranslate,
	R2D_ConsoleBackground,
	R2D_EditorBackground,
	R2D_TileClear,
	D3D9_Draw_Fill,
	D3D9_Draw_FillRGB,
	D3D9_Draw_FadeScreen,
	D3D9_Draw_BeginDisc,
	D3D9_Draw_EndDisc,

	R2D_Image,
	R2D_ImageColours,

	D3D9_R_Init,
	D3D9_R_DeInit,
	D3D9_R_RenderView,

	D3D9_R_NewMap,
	D3D9_R_PreNewMap,
	D3D9_R_LightPoint,

	D3D9_R_PushDlights,
	D3D9_R_AddStain,
	D3D9_R_LessenStains,

	NULL,
	NULL,
	NULL,

	RMod_Init,
	RMod_ClearAll,
	RMod_ForName,
	RMod_FindName,
	RMod_Extradata,
	RMod_TouchModel,

	RMod_NowLoadExternal,
	RMod_Think,
	Mod_GetTag,
	Mod_TagNumForName,
	Mod_SkinNumForName,
	Mod_FrameNumForName,
	Mod_FrameDuration,


	D3D9_VID_Init,
	D3D9_VID_DeInit,
	D3D9_VID_LockBuffer,
	D3D9_VID_UnlockBuffer,
	D3D9_D_BeginDirectRect,
	D3D9_D_EndDirectRect,
	D3D9_VID_ForceLockState,
	D3D9_VID_ForceUnlockedAndReturnState,
	D3D9_VID_SetPalette,
	D3D9_VID_ShiftPalette,
	D3D9_VID_GetRGBInfo,
	D3D9_VID_SetWindowCaption,

	D3D9_SCR_UpdateScreen,

	"no more"
};

#endif