/* window.c */

/* Frontend code for InterGif
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *      Based on a BASIC version by Iain Logan
 *
 * History:
 *      14-Oct-96 pdh Started
 *      27-Oct-96 pdh Frob to allow save as sprite
 *      27-Oct-96 *** Release 4beta1
 *      29-Oct-96 *** Release 4beta2
 *      07-Nov-96 pdh version.h stuff
 *      07-Nov-96 *** Release 4
 *      15-Dec-96 *** Release 5beta1
 *      27-Jan-97 *** Release 5beta2
 *      29-Jan-97 *** Release 5beta3
 *      30-Jan-97 pdh Make a separate application
 *      03-Feb-97 *** Release 5
 *      07-Feb-97 pdh Accept type AFF files (Draw)
 *      07-Feb-97 *** Release 5.01
 *      01-Mar-97 pdh Fix Stu's '-split' bug
 *      07-Apr-97 *** Release 6beta1
 *      20-May-97 *** Release 6beta2
 *      24-Aug-97 *** Release 6
 *      27-Sep-97 pdh Make 'set delay' optional
 *      27-Sep-97 *** Release 6.01
 *      07-Nov-97 pdh 'Trim' option
 *      08-Nov-97 *** Release 6.02
 *      09-Feb-98 pdh Icon bar icon <applause>; fix >16char leafnames; fix
 *                    Dean's 'no palette file' bug
 *      12-Feb-98 pdh ChangeFSI window
 *      21-Feb-98 *** Release 6.03
 *      26-May-98 pdh Be smarter about inventing save name
 *      07-Jun-98 *** Release 6.04
 *      01-Aug-98 pdh Remove auto-setting of Trim (was confusing people)
 *      21-Aug-98 *** Release 6.05
 *      05-Oct-98 pdh Give slotsize of 64k not all remaining memory
 *      19-Feb-99 *** Release 6.07
 *      17-Apr-99 pdh No, give it all remaining memory (64k isn't enough to
 *                    system() ChangeFSI)
 *      26-Mar-00 pdh Add "Keep unused entries" icon for -same
 *      26-Mar-00 *** Release 6.10
 *      18-Aug-00 pdh Add "Error diffusion" to drive MW's Floyd-Steinberg option
 *      28-Aug-00 *** Release 6.11
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
#include "DeskLib:Str.h"

static int quitapp = FALSE;
static int ackpending = FALSE;
static event_pollblock e;

static window_handle wh, palh, wInfo, wCFSI, spropth;
static task_handle me;
static char savename[256] = "image/gif";
static char *saveicon;
static char *palette_colouricon;
static char palettefile[256] = { 0 };
static char loadname[256] = { 0 };
static char *cfsi_optionicon;
static char cfsi_options[48] = "28r 1:1 1:1";
static BOOL cfsi = FALSE;

static BOOL draggingfile = FALSE;
static BOOL iconbar = FALSE;

extern window_block Template_intergif;
extern window_block Template_quantise;
extern window_block Template_spropt;
extern window_block Template_info;
extern window_block Template_cfsi;
extern sprite_areainfo Sprite_Area;
extern menu_block Menu_iconbar;


#define igicon_INTERLACED 11
#define igicon_LOOP 7
#define igicon_JOIN 8
#define igicon_SPLIT 9
#define igicon_DELAY 25
#define igicon_DELAY_USE 26
#define igicon_TRANS_NONE 19
#define igicon_TRANS_USE 3
#define igicon_TRANS_FORCE 6
#define igicon_TRANS_VALUE 21
#define igicon_HELP_FILE 23
#define igicon_HELP_WEB 15
#define igicon_OUT_GIF 22
#define igicon_OUT_SPRITE 32
#define igicon_DRAGGABLE 16
#define igicon_SAVENAME 17
#define igicon_SAVE 13
#define igicon_PALETTE 29
#define igicon_TRIM 30
#define igicon_CFSI 31
#define igicon_SPROPTIONS 20

#define palicon_KEEP    9
#define palicon_256     12
#define palicon_216     13
#define palicon_g16     14
#define palicon_g256    16
#define palicon_FILE     6
#define palicon_LOADICON    8
#define palicon_DROPZONE    7
#define palicon_OPTIMISE    1
#define palicon_OPT_COLOURS 4
#define palicon_KEEP_UNUSED 11
#define palicon_OK          2
#define palicon_CANCEL      3
#define palicon_DITHER      15

#define cfsiicon_OPTIONS    1
#define cfsiicon_CFSI       4
#define cfsiicon_CANCEL     6
#define cfsiicon_OK         5

#define spropticon_FORCENEW 14
#define spropticon_HRES22 2
#define spropticon_HRES45 3
#define spropticon_HRES90 4
#define spropticon_HRES180 5
#define spropticon_VRES22 6
#define spropticon_VRES45 7
#define spropticon_VRES90 8
#define spropticon_VRES180 9
#define spropticon_OK 10
#define spropticon_CANCEL 11
#define spropticon_HRESLABEL 13
#define spropticon_VRESLABEL 12

/* Palette window state */

