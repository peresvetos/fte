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
// console.c

#include "quakedef.h"

console_t	con_main;
console_t	*con_current;			// point to either con_main

//#undef AVAIL_FREETYPE
#ifdef AVAIL_FREETYPE
	extern struct font_s *conchar_font;
	int GLFont_DrawChar(struct font_s *font, int px, int py, unsigned int charcode);
	void GLFont_BeginString(struct font_s *font, int vx, int vy, int *px, int *py);

	#define Font_DrawChar(x,y,c) (conchar_font?GLFont_DrawChar(conchar_font, x, y, c):(Draw_ColouredCharacter(x, y, c),(x)+8))

	#define Font_CharWidth(c) (conchar_font?GLFont_CharWidth(conchar_font, c):8)
	#define Font_CharHeight() (conchar_font?GLFont_CharHeight(conchar_font):8)
#else
	#define Font_DrawChar(x,y,c) (Draw_ColouredCharacter(x, y, c),(x)+8)
	#define Font_CharWidth(c) 8
	#define Font_CharHeight() 8
#endif

static int Con_LineBreaks(conchar_t *start, conchar_t *end, int scrwidth, int maxlines, conchar_t **starts, conchar_t **ends);
static int Con_DrawProgress(int left, int right, int y);

#ifdef QTERM
#include <windows.h>
typedef struct qterm_s {
	console_t *console;
	qboolean running;
	HANDLE process;
	HANDLE pipein;
	HANDLE pipeout;

	HANDLE pipeinih;
	HANDLE pipeoutih;

	struct qterm_s *next;
} qterm_t;

qterm_t *qterms;
qterm_t *activeqterm;
#endif

//int 		con_linewidth;	// characters across screen
//int			con_totallines;		// total lines in console scrollback

float		con_cursorspeed = 4;


cvar_t		con_numnotifylines = SCVAR("con_notifylines","4");		//max lines to show
cvar_t		con_notifytime = SCVAR("con_notifytime","3");		//seconds
cvar_t		con_centernotify = SCVAR("con_centernotify", "0");
cvar_t		con_displaypossibilities = SCVAR("con_displaypossibilities", "1");
cvar_t		con_maxlines = SCVAR("con_maxlines", "1024");
cvar_t		cl_chatmode = SCVAR("cl_chatmode", "2");

#define	NUM_CON_TIMES 24
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

//int			con_vislines;
int			con_notifylines;		// scan lines to clear for notify lines

#define		MAXCMDLINE	256
extern	unsigned char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		

qboolean	con_initialized;

void Con_ResizeCon (console_t *con);

qboolean Con_IsActive (console_t *con)
{
	return (con == con_current) | (con->unseentext*2);
}
void Con_Destroy (console_t *con)
{
	console_t *prev;

	if (con == &con_main)
		return;

	for (prev = &con_main; prev->next; prev = prev->next)
	{
		if (prev->next == con)
		{
			prev->next = con->next;
			break;
		}
	}

	BZ_Free(con);

	if (con_current == con)
		con_current = &con_main;
}
console_t *Con_FindConsole(char *name)
{
	console_t *con;
	for (con = &con_main; con; con = con->next)
	{
		if (!strcmp(con->name, name))
			return con;
	}
	return NULL;
}
console_t *Con_Create(char *name)
{
	console_t *con;
	con = Z_Malloc(sizeof(console_t));
	Q_strncpyz(con->name, name, sizeof(con->name));

	Con_ResizeCon(con);
	con->next = con_main.next;
	con_main.next = con;

	return con;
}
void Con_SetActive (console_t *con)
{
	con_current = con;
}
qboolean Con_NameForNum(int num, char *buffer, int buffersize)
{
	console_t *con;
	for (con = &con_main; con; con = con->next, num--)
	{
		if (num <= 0)
		{
			Q_strncpyz(buffer, con->name, buffersize);
			return true;
		}
	}
	if (buffersize>0)
		*buffer = '\0';
	return false;
}

void Con_PrintCon (console_t *con, char *txt);




