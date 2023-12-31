/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 *  gfx_common.c
 *  common GFX function for MPlayer AmigaOS
 *  Written by DET Nicolas
 *  Maintained and updated by Fabien Coeurjoly
 *  Rewritten by Kjetil Hvalstrand for AmigaOS4
*/

/*markus
  we do not currently compile the old printf debug
  future code to be removed as it will work
*/
//#define OLD_DEBUG
//end markus

#define HAVE_AREXX 1

#define SYSTEM_PRIVATE

#include <exec/types.h>

#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/dos.h>

#ifndef __amigaos4__
#include <proto/alib.h>
#endif

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/layers.h>
#include <proto/wb.h>
#include <proto/keymap.h>

#include <cybergraphx/cybergraphics.h>
#include <graphics/gfxbase.h>

#ifndef __amigaos4__
#include <intuition/extensions.h>
#endif

#include <libraries/gadtools.h>

#if defined(__morphos__) || defined(__aros__) || defined(__amigaos3__)
#include <devices/rawkeycodes.h>
#endif

#ifdef __amigaos4__
#include <libraries/keymap.h>
#include <proto/Picasso96API.h>

extern struct Catalog *catalog ;
#define MPlayer_NUMBERS
#define MPlayer_STRINGS
#include "../amigaos/MPlayer_catalog.h"
extern STRPTR myGetCatalogStr (struct Catalog *catalog, LONG num, STRPTR def);
#define CS(id) myGetCatalogStr(catalog,id,id##_STR)

// For AmigaOS4 Blanker and Application_lib
#include <proto/application.h>
//#include <libraries/application.h>
extern uint32 AppID;
extern struct MsgPort *applibPort;
#endif

//fullpath patch
extern char fullpath_mplayer[1024];
extern VOID VARARGS68K EasyRequester(CONST_STRPTR text, CONST_STRPTR button, ...);

#ifndef   DOS_RDARGS_H
#include <dos/rdargs.h>
#endif

#ifndef   INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif

#include <devices/inputevent.h>
#include <utility/hooks.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>

#include <osdep/keycodes.h>

#include <string.h>

#ifdef __amigaos4__
// -- Missing strcasestr in string.h

#include "cgx_common.h"

#include "../amigaos/debug.h"
#include "../amigaos/amigaos_stuff.h"
#include "../amigaos/arexx.h"
#include "../amigaos/menu.h"

// markus
#include "../amigaos/window_icons.h"
#include <stdbool.h>
#include <proto/icon.h>

extern int spawn_count;

struct kIcon iconifyIcon = { NULL, NULL };
struct kIcon padlockicon = { NULL, NULL };
struct kIcon fullscreenicon = { NULL, NULL };

//static bool is_uniconified(void);
static bool enable_Iconify(void);
static void dispose_Iconify(void);

bool empty_msg_queue(struct MsgPort *port);


struct
{
	int x,y;
	int bw,bh;
	int w,h
} save_window = {0,0,0,0,0,0} ;

//

BOOL choosing_flag = FALSE;
uint32_t is_fullscreen = 0;
uint32_t can_go_faster = 0;
BOOL menu_in_use = FALSE;

void set_gfx_rendering_option(void)
{
/*
	printf("\n");
	printf("Is_fullscreen = %s\n",is_fullscreen ? "True" : "False" );
	printf("Menu_in_use = %s\n",menu_in_use ? "True" : "False" );
	printf("Choosing_flag = %s\n",choosing_flag ? "True" : "False" );
*/
	can_go_faster = (is_fullscreen) && (menu_in_use == FALSE) && (choosing_flag == FALSE) ? is_fullscreen : 0;
/*
	printf("Can_go_faster = %s\n", can_go_faster ? "True" : "False" );
	printf("\n");
*/
}

static char *strcasestr(char *heystack,char *needle)
{
	char h,  n;
	char *hptr,*nptr;
	char *first = NULL;
	int needle_len = strlen(needle);
	int found_len =0;

	nptr = needle;
	for (hptr = heystack; *hptr; hptr++)
	{
		h = *hptr;
		n = *nptr;
		if ((h>='a')&&(h<='z')) h=h-'a'+'A';
		if ((n>='a')&&(n<='z')) n=n-'a'+'A';

		if (h==n)
		{
			first = first ? first : hptr ; 	found_len++;	nptr++;
		}
		else
		{
			nptr = needle;	first = NULL;	found_len=0;
		}

		if (found_len == needle_len) break;
	}
	return first;
}
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef __morphos__
#include "morphos_stuff.h"
#endif

#include "mp_msg.h"
#include "video_out.h"
#include "mp_fifo.h"
#include "config.h"
#include "input/input.h"
#include "input/mouse.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "gui/morphos/gui.h"
#endif

extern APTR window_mx;
//extern IMAGE_WIDTH;
//extern IMAGE_HEIGHT;

// Set here, and use in the vo_gfx_**
ULONG gfx_monitor = 0;
ULONG gfx_novsync = 0;
ULONG gfx_nodri   = 0;
ULONG gfx_nodma   = 0;
char *gfx_screenname = NULL;


ULONG WantedModeID = 0;

// Blank pointer
UWORD *EmptyPointer = NULL;


#define NOBORDER		0
#define TINYBORDER	1
#define DISAPBORDER	2
#define ALWAYSBORDER	3

ULONG gfx_BorderMode = ALWAYSBORDER;


// Timer for the pointer...
ULONG p_mics1=0, p_mics2=0, p_secs1=0, p_secs2=0;
BOOL mouse_hidden=FALSE;

static BOOL mouseonborder;

void gfx_ShowMouse(struct Screen * screen, struct Window * window, ULONG enable);

#ifdef __amigaos4__

#define RAWKEY_ESCAPE    0x45
#define RAWKEY_CONTROL   0x63
#define RAWKEY_DELETE    0x46
#define RAWKEY_KP_ENTER  0x43
#define RAWKEY_KP_7      0x3D
#define RAWKEY_KP_8      0x3E
#define RAWKEY_KP_9      0x3F
#define RAWKEY_KP_4      0x2D
#define RAWKEY_KP_5      0x2E
#define RAWKEY_KP_6      0x2F
#define RAWKEY_KP_1      0x1D
#define RAWKEY_KP_2      0x1E
#define RAWKEY_KP_3      0x1F
#define RAWKEY_KP_0      0x0F
#define RAWKEY_LAMIGA    0x66
#define RAWKEY_RAMIGA    0x67

#endif

/* AppWindow stuff */
struct MsgPort *awport;
struct AppWindow *appwin;
ULONG  appwinsig, appid = 1, appuserdata = 0;