static int palette_which = palicon_KEEP;
static int palette_opt_colours = 255;
static BOOL palette_open = FALSE;
static BOOL palette_keep_unused = FALSE;
static BOOL palette_dither = FALSE;
static void GetPaletteData( void );

static BOOL force_new = FALSE;
static BOOL spropt_open = FALSE;
static int xdpi = 90;
static int ydpi = 90;
static void GetSprOptData( void );

static BOOL cfsi_open = FALSE;
static void GetCFSIData( void );

void Error_Report2(int errornum, char *report, ...)
{
    va_list ap;
    os_error    error;
    error_flags eflags;

    va_start( ap, report );

        vsprintf(error.errmess, report, ap);
        error.errnum = errornum;

        eflags.value = 1;
        (void) Wimp_ReportError(&error, eflags.value, "InterGif");

    va_end(ap);
}

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

static char *Leaf( char *filename )
{
    char *pos = strrchr( filename, '.' );
    char *pos2 = strrchr( filename, ':' );
    if ( pos2 > pos )
        pos = pos2;
    return pos ? pos+1 : filename;
}

/* 6.04: be smarter about suggested output filenames
 */
void InventSaveFilename( const char *loadname, char *buffer, int type )
{
    if ( type != 0x695 )
    {
        sprintf( buffer, "%s/gif", loadname );
    }
    else
    {
        int len = strlen(loadname);

        strcpy( buffer, loadname );
        if ( len > 4 && strcmp( buffer+len-4, "/gif" ) == 0 )
        {
            buffer[len-4] = '\0';
        }
        else
        {
            char *suffix = strrchr( buffer, '/' );
            char *leaf = strrchr( buffer, '.' );

            if ( !leaf )
                leaf = strrchr( buffer, ':' );

            if ( suffix && leaf && suffix > leaf )
            {
                *suffix = '\0';
            }
            else
                strcat( buffer, "2" );
        }
    }
}

static void LoadFile( const char *name, int type )
{
    caret_block cb;

    strcpy( loadname, name );
    InventSaveFilename( name, savename, type );

    if ( Icon_GetShade( wh, igicon_SAVENAME ) )
    {
        Icon_Unshade( wh, igicon_SAVENAME );
        Icon_Unshade( wh, igicon_SAVE );
        Icon_Unshade( wh, igicon_DRAGGABLE );
    }
    else
        Wimp_SetIconState( wh, igicon_SAVENAME, 0, 0 );/* just redraw it */

    cb.window = wh;
    cb.icon = igicon_SAVENAME;
    cb.offset.x = -1;
    cb.offset.y = -1;
    cb.height = -1;
    cb.index = strlen( savename );
    Wimp_SetCaretPosition( &cb );
}

