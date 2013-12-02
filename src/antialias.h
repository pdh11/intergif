/* AntiAlias.h */

/* Header file for anti-aliasing frames down to 1/FACTOR the size
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 */

#ifndef animlib_antialias_h
#define animlib_antialias_h

#ifndef animlib_h
#include "animlib.h"
#endif

#define ANIM_AAFACTOR 3

/* in LoadDraw.c */
void Anim_AntiAlias( const pixel *srcBits, unsigned int abwSrc, pixel *dstBits,
                     unsigned int w, unsigned int h );

void Anim_AntiAlias24( const unsigned int *srcBits, unsigned int wSrc,
                       unsigned int *destBits, unsigned int w, unsigned int h );
#endif