#ifdef QTERM
void QT_Update(void)
{
	char buffer[2048];
	DWORD ret;
	qterm_t *qt;
	qterm_t *prev = NULL;
	for (qt = qterms; qt; qt = (prev=qt)->next)
	{
		if (!qt->running)
		{
			if (Con_IsActive(qt->console))
				continue;

			Con_Destroy(qt->console);

			if (prev)
				prev->next = qt->next;
			else
				qterms = qt->next;

			CloseHandle(qt->pipein);
			CloseHandle(qt->pipeout);

			CloseHandle(qt->pipeinih);
			CloseHandle(qt->pipeoutih);
			CloseHandle(qt->process);

			Z_Free(qt);
			break;	//be lazy.
		}
		if (WaitForSingleObject(qt->process, 0) == WAIT_TIMEOUT)
		{
			if ((ret=GetFileSize(qt->pipeout, NULL)))
			{
				if (ret!=INVALID_FILE_SIZE)
				{
					ReadFile(qt->pipeout, buffer, sizeof(buffer)-32, &ret, NULL);
					buffer[ret] = '\0';
					Con_PrintCon(qt->console, buffer);
				}
			}
		}
		else
		{
			Con_PrintCon(qt->console, "Process ended\n");
			qt->running = false;
		}
	}
}

void QT_KeyPress(void *user, int key)
{
	qbyte k[2];
	qterm_t *qt = user;
	DWORD send = key;	//get around a gcc warning


	k[0] = key;
	k[1] = '\0';

	if (qt->running)
	{
		if (*k == '\r')
		{
//					*k = '\r';
//					WriteFile(qt->pipein, k, 1, &key, NULL);
//					Con_PrintCon(k, &qt->console);
			*k = '\n';
		}
		if (GetFileSize(qt->pipein, NULL)<512)
		{
			WriteFile(qt->pipein, k, 1, &send, NULL);
			Con_PrintCon(qt->console, k);
		}
	}
	return;
}

void QT_Create(char *command)
{
	HANDLE StdIn[2];
	HANDLE StdOut[2];
	qterm_t *qt;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO SUInf;
	PROCESS_INFORMATION ProcInfo;

	int ret;

	qt = Z_Malloc(sizeof(*qt));
	
	memset(&sa,0,sizeof(sa));
	sa.nLength=sizeof(sa);
	sa.bInheritHandle=true;

	CreatePipe(&StdOut[0], &StdOut[1], &sa, 1024);
	CreatePipe(&StdIn[1], &StdIn[0], &sa, 1024);

	memset(&SUInf, 0, sizeof(SUInf));
	SUInf.cb = sizeof(SUInf);
	SUInf.dwFlags = STARTF_USESTDHANDLES;
/*
	qt->pipeout		= StdOut[0];
	qt->pipein		= StdIn[0];
*/
	qt->pipeoutih	= StdOut[1];
	qt->pipeinih	= StdIn[1];

	if (!DuplicateHandle(GetCurrentProcess(), StdIn[0], 
						GetCurrentProcess(), &qt->pipein, 0, 
						FALSE,                  // not inherited 
						DUPLICATE_SAME_ACCESS))
		qt->pipein = StdIn[0];
	else
		CloseHandle(StdIn[0]); 
	if (!DuplicateHandle(GetCurrentProcess(), StdOut[0], 
						GetCurrentProcess(), &qt->pipeout, 0, 
						FALSE,                  // not inherited 
						DUPLICATE_SAME_ACCESS))
		qt->pipeout = StdOut[0];
	else
		CloseHandle(StdOut[0]); 

	SUInf.hStdInput		= qt->pipeinih;
	SUInf.hStdOutput	= qt->pipeoutih;
	SUInf.hStdError		= qt->pipeoutih;	//we don't want to have to bother working out which one was written to first.

	if (!SetStdHandle(STD_OUTPUT_HANDLE, SUInf.hStdOutput))
		Con_Printf("Windows sucks\n");
	if (!SetStdHandle(STD_ERROR_HANDLE, SUInf.hStdError))
		Con_Printf("Windows sucks\n");
	if (!SetStdHandle(STD_INPUT_HANDLE, SUInf.hStdInput))
		Con_Printf("Windows sucks\n");

	printf("Started app\n");
	ret = CreateProcess(NULL, command, NULL, NULL, true, CREATE_NO_WINDOW, NULL, NULL, &SUInf, &ProcInfo);

	qt->process = ProcInfo.hProcess;
	CloseHandle(ProcInfo.hThread);

	qt->running = true;

	qt->console = Con_Create("QTerm");
	qt->console->redirect = QT_KeyPress;
	Con_PrintCon(qt->console, "Started Process\n");
	Con_SetVisible(qt->console);

	qt->next = qterms;
	qterms = activeqterm = qt;
}