static void SaveFile( char *filename )
{
    icon_block *pib = (icon_block*)((&Template_intergif)+1);
    char buffer[700];   /* two filenames and then some */
    os_error *e;
    int next, oldnext, free, current;

    if (strchr( filename, ':' ) == NULL
    	&& strchr( filename, '.' ) == NULL )
    {
    	Error_Report2( 0, "To save, drag the icon to a directory display" );
    	return;
    }

    sprintf( buffer, "<InterGif$Dir>.intergif %s -desktop",
                     loadname );

    if ( Icon_GetSelect( wh, igicon_DELAY_USE ) )
    {
        strcat( buffer, " -d " );
        strcat( buffer, pib[igicon_DELAY].data.indirecttext.buffer );
    }

    if ( Icon_GetSelect( wh, igicon_SPLIT ) )
        strcat( buffer, " -split" );

    if ( Icon_GetSelect( wh, igicon_OUT_SPRITE ) )
        strcat( buffer, " -s" );

    if ( Icon_GetSelect( wh, igicon_INTERLACED ) )
        strcat( buffer, " -i" );

    if ( Icon_GetSelect( wh, igicon_LOOP ) )
        strcat( buffer, " -loop" );

    if ( Icon_GetSelect( wh, igicon_TRIM ) )
        strcat( buffer, " -trim" );

    if ( Icon_GetSelect( wh, igicon_JOIN ) )
        strcat( buffer, " -join" );

    if ( !Icon_GetSelect( wh, igicon_TRANS_NONE ) )
    {
        strcat( buffer, " -t " );   /* note extra space */

        if ( Icon_GetSelect( wh, igicon_TRANS_FORCE ) )
            strcat( buffer, pib[igicon_TRANS_VALUE].data.indirecttext.buffer );
    }

    if ( palette_open )
        GetPaletteData();

    if ( palette_which == palicon_FILE && (!palettefile[0] ) )
    {
        Error_Report2( 0, "Palette 'From file' chosen but no palette file supplied" );
        return;
    }

    switch ( palette_which )
    {
    case palicon_216:
        strcat( buffer, " -216" );
        break;
    case palicon_256:
        strcat( buffer, " -256" );
        break;
    case palicon_g16:
        strcat( buffer, " -g16" );
        break;
    case palicon_g256:
        strcat( buffer, " -g256" );
        break;
    case palicon_OPTIMISE:
        sprintf( buffer + strlen(buffer), " -best %d", palette_opt_colours );
        break;
    case palicon_FILE:
        sprintf( buffer + strlen(buffer), " -pal %s", palettefile );
        break;
    }

    if ( palette_keep_unused )
        strcat( buffer, " -same" );

    if ( palette_dither && (palette_which != palicon_KEEP) )
        strcat( buffer, " -diffuse -zigzag" );

    if ( spropt_open )
        GetSprOptData();

    if ( force_new )
    {
        strcat( buffer, " -new" );
        if (xdpi != 90) {
            sprintf( buffer + strlen(buffer), " -xdpi %d", xdpi );
        }
        if (ydpi != 90) {
            sprintf( buffer + strlen(buffer), " -ydpi %d", ydpi );
        }
    }

    if ( cfsi_open )
        GetCFSIData();

    if ( cfsi )
    {
        strcat( buffer, " -c \"" );
        strcat( buffer, cfsi_options );
        strcat( buffer, "\"" );
    }

    strcat( buffer, " -o " );
    strcat( buffer, filename );

    current = oldnext = free = -1;
    Wimp_SlotSize( &current, &oldnext, &free );

    next = free - 4*1024; /* 64*1024; */
    current = free = -1;
    Wimp_SlotSize( &current, &next, &free );

    Hourglass_On();
    e = Wimp_StartTask( buffer );
    Hourglass_Off();

    current = free = -1;
    Wimp_SlotSize( &current, &oldnext, &free );

    if ( e )
        Error_Report2( e->errnum, e->errmess );
    else
    {
        if ( _kernel_getenv( "InterGif$Error", buffer, 700 ) )
            Error_Report2( 0, "Internal error: error variable not set" );
        else if ( *buffer )
            Error_Report2( 0, buffer );
    }
}