ULONG gfx_common_rgb_format = PIXF_A8R8G8B8;
uint32_t   amiga_image_width;		// Well no comment
uint32_t   amiga_image_height;
float amiga_aspect_ratio;


/* Drag/Size gadgets definitions */
static struct Gadget MyDragGadget =
{
	NULL,
	0,0,					// Pos
	0,0,					// Size
	0,					// Flags
	0,					// Activation
	GTYP_WDRAGGING,			// Type
	NULL,					// GadgetRender
	NULL,					// SelectRender
	NULL,					// GadgetText
	0L,					// Obsolete
	NULL,					// SpecialInfo
	0,					// Gadget ID
	NULL					// UserData
};

static struct Gadget MySizeGadget =
{
	NULL,
	0,0,					// Pos
	0,0,					// Size
	0,					// Flags
	0,					// Activation
	GTYP_SIZING,			// Type
	NULL,					// GadgetRender
	NULL,					// SelectRender
	NULL,					// GadgetText
	0L,					// Obsolete
	NULL,					// SpecialInfo
	0,					// Gadget ID
	NULL					// UserData
};

/***************************/

#ifdef OLD_DEBUG
/*markus qu'est-ce que c'est ? ;-D */
static const char *Messages[]=
{
	"Le dormeur doit se r�veiller.\n",
	"B�ni soit le faiseur et son nom.\n",
	"Uzul parle moi de ton monde natal.\n",
};

void gfx_Message(void)
{
#ifndef __amigaos4__
	struct timeval tv;
	GetSysTime(&tv);
	if ( ( tv.tv_secs % 60) < 5)  Printf("Message: %s\n", Messages[ tv.tv_micro % 3 ] );
#endif

#ifdef __amigaos4__
	struct TimeVal tv;
	GetSysTime(&tv);
	if ( ( tv.Seconds % 60) < 5)  Printf("Message: %s\n", Messages[ tv.Microseconds % 3 ] );
#endif
}

#endif //OLD_DEBUG

BOOL gfx_help(struct gfx_command *commands, char *cmd_name, ULONG *value)
{
	struct gfx_command *cmd;

	printf("\nHelp\n\n");

	for (cmd = commands ; cmd -> name ; cmd ++)
	{
		printf("   %s\n",cmd -> help);
	}
	printf("\n");

	return FALSE;
}

static ULONG find_monitor_id(int n);

BOOL fix_rgb_modes(struct gfx_command *commands, char *cmd_name, ULONG *value)
{
	int tmp;
	sscanf(cmd_name,"%d",&tmp);

	switch(tmp)
	{
		case 16:	gfx_common_rgb_format = PIXF_R5G6B5; break;
		case 24:	gfx_common_rgb_format = PIXF_R8G8B8; break;
		case 32:	gfx_common_rgb_format = PIXF_A8R8G8B8; break;
	}
	return FALSE;
}

BOOL fix_border(struct gfx_command *commands, char *cmd_name, ULONG *value)
{
	if (strcasecmp(cmd_name, "NOBORDER" ) == 0 ) gfx_BorderMode = NOBORDER;
	else if (strcasecmp(cmd_name, "TINYBORDER" ) == 0 ) gfx_BorderMode = TINYBORDER;
	else if (strcasecmp(cmd_name, "ALWAYSBORDER" ) == 0 ) gfx_BorderMode = ALWAYSBORDER;
	else if (strcasecmp(cmd_name, "DISAPBORDER" ) == 0 ) gfx_BorderMode = DISAPBORDER;
	return FALSE;
}

BOOL gfx_process_arg(char *arg, struct gfx_command *commands)
{
	struct gfx_command *cmd;
	char *out;
	BOOL found = FALSE;
	BOOL error = FALSE;



	for (cmd = commands ; cmd -> name ; cmd ++)
	{
		if (strncasecmp(arg, cmd -> name, strlen(cmd->name))==0)
		{
			out = arg + strlen(cmd -> name);

			if (out[-1]=='=')
			{
				found = TRUE;
				if ((cmd -> target)||(cmd->type=='s'))
				{
					switch (cmd -> type)
					{
						case 's': 	printf("Arg %s is a switch, takes no values\n",cmd->name);
							error = TRUE;
							break;
						case 'n': 	sscanf( out, "%d", (int *)  cmd -> target);
							break;
						case 'h': 	sscanf( out, "%x", (int *)  cmd -> target);
							break;
						case 'k':
							if (*cmd -> target) free( (char *) *cmd -> target);
							*((char **) (cmd -> target)) = strdup(out);
							break;
					}
					if (cmd -> hook_fn) error = cmd -> hook_fn(commands,arg, cmd -> target) ? TRUE : error;
				}
			}
			else if (out[0] == 0 )
			{
				printf("------> %s\n",arg);

				found = TRUE;
				if (cmd -> type == 's')
				{
					if (cmd -> target)
					{
						*((ULONG *) (cmd -> target)) = 1 ;
						printf("---------- target %08x set true  ------\n", cmd -> target);
					}
					if (cmd -> hook_fn) error = cmd -> hook_fn(commands,arg, cmd -> target) ? TRUE : error;
				}
				else
				{
					printf("Arg %s needs a value\n",cmd -> name);
					error= TRUE;
				}
			}

			if (found)	break;
		}
	}

	if (!found)
	{
		printf("Arg %s unknown, see help for a list of supported args\n",arg);
		error = FALSE;
	}

	return !error;
}


BOOL gfx_process_args(char *arg, struct gfx_command *commands)
{
	char *itm;
	char *end;
	ULONG len;
	char *_itm;
	BOOL ret = TRUE;

	for (itm = arg; itm ; itm = strchr(itm+1, ':'))
	{
		itm = (itm[0]==':') ? itm+1 : itm;
		end = strchr( itm, ':' );
		len = end ? end-itm : strlen(itm);

		if (_itm = strndup(itm,len))
		{
			ret = gfx_process_arg(_itm,commands);
			free(_itm);
		}
		if (ret == FALSE) return FALSE;
	}

	return ret;
}


struct gfx_command gfx_commands[] = {
	{	"help",		's',		NULL,	"HELP/S",			gfx_help },
	{	"NOBORDER",		's',		NULL,	"NOBORDER/S",		fix_border },
	{	"TINYBORDER",	's',		NULL,	"TINYBORDER/S",		fix_border },
	{	"ALWAYSBORDER",	's',		NULL,	"ALWAYSBORDER/S",		fix_border },
	{	"DISAPBORDER",	's',		NULL,	"DISAPBORDER/S",		fix_border },

