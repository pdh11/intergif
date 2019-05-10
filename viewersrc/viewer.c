/* viewer.c */

/* Animated GIF viewer based on InterGif
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *
 * History:
 *      08-Feb-98 pdh Swiped from InterGif
 *      21-Feb-98 *** Release 1.00 (InterGif 6.03)
 *      07-Jun-98 pdh Added number of frames to titlebar
 *      07-Jun-98 *** Release 1.01 (InterGif 6.04)
 *      21-Aug-98 *** Release 1.02 (InterGif 6.05)
 *      05-Oct-98 *** Release 1.03 (InterGif 6.06)
 *      18-Jan-99 *** Release 1.04 (InterGif 6.07
 *      18-Jan-99 *** Release 1.05 (InterGif 6.10)
 *      10-Dec-00 *** Release 1.06 (InterGif 6.12)
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "kernel.h"

#include "DeskLib:Wimp.h"
#include "DeskLib:WimpSWIs.h"
#include "DeskLib:GFX.h"
#include "DeskLib:Menu.h"
#include "DeskLib:Screen.h"
#include "DeskLib:Template.h"
#include "DeskLib:DragASpr.h"
#include "DeskLib:Window.h"
#include "DeskLib:Error.h"
#include "DeskLib:Icon.h"
#include "DeskLib:Sprite.h"
#include "DeskLib:Hourglass.h"
#include "DeskLib:Menu.h"

/* #include "Extras:File.h" */
#define fdebugf 1?0:fprintf
/* #define fdebugf fprintf */

#include "AnimLib.h"

static int quitapp = FALSE;
static int ackpending = FALSE;
static event_pollblock e;
static task_handle me;
static icon_handle iBarIcon;
static window_handle wInfo;

#define playicon_START 3
#define playicon_BACKWARDS 0
#define playicon_STOP 1
#define playicon_FORWARDS 2
#define playicon_END 4

typedef struct viewstr {
    window_handle wFilm, wPlay;
    anim a;
    sprite_areainfo *buffer;
    char *leafname;
    int nFrame;
    int nNextFrameTime;
    int nPlaying;   /* 0, +1, -1 */
    int nBGColour;
    struct viewstr *pNext;
    struct viewstr *pNextPlaying;
} viewstr, *view;

static viewstr *views = NULL;
static viewstr *playlist = NULL;

extern window_block Template_film;
extern window_block Template_play;
extern window_block Template_info;
extern sprite_areainfo Sprite_Area;
extern menu_block Menu_iconbar;

extern int OS_ReadMonotonicTime(void);

static wimp_point playtools; /* Size of playtools window */

#if 0
extern void Error_Report(int errornum, char *report, ...)
{
    va_list ap;
    os_error    error;
    error_flags eflags;

    va_start( ap, report );

        vsprintf(error.errmess, report, ap);
        error.errnum = errornum;

        eflags.value = 1;
        (void) Wimp_ReportError(&error, eflags.value, "InterGif Viewer");

    va_end(ap);
}
#endif

#define message_OPENURL 0x4AF80

static os_error *Internet_OpenURL( const char *url )
{
    message_block m;

    strcpy( m.data.bytes, url );
    m.header.size = 256;
    m.header.myref = 0;
    m.header.action = (message_action) message_OPENURL;

    Wimp_SendMessage( event_SEND, &m, 0, 0 );

    /* NB, we could do better than this: we could check whether that message
     * bounces and, if it does, run "Wimp_Task <Alias$URLOpen_HTTP> url".
     * However, this could be "severe" e.g. cause dial-on-demand or whatever
     * and so we don't bother.
     *     The protocol is described in full on Ant Limited's Web site, but
     * everyone else's browsers follow it too.
     */
    return NULL;
}

static void View_Goto( view v, int n )
{
    if ( n != v->nFrame && v->a )
    {
        sprite_header *spr = (sprite_header*)(v->buffer + 1);
        pixel *img = (pixel*)spr + spr->imageoffset, *mask;
        frame f = v->a->pFrames + n;
        window_redrawblock wrb;
        BOOL more;

        Anim_DecompressAligned( f->pImageData, f->nImageSize,
                                v->a->nWidth, v->a->nHeight, img );

        mask = (pixel*)spr + spr->maskoffset;

        if ( f->pMaskData )
        {
            int i,n = spr->maskoffset - spr->imageoffset;
            int bg = v->nBGColour;

            fdebugf( stderr, "view_goto: bg=%d\n", bg );

            Anim_DecompressAligned( f->pMaskData, f->nMaskSize,
                                    v->a->nWidth, v->a->nHeight, mask );

            for ( i=0; i<n; i++ )
                if ( !mask[i] )
                {
                    img[i] = bg;
                    mask[i] = 255;
                }
        }
        else
            memset( mask, 255, (spr->maskoffset - spr->imageoffset) );

/*         File_SaveMemoryStamped( "<intergif$viewer$dir>.dump",0xff9, */
/*                                 ((char*)v->buffer)+4, ((char*)v->buffer) + v->buffer->areasize ); */

        wrb.window = v->wFilm;
        wrb.rect.min.x = wrb.rect.max.y = 0;
        wrb.rect.max.x = 0x8000;
        wrb.rect.min.y = -0x8000;
        Wimp_UpdateWindow( &wrb, &more );

        while ( more )
            Wimp_GetRectangle( &wrb, &more );

        v->nFrame = n;
    }
}