static void DoPaletteRadio( void )
{
    Icon_SetShade( palh, palicon_DROPZONE,
                   !Icon_GetSelect( palh, palicon_FILE ) );
    Icon_SetShade( palh, palicon_LOADICON,
                   !Icon_GetSelect( palh, palicon_FILE ) );
    Icon_SetShade( palh, palicon_OPT_COLOURS,
                   !Icon_GetSelect( palh, palicon_OPTIMISE ) );
    Icon_SetShade( palh, palicon_DITHER,
                   Icon_GetSelect( palh, palicon_KEEP ) );
}

static void SetPaletteRadio( void )
{
    Icon_SetSelect( palh, palicon_KEEP, palette_which == palicon_KEEP );
    Icon_SetSelect( palh, palicon_256, palette_which == palicon_256 );
    Icon_SetSelect( palh, palicon_216, palette_which == palicon_216 );
    Icon_SetSelect( palh, palicon_FILE, palette_which == palicon_FILE );
    Icon_SetSelect( palh, palicon_OPTIMISE, palette_which == palicon_OPTIMISE );
    DoPaletteRadio();
}

static void GetPaletteData( void )
{
    icon_handle icons[10];

    Wimp_WhichIcon( palh, icons, 0x3F0000, 0x240000 );  /* ESG=4 && selected */

    palette_which = icons[0] == -1 ? palicon_KEEP : icons[0];
    palette_opt_colours = atoi( palette_colouricon );
    if ( palette_opt_colours < 2 )
        palette_opt_colours = 2;
    if ( palette_opt_colours > 256 )
        palette_opt_colours = 256;

    palette_dither = Icon_GetSelect( palh, palicon_DITHER );
    palette_keep_unused = Icon_GetSelect( palh, palicon_KEEP_UNUSED );
}

static void LoadPalette( char *name )
{
    strcpy( palettefile, name );

    palette_which = palicon_FILE;
    SetPaletteRadio();
}

static void DoSprOptRadio( void )
{
    BOOL force = Icon_GetSelect( spropth, spropticon_FORCENEW );
    Icon_SetShade( spropth, spropticon_HRES22, !force );
    Icon_SetShade( spropth, spropticon_HRES45, !force );
    Icon_SetShade( spropth, spropticon_HRES90, !force );
    Icon_SetShade( spropth, spropticon_HRES180, !force );
    Icon_SetShade( spropth, spropticon_VRES22, !force );
    Icon_SetShade( spropth, spropticon_VRES45, !force );
    Icon_SetShade( spropth, spropticon_VRES90, !force );
    Icon_SetShade( spropth, spropticon_VRES180, !force );
    Icon_SetShade( spropth, spropticon_HRESLABEL, !force );
    Icon_SetShade( spropth, spropticon_VRESLABEL, !force );
}

static void SetupSprOpt( void )
{
     Icon_SetSelect( spropth, spropticon_FORCENEW, force_new );
     Icon_SetSelect( spropth, spropticon_HRES22, xdpi == 22 );
     Icon_SetSelect( spropth, spropticon_HRES45, xdpi == 45 );
     Icon_SetSelect( spropth, spropticon_HRES90, xdpi != 22 && xdpi != 45 && xdpi != 180 );
     Icon_SetSelect( spropth, spropticon_HRES180, xdpi == 180 );
     Icon_SetSelect( spropth, spropticon_VRES22, ydpi == 22 );
     Icon_SetSelect( spropth, spropticon_VRES45, ydpi == 45 );
     Icon_SetSelect( spropth, spropticon_VRES90, ydpi != 22 && ydpi != 45 && ydpi != 180 );
     Icon_SetSelect( spropth, spropticon_VRES180, ydpi == 180 );
     DoSprOptRadio();
}

