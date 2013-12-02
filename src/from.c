/* From.c */

/* Whole-animation constructors for InterGif
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *      Martin Würthner
 *
 * History:
 *      21-Aug-98 pdh Split off from intergif.c
 *      21-Aug-98 *** Release 6.05
 *      05-Oct-98 pdh Fix pixmasks
 *      05-Oct-98 *** Release 6.06
 *      19-Feb-99 *** Release 6.07
 *      26-Mar-00 pdh Make bSame work with palettemappers
 *      26-Mar-00 *** Release 6.10
 *      18-Aug-00 mw  Floyd-Steinberg dithering of deep source images
 *      28-Aug-00 *** Release 6.11
 *
 */

#include <stdio.h>

#include "animlib.h"
#include "utils.h"

#if 0
#define debugf printf
#define DEBUG 1
#else
#define debugf 1?0:printf
#define DEBUG 0
#endif

/* Convert %0bbbbbgggggrrrrr to &BBGGRR00 */
unsigned int Anim_Pix16to32(unsigned int x)
{
    unsigned int b = (x>>(10-3)) & 0xF8;
    unsigned int g = (x>>(5-3)) & 0xF8;
    unsigned int r = (x<<3) & 0xF8;
    b += (b>>5);
    g += (g>>5);
    r += (r>>5);
    return (b<<24) + (g<<16) + (r<<8);
}

/* Convert &BBGGRR00 to grey-scale */
unsigned int ToGrey ( unsigned int c )
{   /* use the CIE luminance weights for r, g and b (0.299, 0.587, 0.114) */
    double grey =   0.114 * ( ( c >> 24 ) & 0xffu ) + 0.587 * ( ( c >> 16 ) & 0xffu )
                  + 0.299 * ( ( c >> 8 ) & 0xffu );
    unsigned int component = (int)grey;
    return ( component << 24 ) + ( component << 16 ) + ( component << 8 );
}

