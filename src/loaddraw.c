/* LoadDraw.c */

/* RiscOS Draw file code for InterGif
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *
 * History:
 *      05-Feb-97 pdh Adapted from aadraw: c.tosprite
 *      07-Feb-97 *** Release 5.01
 *      10-Mar-97 pdh Frob a lot for new anim library
 *      07-Apr-97 *** Release 6beta1
 *      20-May-97 *** Release 6beta2
 *      24-Aug-97 *** Release 6
 *      27-Sep-97 *** Release 6.01
 *      08-Nov-97 *** Release 6.02
 *      10-Feb-98 pdh Anti-alias in strips if short of memory
 *      21-Feb-98 *** Release 6.03
 *      07-Jun-98 *** Release 6.04
 *      01-Aug-98 pdh Frob for anim_imagefn stuff
 *      21-Aug-98 *** Release 6.05
 *      05-Oct-98 pdh Disable Wimp_CommandWindow; fix error reporting
 *      05-Oct-98 *** Release 6.06
 *      19-Feb-99 *** Release 6.07
 *      26-Mar-00 *** Release 6.10
 *      10-Dec-00 pdh Fix 8bpp code
 *      10-Dec-00 *** Release 6.12
 *
 * References:
 *      http://utter.chaos.org.uk/~pdh/software/aadraw.htm
 *          pdh's AADraw home page
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#include "animlib.h"
#include "antialias.h"

/* We're allowed to include these headers, 'cos we only compile under RiscOS
 * anyway
 */
#include "DeskLib:Sprite.h"
#include "DeskLib:Str.h"
#include "DeskLib:GFX.h"
#include "DeskLib:Wimp.h"
#include "DeskLib:WimpSWIs.h"
#include "DeskLib:SWI.h"
#include "DeskLib:ColourTran.h"

#if 0
#define debugf printf
#define DEBUG 1
#else
#define debugf 1?0:printf
#define DEBUG 0
#endif


typedef struct
{
    int x0,y0;
    int x1,y1;
    int x2,y2;
} draw_matrix;


/*---------------------------------------------------------------------------*
 * System calls in riscos.s                                                  *
 *---------------------------------------------------------------------------*/

os_error *DrawFile_BBox( int flags, const void *pDrawfile, int drawsize,
                         draw_matrix *pDM, wimp_box *pResult );
os_error *DrawFile_Render( int flags, const void *pDrawfile, int drawsize,
                           draw_matrix *pDM, wimp_box *pResult );

BOOL      OSModule_Present( char *name );
os_error *OSModule_Load( char *pathname );

int TaskWindow_TaskInfo( int index );


/*---------------------------------------------------------------------------*
 * Constructor                                                               *
 *---------------------------------------------------------------------------*/