static void GetSprOptData( void )
{
  icon_handle icons[10];
  int spropt_which_hres, spropt_which_vres;
  static int resolutions[] = {22, 45, 90, 180};

  force_new = Icon_GetSelect( spropth, spropticon_FORCENEW );

  Wimp_WhichIcon( spropth, icons, 0x3F0000, 0x210000 );  /* ESG=1 && selected */
  spropt_which_hres = icons[0] == -1 ? spropticon_HRES90 : icons[0];
  xdpi = resolutions[spropt_which_hres - spropticon_HRES22];

  Wimp_WhichIcon( spropth, icons, 0x3F0000, 0x220000 );  /* ESG=2 && selected */
  spropt_which_vres = icons[0] == -1 ? spropticon_VRES90 : icons[0];
  ydpi = resolutions[spropt_which_vres - spropticon_VRES22];
}

static void OpenTransient( window_handle w, window_block *block )
{
    window_openblock wob;
    mouse_block mb;

    wob.window = w;

    Wimp_GetPointerInfo( &mb );

    /* Open like a submenu would (but not *as* a submenu as we must allow
     * drags to be made unto us)
     */

    wob.screenrect.min.x = mb.pos.x - 64;
    wob.screenrect.max.x = wob.screenrect.max.x
                           + block->workarearect.max.x;
    wob.screenrect.max.y = mb.pos.y;
    wob.screenrect.min.y = wob.screenrect.max.y
                           + block->workarearect.min.y;
    wob.scroll.x = 0;
    wob.scroll.y = 0;
    wob.behind = -1;
    Wimp_OpenWindow( &wob );
}

static void OpenPaletteWindow( void )
{
    if ( !palette_open )
    {
        SetPaletteRadio();

        sprintf( palette_colouricon, "%d", palette_opt_colours );
    }

    OpenTransient( palh, &Template_quantise );

    palette_open = TRUE;
}

static void OpenSprOptWindow( void )
{
    if ( !spropt_open )
    {
        SetupSprOpt();
    }

    OpenTransient( spropth, &Template_spropt );

    palette_open = TRUE;
}

static void GetCFSIData( void )
{
    cfsi = Icon_GetSelect( wCFSI, cfsiicon_CFSI );
    if ( cfsi )
    {
        strcpy( cfsi_options, cfsi_optionicon );
    }
}

static void OpenCFSIWindow( void )
{
    if ( !cfsi_open )
    {
        if ( getenv( "ChangeFSI$Dir" ) )
        {
            Icon_Unshade( wCFSI, cfsiicon_CFSI );
            Icon_SetShade( wCFSI, cfsiicon_OPTIONS, !cfsi );
            Icon_SetSelect( wCFSI, cfsiicon_CFSI, cfsi );
            strcpy( cfsi_optionicon, cfsi_options );
        }
        else
        {
            Icon_Shade( wCFSI, cfsiicon_CFSI );
            Icon_Shade( wCFSI, cfsiicon_OPTIONS );
        }
    }

    OpenTransient( wCFSI, &Template_cfsi );

    cfsi_open = TRUE;
}

static void OpenMainWindow( void )
{
    window_openblock wob;

    wob.window = wh;
    wob.screenrect = Template_intergif.screenrect;
    wob.scroll = Template_intergif.scroll;
    wob.behind = (window_handle)-1;     /* in front */
    Wimp_OpenWindow( &wob );
}

static void MenuSelection( const int *sel )
{
    switch ( *sel )
    {
    case 1: /* Web site */
        Internet_OpenURL( "http://utter.chaos.org.uk/~pdh/software/intergif.htm" );
        break;
    case 2: /* Help */
        _kernel_oscli( "Filer_Run <InterGif$Dir>.!Help" );
        break;
    case 3: /* Quit */
        quitapp = TRUE;
        break;
    }
}

