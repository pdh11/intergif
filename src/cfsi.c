/* cfsi.c */

/* Calling ChangeFSI from InterGif
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *
 * History:
 *      12-Feb-98 pdh Created
 *      21-Feb-98 *** Release 6.03
 *      07-Jun-98 *** Release 6.04
 *      21-Aug-98 *** Release 6.05
 *      05-Oct-98 *** Release 6.06
 *      19-Feb-99 *** Release 6.07
 *      26-Mar-00 *** Release 6.10
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "kernel.h"

#include "DeskLib:Wimp.h"
#include "DeskLib:WimpSWIs.h"

#include "utils.h"
#include "animlib.h"

#include "cfsi.h"

#define CHANGEFSIERROR "ChangeFSI$ReturnCode"

BOOL ChangeFSI( const char *in, const char *out, const char *options )
{
#ifdef __acorn
    char buffer[300];
    int current, next, oldnext, free;

    if ( !getenv("ChangeFSI$Dir") )
    {
        Anim_SetError("ChangeFSI not found");
        return FALSE;
    }

    _kernel_setenv( CHANGEFSIERROR, NULL );

    sprintf( buffer, "/<ChangeFSI$Dir>.ChangeFSI %s %s %s -nomode",
             in, out, options );

/*
    current = oldnext = free = -1;
    Wimp_SlotSize( &current, &oldnext, &free );

    next = free;
    current = free = -1;
    Wimp_SlotSize( &current, &next, &free );*/

    if ( system( buffer ) )
    {
/*        current = free = -1;
        Wimp_SlotSize( &current, &oldnext, &free ); */

        Anim_SetError( "Couldn't start ChangeFSI task" );
        return FALSE;
    }

/*    current = free = -1;
    Wimp_SlotSize( &current, &oldnext, &free ); */

    if ( getenv( CHANGEFSIERROR ) )
    {
        Anim_SetError( getenv(CHANGEFSIERROR) );
        return FALSE;
    }

    return TRUE;
#else
    Anim_SetError( "-c only works on RiscOS" );
    return FALSE;
#endif
}

/* eof */