static void View_Stop( view v )
{
    view *pv;

    if ( v->nPlaying != 0 )
        for ( pv = &playlist; *pv; pv = &((*pv)->pNextPlaying) )
            if ( *pv == v )
            {
                *pv = v->pNextPlaying;
                v->nPlaying = 0;
                Icon_Deselect( v->wPlay, playicon_FORWARDS );
                Icon_Deselect( v->wPlay, playicon_BACKWARDS );
                break;
            }
}

static void View_NextFrame( view v, int nHowPlaying )
{
    int n;

    if ( nHowPlaying > 0 )
    {
        n = v->nFrame + 1;

        if ( n == v->a->nFrames )
        {
            if ( v->a->flags & animflag_LOOP )
                n = 0;
            else
            {
                View_Stop(v);
                return;
            }
        }
    }
    else
    {
        n = v->nFrame - 1;

        if ( n == -1 )
        {
            if ( v->a->flags & animflag_LOOP )
                n = v->a->nFrames-1;
            else
            {
                View_Stop(v);
                return;
            }
        }
    }
    View_Goto( v, n );
    v->nNextFrameTime += v->a->pFrames[n].csDelay;

    if ( v->nNextFrameTime <= OS_ReadMonotonicTime() )
        v->nNextFrameTime = OS_ReadMonotonicTime()+1;
    fdebugf( stderr, "next: nft=%u\n", v->nNextFrameTime );
}

static void View_Play( view v, BOOL bForwards )
{
    if ( v->nPlaying == 0)
    {
        v->pNextPlaying = playlist;
        playlist = v;
    }

    if ( v->nPlaying != (bForwards ? 1 : -1) )
    {
        int t = OS_ReadMonotonicTime();
        v->nNextFrameTime = t + v->a->pFrames[v->nFrame].csDelay;
        v->nPlaying = bForwards ? 1 : -1;
        Icon_Select( v->wPlay, bForwards ? playicon_FORWARDS
                                         : playicon_BACKWARDS );
        Icon_Deselect( v->wPlay, bForwards ? playicon_BACKWARDS
                                         : playicon_FORWARDS );
    }
}

static void View_Open( view v, window_openblock *wob )
{
    window_openblock wob2;

    if ( v->wPlay )
    {
        wob2 = *wob;
        wob2.window = v->wPlay;
        wob2.screenrect.max.y = wob2.screenrect.min.y - screen_delta.y;
        wob2.screenrect.min.y -= playtools.y + screen_delta.y;
        wob2.screenrect.max.x = wob2.screenrect.min.x + playtools.x;
        Wimp_OpenWindow( &wob2 );
        wob->behind = v->wPlay;
    }
    Wimp_OpenWindow( wob );
}

static view View_Find( window_handle wh )
{
    view result = views;

    while ( result )
    {
        if ( result->wFilm == wh )
            return result;
        result = result->pNext;
    }
    return NULL;
}

static void View_PlayClick( view v, BOOL bSelect )
{
    switch ( e.data.mouse.icon )
    {
    case playicon_START:
        View_Stop( v );
        View_Goto( v, 0 );
        break;
    case playicon_FORWARDS:
        if ( bSelect )
            View_Play( v, TRUE );
        else
        {
            View_Stop( v );
            View_NextFrame( v, 1 );
        }
        break;
    case playicon_BACKWARDS:
        if ( bSelect )
            View_Play( v, FALSE );
        else
        {
            View_Stop( v );
            View_NextFrame( v, -1 );
        }
        break;
    case playicon_STOP:
        View_Stop( v );
        break;
    case playicon_END:
        View_Stop( v );
        View_Goto( v, v->a->nFrames - 1 );
        break;
    }
}