static void WimpButton( void )
{
    if ( iconbar && (e.data.mouse.window < 0) )
    {
        if ( e.data.mouse.button.data.menu )
            Menu_Show( &Menu_iconbar, e.data.mouse.pos.x, -1 );
        else if ( e.data.mouse.button.data.select )
            OpenMainWindow();
    }
    else if ( e.data.mouse.window == palh && e.data.mouse.button.data.select )
    {
        switch ( e.data.mouse.icon )
        {
        case palicon_KEEP:
        case palicon_256:
        case palicon_216:
        case palicon_FILE:
        case palicon_OPTIMISE:
            DoPaletteRadio();
            break;
        case palicon_OK:
            GetPaletteData();
            /* fall through */
        case palicon_CANCEL:
            Wimp_CloseWindow( palh );
            palette_open = FALSE;
            break;
        }
        return;
    }
    else if ( e.data.mouse.window == spropth && e.data.mouse.button.data.select )
    {
        switch ( e.data.mouse.icon )
        {
        case spropticon_FORCENEW:
            DoSprOptRadio();
            break;
        case spropticon_OK:
            GetSprOptData();
            /* fall through */
        case spropticon_CANCEL:
            Wimp_CloseWindow( spropth );
            spropt_open = FALSE;
            break;
        }
        return;
    }
    else if ( e.data.mouse.window == wCFSI && e.data.mouse.button.data.select )
    {
        switch ( e.data.mouse.icon )
        {
        case cfsiicon_CFSI:
            Icon_SetShade( wCFSI, cfsiicon_OPTIONS,
                           !Icon_GetSelect( wCFSI, cfsiicon_CFSI ) );
            break;
        case cfsiicon_OK:
            GetCFSIData();
            /* fall through */
        case cfsiicon_CANCEL:
            Wimp_CloseWindow( wCFSI );
            cfsi_open = FALSE;
            break;
        }
        return;
    }

    if ( e.data.mouse.window != wh )
        return;

    if ( e.data.mouse.button.data.select )
    {
        switch ( e.data.mouse.icon )
        {
        case igicon_TRANS_FORCE:
        case igicon_TRANS_USE:
        case igicon_TRANS_NONE:
            Icon_SetShade( wh, igicon_TRANS_VALUE,
                           !Icon_GetSelect( wh, igicon_TRANS_FORCE ) );
            break;
#if 0
            /* fall through */
        case igicon_INTERLACED:
            if ( !Icon_GetSelect( wh, igicon_TRANS_NONE )
                 && Icon_GetSelect( wh, igicon_INTERLACED ) )
                Icon_SetSelect( wh, igicon_TRIM, TRUE );
            break;
#endif
        case igicon_DELAY_USE:
            Icon_SetShade( wh, igicon_DELAY,
                           !Icon_GetSelect( wh, igicon_DELAY_USE ) );
            break;
        case igicon_HELP_FILE:
            _kernel_oscli( "Filer_Run <InterGif$Dir>.!Help" );
            break;
        case igicon_HELP_WEB:
            Internet_OpenURL( "http://utter.chaos.org.uk/~pdh/software/intergif.htm" );
            break;
        case igicon_OUT_GIF:
        case igicon_OUT_SPRITE:
            {
                BOOL bSprite = Icon_GetSelect( wh, igicon_OUT_SPRITE );

                strcpy( saveicon, bSprite ? "Sfile_ff9" : "Sfile_695" );
                Wimp_SetIconState( wh, igicon_DRAGGABLE, 0, 0 );
                Icon_SetShade( wh, igicon_INTERLACED, bSprite );
                Icon_SetShade( wh, igicon_LOOP,       bSprite );
                Icon_SetShade( wh, igicon_DELAY,
                               bSprite
                                || !Icon_GetSelect( wh, igicon_DELAY_USE ) );
                Icon_SetShade( wh, igicon_DELAY_USE,  bSprite );
                Icon_SetShade( wh, igicon_SPROPTIONS,  !bSprite );
            }
            break;
        case igicon_SAVE:
            SaveFile( savename );
            break;
        case igicon_PALETTE:
            OpenPaletteWindow();
            break;
        case igicon_SPROPTIONS:
            OpenSprOptWindow();
            break;
        case igicon_CFSI:
            OpenCFSIWindow();
            break;
        }
    }
    else if ( e.data.mouse.button.data.adjust )
    {
        if ( e.data.mouse.icon == igicon_TRANS_FORCE )
            Icon_SetShade( wh, igicon_TRANS_VALUE,
                           !Icon_GetSelect( wh, igicon_TRANS_FORCE ) );
    }
    else if ( e.data.mouse.button.data.dragselect )
    {
        if ( e.data.mouse.icon == igicon_DRAGGABLE && *loadname )
        {
            icon_block *pib = (icon_block*) ((&Template_intergif)+1);
            window_state ws;

            Wimp_GetWindowState( wh, &ws );
            pib += igicon_DRAGGABLE;
            ws.openblock.screenrect.min.x += pib->workarearect.min.x;
            ws.openblock.screenrect.max.x = ws.openblock.screenrect.min.x + 68;
            ws.openblock.screenrect.max.y += pib->workarearect.max.y;
            ws.openblock.screenrect.min.y = ws.openblock.screenrect.max.y - 68;
            DragASprite_Start( 0xC5, (void*)1, saveicon+1,
                               &ws.openblock.screenrect, NULL );
            draggingfile = TRUE;
        }
    }

}