	{	"NOVSYNC",		's',		(ULONG *) &gfx_novsync,		"NOVSYNC/S", NULL },
	{	"NODRI",		's',		(ULONG *) &gfx_nodri,		"NODRI/S (no direct rendering)", NULL },
	{	"NODMA",		's',		(ULONG *) &gfx_nodma,		"NODMA/S (no bitmap buffer, only vram bitmap)", NULL },

	{	"MODEID=",		'h',		(ULONG *) &WantedModeID,	"MODEID=ScreenModeID/N (some video outputs might ignore this one)", NULL },
	{	"PUBSCREEN=",	'k',		(ULONG *) &gfx_screenname,	"PUBSCREEN=ScreenName/K", NULL },
	{	"MONITOR=",		'n',		(ULONG *) &gfx_monitor,		"MONITOR=Monitor nr/N", NULL },

	{NULL,'?',NULL,NULL, NULL}
};


/****************************/
BOOL gfx_GiveArg(const char *arg)
{
	return gfx_process_args( (char *) arg, gfx_commands);
}




/****************************/
VOID gfx_ReleaseArg(VOID)
{
// DBUG("%s:%d\n",__FUNCTION__,__LINE__);
	if (gfx_screenname)
	{
		free(gfx_screenname);
		gfx_screenname = NULL;
	}
}

/****************************/
// Our trasparency hook
static BOOL ismouseon(struct Window *window);

#ifndef __amigaos4__
static void MyTranspHook(struct Hook *hook,struct Window *window,struct TransparencyMessage *msg);

struct Hook transphook =
{
   {NULL, NULL},
   (ULONG (*) (VOID) ) HookEntry,
   (APTR) MyTranspHook,
   NULL
};
#endif

/****************************/
void UpdateGadgets(struct Window * My_Window, int WindowWidth, int WindowHeight)
{
	if(gfx_BorderMode == NOBORDER) return; // javierdlr

	MyDragGadget.LeftEdge = (WORD) 0;
	MyDragGadget.TopEdge  = (WORD) 0;
	MyDragGadget.Width    = (WORD) WindowWidth-10;
	MyDragGadget.Height   = (WORD) 30;

	// But do not say if this func can fail (or give an error return code)
	// then I assume it always success
	AddGadget(My_Window, &MyDragGadget, 0);

	MySizeGadget.LeftEdge = (WORD) WindowWidth-10;
	MySizeGadget.TopEdge  = (WORD) 0;
	MySizeGadget.Width    = (WORD) 10;
	MySizeGadget.Height   = (WORD) WindowHeight;

	// But do not say if this func can fail (or give an error return code)
	// then I assume it always success
	AddGadget(My_Window, &MySizeGadget, 0);

	RefreshGadgets(&MyDragGadget, My_Window, 0);
	RefreshGadgets(&MySizeGadget, My_Window, 0);
}

void RemoveGadgets(struct Window * My_Window)
{
	RemoveGadget(My_Window, &MyDragGadget);
	RemoveGadget(My_Window, &MySizeGadget);
}

void gfx_StartWindow(struct Window *My_Window)
{
	switch (gfx_BorderMode)
	{
		case NOBORDER:
		case TINYBORDER:
		{
			LONG WindowWidth;
			LONG WindowHeight;

			WindowHeight=My_Window->Height - (My_Window->BorderBottom + My_Window->BorderTop);
			WindowWidth=My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);

			UpdateGadgets(My_Window, WindowWidth, WindowHeight);
		}
		break;
		case DISAPBORDER:
		{
#ifndef __amigaos4__
			struct TagItem tags[] =
			{
				{TRANSPCONTROL_REGIONHOOK, (ULONG) &transphook},
				{TAG_DONE}
			};

			TransparencyControl(My_Window, TRANSPCONTROLMETHOD_INSTALLREGIONHOOK, tags);
#endif
		}
	}
}

/****************************/
void gfx_StopWindow(struct Window *My_Window)
{
	if(My_Window)
	{
		switch (gfx_BorderMode)
		{
			case TINYBORDER:
			case NOBORDER:
				RemoveGadgets(My_Window);
				break;
		}
	}
}