static void MakeByteWide( const anim_imageinfo *aii, pixel *pOutput,
                          palettemapper pmap, BOOL bDiffuse, BOOL bZigZag, BOOL bGrey)
{
    int bpp = aii->nBPP;
    int x,y,i;
    const char *pBits = (const char*) aii->pBits;
    int h = aii->nHeight, w = aii->nWidth;

    if ( bpp > 8 )
    {
        /* Direct colour (must be mapped -- or dithered) */

        /* Dithering variables: Some memory could be saved by using shorts, but
         * this would be slower and we are only talking about one pixel row
	 * here anyway.
         */
        int *lasterr_r, *lasterr_g, *lasterr_b;  /* error in previous row */

        int preverr_r=0, preverr_g=0, preverr_b=0;/* error in previous pixel */
        pixel index;
        unsigned int palette_entry;

        if ( bDiffuse )
        {
            /* allocate the buffer line for last line's errors */
            lasterr_r = (int*)Anim_Allocate(sizeof(int) * w);
            lasterr_g = (int*)Anim_Allocate(sizeof(int) * w);
            lasterr_b = (int*)Anim_Allocate(sizeof(int) * w);
            for (i = 0; i < w; i++)
            {
                lasterr_r[i] = 0;
                lasterr_g[i] = 0;
                lasterr_b[i] = 0;
            }
        }

        /* Note y gets incremented "manually" in the zig-zag dithering case */
        for ( y=0; y<h; y++ )
        {
            const unsigned short *pShorts = (unsigned short*) pBits;
            const unsigned int *pWords = (unsigned int*) pBits;
            int r=0, g=0, b=0;        /* signed because we add the error
                                       * values, so they can get negative */
            int tlerr_r=0, tlerr_g=0, tlerr_b=0;  /* backup value of preverr[x - 1] */

            for ( x=0; x<w; x++ )
            {
                unsigned int pix;
                if ( bpp == 16 )
                {
                    pix = *pShorts++;
                    pix = Anim_Pix16to32(pix);
                }
                else /* bpp == 32 */
                    pix = (*pWords++) << 8;

		/* if we are doing greyscale dither, then we cannot simply use
		 * FSI dithering on the original pixel values because we want
		 * to spread the luminance error and not the indvidual R, G and
		 * B components error - therefore, we reduce the colour space
		 * to a single dimension immediately.
		 */
                if ( bGrey )
                    pix = ToGrey( pix );

                if ( bDiffuse )
                {
                    /* error correction, use 28.4 fixed point arithmetics */

                    b = ((unsigned int)(pix >> 20)) & 0xff0;
                    g = (pix >> 12) & 0xff0;
                    r = (pix >> 4) & 0xff0;

                    if (x > 0)
                    {
                        /* 7/16 of error from pixel to the left */
                        r -= preverr_r * 7;
                        g -= preverr_g * 7;
                        b -= preverr_b * 7;

                        if (y > 0)
                        {
                            /* 1/16 of error from pixel to the top-left, always
                             * defined for x > 0. NB: we cannot take
                             * lasterr_*[x - 1] as this has already been
                             * overwritten
                             */
                            r -= tlerr_r;
                            g -= tlerr_g;
                            b -= tlerr_b;
                        }
                    }

                    if (y > 0)
                    {
                        /* 5/16 of error from pixel above */
                        r -= lasterr_r[x] * 5;
                        g -= lasterr_g[x] * 5;
                        b -= lasterr_b[x] * 5;

                        if (x < w - 1)
                        {
                            /* 3/16 of error from pixel to the top-right */
                            r -= lasterr_r[x + 1] * 3;
                            g -= lasterr_g[x + 1] * 3;
                            b -= lasterr_b[x + 1] * 3;
                        }
                    }

                    /* normalize the values */
                    r = (r + 8) >> 4;
                    g = (g + 8) >> 4;
                    b = (b + 8) >> 4;

                    if (r < 0) r = 0;
                    if (r > 255) r = 255;

                    if (g < 0) g = 0;
                    if (g > 255) g = 255;

                    if (b < 0) b = 0;
                    if (b > 255) b = 255;

                    /* this is the corrected pixel value */
                    pix = (b << 24) + (g << 16) + (r << 8);
                }

                index = PaletteMapper_MapIndex( pmap, pix );
                /* could be done twice as fast if we could get the index and
                 * the colour at the same time!
                 */
                palette_entry = PaletteMapper_Map( pmap, pix );
                *pOutput++ = index;

                if (bDiffuse)
                {
                    /* now compute the error for this pixel */
                    /* r, g, b defines the colour we wanted */
                    int ar, ag, ab;    /* the colour we got */
                    ab = ((unsigned int)(palette_entry >> 24)) & 0xff;
                    ag = (palette_entry >> 16) & 0xff;
                    ar = (palette_entry >> 8) & 0xff;

                    preverr_r = ar - r;
                    preverr_g = ag - g;
                    preverr_b = ab - b;

                    /* we keep a backup copy of lasterr_*[x] because we need it
                     * for the following pixel
                     */
                    tlerr_r = lasterr_r[x];
                    tlerr_g = lasterr_g[x];
                    tlerr_b = lasterr_b[x];

                    lasterr_r[x] = preverr_r;
                    lasterr_g[x] = preverr_g;
                    lasterr_b[x] = preverr_b;
                }
            }
            pBits += aii->nLineWidthBytes;

            /* If this was not the last row, do another iteration in the
             * opposite direction. Code is duplicated, but "if (bDiffuse)" and
             * "if (y > 0)" can be omitted now
             */
            if ( bDiffuse && bZigZag && y < h - 1 )
            {
                /* do another row, but now from right to left to reduce
                 * patterning (allegedly)
                 */
                if (bpp == 16)
                    pShorts = ((unsigned short*) pBits) + (w - 1);
                else
                    pWords = ((unsigned int*) pBits) + (w - 1);

                pOutput += w - 1;
                for ( x = w - 1; x >= 0; x-- )
                {
                    int ar, ag, ab;    /* the colour we got */
                    unsigned int pix;

                    if ( bpp == 16 )
                    {
                        pix = *pShorts--;
                        pix = Anim_Pix16to32(pix);
                    }
                    else /* bpp == 32 */
                        pix = (*pWords--) << 8;

		    if ( bGrey )
		        pix = ToGrey( pix );

                    /* error correction, use 28.4 fixed point arithmetics */
                    b = ((unsigned int)(pix >> 20)) & 0xff0;
                    g = (pix >> 12) & 0xff0;
                    r = (pix >> 4) & 0xff0;

                    if (x < w - 1)
                    {
                        /* 7/16 of error from pixel to the right */
                        r -= preverr_r * 7;
                        g -= preverr_g * 7;
                        b -= preverr_b * 7;

                        /* 3/16 of error from pixel to the top-right, always
                         * defined for x > 0. NB: we cannot take
                         * lasterr_*[x + 1] as this has already been
                         * overwritten
                         */
                        r -= tlerr_r * 3;
                        g -= tlerr_g * 3;
                        b -= tlerr_b * 3;
                    }

                    /* 5/16 of error from pixel above */
                    r -= lasterr_r[x] * 5;
                    g -= lasterr_g[x] * 5;
                    b -= lasterr_b[x] * 5;

                    if (x > 0)
                    {
                        /* 1/16 of error from pixel to the top-left */
                        r -= lasterr_r[x - 1];
                        g -= lasterr_g[x - 1];
                        b -= lasterr_b[x - 1];
                    }

                    /* normalize the values */
                    r = (r + 8) >> 4;
                    g = (g + 8) >> 4;
                    b = (b + 8) >> 4;

                    if (r < 0) r = 0;
                    if (r > 255) r = 255;

                    if (g < 0) g = 0;
                    if (g > 255) g = 255;

                    if (b < 0) b = 0;
                    if (b > 255) b = 255;

                    /* this is the corrected pixel value */
                    pix = (b << 24) + (g << 16) + (r << 8);

                    index = PaletteMapper_MapIndex( pmap, pix );
                    /* could be done twice as fast if we could get the index
                     * and the colour at the same time!
                     */
                    palette_entry = PaletteMapper_Map( pmap, pix );
                    *pOutput-- = index;

                    /* now compute the error for this pixel */
                    /* r, g, b defines the colour we wanted */
                    ab = ((unsigned int)(palette_entry >> 24)) & 0xff;
                    ag = (palette_entry >> 16) & 0xff;
                    ar = (palette_entry >> 8) & 0xff;

                    preverr_r = ar - r;
                    preverr_g = ag - g;
                    preverr_b = ab - b;

                    /* we keep a backup copy of lasterr_*[x] because we need it
                     * for the following pixel
                     */
                    tlerr_r = lasterr_r[x];
                    tlerr_g = lasterr_g[x];
                    tlerr_b = lasterr_b[x];

                    lasterr_r[x] = preverr_r;
                    lasterr_g[x] = preverr_g;
                    lasterr_b[x] = preverr_b;
                }

                pBits += aii->nLineWidthBytes;
                y++; /* increment "manually" */
                pOutput += w + 1;
            }
        }

        if ( bDiffuse )
        {
            Anim_Free(&lasterr_b);
            Anim_Free(&lasterr_g);
            Anim_Free(&lasterr_r);
        }
    }
    else
    {
        /* Palettised colour (no mapping needed) */
        for ( y=0; y<h; y++ )
        {
            unsigned int imagewant = (1<<bpp) - 1;
            unsigned char *imgptr = (unsigned char*) pBits;
            unsigned int imageshift = 0;

            for ( x=0; x<w; x++ )
            {
                unsigned int pix = (*imgptr >> imageshift) & imagewant;

                *pOutput++ = pix;

                imageshift += bpp;
                if ( imageshift == 8 )
                {
                    imgptr++;
                    imageshift = 0;
                }
            }
            pBits += aii->nLineWidthBytes;
        }
    }
}