static void wimpinit( void )
{
    unsigned int version = 310;
    icon_block *pib;
    static int messages = 0;
    menu_item *pmi = (menu_item*) ((&Menu_iconbar)+1);

    Wimp_Initialise( &version, "InterGif", &me, &messages );
    Screen_CacheModeInfo();

    pib = (icon_block*) ((&Template_intergif)+1);
    pib[igicon_SAVENAME].data.indirecttext.buffer = savename;
    strcpy( pib[igicon_DELAY].data.indirecttext.buffer, "8" );
    strcpy( pib[igicon_TRANS_VALUE].data.indirecttext.buffer, "0" );
    saveicon = pib[igicon_DRAGGABLE].data.indirecttext.validstring;
    Template_intergif.spritearea = &Sprite_Area;
    Wimp_CreateWindow( &Template_intergif, &wh );
    Icon_Shade( wh, igicon_DRAGGABLE );
    Icon_Shade( wh, igicon_SAVENAME );
    Icon_Shade( wh, igicon_SAVE );
    Icon_Shade( wh, igicon_DELAY );
    Icon_Shade( wh, igicon_TRANS_VALUE );
    Icon_Select( wh, igicon_OUT_GIF );
    Icon_Select( wh, igicon_LOOP );
    Icon_Select( wh, igicon_TRANS_USE );
    Icon_Shade( wh, igicon_SPROPTIONS );

    pib = (icon_block*) ((&Template_quantise)+1);
    Template_quantise.spritearea = &Sprite_Area;
    palette_colouricon = pib[palicon_OPT_COLOURS].data.indirecttext.buffer;
    Wimp_CreateWindow( &Template_quantise, &palh );
    Icon_Shade( palh, palicon_OPT_COLOURS );
    Icon_Shade( palh, palicon_DITHER );
    Icon_Shade( palh, palicon_LOADICON );
    Icon_Shade( palh, palicon_DROPZONE );
    Icon_Select( palh, palicon_KEEP );

    pib = (icon_block*) ((&Template_spropt)+1);
    Template_spropt.spritearea = &Sprite_Area;
    Wimp_CreateWindow( &Template_spropt, &spropth );

    pib = (icon_block*) ((&Template_cfsi)+1);
    Template_cfsi.spritearea = &Sprite_Area;
    cfsi_optionicon = pib[cfsiicon_OPTIONS].data.indirecttext.buffer;
    Wimp_CreateWindow( &Template_cfsi, &wCFSI );

    OpenMainWindow();

    if ( iconbar )
    {
        Icon_BarIcon( "!InterGif", -1 );
        Wimp_CreateWindow( &Template_info, &wInfo );
        pmi->submenu.window = wInfo;
    }
}