BOOL Anim_ConvertDraw( const void *data, size_t nSize,
                       anim_animfn animfn, anim_imagefn fn, void *handle )
{
    draw_matrix tm = { 0x10000 * ANIM_AAFACTOR, 0,
                       0, 0x10000 * ANIM_AAFACTOR,
                       0, 0 };
    wimp_box box;
    int spritex, spritey;
    int i, r,g,b;
    sprite_outputstate sos;
    int areasize;
    sprite_area area = NULL;
    sprite_header *pSprite;
    unsigned int palette[256];
    unsigned int *pPal;
    int w,h;
    int pass, passes, sectiony, basey2;
    anim_imageinfo pixels;
    os_error *e = NULL;
    BOOL bits24 = TRUE;
    screen_modeval spritemode;

    /* check it's a Draw file, if not return NULL */

    if ( *(int*)data != 0x77617244 )            /* "Draw" */
        return FALSE;

    if ( TaskWindow_TaskInfo(0) != 0 )
    {
        Anim_SetError( "InterGif cannot process Draw files in a taskwindow. "
                     "Please either press F12 or use the desktop front-end." );
        return FALSE;
    }

    if ( !OSModule_Present( "DrawFile" ) )
    {
        os_error *e;
        e = OSModule_Load( "System:Modules.Drawfile" );

        if ( e )
        {
            Anim_SetError( "DrawFile module not loaded: %s", e->errmess );
            return FALSE;
        }
    }

    if ( DrawFile_BBox( 0, data, nSize, &tm, &box ) )
    {
        Anim_SetError( "Invalid Draw file\n" );
        return FALSE;
    }

    tm.x2 = 3584-box.min.x;
    tm.y2 = basey2 = 3584-box.min.y;
    box.min.x = box.min.x / 256;
    box.max.x = box.max.x / 256;
    box.min.y = box.min.y / 256;
    box.max.y = box.max.y / 256;

    w = (box.max.x - box.min.x)/2 + 16;
    w = (w + ANIM_AAFACTOR - 1) / ANIM_AAFACTOR;

    h = (box.max.y - box.min.y)/2 + 16;
    h = (h + ANIM_AAFACTOR - 1) / ANIM_AAFACTOR;

    if ( animfn && !(*animfn)( handle, w, h, 0 ) )
    {
        debugf( "convertdraw: animfn returned FALSE, exiting\n" );
        return FALSE;
    }

/*    pixels.pBits = NULL;
    Anim_FlexAllocate( &pixels.pBits, w*h ); */

    debugf( "convertdraw: w=%d h=%d box=(%d,%d)..(%d,%d)\n", w, h,
        box.min.x, box.min.y, box.max.x, box.max.y );

tryagain:

    if ( bits24 )
        pixels.pBits = Anim_Allocate( w*h*4 );
    else
        pixels.pBits = Anim_Allocate( w*h ); /* Note not word-aligned */

    if ( !pixels.pBits )
    {
        Anim_NoMemory( "fromdraw" );
        return FALSE;
    }

#if DEBUG
    memset( pixels.pBits, 91, w*h );
#endif

    spritex = w * ANIM_AAFACTOR;
    spritey = h * ANIM_AAFACTOR;

    /* Plotting it three times the size and anti-aliasing down produces very
     * nice results, but needs lllots of memory. If memory's a bit short, we
     * try doing it in strips in a multi-pass fashion -- not unlike !Printers.
     */

    for ( passes = 1; passes < 65; passes*=2 )
    {
        sectiony = (( h + passes-1 ) / passes) * ANIM_AAFACTOR;

        if ( bits24 )
        {
            areasize = sectiony*spritex*4 + sizeof(sprite_header);
            areasize += sizeof(sprite_areainfo);
        }
        else
        {
            areasize = sectiony * ( (spritex+3) & ~3) + sizeof(sprite_header);
            areasize += 256*8 + sizeof(sprite_areainfo);
        }

        if ( !sectiony )
            break;

        debugf( "fromdraw: Allocating %d bytes\n", areasize );

        Anim_FlexAllocate( &area, areasize );

        if ( area )
            break;

        debugf( "fromdraw: Allocate failed, trying smaller\n" );
    }

    if ( !area )
    {
        Anim_NoMemory( "fromdraw2" );
        Anim_Free( &pixels.pBits );
        return FALSE;
    }

    debugf( "fromdraw: Allocate succeeded\n" );

    area->areasize = areasize;
    area->numsprites = 0;
    area->firstoffset =
        area->freeoffset = 16;

    debugf( "Creating big sprite (%dx%d,%d bytes) total size would be %dx%d\n",
            spritex, sectiony, areasize, spritex, spritey );

    if (bits24) {
        spritemode.sprite_mode.istype = 1;
        spritemode.sprite_mode.horz_dpi = 90;
        spritemode.sprite_mode.vert_dpi = 90;
        spritemode.sprite_mode.type = 6;
    } else {
        spritemode.screen_mode = 28;
    }
    e = Sprite_Create( area, "drawfile", FALSE, spritex, sectiony, spritemode);

    /* Sprite_Create will give an error on old (non-24-bit-capable) machines
     * so try again in 8bit
     */
    if ( e && bits24 )
    {
        Anim_Free( &pixels.pBits );
        Anim_FlexFree( &area );
        bits24 = FALSE;
        goto tryagain;
    }

#if DEBUG
    if ( e )
    {
        debugf( "fromdraw: spritecreate fails, %s\n", e->errmess );
    }
#endif
    debugf( "areasize=%d, freeoffset now %d\n", areasize, area->freeoffset );

    pSprite = (sprite_header*)((char*)area + 16);

    if ( !bits24 )
    {
        debugf( "Building palette\n" );

        pPal = palette;
        memset( pPal, 0, 256*4 );
        pPal[255] = 0xFFFFFF00;
        for ( r=0; r<6; r++ )
            for ( g=0; g<6; g++ )
                for ( b=0; b<6; b++ )
                    *pPal++ = r*0x33000000 + g*0x330000 + b*0x3300;

        debugf( "Copying palette into sprite\n" );

        pPal = (unsigned int*)(pSprite+1);

        for ( i=0; i<255; i++ )
        {
            *pPal++ = palette[i];
            *pPal++ = palette[i];
        }

        pSprite->imageoffset += 256*8;
        pSprite->maskoffset  += 256*8;
        pSprite->offset_next += 256*8;
        area->freeoffset     += 256*8;
    }

    debugf( "areasize=%d, freeoffset now %d\n", areasize, area->freeoffset );

    Wimp_CommandWindow(-1);

    for ( pass=0; pass<passes; pass++ )
    {
        int thisy = sectiony/ANIM_AAFACTOR;
        int offset = (pass*sectiony)/ANIM_AAFACTOR;

        tm.y2 = basey2 - (h-offset-thisy)*256*ANIM_AAFACTOR*2;

        if ( thisy + offset > h )
            thisy = h - offset;

        debugf( "Pass %d: thisy=%d offset=%d\n", pass, thisy, offset );

        offset *= w;

        debugf( "Blanking sprite\n" );

        memset( ((char*)pSprite) + pSprite->imageoffset, 255,
                (pSprite->width+1)*sectiony*4 );

        debugf( "Plotting drawfile\n" );

        e = Sprite_Redirect( area, "drawfile", NULL, &sos );
        if ( !e )
        {
            DrawFile_Render( 0, data, nSize, &tm, NULL );
            Sprite_UnRedirect( &sos );
        }

#if DEBUG
        Sprite_Save( area, "igdebug" );
        /*{
            FILE *f = fopen( "igdebug2", "wb" );
            fwrite( &area->numsprites, 1, area->areasize-4, f );
            fclose(f);
        } */
#endif

        if ( e )
        {
            debugf( "Error: %s\n", e->errmess );
            break;
        }

        debugf( "Anti-aliasing\n" );

        if ( bits24 )
        {
            Anim_AntiAlias24( (unsigned int*)( (char*)pSprite + pSprite->imageoffset ),
                              spritex,
                              ((unsigned int*)pixels.pBits) + offset,
                              w, thisy );
        }
        else
        {
            Anim_AntiAlias( (pixel*)pSprite + pSprite->imageoffset,
                            (spritex+3) & ~3,
                            ((pixel*)pixels.pBits) + offset,
                            w, thisy );
        }
    }

    Anim_FlexFree( &area );

    if ( e )
    {
        Anim_SetError( "fromdraw: plot failed, %s", e->errmess );
        Anim_Free( &pixels.pBits );
        return FALSE;
    }

    Wimp_CommandWindow(-1);

    /* Compress the frame */

    pixels.nWidth = w;
    pixels.nLineWidthBytes = bits24 ? w*4 : w;
    pixels.nHeight = h;
    pixels.nBPP = bits24 ? 32 : 8;
    pixels.csDelay = 8;

#if DEBUG
    {
        sprite_areainfo sai;
        int imgsize = h*pixels.nLineWidthBytes;
        sprite_header sh;
        FILE *f = fopen("igdebug2", "wb");

        sai.numsprites = 1;
        sai.firstoffset = 16;
        sai.freeoffset = 16 + sizeof(sprite_header) + imgsize;
        fwrite( &sai.numsprites, 1, 12, f );
        sh.offset_next = sizeof(sprite_header) + imgsize;
        strncpy( sh.name, "drawfile2", 12 );
        sh.width = (pixels.nLineWidthBytes/4)-1;
        sh.height = h-1;
        sh.leftbit = 0;
        sh.rightbit = 31;
        sh.imageoffset = sh.maskoffset = sizeof(sprite_header);
        sh.screenmode = bits24 ? (6<<27) + (90<<14) + (90<<1) + 1 : 28;
        fwrite( &sh, 1, sizeof(sprite_header), f );
        fwrite( pixels.pBits, 1, imgsize, f );
        fclose( f );
    }
#endif

    if ( !(*fn)( handle, &pixels, bits24 ? -1 : 255, NULL,
                 bits24 ? NULL : palette ) )
    {
        debugf("convertdraw: fn returned FALSE, exiting\n" );
        Anim_Free( &pixels.pBits );
        return FALSE;
    }

    Anim_Free( &pixels.pBits );

    return TRUE;
}