typedef struct {
    anim a;
    palettemapper pmap;
    BOOL bSame;
    BOOL bDiffuse;
    BOOL bZigZag;
    BOOL bGrey;
} animmaker_args;

static BOOL Animmaker( void *handle, unsigned int xs, unsigned int ys,
                       unsigned int flags )
{
    animmaker_args *pArgs = (animmaker_args*) handle;
    anim a;

    if ( pArgs->a )
        return TRUE;

    a = Anim_Create();

    if ( !a )
    {
        debugf( "animmaker: Anim_Create failed\n" );
        return FALSE;
    }
    pArgs->a = a;
    a->nWidth = xs;
    a->nHeight = ys;
    a->flags = flags;

    debugf( "animmaker: %dx%d, flags 0x%x\n", xs, ys, flags );

    return TRUE;
}

static BOOL Animframe( void *handle, const anim_imageinfo *pixels,
                       int pixmask, const anim_imageinfo *bitmask,
                       const unsigned int *pColours )
{
    animmaker_args *pArgs = (animmaker_args*) handle;
    anim a = pArgs->a;
    int bpp = pixels->nBPP;
    pixel *image = NULL, *mask = NULL;
    unsigned int xs, ys;
    BOOL bMask = ( pixmask != -1 || bitmask );
    unsigned int mappedpal[256];
    const unsigned int *pPalette = pColours;
    int ncol = 1<<bpp;
    BOOL result = TRUE;

    xs = pixels->nWidth;
    ys = pixels->nHeight;

    if ( xs != a->nWidth || ys != a->nHeight )
    {
        Anim_SetError( "All images must be the same size" );
        return FALSE;
    }

    if ( bpp > 8 && !pArgs->pmap )
    {
        Anim_SetError( "Please choose a palette-reduction option when using images with more than 256 colours" );
        return FALSE;
    }

    image = Anim_Allocate( xs*ys );

    if ( bMask )
        mask = Anim_Allocate( xs*ys );

    if ( !image || (bMask && !mask ) )
    {
        Anim_NoMemory("animmaker");
        return FALSE;
    }

    if ( bitmask )
    {
        debugf( "animmaker: calling mbw(mask)\n" );
        MakeByteWide( bitmask, mask, NULL, FALSE, FALSE, FALSE );
    }

    debugf( "animmaker: calling mbw(image)\n" );

    MakeByteWide( pixels, image, pArgs->pmap, pArgs->bDiffuse, pArgs->bZigZag, pArgs->bGrey );

    debugf( "animmaker: mbw ok\n" );

    if ( pixmask != -1 )
    {
        unsigned int i;

        debugf( "animmaker: making pixmask\n" );

        for ( i=0; i<xs*ys; i++ )
            mask[i] = (image[i] == pixmask) ? 0 : 255;
    }

    if ( pArgs->pmap )
    {
        if ( pArgs->bSame )
        {
            /* Palettemapper and bSame means put pmap's entire palette into
             * the output
             */
            if ( bpp <= 8 )
            {
                int i;

                /* Use 'mappedpal' as temp buffer here */
                for ( i=0; i<ncol; i++ )
                {
                    mappedpal[i] = PaletteMapper_MapIndex( pArgs->pmap,
                                                           pColours[i] );
                }
                for ( i=0; i<xs*ys; i++ )
                    image[i] = mappedpal[image[i]];
            }
            ncol = PaletteMapper_GetPalette( pArgs->pmap, mappedpal );
        }
        else
        {
            if ( bpp > 8 )
                ncol = PaletteMapper_GetPalette( pArgs->pmap, mappedpal );
            else
            {
                int i;

                for ( i=0; i<ncol; i++ )
                    mappedpal[i] = PaletteMapper_Map( pArgs->pmap,
                                                      pColours[i] );
            }
        }
        pPalette = mappedpal;
    }

    if ( Anim_AddFrame( a, xs, ys, image, bMask ? mask : NULL, ncol,
                         pPalette ) )
    {
        a->pFrames[a->nFrames-1].csDelay = pixels->csDelay;
    }
    else
    {
        result = FALSE;
        debugf( "animmaker: Anim_AddFrame failed\n" );
    }

    Anim_Free( &image );
    Anim_Free( &mask );

    return result;
}


        /*=======================*
         *   Exported routines   *
         *=======================*/