static sprite_areainfo *CreateBufferSprite( anim a, int *bgc )
{
    int abw = ((a->nWidth + 3 ) & ~3) * a->nHeight;
    int nBytes = abw*2 + 44 + 16 + 256*8;
    sprite_areainfo *result = malloc(nBytes);
    sprite_header *spr = (sprite_header*)(result+1);
    int i,n;
    unsigned int *pPalDest = (unsigned int*)(spr+1);
    unsigned int *pPalSrc;

    if ( !result )
        return NULL;

    result->areasize = nBytes;
    result->numsprites = 1;
    result->firstoffset = 16;
    result->freeoffset = nBytes;

    spr->offset_next = nBytes-16;
    strncpy( spr->name, "buffer", 12 );
    spr->width = ((a->nWidth+3)>>2)-1;
    spr->height = a->nHeight-1;
    spr->leftbit = 0;
    spr->rightbit = ((a->nWidth & 3) * 8 - 1) & 31;
    spr->imageoffset = 44 + 256*8;
    spr->maskoffset = 44 + 256*8 + abw;
    spr->screenmode.screen_mode = 28;

    n = a->pFrames->pal->nColours;
    pPalSrc = a->pFrames->pal->pColours;
    for ( i=0; i<n; i++ )
    {
        *pPalDest++ = *pPalSrc;
        *pPalDest++ = *pPalSrc++;
    }

    if ( !bgc )
        return result;

    /* A bit of faff here to return a useful (near-white) background colour */

    pPalDest = (unsigned int*)(spr+1);

    if ( n < 256 )
    {
        pPalDest[255*2] = 0xFFFFFF00;
        pPalDest[255*2+1] = 0xFFFFFF00;
        *bgc = 255;
        fdebugf( stderr, "Returning bg=255\n" );
    }
    else
    {
        unsigned int max = 0;
        for ( i=0; i<256; i++ )
        {
            unsigned int dark = pPalDest[i*2];
            dark = (dark>>24) + ((dark>>16)&0xFF) + ((dark>>8)&0xFF);
            if ( dark > max )
            {
                max = dark;
                n = i;
            }
        }
        *bgc = n;
        fdebugf( stderr,"Returning bg=%d\n", n );
    }

    return result;
}

static void View_New( const char *name )
{
    view theView = malloc( sizeof(viewstr) );
    icon_block *pib = (icon_block*)((&Template_film)+1);
    window_state ws;
    int x,y;
    const char *leafname;
    int leaflen;

    theView->a = Anim_FromFile( name, NULL, FALSE, FALSE, FALSE, FALSE );

    if ( !theView->a )
    {
        Error_Report( 0, Anim_Error );
        free( theView );
        return;
    }

    if ( !Anim_CommonPalette( theView->a ) )
    {
        Error_Report( 0, Anim_Error );
        Anim_Destroy( &theView->a );
        free( theView );
        return;
    }

    theView->buffer = CreateBufferSprite( theView->a, &theView->nBGColour );

    leafname = strrchr( name, '.' );
    if ( !leafname )
        leafname = name;
    else
        leafname++;

    leaflen = strlen(leafname)+35;
    theView->leafname = malloc( leaflen );
    sprintf( theView->leafname, "%s %dx%d", leafname, theView->a->nWidth,
             theView->a->nHeight );

    if ( theView->a->nFrames > 1 )
    {
        sprintf( theView->leafname + strlen(theView->leafname),
                 ", %d frames", theView->a->nFrames );
    }

    if ( !theView->buffer )
    {
        Error_Report( 0, "Can't create buffer sprite" );
        Anim_Destroy( &theView->a );
        free( theView );
        return;
    }

    theView->pNext = views;
    views = theView;

    strcpy( pib->data.spritename, "buffer" );
    x = theView->a->nWidth*2;
    y = -theView->a->nHeight*2;
    pib->workarearect.max.x = x;
    pib->workarearect.min.y = y;

    Template_film.screenrect.max.x = Template_film.screenrect.min.x + x;
    Template_film.screenrect.min.y = Template_film.screenrect.max.y + y;
    Template_film.minsize.x = x;
    Template_film.minsize.y = -y;
    Template_film.workarearect = pib->workarearect;
    Template_film.spritearea = theView->buffer;
    Template_film.title.indirecttext.buffer = theView->leafname;

    Wimp_CreateWindow( &Template_film, &theView->wFilm );

    if ( theView->a->nFrames > 1 )
    {
        playtools.x = Template_play.screenrect.max.x
                         - Template_play.screenrect.min.x;
        playtools.y = Template_play.screenrect.max.y
                         - Template_play.screenrect.min.y;
        Template_play.spritearea = &Sprite_Area;
        Wimp_CreateWindow( &Template_play, &theView->wPlay );
    }
    else
        theView->wPlay = 0;

    theView->nFrame = -1;
    theView->nPlaying = 0;
    View_Goto(theView,0);

    Wimp_GetWindowState( theView->wFilm, &ws );
    ws.openblock.behind = (window_handle)-1;
    View_Open( theView, &ws.openblock );
}

