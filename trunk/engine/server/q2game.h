/*
Copyright (C) 1997-2001 Id Software, Inc.

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

// game.h -- game dll information visible to server

typedef enum multicast_e
{
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
} multicast_t;

extern float	pm_stepheight;

#ifdef Q2SERVER

struct trace_s;
struct q2trace_s;
struct pmove_s;
typedef struct pmove_s pmove_t;




#define	Q2GAME_API_VERSION	3

// edict->svflags

#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_DEADMONSTER			0x00000002	// treat as CONTENTS_DEADMONSTER for collision
#define	SVF_MONSTER				0x00000004	// treat as CONTENTS_MONSTER for collision

// edict->solid values

typedef enum
{
Q2SOLID_NOT,			// no interaction with other objects
Q2SOLID_TRIGGER,		// only touch when inside, after moving
Q2SOLID_BBOX,			// touch on edge
Q2SOLID_BSP			// bsp clip, touch on edge
} q2solid_t;


#define	MAXTOUCH	32
typedef struct q2pmove_s
{
	// state (in / out)
	q2pmove_state_t	s;

	// command (in)
	q2usercmd_t		cmd;
	qboolean		snapinitial;	// if s has been changed outside pmove

	// results (out)
	int			numtouch;
	struct edict_s	*touchents[MAXTOUCH];

	vec3_t		viewangles;			// clamped
	float		viewheight;

	vec3_t		mins, maxs;			// bounding box size

	struct edict_s	*groundentity;
	int			watertype;
	int			waterlevel;

	// callbacks to test the world
	q2trace_t		(VARGS *trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int			(VARGS *pointcontents) (vec3_t point);
} q2pmove_t;

void VARGS Q2_Pmove (q2pmove_t *pmove);

//===============================================================

// link_t is only used for entity area links now
/*typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;*/

#define	MAX_ENT_CLUSTERS	16


//typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;


#ifndef GAME_INCLUDE

struct q2gclient_s
{
	q2player_state_t	ps;		// communicated by server to clients
	int				ping;
	// the game dll can add anything it wants after
	// this point in the structure
};

typedef struct q2entity_state_s
{
	int		number;			// edict index

	vec3_t	origin;
	vec3_t	angles;
	vec3_t	old_origin;		// for lerping
	int		modelindex;
	int		modelindex2, modelindex3, modelindex4;	// weapons, CTF flags, etc
	int		frame;
	int		skinnum;
	unsigned int		effects;		// PGM - we're filling it, so it needs to be unsigned
	int		renderfx;
	int		solid;			// for client side prediction, 8*(bits 0-4) is x/y radius
							// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
							// gi.linkentity sets this properly
	int		sound;			// for looping sounds, to guarantee shutoff
	int		event;			// impulse events -- muzzle flashes, footsteps, etc
							// events only go out for a single frame, they
							// are automatically cleared each frame
} q2entity_state_t;


typedef struct q2edict_s q2edict_t;
struct q2edict_s
{
	q2entity_state_t	s;
	struct q2gclient_s	*client;
	qboolean	inuse;
	int			linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t		area;				// linked to a division node or leaf
	
	int			num_clusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			headnode;			// unused if num_clusters != -1
	int			areanum, areanum2;

	//================================

	int			svflags;			// SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;
	q2solid_t		solid;
	int			clipmask;
	q2edict_t		*owner;

	// the game dll can add anything it wants after
	// this point in the structure
};

#endif		// GAME_INCLUDE

//===============================================================