void Con_QTerm_f(void)
{
	if(Cmd_IsInsecure())
		Con_Printf("Server tried stuffcmding a restricted commandqterm %s\n", Cmd_Args());
	else
		QT_Create(Cmd_Args());
}
#endif




void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque();
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		if (m_state)
			key_dest = key_menu;
		else
			key_dest = key_game;
	}
	else
		key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console)
	{
		if (cls.state == ca_active)
			key_dest = key_game;
	}
	else
		key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	conline_t *t;
	while (con_main.current)
	{
		t = con_main.current;
		con_main.current = t->older;
		Z_Free(t);
	}
	con_main.display = con_main.current = con_main.oldest = NULL;

	Con_ResizeCon(&con_main);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	key_dest = key_message;
}

/*
================
Con_Resize

================
*/
void Con_ResizeCon (console_t *con)
{
	if (con->current == NULL)
	{
		con->oldest = con->current = Z_Malloc(sizeof(conline_t));
		con->linecount = 0;
	}
	if (con->display == NULL)
		con->display = con->current;
}
					
/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	console_t *c;
	for (c = &con_main; c; c = c->next)
		Con_ResizeCon (c);
}

void Con_ForceActiveNow(void)
{
	key_dest = key_console;
	scr_conlines = scr_con_current = vid.height;
}

/*
================
Con_Init
================
*/
void Log_Init (void);