int main( int argc, char *argv[] )
{
    event_pollmask mask;

    if ( argc > 1 && !stricmp(argv[1],"-iconbar") )
        iconbar = TRUE;

    mask.value = 0;
    mask.data.null = 1;

    wimpinit();

    while (!quitapp)
    {
        if (ackpending)
            mask.data.null = 0;
        else
            mask.data.null = 1;

        Wimp_Poll(mask,&e);

        switch (e.type)
        {
        case event_NULL:
       	    if (ackpending)
            {
                ackpending = FALSE;
                _kernel_oscli("delete <Wimp$Scrap>");
                Error_Report2( 0, "Data transfer failed - receiver died" );
            }
            break;
        case event_OPEN:
            Wimp_OpenWindow( &e.data.openblock );
            break;
        case event_CLOSE:
            if ( e.data.openblock.window == wh && !iconbar )
                quitapp = TRUE;
            else if ( e.data.openblock.window == palh )
                palette_open = FALSE;
            else if ( e.data.openblock.window == spropth )
                spropt_open = FALSE;
            Wimp_CloseWindow( e.data.openblock.window );
            break;
        case event_KEY:
            if ( e.data.key.caret.window == wh
                 && e.data.key.caret.icon == igicon_SAVENAME
                 && e.data.key.code == 13 )
                SaveFile( savename );
            else if ( e.data.key.caret.window == wCFSI
                      && e.data.key.code == 13 )
            {
                GetCFSIData();
                Wimp_CloseWindow( wCFSI );
                cfsi_open = FALSE;
            }
            else
                Wimp_ProcessKey( e.data.key.code );
            break;
        case event_BUTTON:
            WimpButton();
            break;
        case event_MENU:
            MenuSelection( e.data.selection );
            break;
        case event_USERDRAG:
            if (draggingfile)
            {
                mouse_block mb;

                DragASprite_Stop();
                draggingfile = FALSE;
                Wimp_GetPointerInfo( &mb );
                e.type = event_SEND;
                e.data.message.header.action = message_DATASAVE;
                e.data.message.header.yourref = 0;
                e.data.message.header.size = 256;
                e.data.message.data.datasave.window = mb.window;
                e.data.message.data.datasave.icon = mb.icon;
                e.data.message.data.datasave.pos = mb.pos;
                e.data.message.data.datasave.estsize = 0;
                e.data.message.data.datasave.filetype = 0x695;
        	strcpy( e.data.message.data.datasave.leafname,
        		Leaf( savename ) );
        	Wimp_SendMessage( event_SEND, &e.data.message,
        	                  (message_destinee) mb.window,
        	    		  mb.icon );
            }
            break;
	case event_SEND:
	case event_SENDWANTACK:
            switch (e.data.message.header.action)
            {
            case message_DATASAVEACK:
        	SaveFile( e.data.message.data.datasaveack.filename );
        	break;
            case message_DATALOAD:
                if ( e.data.message.data.dataload.filetype == 0xFED
                          && e.data.message.data.dataload.window == palh )
                {
                    LoadPalette( e.data.message.data.dataload.filename );
                }
                else if ( e.data.message.data.dataload.filetype == 0xFF9
                     || e.data.message.data.dataload.filetype == 0xC2A
                     || e.data.message.data.dataload.filetype == 0x695
                     || e.data.message.data.dataload.filetype == 0xAFF
                     || cfsi )
                {
                    if ( e.data.message.data.dataload.window < 0 )
                        OpenMainWindow();
                    LoadFile( e.data.message.data.dataload.filename,
                              e.data.message.data.dataload.filetype );
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
