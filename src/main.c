/* Main.c */

/* Main code for InterGif
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *      Martin Würthner
 *
 *
 * History:
 *      14-Dec-95 *** Release 1.00
 *      03-Apr-96 pdh Add compression of multiple sprites into Netscape 2
 *                    animated GIF format
 *      04-Apr-96 pdh Add '-d' and '-loop' options
 *      04-Apr-96 *** Release 2.00
 *      21-May-96 *** Release 2.01
 *      11-Aug-96 pdh Add support for colour depth optimisation (and '-same')
 *      11-Aug-96 *** Release 2.02
 *      25-Aug-96 pdh Add support for TCA files
 *      25-Aug-96 *** Release 3.00
 *      01-Sep-96 pdh Add '-split' option
 *      01-Sep-96 pdh Work out bLoop and transpixel from input if possible
 *      01-Sep-96 *** Release 3.01
 *      25-Sep-96 pdh Added '-kludge' option for fiddling with replace method
 *      21-Oct-96 pdh Remove '-kludge' (replaced with better optimisation code)
 *      27-Oct-96 pdh Add '-s' option
 *      27-Oct-96 *** Release 4beta1
 *      29-Oct-96 *** Release 4beta2
 *      07-Nov-96 pdh version.h stuff
 *      07-Nov-96 *** Release 4
 *      27-Nov-96 pdh Added '-join' option
 *      15-Dec-96 *** Release 5beta1
 *      27-Jan-97 *** Release 5beta2
 *      29-Jan-97 *** Release 5beta3
 *      30-Jan-97 pdh Made able to run as subtask
 *      03-Feb-97 *** Release 5
 *      07-Feb-97 *** Release 5.01
 *      10-Mar-97 pdh Frob for new anim library
 *      11-Mar-97 pdh Add '-216' and '-256' options
 *      07-Apr-97 *** Release 6beta1
 *      20-May-97 *** Release 6beta2
 *      02-Jun-97 pdh Moved more of the work into intergif.c
 *      24-Aug-97 *** Release 6
 *      27-Sep-97 pdh Add bForceDelay field to anim_GIFflags
 *      27-Sep-97 *** Release 6.01
 *      08-Nov-97 *** Release 6.02
 *      08-Feb-98 pdh Add my_stricmp
 *      12-Feb-98 pdh Add '-c' option
 *      21-Feb-98 *** Release 6.03
 *      07-Jun-98 *** Release 6.04
 *      21-Aug-98 *** Release 6.05
 *      04-Nov-98 *** Release 6.06
 *      19-Feb-99 *** Release 6.07
 *      26-Mar-00 *** Release 6.10
 *      18-Aug-00 mw  Add '-diffuse' and '-zigzag'
 *      28-Aug-00 *** Release 6.11
 *
 * References:
 *      http://utter.chaos.org.uk/~pdh/software/intergif.htm
 *          pdh's InterGif home page
 *      http://www.n-vision.com/panda/gifanim/
 *          A site all about animated GIFs for other platforms
 *      http://asterix.seas.upenn.edu/~mayer/lzw_gif/gif89.html
 *          The GIF89a spec
 *      http://www.iota.co.uk/info/animfile/
 *          The Complete Animator file format
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __acorn
#include "kernel.h"
#include "DeskLib:Core.h"
#include "memflex.h"
#endif

#include "version.h"
#include "animlib.h"
#include "count.h"
#include "split.h"
#include "intergif.h"

#if 0
#define debugf printf
#define DEBUG 1
#else
#define debugf 1?0:printf
#define DEBUG 0
#endif

static const char ERROR_VAR[] = "InterGif$Error";

static char *infile = NULL;
static char *outfile = NULL;
static char *palfile = NULL;
static char *cfsi = NULL;
static anim_GIFflags flags;
static BOOL bSplit = FALSE;
static BOOL bSprite = FALSE;
static BOOL bList = FALSE;
static BOOL bJoin = FALSE;
static BOOL b216 = FALSE;
static BOOL b256 = FALSE;
static BOOL g16 = FALSE;
static BOOL g256 = FALSE;
static BOOL bSame = FALSE;
static BOOL bTrim = FALSE;
static BOOL bDiffuse = FALSE;
static BOOL bZigZag = FALSE;
static BOOL bNewFormat = FALSE;
static BOOL bUseTrans = FALSE;
static BOOL bForceTrans = FALSE;
static pixel nForceTransPixel;
static int nBest = -1;
static int xdpi = 90;
static int ydpi = 90;

static BOOL bDesktop = FALSE;

static void Usage( void )
{
#ifdef __acorn
    if ( bDesktop )
        _kernel_setenv( ERROR_VAR, "Internal error: bad arguments" );
    else
#endif
    {
#ifdef BETA
        fputs( "InterGif " VERSION " BETA " BETA " (" __DATE__ ") by Peter Hartley et al (K) All Rites Reversed\n"
               "Please do NOT redistribute beta copies of InterGif\n", stdout );
#else

        fputs( "InterGif " VERSION " by Peter Hartley et al (K) All Rites Reversed\n",
               stdout );
#endif
        fputs(
#ifdef __acorn
"Usage: intergif [-i] [-o out] [-t [pixel]] [-d cs] [-loop] [-split] [-join]\n"
"           [-c args] [palette-options] infile\n"
"       intergif -s [-o out] [-split] [-join] [-c args] [palette-options] infile\n"
#else
"Usage: intergif [-i] [-o out] [-t [pixel]] [-d cs] [-loop] [-split] [-join]\n"
"           [palette-options] infile\n"
#endif
"        -i Make interlaced gif\n"
"        -o Name output file (default is <infile>/gif)\n"
"        -t Make gif with transparency\n"
"        -trim Remove any wholly transparent borders\n"
"        -d Delay time between frames (centiseconds)\n"
"        -diffuse Use Floyd-Steinberg error diffusion (on true-colour input)\n"
"        -zigzag Use 'zig-zag' Floyd-Steinberg (recommended)\n"
"        -loop Add 'Netscape' looping extension\n"
"        -split Make each frame into a separate output file\n"
"        -join Take each frame from a separate input file\n"
"        -list Interpret infile as a list of input files\n"
#ifdef __acorn
"        -c Preprocess with ChangeFSI, eg -c \"28 2:1 2:1\"\n"
"        -s Make sprite output instead of gif\n"
"        -new Output a new format sprite\n"
"        -xdpi 180 | 90 | 45 | 22 Horizontal dpi specification for new format sprite\n"
"        -ydpi 180 | 90 | 45 | 22 Vertical dpi specification for new format sprite\n"
"        infile  A RiscOS sprite file, Draw file, Complete Animator file, or gif\n"
#else
"        infile  A Complete Animator file, RiscOS sprite, or gif\n"
#endif
"        palette-options are:\n"
"        [-same] [-216 | -256 | -g16 | -g256 | -pal palfile | -best n]\n"
"           -same Preserve unused colours in input palette\n"
"           -216 Map to 216-colour cube as used by Netscape\n"
#ifdef __acorn
"           -256 Map to standard 256-colour Acorn palette\n"
"           -pal Map to supplied palette\n"
#endif
"           -g16 Map to 16 colour greyscale palette\n"
"           -g256 Map to 256-colour greyscale palette\n"
"           -best Find optimal n colours and map to them\n"
#ifdef __acorn
"        One of -216, -256, -g16, -g256, -pal or -best must be used with Draw files\n"
"        or deep sprites\n"
#endif
#ifndef BETA
"See http://utter.chaos.org.uk/~pdh/software/intergif.htm for details\n"
#endif
            , stdout );
    }

    exit( 1 );
}

static int my_stricmp( const char *s1, const char *s2 )
{
    int i;
    while ( *s1 && *s2 )
    {
        i = tolower(*s1) - tolower(*s2);
        if ( i )
            return i;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

static void DecodeArgs( int argc, char *argv[] )
{
    int i;

#if DEBUG
    for ( i=0; i<argc; i++ )
        debugf( "%s ", argv[i] );
    debugf( "\n\n" );
#endif

    *((int*)&flags) = 0;

    flags.nDefaultDelay = 8;

    for ( i = 1; i < argc; i++ )
    {
        if ( !my_stricmp( argv[i], "-i" ) )
        {
            flags.bInterlace = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-o" ) )
        {
            if ( i == argc-1 )
                Usage();
            outfile = argv[i+1];
            i++;
        }
        else if ( !my_stricmp( argv[i], "-t" ) )
        {
            if ( i < argc-1 && isdigit( argv[i+1][0] ) )
            {
                bForceTrans = TRUE;
                nForceTransPixel = atoi( argv[i+1] );
                i++;
            }
            else
                bUseTrans = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-trim" ) )
        {
            bTrim = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-d" ) )
        {
            if ( i == argc-1 )
                Usage();
            flags.nDefaultDelay = atoi( argv[i+1] );
            if ( flags.nDefaultDelay == 0 )
                flags.nDefaultDelay = 8;
            flags.bForceDelay = TRUE;
            i++;
        }
        else if ( !my_stricmp( argv[i], "-diffuse" ) )
        {
            bDiffuse = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-zigzag" ) )
        {
            bDiffuse = TRUE;
            bZigZag = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-loop" ) )
        {
            flags.bLoop = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-same" ) )
        {
            bSame = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-split" ) )
        {
            bSplit = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-216" ) )
        {
            b216 = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-256" ) )
        {
            b256 = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-g256" ) )
        {
            g256 = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-g16" ) )
        {
            g16 = TRUE;
        }
#ifdef __acorn
        else if ( !my_stricmp( argv[i], "-pal" ) )
        {
            if ( i == argc-1 )
                Usage();
            palfile = argv[i+1];
            i++;
        }
        else if ( !my_stricmp( argv[i], "-c" ) )
        {
            if ( i == argc-1 )
                Usage();
            cfsi = argv[i+1];
            i++;
        }
#endif
        else if ( !my_stricmp( argv[i], "-best" ) )
        {
            if ( i == argc-1 )
                Usage();
            nBest = atoi( argv[i+1] );
            if ( nBest < 2 || nBest > 256 )
                nBest = -1;
            i++;
        }
        else if ( !my_stricmp( argv[i], "-list" ) )
        {
            bList = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-join" ) )
        {
            bJoin = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-s" ) )
        {
            bSprite = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-new" ) )
        {
            bSprite = TRUE; bNewFormat = TRUE;
        }
        else if ( !my_stricmp( argv[i], "-xdpi" ) )
        {
            if ( i == argc-1 )
                Usage();
            xdpi = atoi( argv[i+1] );
            if (xdpi != 180 && xdpi != 90 && xdpi != 45 && xdpi != 22) Usage();
            i++;
        }
        else if ( !my_stricmp( argv[i], "-ydpi" ) )
        {
            if ( i == argc-1 )
                Usage();
            ydpi = atoi( argv[i+1] );
            if (ydpi != 180 && ydpi != 90 && ydpi != 45 && ydpi != 22) Usage();
            i++;
        }
        else if ( !my_stricmp( argv[i], "-desktop" ) )
        {
            bDesktop = TRUE;
        }
        else
        {
            if ( infile )
                Usage();
            infile = argv[i];
        }
    }
    if ( bList && bJoin )
    {
        printf( "-join and -list are mutually exclusive\n" );
        Usage();
    }
    if ( bList && cfsi )
    {
        printf( "-cfsi cannot be used with -list\n" );
        Usage();
    }
    if ( !infile )
        Usage();
    if ( !outfile )
    {
        outfile = malloc( strlen( infile )+6 );
        sprintf( outfile, "%s/gif", infile );
    }
}

static void Error( void )
{
#ifdef __acorn
    if ( bDesktop )
        _kernel_setenv( ERROR_VAR, Anim_Error );
    else
#endif
        fprintf( stdout, "%s\n", Anim_Error );
    exit(1);
}

int main( int argc, char *argv[] )
{
    DecodeArgs( argc, argv );

#ifdef __acorn
    if ( bDesktop )
        _kernel_setenv( ERROR_VAR, NULL );   /* remove variable */
#endif

#ifdef BETA
    if ( !bDesktop )
        fputs(
"InterGif " VERSION " BETA " BETA " (" __DATE__ ") by Peter Hartley (K) All Rites Reversed\n"
"Please do NOT redistribute beta copies of InterGif\n", stdout
             );
#endif

    if ( !InterGif( infile, bList, bJoin, bSplit, bSprite, flags, bForceTrans,
                    nForceTransPixel, bUseTrans, bTrim, b216, b256, g16, g256, bDiffuse, bZigZag,
                    bNewFormat, xdpi, ydpi,
                    palfile, bSame, nBest, cfsi, outfile ) )
        Error();

#ifdef __acorn
    _kernel_setenv( ERROR_VAR, "" );
#endif

    return 0;
}