void Con_Init (void)
{
	con_current = &con_main;
	con_main.linebuffered = Con_ExecuteLine;
	con_main.commandcompletion = true;
	Con_CheckResize ();
	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_Register (&con_notifytime, "Console controls");
	Cvar_Register (&con_centernotify, "Console controls");
	Cvar_Register (&con_numnotifylines, "Console controls");
	Cvar_Register (&con_displaypossibilities, "Console controls");
	Cvar_Register (&cl_chatmode, "Console controls");
	Cvar_Register (&con_maxlines, "Console controls");

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
#ifdef QTERM
	Cmd_AddCommand ("qterm", Con_QTerm_f);
#endif
	con_initialized = true;

	Log_Init();
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/

void Con_PrintCon (console_t *con, char *txt)
{
	conchar_t expanded[4096];
	conchar_t *c;
	static int	cr;

	COM_ParseFunString(CON_WHITEMASK, txt, expanded, sizeof(expanded), false);

	c = expanded;
	while (*c)
	{
		conchar_t *o;

		if ((*c&CON_CHARMASK)=='\t')
			*c = (*c&~CON_CHARMASK)|' ';

		switch (*c & (CON_CHARMASK&~CON_HIGHCHARSMASK))
		{
		case '\r':
			cr = true;
			break;
		case '\n':
			if (cr)
				cr = false;
			while (con->linecount >= con_maxlines.value)
			{
				if (con->oldest == con->current)
					break;

				if (con->display == con->oldest)
					con->display = con->oldest->newer;
				con->oldest = con->oldest->newer;
				Z_Free(con->oldest->older);
				con->oldest->older = NULL;
				con->linecount--;
			}
			con->linecount++;
			if (con == &con_main)
				con_times[con->linesprinted++%NUM_CON_TIMES] = realtime;
			con->current->newer = Z_Malloc(sizeof(sizeof(conline_t)));
			con->current->newer->older = con->current;
			con->current = con->current->newer;
			con->current->length = 0;
			if (con->display == con->current->older)
				con->display = con->current;
			break;
		default:
			if (cr)
				con->current->length = 0;
			if (con->display == con->current)
			{
				con->current = BZ_Realloc(con->current, sizeof(*con->current)+(con->current->length+1)*sizeof(conchar_t));
				con->display = con->current;
			}
			else
				con->current = BZ_Realloc(con->current, sizeof(*con->current)+(con->current->length+1)*sizeof(conchar_t));
			if (con->current->older)
				con->current->older->newer = con->current;
			o = (conchar_t *)(con->current+1)+con->current->length;
			*o = *c;
			con->current->length+=1;
			break;
		}
		c++;
	}
}

void Con_Print (char *txt)
{
	Con_PrintCon(&con_main, txt);	//client console
}

void Con_CycleConsole(void)
{
	con_current = con_current->next;
	if (!con_current)
		con_current = &con_main;
}

void Con_Log(char *s);

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/

#ifndef CLIENTONLY
extern redirect_t	sv_redirected;
extern char	outputbuf[8000];
void SV_FlushRedirect (void);
#endif

#define	MAXPRINTMSG	4096
// FIXME: make a buffer size safe vsprintf?
void VARGS Con_Printf (const char *fmt, ...)
{	
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

#ifndef CLIENTONLY
	// add to redirected message
	if (sv_redirected)
	{
		if (strlen (msg) + strlen(outputbuf) > sizeof(outputbuf) - 1)
			SV_FlushRedirect ();
		strcat (outputbuf, msg);
		if (sv_redirected != -1)
			return;
	}
#endif

// also echo to debugging console
	Sys_Printf ("%s", msg);	// also echo to debugging console

// log all messages to file
	Con_Log (msg);

	if (!con_initialized)
		return;

// write it to the scrollable buffer
	Con_Print (msg);
/*
	if (con != &con_main)
		return;

// update the screen immediately if the console is displayed
	if (cls.state != ca_active && !filmactive)
#ifndef CLIENTONLY
	if (progfuncs != svprogfuncs)	//cover our back - don't do rendering stuff that will change this
#endif
	{
	// protect against infinite loop if something in SCR_UpdateScreen calls
	// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}
*/
}

void VARGS Con_SafePrintf (char *fmt, ...)
{	
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

// write it to the scrollable buffer
	Con_Printf ("%s", msg);	
}

void VARGS Con_TPrintf (translation_t text, ...)
{	
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	char *fmt = languagetext[text][cls.language];
	
	va_start (argptr,text);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

// write it to the scrollable buffer
	Con_Printf ("%s", msg);
}

void VARGS Con_SafeTPrintf (translation_t text, ...)
{	
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	char *fmt = languagetext[text][cls.language];
	
	va_start (argptr,text);
	vsnprintf (msg,sizeof(msg), fmt,argptr);
	va_end (argptr);

// write it to the scrollable buffer
	Con_Printf ("%s", msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void VARGS Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	extern cvar_t log_developer;

	if (!developer.value && !log_developer.value)
		return; // early exit

	va_start (argptr,fmt);
	vsnprintf (msg,sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	if (!developer.value)
		Con_Log(msg);
	else
		Con_PrintCon(&con_main, msg);
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (int left, int right, int y)
{
#ifdef _WIN32
	extern qboolean ActiveApp;
#endif
	int		i;
	int lhs, rhs;
	int p;
	unsigned char	*text, *fname;
	extern int con_commandmatch;
	conchar_t maskedtext[MAXCMDLINE];
	conchar_t *endmtext;

	int x;

	if (key_dest != key_console && con_current->vislines != vid.height)
		return;		// don't draw anything (always draw if not active)

	if (!con_current->linebuffered)
		return;	//fixme: draw any unfinished lines of the current console instead.

	text = key_lines[edit_line];

	//copy it to an alternate buffer and fill in text colouration escape codes.
	//if it's recognised as a command, colour it yellow.
	//if it's not a command, and the cursor is at the end of the line, leave it as is,
	//	but add to the end to show what the compleation will be.

	endmtext = COM_ParseFunString(CON_WHITEMASK, text, maskedtext, sizeof(maskedtext)-1*sizeof(conchar_t), true);

	//FIXME: key_linepos is not corrected for multibyte codes.
	if (endmtext < maskedtext+key_linepos)
		key_linepos = endmtext-maskedtext;


	endmtext[1] = 0;

	i = 0;
	x = left;

	if (con_current->commandcompletion)
	{
		if (text[1] == '/' || Cmd_IsCommand(text+1))
		{	//color the first token yellow, it's a valid command
			for (p = 1; (maskedtext[p]&CON_CHARMASK)>' '; p++)
				maskedtext[p] = (maskedtext[p]&CON_CHARMASK) | (COLOR_YELLOW<<CON_FGSHIFT);
		}
//		else
//			Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

		if (!text[key_linepos])	//cursor is at end
		{
			int cmdstart;
			cmdstart = text[1] == '/'?2:1;
			fname = Cmd_CompleteCommand(text+cmdstart, true, true, con_commandmatch);
			if (fname)	//we can compleate it to:
			{
				for (p = key_linepos-cmdstart; fname[p]>' '; p++)
					maskedtext[p+cmdstart] = (unsigned int)fname[p] | (COLOR_GREEN<<CON_FGSHIFT);
				maskedtext[p+cmdstart] = '\0';
			}
		}
	}
//	else
//		Plug_SpellCheckMaskedText(maskedtext+1, i-1, x, y, 8, si, con_current->linewidth);

#ifdef _WIN32
	if (ActiveApp)
#endif
	if (((int)(realtime*con_cursorspeed)&1))
	{
		maskedtext[key_linepos] = 0xe000|11|CON_WHITEMASK;	//make it blink
	}

	for (lhs = 0, i = key_linepos-1; i >= 0; i--)
	{
		lhs += Font_CharWidth(maskedtext[i]);
	}
	for (rhs = 0, i = key_linepos+1; maskedtext[i]; i++)
	{
		rhs += Font_CharWidth(maskedtext[i]);
	}

	//put the cursor in the middle
	x = (right-left)/2;
	//move the line to the right if there's not enough text to touch the right hand side
	if (x < right-rhs)
		x = right - rhs;
	//if the left hand side is on the right of the left point (overrides right alignment)
	if (x - lhs > 0)
		x = lhs;
		
	lhs = x - lhs + left;
	for (i = 0; i < key_linepos; i++)
	{
		lhs = Font_DrawChar(lhs, y, maskedtext[i]);
	}
	rhs = x + left;
	for (i = key_linepos; maskedtext[i]; i++)
	{
		rhs = Font_DrawChar(rhs, y, maskedtext[i]);
	}
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	conchar_t *starts[NUM_CON_TIMES], *ends[NUM_CON_TIMES];
	conchar_t *c;
	conline_t *l;
	console_t *con = &con_main;
	int lines=NUM_CON_TIMES;
	int line;
	int x = 0, y = 0;
	unsigned int cn = con->linesprinted+NUM_CON_TIMES;

	int maxlines;
	float t;

	maxlines = con_numnotifylines.value;
	if (maxlines < 0)
		maxlines = 0;
	if (maxlines > NUM_CON_TIMES)
		maxlines = NUM_CON_TIMES;

	y = Con_DrawProgress(0, vid.width, 0);

	l = con->current;
	if (!l->length)
		l = l->older;
	for (; l && cn > con->linesprinted && lines > NUM_CON_TIMES-maxlines; l = l->older)
	{
		t = con_times[--cn % NUM_CON_TIMES];
		if (!t)
			break; //cleared
		t = realtime - t;
		if (t > con_notifytime.value)
			break;

		line = Con_LineBreaks((conchar_t*)(l+1), (conchar_t*)(l+1)+l->length, vid.width, lines, starts, ends);
		if (!line && lines > 0)
		{
			lines--;
			starts[lines] = NULL;
			ends[lines] = NULL;
		}
		while(line --> 0 && lines > 0)
		{
			lines--;
			starts[lines] = starts[line];
			ends[lines] = ends[line];
		}
		if (lines == 0)
			break;
	}
	//clamp it properly
	while (lines < NUM_CON_TIMES-maxlines)
		lines++;
	while (lines < NUM_CON_TIMES)
	{
		x = 0;
		if (con_centernotify.value)
		{
			for (c = starts[lines]; c < ends[lines]; c++)
			{
#ifdef AVAIL_FREETYPE
				x += GLFont_CharWidth(conchar_font, *c);
#else
				x += 8;
#endif
			}
			x = (vid.width - x) / 2;
		}
		for (c = starts[lines]; c < ends[lines]; c++)
			x = Font_DrawChar(x, y, *c);

		y += 8;

		lines++;
	}


	if (key_dest == key_message)
	{
		conchar_t *starts[8];
		conchar_t *ends[8];
		conchar_t markup[MAXCMDLINE+64];
		conchar_t *c;
		int lines, i;
		c = COM_ParseFunString(CON_WHITEMASK, va(chat_team?"say_team: %s":"say: %s", chat_buffer), markup, sizeof(markup), true);
		*c++ = (0xe00a+((int)(realtime*con_cursorspeed)&1))|CON_WHITEMASK;
		lines = Con_LineBreaks(markup, c, vid.width, 8, starts, ends);
		for (i = 0; i < lines; i++)
		{
			c = starts[i];
			x = 0;
			while (c < ends[i])
			{
				x = Font_DrawChar(x, y, *c++);
			}
			y += Font_CharHeight();
		}
	}

	if (y > con_notifylines)
		con_notifylines = y;
}

//send all the stuff that was con_printed to sys_print. 
//This is so that system consoles in windows can scroll up and have all the text.
void Con_PrintToSys(void)	
{
	console_t *curcon = &con_main;
	conline_t *l;
	int i;
	conchar_t *t;
	for (l = curcon->oldest; l; l = l->newer)
	{
		t = (conchar_t*)(l+1);
		//fixme: utf8?
		for (i = 0; i < l->length; i++)
			Sys_Printf("%c", t[i]&0xff);
		Sys_Printf("\n");
	}
}

static int Con_LineBreaks(conchar_t *start, conchar_t *end, int scrwidth, int maxlines, conchar_t **starts, conchar_t **ends)
{
	int l, bt;
	int px;
	int foundlines = 0;

	while (start < end)
	{
	// scan the width of the line
		for (px=0, l=0 ; px <= scrwidth;)
		{
			if ((start[l]&CON_CHARMASK) == '\n' || (start+l >= end))
				break;
			l++;
			px += Font_CharWidth(start[l]);
		}
		//if we did get to the end
		if (px > scrwidth)
		{
			bt = l;
			//backtrack until we find a space
			while(l > 0 && (start[l-1]&CON_CHARMASK)>' ')
			{
				l--;
			}
			if (l == 0 && bt>0)
				l = bt-1;
			px -= Font_CharWidth(start[l]);
		}

		starts[foundlines] = start;
		ends[foundlines] = start+l;
		foundlines++;
		if (foundlines == maxlines)
			break;

		start+=l;
//		for (l=0 ; l<40 && *start && *start != '\n'; l++)
 //			start++;

		if ((*start&CON_CHARMASK) == '\n'||!l)
			start++;                // skip the \n
	}

	return foundlines;
}

//returns the bottom of the progress bar
static int Con_DrawProgress(int left, int right, int y)
{
#ifdef RUNTIMELIGHTING
	extern model_t *lightmodel;
	extern int relitsurface;
#endif

	conchar_t			dlbar[1024];
	unsigned char	progresspercenttext[128];
	char *progresstext = NULL;
	char *txt;
	int x, tw;
	int i, j;
	int barwidth, barleft;
	float progresspercent = 0;
	*progresspercenttext = 0;
	
	// draw the download bar
	// figure out width
	if (cls.downloadmethod)
	{
		unsigned int count, total;
		qboolean extra;
		progresstext = cls.downloadlocalname;
		progresspercent = cls.downloadpercent;

		if ((int)(realtime/2)&1)
			sprintf(progresspercenttext, " %02d%% (%ukbps)", (int)progresspercent, CL_DownloadRate()/1000);
		else
		{
			CL_GetDownloadSizes(&count, &total, &extra);
			if (total == 0)
			{
				//just show progress
				sprintf(progresspercenttext, " %02d%%", progresspercent);
			}
			else
			{
				sprintf(progresspercenttext, " %02d%% (%u%skb)", (int)progresspercent, total/1024, extra?"+":"");
			}
		}
	}
#ifdef RUNTIMELIGHTING
	else if (lightmodel)
	{
		if (relitsurface < lightmodel->numsurfaces)
		{
			progresstext = "light";
			progresspercent = (relitsurface*100.0f) / lightmodel->numsurfaces;
			sprintf(progresspercenttext, " %02d%%", (int)progresspercent);
		}
	}
#endif

	//at this point:
	//progresstext: what is being downloaded/done (can end up truncated)
	//progresspercent: its percentage (used only for the slider)
	//progresspercenttext: that percent as text, essentually the right hand part of the bar.

	if (progresstext)
	{
		//chop off any leading path
		if ((txt = strrchr(progresstext, '/')) != NULL)
			txt++;
		else
			txt = progresstext;

		x = 0;
		COM_ParseFunString(CON_WHITEMASK, txt, dlbar, sizeof(dlbar), false); 
		for (i = 0; dlbar[i]; )
		{
			x += Font_CharWidth(dlbar[i]);
			i++;
		}

		//if the string is wider than a third of the screen
		if (x > (right - left)/3)
		{
			//truncate the file name and add ...
			x += 3*Font_CharWidth('.'|CON_WHITEMASK);
			while (x > (right - left)/3 && i > 0)
			{
				i--;
				x -= Font_CharWidth(dlbar[i]);
			}

			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i++] = '.'|CON_WHITEMASK;
			dlbar[i] = 0;
		}

		//i is the char index of the dlbar so far, x is the char width of it.

		//add a couple chars
		dlbar[i] = ':'|CON_WHITEMASK;
		x += Font_CharWidth(dlbar[i]);
		i++;
		dlbar[i] = ' '|CON_WHITEMASK;
		x += Font_CharWidth(dlbar[i]);
		i++;

		COM_ParseFunString(CON_WHITEMASK, progresspercenttext, dlbar+i, sizeof(dlbar)-i*sizeof(conchar_t), false);
		for (j = i, tw = 0; dlbar[j]; )
		{
			tw += Font_CharWidth(dlbar[j]);
			j++;
		}

		barwidth = (right-left) - (x + tw);

		//draw the right hand side
		x = right - tw;
		for (j = i; dlbar[j]; j++)
			x = Font_DrawChar(x, y, dlbar[j]);

		//draw the left hand side
		x = left;
		for (j = 0; j < i; j++)
			x = Font_DrawChar(x, y, dlbar[j]);

		//and in the middle we have lots of stuff

		barwidth -= (Font_CharWidth(0xe080|CON_WHITEMASK) + Font_CharWidth(0xe082|CON_WHITEMASK));
		x = Font_DrawChar(x, y, 0xe080|CON_WHITEMASK);
		barleft = x;
		for(;;)
		{
			if (x + Font_CharWidth(0xe081|CON_WHITEMASK) > barleft+barwidth)
				break;
			x = Font_DrawChar(x, y, 0xe081|CON_WHITEMASK);
		}
		x = Font_DrawChar(x, y, 0xe082|CON_WHITEMASK);

		Font_DrawChar(barleft+(barwidth*progresspercent)/100 - Font_CharWidth(0xe083|CON_WHITEMASK)/2, y, 0xe083|CON_WHITEMASK);

		y += Font_CharHeight();
	}
	return y;
}

//draws console selection choices at the top of the screen, if multiple consoles are available
//its ctrl+tab to switch between them
int Con_DrawAlternateConsoles(int lines)
{
	char *txt;
	int x, y = 0;
	if (lines == scr_conlines && con_main.next)
	{
		console_t *con = con_current;
		for (x = 0, con = &con_main; con; con = con->next)
		{
			if (con == &con_main)
				txt = "MAIN";
			else
				txt = con->name;

			if (x != 0 && x+(strlen(txt)+1)*8 > vid.width)
			{
				x = 0;
				y += 8;
			}
			Draw_FunString(x, y, va("^&F%i%s", (con==con_current)+con->unseentext*4, txt));
			x+=(strlen(txt)+1)*8;
		}
		y += 8;
	}
	return y;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (int lines, qboolean noback)
{
	extern qboolean scr_con_forcedraw;
	int x, y, sx, ex, linecount, linelength;
	conline_t *l;
	conchar_t *s;
	int selsx, selsy, selex, seley, selactive;
	int top;
	conchar_t *starts[64], *ends[sizeof(starts)/sizeof(starts[0])];
	int i;
	extern int glwidth;

	if (lines <= 0)
		return;

#ifdef QTERM
	if (qterms)
		QT_Update();
#endif

// draw the background
	if (!noback)
		Draw_ConsoleBackground (0, lines, scr_con_forcedraw);

	con_current->unseentext = false;

	con_current->vislines = lines;

	x = 8;
	y = lines;

	selactive = Key_GetConsoleSelectionBox(&selsx, &selsy, &selex, &seley);

#ifdef AVAIL_FREETYPE
	if (conchar_font)
	{
		GLFont_BeginString(conchar_font, x, y, &x, &y);
		GLFont_BeginString(conchar_font, selsx, selsy, &selsx, &selsy);
		GLFont_BeginString(conchar_font, selex, seley, &selex, &seley);
		ex = glwidth;
	}
	else
#endif
		ex = vid.width;
	sx = x;
	ex -= sx;

	y -= Font_CharHeight();
	Con_DrawProgress(x, ex - x, y);
	y -= Font_CharHeight();
	Con_DrawInput (x, ex - x, y);

	if (selactive)
	{
		if (selsx < x)
			selsx = x;
		if (selex < x)
			selex = x;

		if (selsy > y)
			selsy = y;
		if (seley > y)
			seley = y;

		selsy -= y;
		seley -= y;
		selsy /= Font_CharHeight();
		seley /= Font_CharHeight();
		selsy--;
		seley--;

		if (selsy == seley)
		{
			//single line selected backwards
			if (selex < selsx)
			{
				x = selex;
				selex = selsx;
				selsx = x;
			}
		}
		if (seley < selsy)
		{	//selection goes upwards
			x = selsy;
			selsy = seley;
			seley = x;

			x = selex;
			selex = selsx;
			selsx = x;
		}
		selsy *= Font_CharHeight();
		seley *= Font_CharHeight();
		selsy += y;
		seley += y;
	}

	top = Con_DrawAlternateConsoles(lines);

	if (!con_current->display)
		con_current->display = con_current->current;
	l = con_current->display;

	if (l != con_current->current)
	{
		y -= 8;
	// draw arrows to show the buffer is backscrolled
		for (x = sx ; x<ex; )
			x = (Font_DrawChar (x, y, '^'|CON_WHITEMASK)-x)*4+x;
	}

	if (l && l == con_current->current && l->length == 0)
		l = l->older;
	for (; l; l = l->older)
	{
		s = (conchar_t*)(l+1);

		linecount = Con_LineBreaks(s, s+l->length, ex-sx, sizeof(starts)/sizeof(starts[0]), starts, ends);

		//if Con_LineBreaks didn't find any lines at all, then it was an empty line, and we need to ensure that its still drawn
		if (linecount == 0)
		{
			linecount = 1;
			starts[0] = ends[0] = NULL;
		}

		while (linecount-- > 0)
		{
			s = starts[linecount];
			linelength = ends[linecount] - s;

			y -= Font_CharHeight();

			if (top && y < top)
				break;

			if (selactive)
			{
				if (y >= selsy)
				{
					if (y <= seley)
					{
						int sstart;
						int send;
						sstart = sx;
						send = sstart+linelength*8;

						//show something on blank lines
						if (send == sstart)
							send = sstart + Font_CharWidth(' ');

						if (y >= seley)
						{
							send = sstart;
							for (i = 0; i < linelength; i++)
							{
								send += Font_CharWidth(s[i]);
								if (send > selex)
									break;
							}
						}
						if (y <= selsy)
						{
							for (i = 0; i < linelength; i++)
							{
								x = Font_CharWidth(s[i]);
								if (sstart + x > selsx)
									break;
								sstart += x;
							}
						}
						
						Draw_Fill(sstart, y, send - sstart, Font_CharHeight(), 0);
					}
				}
			}

			x = sx;
			for (i = 0; i < linelength; i++)
			{
				x = Font_DrawChar(x, y, *s++);
			}

			if (!top && y < top)
				break;
		}
	}

// draw the input prompt, user text, and cursor if desired
	DrawCursor();
}


/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf (text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do
	{
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
}