#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>

#ifndef POP_BRIGHTEN
#define POP_BRIGHTEN   0
#endif

#ifndef POP_DARKEN
#define POP_DARKEN     1
#endif

#ifndef ProcessPixelArray
VOID ProcessPixelArray(struct RastPort *, ULONG, ULONG, ULONG, ULONG, ULONG, LONG, struct TagItem *);
#endif

#include "gui/interface.h"
#include "gui.h"

struct SpacerData
{
	ULONG brightness_threshold;
	ULONG brightness_checked;
};

DEFNEW(Spacer)
{
	FORTAG( INITTAGS )
	{
		case MUIA_Background:
			tag->ti_Tag = TAG_IGNORE;
			break;

		case MUIA_Frame:
			tag->ti_Tag = TAG_IGNORE;
			break;
	}
	NEXTTAG

	obj = (Object *)DoSuperNew(cl, obj,
		MUIA_CycleChain,   FALSE,
		InnerSpacing(0,0),
		TAG_MORE, INITTAGS
	);

	if (obj)
	{
		struct SpacerData *data = INST_DATA(cl,obj);

		data->brightness_checked   = FALSE;
		data->brightness_threshold = 255;
	}

	return ((IPTR)obj);
}

#define SPACE_WIDTH  10

DEFMMETHOD(AskMinMax)
{
	ULONG maxwidth, defwidth, minwidth;

	DOSUPER;

	maxwidth = SPACE_WIDTH;
	defwidth = SPACE_WIDTH;
	minwidth = SPACE_WIDTH;

	msg->MinMaxInfo->MinWidth  += minwidth;
	msg->MinMaxInfo->DefWidth  += defwidth;
	msg->MinMaxInfo->MaxWidth  += maxwidth;

	msg->MinMaxInfo->MinHeight += 22;
	msg->MinMaxInfo->MaxHeight += 48;
	msg->MinMaxInfo->DefHeight += 22;

	return 0;
}

DEFMMETHOD(Draw)
{
	DOSUPER;

	if (msg->flags & MADF_DRAWOBJECT)
	{
		struct RastPort *rp = _rp(obj);

		ULONG mleft         = _mleft(obj);
		ULONG mtop          = _mtop(obj);
		ULONG mwidth        = _mwidth(obj);
		ULONG mheight       = _mheight(obj);

		ULONG vertoffs   = mleft + (mwidth / 2);

#if !defined(__AROS__)
		ProcessPixelArray( rp, vertoffs,     mtop + 1, 1, mheight - 2, POP_BRIGHTEN, 70, NULL);
		ProcessPixelArray( rp, vertoffs - 1, mtop + 1, 1, mheight - 2, POP_DARKEN,   70, NULL);
#endif
	}

	return 0;
}

BEGINMTABLE2(spacerclass)
DECNEW(Spacer)
DECMMETHOD(AskMinMax)
DECMMETHOD(Draw)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Area, spacerclass, SpacerData)