static void View_Destroy( view *pv )
{
    view v = *pv;
    view *pv2 = &views;

    if ( v )
    {
        /* Unlink from list */
        while ( *pv2 && (*pv2 != v) )
            pv2 = &((*pv2)->pNext);

        if ( *pv2 )
            *pv2 = v->pNext;

        /* Generally destroy */
        View_Stop( v );
        Wimp_DeleteWindow( v->wFilm );
        if ( v->wPlay )
            Wimp_DeleteWindow( v->wPlay );
        free( v->leafname );
        free( v->buffer );
        Anim_Destroy( &v->a );
        free( v );
        *pv = NULL;
    }
}

static void WimpButton( void )
{
    if ( e.data.mouse.button.data.menu && e.data.mouse.window < 0 )
    {
        Menu_Show( &Menu_iconbar, e.data.mouse.pos.x, -1 );
    }
    else
    {
        view v = views;

        while ( v )
        {
            if ( e.data.mouse.window == v->wPlay )
            {
                if ( e.data.mouse.button.data.select )
                    View_PlayClick( v, TRUE );
                else if ( e.data.mouse.button.data.adjust )
                    View_PlayClick( v, FALSE );
                return;
            }
            v = v->pNext;
        }
    }
}

static void MenuSelection( const int *sel )
{
    switch( *sel )
    {
    case 1:  /* Web site */
        Internet_OpenURL("http://utter.chaos.org.uk/~pdh/software/intergif.htm");
        break;
    case 2:  /* Help */
        _kernel_oscli("Filer_Run <InterGif$Viewer$Dir>.!Help");
        break;
    case 3:  /* Quit */
        quitapp = TRUE;
        break;
    }
}

static void wimpinit( void )
{
    unsigned int version = 310;
    static int messages = 0;
    menu_item *pmi = (menu_item*)((&Menu_iconbar)+1);

    Wimp_Initialise( &version, "InterGif Viewer", &me, &messages );
    Screen_CacheModeInfo();

    iBarIcon = Icon_BarIcon( "!IGViewer", -1 );

    Wimp_CreateWindow( &Template_info, &wInfo );
    pmi->submenu.window = wInfo;
}

int main( int argc, char *argv[] )
{
    event_pollmask mask;
    view v;
    int t;

    mask.value = 0;
    mask.data.null = 1;

    wimpinit();

    for ( t = 1; t < argc; t++ )
        View_New( argv[t] );

    while (!quitapp)
    {
        if ( playlist )
        {
            t = playlist->nNextFrameTime;

            for ( v = playlist->pNextPlaying; v; v = v->pNextPlaying )
                if ( v->nNextFrameTime < t )
                    t = v->nNextFrameTime;

            mask.data.null = 0;
            fdebugf( stderr, "pollidle(%u)\n", t );
            Wimp_PollIdle( mask, &e, t );
        }
        else
        {
            mask.data.null = 1;
            Wimp_Poll( mask, &e );
        }

        switch (e.type)
        {
        case event_NULL:
            t = OS_ReadMonotonicTime();
            fdebugf( stderr, "null: t=%u\n", t );
            for ( v = playlist; v; v = v->pNextPlaying )
                if ( ( t - v->nNextFrameTime ) > 0 )
                    View_NextFrame( v, v->nPlaying );
            break;
        case event_OPEN:
            v = View_Find( e.data.openblock.window );
            if ( v )
                View_Open( v, &e.data.openblock );
            else
                Wimp_OpenWindow( &e.data.openblock );
            break;
        case event_CLOSE:
            v = View_Find( e.data.openblock.window );

            if ( v )
                View_Destroy( &v );
            else
                Wimp_CloseWindow( e.data.openblock.window );
            break;
        case event_KEY:
            Wimp_ProcessKey( e.data.key.code );
            break;
        case event_MENU:
            MenuSelection( e.data.selection );
            break;
        case event_BUTTON:
            WimpButton();
            break;
	case event_SEND:
	case event_SENDWANTACK:
            switch (e.data.message.header.action)
            {
            case message_DATAOPEN:
                if ( e.data.message.data.dataload.filetype == 0x695 )
                {
                    View_New( e.data.message.data.dataopen.filename );
                    e.data.message.header.action = message_DATALOADACK;
                    e.data.message.header.yourref =
                        e.data.message.header.myref;
                    e.data.message.header.size = 20;
                    Wimp_SendMessage( event_SEND, &e.data.message,
                                      e.data.message.header.sender, 0 );
                }
                break;
            case message_DATALOAD:
                if ( e.data.message.data.dataload.filetype == 0xFF9
                     || e.data.message.data.dataload.filetype == 0xC2A
                     || e.data.message.data.dataload.filetype == 0x695
                     || e.data.message.data.dataload.filetype == 0xAFF )
                {
                    View_New( e.data.message.data.dataload.filename );
                }
        	break;
            case message_DATALOADACK:
        	ackpending = FALSE;
        	break;
            case message_QUIT:
        	quitapp=TRUE;
        	break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
    Wimp_CloseDown(me);
    return 0;
}

/* eof */