/*---------------------------------------------------------------------------*
 * Anti-aliasing routine -- reduces by a scale of ANIM_AAFACTOR in each      *
 * direction.                                                                *
 * Does sensible things with background areas so it's easy to make a true    *
 * transparent GIF from the output.                                          *
 * Using a regular colour cube to dither into makes the code trivial         *
 * (compared to what ChangeFSI must do, for instance) -- but that's still no *
 * excuse for not doing Floyd-Steinberg :-(                                  *
 *---------------------------------------------------------------------------*/

void Anim_AntiAlias( const pixel *srcBits, unsigned int abwSrc,
                     pixel *destBits, unsigned int w, unsigned int h )
{
    int x,y,i,j;
    unsigned int r,g,b,t;
    char byte;
    char bgbyte;
    int abwDest = w; /* Dest is byte-packed, NOT sprite format */
    char ra[256],ga[256],ba[256];
    char dividebyf2[ANIM_AAFACTOR*ANIM_AAFACTOR*6];

    /* Precalculate divisions by 6 to speed up dither */
    for (r=0; r<6; r++)
        for (g=0; g<6; g++)
            for (b=0; b<6; b++)
            {
                ra[r*36+g*6+b] = r;
                ga[r*36+g*6+b] = g;
                ba[r*36+g*6+b] = b;
            }

    /* Precalculate divisions by ANIM_AAFACTOR*ANIM_AAFACTOR */
    for ( i=0; i < ANIM_AAFACTOR*ANIM_AAFACTOR*6; i++ )
    {
        j = (i<<8) / (ANIM_AAFACTOR*ANIM_AAFACTOR);
        dividebyf2[i] = (j>>8); /* + ((j&128)?1:0);*/
    }

    bgbyte = 215;

    for ( y=0; y < h; y++ )
    {
        for ( x=0; x < w; x++ )
        {
            r=g=b=t=0;
            for (i=0; i<ANIM_AAFACTOR; i++)
                for (j=0; j<ANIM_AAFACTOR; j++)
                {
                    byte = srcBits[x*ANIM_AAFACTOR+i+j*abwSrc];
                    if ( byte==255 )
                    {
                        byte=bgbyte;
                        t++;
                    }
                    b += ba[byte]; g += ga[byte]; r += ra[byte];
                }
            if ( t == ANIM_AAFACTOR*ANIM_AAFACTOR )
                destBits[x] = 255;
            else
            {
                destBits[x] = 36 * dividebyf2[r]
                            +  6 * dividebyf2[g]
                            +      dividebyf2[b];
            }
        }
        destBits += abwDest;
        srcBits += abwSrc*ANIM_AAFACTOR;
    }
}