anim Anim_FromData( const void *data, unsigned int size,
                    palettemapper pmap, BOOL bSame, BOOL bDiffuse, BOOL bZigZag, BOOL bGrey )
{
    animmaker_args aa = { NULL, NULL, FALSE };
    BOOL result;

    aa.pmap = pmap;
    aa.bSame = bSame;
    aa.bDiffuse = bDiffuse;
    aa.bZigZag = bZigZag;
    aa.bGrey = bGrey;
    result = Anim_ConvertData( data, size, &Animmaker, &Animframe, &aa );

    if ( !result )
        Anim_Destroy( &aa.a );

    return aa.a;
}

anim Anim_FromFile( const char *filename, palettemapper pmap, BOOL bSame, BOOL bDiffuse, BOOL bZigZag, BOOL bGrey )
{
    animmaker_args aa = { NULL, NULL, FALSE };
    BOOL result;

    aa.pmap = pmap;
    aa.bSame = bSame;
    aa.bDiffuse = bDiffuse;
    aa.bZigZag = bZigZag;
    aa.bGrey = bGrey;
    result = Anim_ConvertFile( filename, &Animmaker, &Animframe, &aa );

    if ( !result )
        Anim_Destroy( &aa.a );

    return aa.a;
}

anim Anim_FromFiles( const char *firstname, palettemapper pmap, BOOL bSame, BOOL bDiffuse, BOOL bZigZag, BOOL bGrey )
{
    animmaker_args aa = { NULL, NULL, FALSE };
    BOOL result;

    aa.pmap = pmap;
    aa.bSame = bSame;
    aa.bDiffuse = bDiffuse;
    aa.bZigZag = bZigZag;
    aa.bGrey = bGrey;
    result = Anim_ConvertFiles( firstname, &Animmaker, &Animframe, &aa );

    if ( !result )
        Anim_Destroy( &aa.a );

    return aa.a;
}

anim Anim_FromList( const char *listname, palettemapper pmap, BOOL bSame, BOOL bDiffuse, BOOL bZigZag, BOOL bGrey )
{
    animmaker_args aa = { NULL, NULL, FALSE };
    BOOL result;

    aa.pmap = pmap;
    aa.bSame = bSame;
    aa.bDiffuse = bDiffuse;
    aa.bZigZag = bZigZag;
    aa.bGrey = bGrey;
    result = Anim_ConvertList( listname, &Animmaker, &Animframe, &aa );

    if ( !result )
        Anim_Destroy( &aa.a );

    return aa.a;
}

/* eof */