void gfx_HandleBorder(struct Window *My_Window, ULONG handle_mouse)
{
	if (gfx_BorderMode == DISAPBORDER && !is_fullscreen)
	{
		ULONG toggleborder = FALSE;

		if (handle_mouse)
		{
			BOOL mouse = ismouseon(My_Window);
			if (mouse != mouseonborder)
			{
				toggleborder = TRUE;
			}
			mouseonborder = mouse;
		}
		else
		{
			toggleborder = TRUE;
		}

		if(toggleborder)
		{
#ifndef __amigaos4__
			TransparencyControl(My_Window,TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
#endif
		}
	}
}

/****************************/
void gfx_Start(struct Window *My_Window)
{
	//if((awport = CreateMsgPort()))
	if((awport = AllocSysObject(ASOT_PORT,NULL)))
	{
		appwin = AddAppWindow(appid, appuserdata, My_Window, awport, NULL);

		if(appwin)
		{
			appwinsig = 1L << awport->mp_SigBit;
		}
	}
}

/****************************/
void gfx_Stop(struct Window *My_Window)
{
	struct AppMessage *amsg;

	if(appwin)
	{
		RemoveAppWindow(appwin);
		appwin = NULL;
	}

	if(awport)
	{
		while((amsg = (struct AppMessage *)GetMsg(awport)))
			ReplyMsg((struct Message *)amsg);

		//DeleteMsgPort(awport);
		FreeSysObject(ASOT_PORT, awport);
		awport = NULL;
	}
}

// markus from UAE comp
/****************************************************/

struct MsgPort *iconifyPort = NULL;
struct DiskObject *_dobj = NULL;
struct AppIcon *appicon = NULL;

bool empty_msg_queue(struct MsgPort *port)
{
	struct Message *msg;

	// empty que.
	while ((msg = (struct Message *) GetMsg( port ) ))
	{
		ReplyMsg( (struct Message *) msg );
		return true;
	}
	return false;
}

/*bool is_uniconified(void)
{
	if (iconifyPort)
	{
		ULONG signal = 1 << iconifyPort->mp_SigBit;
		if (SetSignal(0L, signal) & signal)
		{
			return empty_msg_queue(iconifyPort);
		}
	}
	return false;
}*/

bool enable_Iconify(void)
{
	_dobj = GetDiskObject( fullpath_mplayer );
	if(_dobj == NULL) {
		_dobj = GetDiskObject("ENVARC:Sys/def_shell");
		if(_dobj == NULL) _dobj = GetDiskObjectNew( fullpath_mplayer ); // gets "tool" icon
	}
DBUG("_dobj = 0x%08lx (%s)\n",_dobj,fullpath_mplayer);
	if(_dobj == NULL) {
		EasyRequester(CS(MSG_Warning_Cant_Iconify), CS(MSG_Warning_Button_OK) );
		//EasyRequester(CS(MSG_Warning_Cant_Iconify), CS(MSG_Warning_Button_OK), (STRPTR)fullpath_mplayer);
		return false;
	}

	_dobj -> do_CurrentX = 0;
	_dobj -> do_CurrentY = 0;

	iconifyPort = (struct MsgPort *) AllocSysObject(ASOT_PORT,NULL);
	if (iconifyPort)
	{
		appicon = AddAppIcon(1, 0, "MPlayer", iconifyPort, 0, _dobj,
				WBAPPICONA_SupportsOpen, TRUE,
			TAG_END);
DBUG("appicon = 0x%08lx\n",appicon);
		if (appicon) return true;
	}

	return false;
}

void dispose_Iconify(void)
{
	if (_dobj)
	{
		RemoveAppIcon( appicon );
		FreeDiskObject(_dobj);
		appicon = NULL;
		_dobj = NULL;
	}

	if (iconifyPort)
	{
		FreeSysObject ( ASOT_PORT, iconifyPort );
		iconifyPort = NULL;
	}
}

// markus & javier
BOOL Iconify(struct Window *w)
{
DBUG("Iconify() 0x%08lx\n",appicon);
	if (appicon != NULL) return FALSE;

	if (enable_Iconify())
	{
		//empty_msg_queue(w->UserPort);
		SetWindowAttrs(w,
		               WA_Hidden, TRUE,
		              TAG_END);
		return TRUE;
	}

	return FALSE;
}

BOOL UnIconify(struct Window *w)
{
DBUG("UnIconify() 0x%08lx (0x%08lx)\n",appicon,w);
	if (appicon == NULL) return FALSE;
//DBUG("1 %ld\n",is_uniconified());
//	if( is_uniconified() )
	{
//DBUG("2\n",NULL);
		SetWindowAttrs(w,
		               WA_Hidden,   FALSE,
		               WA_Activate, TRUE,
		              TAG_END);
		WindowToFront(w);
		dispose_Iconify();
	}

	return TRUE;
}
// end markus & javier

/****************************************************/

int secs,secs2,mics,mics2;
int resize_sec;

BOOL is_user_resized = TRUE;


#if 1
extern void AmigaOS_do_appwindow(void);
extern ULONG appwindow_sig;
#else
#define AmigaOS_do_appwindow()
#define appwindow_sig 0
#endif

uint32_t microsec;
static struct timezone dontcare = { 0,0 };
static struct timeval before, after;

BOOL gfx_CheckEvents(struct Screen *My_Screen, struct Window *My_Window, uint32_t *window_height, uint32_t *window_width,
                     uint32_t *window_left, uint32_t *window_top )
{
	ULONG retval = FALSE;
	ULONG info_sig=1L;
	ULONG window_sig ;
	ULONG sig;

	if ( ! MutexAttempt( window_mx ) ) return retval;

	if (My_Window == NULL)
	{
		MutexRelease(window_mx);
		return retval;
	}

	window_sig = 1L << (My_Window->UserPort)->mp_SigBit;

	info_sig = window_sig | appwindow_sig ;

	if (iconifyPort)
	{
		info_sig |= 1 << iconifyPort->mp_SigBit;
	}

	if(AppID) // application.library messsage port
	{
		info_sig |= 1L << applibPort->mp_SigBit;
	}

#ifdef CONFIG_GUI
if(!use_gui)
{
#endif

#ifdef CONFIG_GUI
}
#endif
//DBUG("is_fullscreen=%ld  mouse_hidden=%ld\n",is_fullscreen ,mouse_hidden);
	if (is_fullscreen  &&  !mouse_hidden)
	{
		if (!p_secs1  &&  !p_mics1)
		{
			CurrentTime(&p_secs1, &p_mics1);
		}
		else
		{
			CurrentTime(&p_secs2, &p_mics2);
			if (p_secs2-p_secs1>=2)
			{
				if (!menu_in_use)
				{
					// Ok, let's hide ;)
					gfx_ShowMouse(My_Screen, My_Window, FALSE);
				}
				p_secs1=p_secs2=p_mics1=p_mics2=0;
			}
		}
	}

	sig = SetSignal(0L, info_sig);

	// "application.library"
	if(!is_fullscreen  &&  AppID  &&  (sig & (1L<<applibPort->mp_SigBit)) )
	{
		AmigaOS_do_applib(My_Window);
	}

	if(iconifyPort  &&  (sig & (1L<<iconifyPort->mp_SigBit)))
	{
		UnIconify(My_Window);
	}

// markus iconify
	/*if (iconifyPort)		// Iconified mode...
	{
		if (is_uniconified())
		{
// DBUG("uniconifying\n",NULL);
			/ * SetWindowAttrs(My_Window,
			        WA_Hidden,   FALSE,
			        WA_Activate, TRUE,
				TAG_END);
//javier
			//ActivateWindow(My_Window);
			WindowToFront(My_Window);
//
			dispose_Iconify(); * /
			UnIconify(My_Window);
		}

//		appw_events();
		return retval;
	}*/
// end markus

	if ( sig & appwindow_sig )
	{
		AmigaOS_do_appwindow();
	}

	if( sig & window_sig )
	{
		struct IntuiMessage * IntuiMsg;
		ULONG Class;
		UWORD Code;
		UWORD Qualifier;
// markus
		UWORD GadgetID;
//

		int MouseX, MouseY;
		struct IntuiWheelData *mouse_wheel;
		ULONG w,h;

		while ( ( IntuiMsg = (struct IntuiMessage *) GetMsg( My_Window->UserPort ) ) )
		{
			Class       = IntuiMsg->Class;
			Code        = IntuiMsg->Code;
			Qualifier   = IntuiMsg->Qualifier;
			MouseX      = IntuiMsg->MouseX;
			MouseY      = IntuiMsg->MouseY;
			mouse_wheel = (struct IntuiWheelData *) IntuiMsg -> IAddress;
//			gettimeofday(&before,&dontcare);	// To get time after

			switch( Class )
			{
				case IDCMP_INTUITICKS:
//DBUG("IDCMP_INTUITICKS\n",NULL);
					if (is_fullscreen) break;

					if  ((IntuiMsg->Seconds - resize_sec >1)&&(IntuiMsg->Seconds - resize_sec<=2))
					{
						w = *window_width;
						h = (ULONG)  ((float) *window_width / amiga_aspect_ratio);

						if (My_Screen)
						{
//							Printf("Have My_Screen\n");

							if (h > (My_Screen -> Height - My_Window -> BorderTop - My_Window ->BorderBottom))
							{
								h = My_Screen -> Height - My_Window -> BorderTop - My_Window ->BorderBottom;
								w = (ULONG)  ((float) h * amiga_aspect_ratio);
							}
						}

						if (h<50) h=50;
						if (w<50) w=50;

						if (h !=  *window_height)
						{
							SetWindowAttrs(My_Window,
								WA_InnerWidth, w,
								WA_InnerHeight, h,
							TAG_END);
						}
						resize_sec = IntuiMsg->Seconds - 10;
					}
					break;

				case IDCMP_CLOSEWINDOW:
//					if(spawn_count == 0) mplayer_put_key(KEY_ESC); // that for whole exit when video-window close
					mplayer_put_key(KEY_ESC); // that for whole exit when video-window close
					// mp_input_queue_cmd(mp_input_parse_cmd("stop"));	// For close
					break;

				case IDCMP_ACTIVEWINDOW:
					#ifndef __amigaos4__
						if (gfx_BorderMode == DISAPBORDER) TransparencyControl(My_Window,TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
					#endif
					break;

				case IDCMP_INACTIVEWINDOW:
					#ifndef __amigaos4__
						if (gfx_BorderMode == DISAPBORDER) TransparencyControl(My_Window,TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
					#endif
					break;

				case IDCMP_MOUSEBUTTONS:
					// Blanks pointer stuff
					if (is_fullscreen && mouse_hidden)
					{
						gfx_ShowMouse(My_Screen, My_Window, TRUE);
					}
DBUG("IDCMP_MOUSEBUTTONS\n",NULL);
					switch(Code)
					{
						case SELECTDOWN:
						// mplayer_put_key(MOUSE_BTN0 | MP_KEY_DOWN);
							if (!secs&&!mics)
							{
								secs=IntuiMsg->Seconds;
								mics=IntuiMsg->Micros;
								break; // no need to test double-click
							}
							else
							{
								secs2=IntuiMsg->Seconds;
								mics2=IntuiMsg->Micros;
							}

							if ( DoubleClick(secs, mics, secs2, mics2) )
							{
							#if HAVE_AREXX
DBUG("  SELECTDOWN (double click)\n",NULL);
								put_command0(MP_CMD_VO_FULLSCREEN);
							#endif
								secs = mics = secs2 = mics2 = 0;
								p_secs1 = p_secs2 = p_mics2 = p_mics1 = 0;
								mouse_hidden=FALSE;
							}
							else
							{
								secs = secs2;
								mics = mics2;
							}
							break;

//						case SELECTUP:
//							mplayer_put_key(MOUSE_BTN0);
//							break;

						/*case MENUDOWN:
							mplayer_put_key(MOUSE_BTN1 | MP_KEY_DOWN);
							break;

						case MENUUP:
							mplayer_put_key(MOUSE_BTN1);
							break;

						case MIDDLEDOWN:
							mplayer_put_key(MOUSE_BTN2 | MP_KEY_DOWN);
							#ifdef CONFIG_GUI
							if (use_gui)
							{
								gui_show_gui ^= TRUE;
								guiGetEvent(guiShowPanel, gui_show_gui);
							}
							#endif
							break;

						case MIDDLEUP:
							mplayer_put_key(MOUSE_BTN2);
							break;*/

						default:
							break;
					}

					break;

				case IDCMP_MOUSEMOVE:
					{
						//char cmd_str[40];

						if ((is_fullscreen)&&(mouse_hidden))
						{
								gfx_ShowMouse(My_Screen, My_Window, TRUE);
						}

						//sprintf(cmd_str,"set_mouse_pos %i %i", MouseX-My_Window->BorderLeft, MouseY-My_Window->BorderTop);
						//mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
					}
					break;

				case IDCMP_MENUVERIFY:
					menu_in_use = TRUE;
					set_gfx_rendering_option();	// Can't go fast when you pick in the menu.

					if ((is_fullscreen) && (mouse_hidden))
					{
							gfx_ShowMouse(My_Screen, My_Window, TRUE);
					}
					break;

				case IDCMP_MENUPICK:
//DBUG("IDCMP_MENUPICK\n",NULL);
					menu_events( IntuiMsg );
					menu_in_use = FALSE;
					set_gfx_rendering_option();
					break;

				case IDCMP_REFRESHWINDOW:
					BeginRefresh(My_Window);
					EndRefresh(My_Window,TRUE);
					break;

			// markus
			case IDCMP_GADGETUP:
// javier
				GadgetID = (IntuiMsg -> IAddress) ? ((struct Gadget *) ( IntuiMsg -> IAddress)) -> GadgetID : 0 ;
//
				switch (GadgetID)
				{
					case GID_ICONIFY:
						if (enable_Iconify())
						{
							empty_msg_queue(My_Window->UserPort);
							SetWindowAttrs(My_Window, WA_Hidden,TRUE, TAG_END);
						}
						// printf ("Markus message iconify \n!!!");
						break;

					case GID_FULLSCREEN:
					// #if HAVE_AREXX
					// put_command0(MP_CMD_VO_FULLSCREEN);
					// #else
						mplayer_put_key('f');   // Fullscreen?
					// #endif

					// toggle_fullscreen();
					// printf ("Markus message fullscreen \n!!!");
						break;

					/*case GID_PADLOCK:
					// toggle_mousegrab();
					printf ("Markus message padlock \n!!!");
						break;*/

					default:
						break;

				}
				break;

				//
				case IDCMP_CHANGEWINDOW:
					*window_left = My_Window->LeftEdge;
					*window_top  = My_Window->TopEdge;
					*window_height = My_Window->Height - (My_Window->BorderBottom + My_Window->BorderTop);
					*window_width  = My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);

					if(gfx_BorderMode == NOBORDER || gfx_BorderMode == TINYBORDER)
					{
						RemoveGadgets(My_Window);
						UpdateGadgets(My_Window, *window_width, *window_height);
					}

					resize_sec = IntuiMsg->Seconds;
					retval = TRUE;

/* // from vo_amiga.c
                       if (0 == pip_format) { flip_page(); }
#if 0
                         uint32 x_offset, y_offset, inner_width, inner_height;

                         IIntuition->GetWindowAttrs(My_Window, WA_InnerWidth, &inner_width, WA_InnerHeight, &inner_height, TAG_DONE);

                         // Change the window size to fill the whole screen with good aspect
                         if ( ( (float)old_d_width / (float)old_d_height) < ( (float)inner_width / (float) inner_height) )
                         {

                                 x_offset = inner_width;
                                 y_offset = inner_height + inner_width * ( (float) old_d_height / old_d_width);
                                 y_offset /= 2;
#endif
                         } else {

                                 y_offset = inner_height;
                                 x_offset = inner_width + inner_height * ( (float) old_d_width / old_d_height);
                                 x_offset /= 2;
#endif
                         }

// printf("new: %ld, %ld\n\n", x_offset, y_offset);

                         {
                             static uint32 old_x_offset = 0;
                             static uint32 old_y_offset = 0;

                             if (x_offset != old_x_offset || y_offset != old_y_offset)
                             {
                                 IIntuition->SetWindowAttrs(My_Window,
                                                            WA_InnerWidth,  x_offset,
                                                            WA_InnerHeight, y_offset,
                                                            TAG_DONE);
                             }

                             old_x_offset = x_offset;
                             old_y_offset = y_offset;
                         }
*/
					break;

				#ifdef __amigaos4__
				case IDCMP_EXTENDEDMOUSE:
					if (mouse_wheel -> WheelY < 0)
					{
						mplayer_put_key(KEY_RIGHT); break;
					}
					else	if (mouse_wheel->WheelY>0)
					{
						mplayer_put_key(KEY_LEFT); break;
					}
					break;
				#endif

case IDCMP_VANILLAKEY:
	switch ( Code )
	{
		case '\e':
			mplayer_put_key(KEY_ESC);
		break;

		default:
			if (Code!='\0') mplayer_put_key(Code); break;
	}
break;

				case IDCMP_RAWKEY:
					switch ( Code )
					{
						case RAWKEY_ESCAPE  : mplayer_put_key(KEY_ESC); break;
						case RAWKEY_PAGEDOWN: mplayer_put_key(KEY_PGDWN); break;
						case RAWKEY_PAGEUP  : mplayer_put_key(KEY_PGUP); break;

						#ifndef __amigaos4__
						case NM_WHEEL_UP: // MouseWheel rulez !
						case NM_WHEEL_DOWN:

						case RAWKEY_RIGHT: mplayer_put_key(KEY_RIGHT); break;
						case RAWKEY_LEFT : mplayer_put_key(KEY_LEFT); break;
						case RAWKEY_UP   : mplayer_put_key(KEY_UP); break;
						case RAWKEY_DOWN : mplayer_put_key(KEY_DOWN); break;
						#endif

						#ifdef __amigaos4__
						case RAWKEY_CRSRRIGHT: mplayer_put_key(KEY_RIGHT); break;
						case RAWKEY_CRSRLEFT : mplayer_put_key(KEY_LEFT); break;
						case RAWKEY_CRSRUP   : mplayer_put_key(KEY_UP); break;
						case RAWKEY_CRSRDOWN : mplayer_put_key(KEY_DOWN); break;
						#endif

						case RAWKEY_F1       : mplayer_put_key(KEY_F+1); break;
						case RAWKEY_F2       : mplayer_put_key(KEY_F+2); break;
						case RAWKEY_F3       : mplayer_put_key(KEY_F+3); break;
						case RAWKEY_F4       : mplayer_put_key(KEY_F+4); break;
						case RAWKEY_F5       : mplayer_put_key(KEY_F+5); break;
						case RAWKEY_F6       : mplayer_put_key(KEY_F+6); break;
						case RAWKEY_F7       : mplayer_put_key(KEY_F+7); break;
						case RAWKEY_F8       : mplayer_put_key(KEY_F+8); break;
						case RAWKEY_F9       : mplayer_put_key(KEY_F+9); break;
						case RAWKEY_F10      : mplayer_put_key(KEY_F+10); break;
						case RAWKEY_F11      : mplayer_put_key(KEY_F+11); break;
						case RAWKEY_F12      : mplayer_put_key(KEY_F+12); break;
						case RAWKEY_RETURN   : mplayer_put_key(KEY_ENTER); break;
						case RAWKEY_TAB      : mplayer_put_key(KEY_TAB); break;
						case RAWKEY_CONTROL  : mplayer_put_key(KEY_CTRL); break;
						case RAWKEY_BACKSPACE: mplayer_put_key(KEY_BACKSPACE); break;
						case RAWKEY_DELETE   : mplayer_put_key(KEY_DELETE); break;
						case RAWKEY_INSERT   : mplayer_put_key(KEY_INSERT); break;
						case RAWKEY_HOME     : mplayer_put_key(KEY_HOME); break;
						case RAWKEY_END      : mplayer_put_key(KEY_END); break;
						case RAWKEY_KP_ENTER : mplayer_put_key(KEY_KPENTER); break;
						case RAWKEY_KP_1     : mplayer_put_key(KEY_KP1); break;
						case RAWKEY_KP_2     : mplayer_put_key(KEY_KP2); break;
						case RAWKEY_KP_3     : mplayer_put_key(KEY_KP3); break;
						case RAWKEY_KP_4     : mplayer_put_key(KEY_KP4); break;
						case RAWKEY_KP_5     : mplayer_put_key(KEY_KP5); break;
						case RAWKEY_KP_6     : mplayer_put_key(KEY_KP6); break;
						case RAWKEY_KP_7     : mplayer_put_key(KEY_KP7); break;
						case RAWKEY_KP_8     : mplayer_put_key(KEY_KP8); break;
						case RAWKEY_KP_9     : mplayer_put_key(KEY_KP9); break;
						case RAWKEY_KP_0     : mplayer_put_key(KEY_KP0); break;

						case RAWKEY_LAMIGA:
						case RAWKEY_RAMIGA: break;

						/*default:
							{
								struct InputEvent ie;
								TEXT c;

								ie.ie_Class        = IECLASS_RAWKEY;
								ie.ie_SubClass     = 0;
								ie.ie_Code         = Code;
								ie.ie_Qualifier    = Qualifier;
								ie.ie_EventAddress = NULL;

								if (MapRawKey(&ie, &c, 1, NULL) == 1)
								{
									mplayer_put_key(c);
								}
							 }*/

					}
					break;
			}
			ReplyMsg( (struct Message *) IntuiMsg);
//			gettimeofday(&after,&dontcare);		// To get time after
//			microsec = (after.tv_usec - before.tv_usec) +(1000000 * (after.tv_sec - before.tv_sec));
//			printf("Event Class %08X - %d us\n", Class, microsec);
	    }
   }
   /*else if(SetSignal(0L, appwinsig ) & appwinsig) // Handle Dropped files
   {
	    struct AppMessage *amsg;
	    struct WBArg   *argptr;

		while ((amsg = (struct AppMessage *) GetMsg(awport)))
		{
			LONG i;

			argptr = amsg->am_ArgList;

			for (i = 0; i < amsg->am_NumArgs; i++)
			{
				TEXT path[512];

				NameFromLock(argptr->wa_Lock, path, sizeof(path));

				AddPart(path, argptr->wa_Name, sizeof(path));

				#ifdef CONFIG_GUI
				if (use_gui)
				{
					guiGetEvent(guiLoadFile, path);
					break; // Let's just handle the first file
				}
				else
				#endif
				{
					TEXT line[512];
					mp_cmd_t * cmd;

					snprintf(line, sizeof(line), "loadfile \"%s\"", path);

					cmd	= mp_input_parse_cmd(line);

					if(cmd)
					{
						if(mp_input_queue_cmd(cmd) == 0)
						{
							mp_cmd_free(cmd);
						}
					}
				}

				argptr++;
			}
			ReplyMsg((struct Message *) amsg);
		}
   }*/

   MutexRelease(window_mx);

   return retval;
}

/****************************/
static BOOL ismouseon(struct Window *window)
{
	if ( ( (window->MouseX >= 0) && (window->MouseX < window->Width) ) &&
	( (window->MouseY >= 0) && (window->MouseY < window->Height) ) ) return TRUE;

	return FALSE;
}

#ifndef __amigaos4__
static void MyTranspHook(struct Hook *hook,struct Window *window,struct TransparencyMessage *msg)
{
	struct Rectangle rect;

	/* Do not hide border if the pointer is inside and the window is activated */
	if ( ismouseon(window) && (window->Flags & WFLG_WINDOWACTIVE) ) return;

	/* Make top border transparent */
	rect.MinX = 0;
	rect.MinY = 0;
	rect.MaxX = window->Width - 1;
	rect.MaxY = window->BorderTop - 1;

	OrRectRegion(msg->Region,&rect);

	/* Left border */
	rect.MinX = 0;
	rect.MinY = window->BorderTop;
	rect.MaxX = window->BorderLeft - 1;
	rect.MaxY = window->Height - window->BorderBottom - 1;

	OrRectRegion(msg->Region,&rect);

	/* Right border */
	rect.MinX = window->Width - window->BorderRight;
	rect.MinY = window->BorderTop;
	rect.MaxX = window->Width - 1;
	rect.MaxY = window->Height - window->BorderBottom - 1;

	OrRectRegion(msg->Region,&rect);

	/* Bottom border */
	rect.MinX = 0;
	rect.MinY = window->Height - window->BorderBottom;
	rect.MaxX = window->Width - 1;
	rect.MaxY = window->Height - 1;

	OrRectRegion(msg->Region,&rect);
}
#endif

static int blanker_count = 0; /* Not too useful, but it should be 0 at the end */

void gfx_ControlBlanker(struct Screen * screen, ULONG enable)
{
	if(enable)
	{
		blanker_count++;
	}
	else
	{
		blanker_count--;
	}

	if (AppID > 0)
	{
//		if (IApplication)
//		{
			SetApplicationAttrs(AppID, APPATTR_AllowsBlanker, enable ? TRUE : FALSE, TAG_DONE);
//		}
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: %s blanker\n", enable ? "Enabling" : "Disabling" );
}

/* Just here for debug purpose */
void gfx_BlankerState(void)
{
//	  kprintf("Blanker_count = %d\n", blanker_count);
}

void gfx_ShowMouse(struct Screen * screen, struct Window * window, ULONG enable)
{
	if(enable)
	{
		ClearPointer(window);
	}
	else if(EmptyPointer)
	{
		SetPointer(window, EmptyPointer, 1, 16, 0, 0);
	}
	mouse_hidden = !enable;
}

/*#ifdef __old__
void gfx_get_max_mode(int depth_bits, ULONG *mw,ULONG *mh)
{
	struct List *ml;
	struct P96Mode	*mn;
	ULONG w,h;
	ULONG a,ma = 0;

	if(ml=p96AllocModeListTags(P96MA_MinDepth, depth_bits, P96MA_MaxDepth, depth_bits,  TAG_DONE))
	{
		for(mn=(struct P96Mode *)(ml->lh_Head);mn->Node.ln_Succ;mn=(struct P96Mode *)mn->Node.ln_Succ)
		{
				w = p96GetModeIDAttr( mn -> DisplayID, P96IDA_WIDTH );
				h = p96GetModeIDAttr( mn -> DisplayID, P96IDA_HEIGHT );
				a = w * h;
				if (a >ma) { ma = a; *mw = w; *mh = h; }
		}
		p96FreeModeList(ml);
	}
}
#endif*/

void gfx_get_max_mode(int RTGBoardNum, int depth_bits, ULONG *mw,ULONG *mh)
{
	ULONG ID;
	ULONG w,h;
	struct DisplayInfo dispi;
	struct DimensionInfo dimi;
	ULONG a,ma = 0;

	depth_bits = depth_bits == 32 ? 24 : depth_bits;

	*mw = 0;
	*mh = 0;

	for( ID = NextDisplayInfo( INVALID_ID ) ; ID !=INVALID_ID ;  ID = NextDisplayInfo( ID ) )
	{
		if (GetDisplayInfoData( NULL, &dispi, sizeof(dispi) ,  DTAG_DISP, ID))
		{
			if (dispi.RTGBoardNum == RTGBoardNum )
			{
				if (GetDisplayInfoData( NULL, &dimi, sizeof(dimi) , DTAG_DIMS, ID))
				{
					if (dimi.MaxDepth == depth_bits)
					{
						w =  dimi.Nominal.MaxX -dimi.Nominal.MinX +1;
						h =  dimi.Nominal.MaxY -dimi.Nominal.MinY +1;
						a = w * h;
						if (a >ma) { ma = a; *mw = w; *mh = h; }
					}
				}
			}
		}
	}
}

void print_screen_info( ULONG monitor, ULONG ModeID )
{
	int w,h;
	struct DimensionInfo di;
	GetDisplayInfoData( NULL, &di, sizeof(di) , DTAG_DIMS, ModeID);

	w =  di.Nominal.MaxX -di.Nominal.MinX +1;
	h =  di.Nominal.MaxY -di.Nominal.MinY +1;

	printf("Monitor #%d, Screen mode ID = 0x%08X (%dx%d)\n",monitor, ModeID,w,h);
}


uint32 find_best_screenmode(int RTGBoardNum, uint32 depth_bits, float aspect, int get_w, int get_h)
{
	ULONG ID;
	struct DisplayInfo dispi;
	struct DimensionInfo di;
	ULONG w,h;

	float aspect_mode;
	uint64 a,get_a, diff_a, found_a, found_better_a;
	uint32 found_mode, better_found_mode;

	if (aspect == -1.0f)
		printf("\nTrying to find the best screen without caring about aspect,\n");
	else
		printf("\nTrying to find the best screen with aspect %0.2f:1\n(Same aspect as your Workbench screen)\n\n",aspect);

	get_a = get_w * get_h;

	aspect = (float) ((int) (aspect * 100));

	found_mode = INVALID_ID;
	better_found_mode = INVALID_ID;

	found_a = ~0;
	found_better_a = ~0;

	depth_bits = depth_bits == 32 ? 24 : depth_bits;

	for( ID = NextDisplayInfo( INVALID_ID ) ; ID !=INVALID_ID ;  ID = NextDisplayInfo( ID ) )
	{

		if (
			(GetDisplayInfoData( NULL, &di, sizeof(di) , DTAG_DIMS, ID)) &&
			(GetDisplayInfoData( NULL, &dispi, sizeof(dispi) ,  DTAG_DISP, ID))
		)
		{
			if ((depth_bits == di.MaxDepth ) && (dispi.RTGBoardNum == RTGBoardNum ))
			{
				w =  di.Nominal.MaxX -di.Nominal.MinX +1;
				h =  di.Nominal.MaxY -di.Nominal.MinY +1;
				a = w * h;

				aspect_mode =(float) ((int) (100 * ((float)  w/ (float) h)));

				if ((aspect == aspect_mode)||(aspect == -100.0f))
				{
					if (a>get_a)
					{
						diff_a = a - get_a;

						if (diff_a <= found_better_a)
						{
							found_better_a = diff_a;
							better_found_mode = ID;
						}
					}

					diff_a = a < get_a ? get_a -a 	: a - get_a;

					if (diff_a <= found_a)
					{
						found_a = diff_a;
						found_mode = ID;
					}

//					printf("Mode %X w %d h %d d %d aspect %0.2f:1 arial %llu\n", ID, w, h , di.MaxDepth, aspect_mode / 100 , a );
				}
				else
				{
//					printf("(Mode %X w %d h %d d %d aspect %0.2f:1 arial %llu)\n", ID, w, h , di.MaxDepth, aspect_mode / 100  , a  );
				}
			}
		}
	}

	if ( (better_found_mode != INVALID_ID ? better_found_mode : found_mode) == 0)
	{
		printf("No mode found, screen mode not supported and this should not happen\n");
		printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);

		for( ID = NextDisplayInfo( INVALID_ID ) ; ID !=INVALID_ID ;  ID = NextDisplayInfo( ID ) )
		{
			if (
				(GetDisplayInfoData( NULL, &di, sizeof(di) , DTAG_DIMS, ID)) &&
				(GetDisplayInfoData( NULL, &dispi, sizeof(dispi) ,  DTAG_DISP, ID))
			)
			{

				if (dispi.RTGBoardNum == RTGBoardNum )
				{
					w =  di.Nominal.MaxX -di.Nominal.MinX +1;
					h =  di.Nominal.MaxY -di.Nominal.MinY +1;

					if ((w>0)&&(h>0)&&(depth_bits == di.MaxDepth)  )
					{
						aspect_mode =(float) ((int) (100 * ((float)  w/ (float) h)));
						printf("Mode %lX w %lu h %lu d %lu aspect %0.2f:1\n", ID, w, h , di.MaxDepth, aspect_mode / 100  );
					}
				}
			}
		}
	}

	return better_found_mode != INVALID_ID ? better_found_mode : found_mode;
}


void gfx_center_window(struct Screen *My_Screen, uint32_t window_width, uint32_t window_height, uint32_t *win_left, uint32_t *win_top)
{
	struct DrawInfo *dri;
	ULONG bw, bh;
	bw =0; bh=0;

	if ( (dri = GetScreenDrawInfo(My_Screen) ) )
	{
		ULONG bw=0, bh=0;

		switch(gfx_BorderMode)
		{
			case NOBORDER:
				bw = 0;
				bh = 0;
				break;

#ifdef __amigaos4__
		default:
				bw = My_Screen->WBorLeft + My_Screen->WBorRight;
				bh = My_Screen->WBorTop + My_Screen->Font->ta_YSize + 1 + My_Screen->WBorBottom;
#endif
#ifdef __morphos__
			default:
				bw = GetSkinInfoAttrA(dri, SI_BorderLeft, NULL) + GetSkinInfoAttrA(dri, SI_BorderRight, NULL);
				bh = GetSkinInfoAttrA(dri, SI_BorderTopTitle, NULL) + GetSkinInfoAttrA(dri, SI_BorderBottom, NULL);
#endif
		}

		*win_left = (My_Screen->Width - (window_width + bw)) / 2;
		*win_top  = (My_Screen->Height - (window_height + bh)) / 2;

		FreeScreenDrawInfo(My_Screen, dri);
	}
}

/************************** CHECK_RADEON HD/RX ***********************/
BOOL isRadeonHDorSM502_chip(struct Screen *scr)
{
	ULONG DisplayID = 0;
	struct DisplayInfo display_info;
	char *ChipDriverName = 0;

	GetScreenAttr( scr, SA_DisplayID, &DisplayID , sizeof(DisplayID)  );
	GetDisplayInfoData( NULL, &display_info, sizeof(display_info) , DTAG_DISP, DisplayID);
	GetBoardDataTags( display_info.RTGBoardNum, GBD_ChipDriver, &ChipDriverName, TAG_END);
DBUG("isRadeonHDorSM502_chip() %s\n",ChipDriverName);
	if(strcasecmp(ChipDriverName,"RadeonHD.chip") == 0
	   || strcasecmp(ChipDriverName,"RadeonRX.chip") == 0
	   || strcasecmp(ChipDriverName,"SiliconMotion502.chip") == 0)
		return TRUE;

	return FALSE;
}

/**************** FULLSCREEN CinemaScopeTM black bars ****************/
void BackFillfunc(struct Hook *hook UNUSED, struct RastPort *rp, struct BackFillMessage *msg)
{
	struct RastPort newrp;

	// Remove any clipping
	newrp = *rp;
	newrp.Layer = NULL;
//DBUG("BackFillfunc()\n",NULL);
	if(!msg) return;
//DBUG("  RectFillColor()\n",NULL);
	RectFillColor(&newrp, msg->Bounds.MinX, msg->Bounds.MinY,
	              msg->Bounds.MaxX, msg->Bounds.MaxY, 0xff000000); // black color
}