//
// functions provided by the main engine
//
//yes, these are all VARGS, for the calling convention rather than actually being varargs.
typedef struct
{
	// special messages
	void	(VARGS *bprintf) (int printlevel, char *fmt, ...);
	void	(VARGS *dprintf) (char *fmt, ...);
	void	(VARGS *cprintf) (q2edict_t *ent, int printlevel, char *fmt, ...);
	void	(VARGS *centerprintf) (q2edict_t *ent, char *fmt, ...);
	void	(VARGS *sound) (q2edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
	void	(VARGS *positioned_sound) (vec3_t origin, q2edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

	// config strings hold all the index strings, the lightstyles,
	// and misc data like the sky definition and cdtrack.
	// All of the current configstrings are sent to clients when
	// they connect, and changes are sent to all connected clients.
	void	(VARGS *configstring) (int num, char *string);

	void	(VARGS *error) (char *fmt, ...);

	// the *index functions create configstrings and some internal server state
	int		(VARGS *modelindex) (char *name);
	int		(VARGS *soundindex) (char *name);
	int		(VARGS *imageindex) (char *name);

	void	(VARGS *setmodel) (q2edict_t *ent, char *name);

	// collision detection
	q2trace_t	(VARGS *trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, q2edict_t *passent, int contentmask);
	int		(VARGS *pointcontents) (vec3_t point);
	qboolean	(VARGS *inPVS) (vec3_t p1, vec3_t p2);
	qboolean	(VARGS *inPHS) (vec3_t p1, vec3_t p2);
	void		(VARGS *SetAreaPortalState) (int portalnum, qboolean open);
	qboolean	(VARGS *AreasConnected) (int area1, int area2);

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  If the size, position, or
	// solidity changes, it must be relinked.
	void	(VARGS *linkentity) (q2edict_t *ent);
	void	(VARGS *unlinkentity) (q2edict_t *ent);		// call before removing an interactive edict
	int		(VARGS *BoxEdicts) (vec3_t mins, vec3_t maxs, q2edict_t **list,	int maxcount, int areatype);
	void	(VARGS *Pmove) (q2pmove_t *pmove);		// player movement code common with client prediction

	// network messaging
	void	(VARGS *multicast) (vec3_t origin, multicast_t to);
	void	(VARGS *unicast) (q2edict_t *ent, qboolean reliable);
	void	(VARGS *WriteChar) (int c);
	void	(VARGS *WriteByte) (int c);
	void	(VARGS *WriteShort) (int c);
	void	(VARGS *WriteLong) (int c);
	void	(VARGS *WriteFloat) (float f);
	void	(VARGS *WriteString) (char *s);
	void	(VARGS *WritePosition) (vec3_t pos);	// some fractional bits
	void	(VARGS *WriteDir) (vec3_t pos);		// single byte encoded, very coarse
	void	(VARGS *WriteAngle) (float f);

	// managed memory allocation
	void	*(VARGS *TagMalloc) (int size, int tag);
	void	(VARGS *TagFree) (void *block);
	void	(VARGS *FreeTags) (int tag);

	// console variable interaction
	cvar_t	*(VARGS *cvar) (char *var_name, char *value, int flags);
	cvar_t	*(VARGS *cvar_set) (char *var_name, char *value);
	cvar_t	*(VARGS *cvar_forceset) (char *var_name, char *value);

	// ClientCommand and ServerCommand parameter access
	int		(VARGS *argc) (void);
	char	*(VARGS *argv) (int n);
	char	*(VARGS *args) (void);	// concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void	(VARGS *AddCommandString) (char *text);

	void	(VARGS *DebugGraph) (float value, int color);
} game_import_t;

//
// functions exported by the game subsystem
//
typedef struct
{
	int			apiversion;

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void		(VARGS *Init) (void);
	void		(VARGS *Shutdown) (void);

	// each new level entered will cause a call to SpawnEntities
	void		(VARGS *SpawnEntities) (char *mapname, char *entstring, char *spawnpoint);

	// Read/Write Game is for storing persistant cross level information
	// about the world state and the clients.
	// WriteGame is called every time a level is exited.
	// ReadGame is called on a loadgame.
	void		(VARGS *WriteGame) (char *filename, qboolean autosave);
	void		(VARGS *ReadGame) (char *filename);

	// ReadLevel is called after the default map information has been
	// loaded with SpawnEntities
	void		(VARGS *WriteLevel) (char *filename);
	void		(VARGS *ReadLevel) (char *filename);

	qboolean	(VARGS *ClientConnect) (q2edict_t *ent, char *userinfo);
	void		(VARGS *ClientBegin) (q2edict_t *ent);
	void		(VARGS *ClientUserinfoChanged) (q2edict_t *ent, char *userinfo);
	void		(VARGS *ClientDisconnect) (q2edict_t *ent);
	void		(VARGS *ClientCommand) (q2edict_t *ent);
	void		(VARGS *ClientThink) (q2edict_t *ent, q2usercmd_t *cmd);

	void		(VARGS *RunFrame) (void);

	// ServerCommand will be called when an "sv <command>" command is issued on the
	// server console.
	// The game can issue gi.argc() / gi.argv() commands to get the rest
	// of the parameters
	void		(VARGS *ServerCommand) (void);

	//
	// global variables shared between game and server
	//

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	// 
	// The size will be fixed when ge->Init() is called
	struct q2edict_s	*edicts;
	int			edict_size;
	int			num_edicts;		// current number, <= max_edicts
	int			max_edicts;
} game_export_t;

game_export_t *GetGameApi (game_import_t *import);



extern game_export_t	*ge;

#endif