void Anim_AntiAlias24( const unsigned int *srcBits, unsigned int wSrc,
                       unsigned int *destBits, unsigned int w, unsigned int h )
{
    int x,y,i,j;
    unsigned int r,g,b,t,word;
    char dividebyf2[ANIM_AAFACTOR*ANIM_AAFACTOR*256];

    /* Precalculate divisions by ANIM_AAFACTOR^2 */
    for ( i=0; i < sizeof(dividebyf2); i++ )
    {
        j = (i<<8) / (ANIM_AAFACTOR*ANIM_AAFACTOR);
        dividebyf2[i] = j>>8;
    }

    for ( y=0; y<h; y++ )
    {
        for ( x=0; x<w; x++ )
        {
            r=g=b=t=0;
            for ( i=0; i<ANIM_AAFACTOR; i++ )
                for ( j=0; j<ANIM_AAFACTOR; j++ )
                {
                    word = srcBits[x*ANIM_AAFACTOR+i+j*wSrc];
                    if ( word == (unsigned)-1 )
                    {
                        t++;
                    }
                    r += (word & 0xFF);
                    g += (word & 0xFF00)>>8;
                    b += (word & 0xFF0000)>>16;
                    /*b += (word & 0xFF000000)>>24;*/
                }

            if ( t == ANIM_AAFACTOR*ANIM_AAFACTOR )
                destBits[x] = (unsigned)-1;
            else
            {
                destBits[x] = ( dividebyf2[b] << 16 )
                            + ( dividebyf2[g] << 8 )
                            + ( dividebyf2[r] << 0 );
            }
        }
        destBits += w;
        srcBits += wSrc*ANIM_AAFACTOR;
    }
}

/* eof */
