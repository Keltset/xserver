/*
 * Copyright 2007-2008 Peter Hutterer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Peter Hutterer, University of South Australia, NICTA
 */

/***********************************************************************
 *
 * Request to query the pointer location of an extension input device.
 *
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <X11/X.h>	/* for inputstr.h    */
#include <X11/Xproto.h>	/* Request macro     */
#include "inputstr.h"	/* DeviceIntPtr      */
#include "windowstr.h"	/* window structure  */
#include <X11/extensions/XI.h>
#include <X11/extensions/XI2proto.h>
#include "extnsionst.h"
#include "exevents.h"
#include "exglobals.h"
#include "eventconvert.h"
#include "xkbsrv.h"

#ifdef PANORAMIX
#include "panoramiXsrv.h"
#endif

#include "xiquerypointer.h"

/***********************************************************************
 *
 * This procedure allows a client to query the pointer of a device.
 *
 */

int
SProcXIQueryPointer(ClientPtr client)
{
    char n;

    REQUEST(xXIQueryPointerReq);
    swaps(&stuff->length, n);
    return (ProcXIQueryPointer(client));
}

int
ProcXIQueryPointer(ClientPtr client)
{
    int rc;
    xXIQueryPointerReply rep;
    DeviceIntPtr pDev, kbd;
    WindowPtr pWin, t;
    SpritePtr pSprite;
    XkbStatePtr state;
    char *buttons = NULL;

    REQUEST(xXIQueryPointerReq);
    REQUEST_SIZE_MATCH(xXIQueryPointerReq);

    rc = dixLookupDevice(&pDev, stuff->deviceid, client, DixReadAccess);
    if (rc != Success)
        return rc;

    if (pDev->valuator == NULL)
    {
        client->errorValue = stuff->deviceid;
        return BadDevice;
    }

    rc = dixLookupWindow(&pWin, stuff->win, client, DixReadAccess);
    if (rc != Success)
    {
        SendErrorToClient(client, IReqCode, X_XIQueryPointer,
                stuff->win, rc);
        return Success;
    }

    if (pDev->valuator->motionHintWindow)
        MaybeStopHint(pDev, client);

    kbd = GetPairedDevice(pDev);

    pSprite = pDev->spriteInfo->sprite;
    rep.repType = X_Reply;
    rep.RepType = X_XIQueryPointer;
    rep.length = 5;
    rep.sequenceNumber = client->sequence;
    rep.root = (GetCurrentRootWindow(pDev))->drawable.id;
    rep.root_x = FP1616(pSprite->hot.x, 0);
    rep.root_y = FP1616(pSprite->hot.y, 0);
    rep.child = None;

    state = &kbd->key->xkbInfo->prev_state;
    rep.mods.base_mods = state->base_mods;
    rep.mods.latched_mods = state->latched_mods;
    rep.mods.locked_mods = state->locked_mods;

    rep.group.base_group = state->base_group;
    rep.group.latched_group = state->latched_group;
    rep.group.locked_group = state->locked_group;

    if (pDev->button)
    {
        int i, down;
        rep.buttons_len = (((pDev->button->numButtons + 7)/8) + 3)/4;
        rep.length += rep.buttons_len;
        buttons = xcalloc(rep.buttons_len, 4);
        if (!buttons)
            return BadAlloc;

        down = pDev->button->buttonsDown;

        for (i = 0; i < pDev->button->numButtons && down; i++)
        {
            if (BitIsOn(pDev->button->down, i))
            {
                SetBit(buttons, i);
                down--;
            }
        }
    } else
        rep.buttons_len = 0;

    if (pSprite->hot.pScreen == pWin->drawable.pScreen)
    {
        rep.same_screen = xTrue;
        rep.win_x = FP1616(pSprite->hot.x - pWin->drawable.x, 0);
        rep.win_y = FP1616(pSprite->hot.y - pWin->drawable.y, 0);
        for (t = pSprite->win; t; t = t->parent)
            if (t->parent == pWin)
            {
                rep.child = t->drawable.id;
                break;
            }
    } else
    {
        rep.same_screen = xFalse;
        rep.win_x = 0;
        rep.win_y = 0;
    }

#ifdef PANORAMIX
    if(!noPanoramiXExtension) {
        rep.root_x += FP1616(panoramiXdataPtr[0].x, 0);
        rep.root_y += FP1616(panoramiXdataPtr[0].y, 0);
        if (stuff->win == rep.root)
        {
            rep.win_x += FP1616(panoramiXdataPtr[0].x, 0);
            rep.win_y += FP1616(panoramiXdataPtr[0].y, 0);
        }
    }
#endif

    WriteReplyToClient(client, sizeof(xXIQueryPointerReply), &rep);
    if (buttons)
        WriteToClient(client, rep.buttons_len * 4, buttons);

    xfree(buttons);

    return Success;
}

/***********************************************************************
 *
 * This procedure writes the reply for the XIQueryPointer function,
 * if the client and server have a different byte ordering.
 *
 */

void
SRepXIQueryPointer(ClientPtr client, int size,
                   xXIQueryPointerReply * rep)
{
    char n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    WriteToClient(client, size, (char *)rep);
}
