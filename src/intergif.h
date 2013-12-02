/* InterGif.h */

/* Header file for central InterGif routine
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 */

#ifndef intergif_intergif_h
#define intergif_intergif_h

#ifndef animlib_h
#include "animlib.h"
#endif

#ifndef BOOL
#define BOOL int
#define TRUE 1
#define FALSE 0
#endif

/* Turn a file or files into a GIF. bSprite and palfile only works on RiscOS.
 */

BOOL InterGif( const char *infile, BOOL bList, BOOL bJoin, BOOL bSplit, BOOL bSprite,
               anim_GIFflags flags, BOOL bForceTrans, pixel nForceTransPixel,
               BOOL bUseTrans, BOOL bTrim, BOOL b216, BOOL b256, BOOL g16, BOOL g256,
               BOOL bDiffuse, BOOL bZigZag, BOOL bNewFormat, int xdpi, int ydpi,
               const char *palfile, BOOL bSame, int nBest, const char *cfsi,
               const char *outfile );

#endif
