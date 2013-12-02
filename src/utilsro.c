/* UtilsRO.c */

/* RiscOS implementation of utils.h
 * (K) All Rites Reversed - Copy What You Like (see file Copying)
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 *
 * History:
 *      10-Nov-96 pdh Started
 *      15-Dec-96 *** Release 5beta1
 *      27-Jan-97 *** Release 5beta2
 *      29-Jan-97 *** Release 5beta3
 *      02-Feb-97 pdh Add Anim_Percent
 *      03-Feb-97 *** Release 5
 *      07-Feb-97 *** Release 5.01
 *      01-Mar-97 pdh Added Reallocate
 *      07-Apr-97 *** Release 6beta1
 *      20-May-97 *** Release 6beta2
 *      24-Aug-97 *** Release 6
 *      27-Sep-97 *** Release 6.01
 *      12-Oct-97 pdh Remove last ExtrasLib dependency
 *      08-Nov-97 *** Release 6.02
 *      21-Feb-98 *** Release 6.03
 *      07-Jun-98 *** Release 6.04
 *      21-Aug-98 *** Release 6.05
 *      19-Feb-99 *** Release 6.07
 *      26-Mar-00 *** Release 6.10
 *
 */

#include <stdlib.h>
#include <string.h>
#include "kernel.h"

#include "DeskLib:File.h"
#include "DeskLib:Hourglass.h"

#include "utils.h"

#if 0
#define debugf printf
#define DEBUG 1
#else
#define debugf 1?0:printf
#define DEBUG 0
#endif

int Anim_FileSize( const char *filename )
{
    return File_Size( (char*)filename );
}

BOOL Anim_LoadFile( const char *filename, void *data )
{
    FILE *f = fopen( filename, "rb" );
    size_t sz;

    if ( !f )
        return FALSE;

    fseek( f, 0, SEEK_END );

    sz = (size_t)ftell(f);

    fseek( f, 0, SEEK_SET );

    fread( data, 1, sz, f );

    fclose( f );

    return TRUE;
}

void Anim_SetFileType( const char *filename, int type )
{
    File_SetType( (char*)filename, type );
}

void Anim_Percent( int percent )
{
    /* Rather fortunately, this does nothing if the hourglass isn't showing */
    Hourglass_Percentage( percent );
}

#if DEBUG
/* Exciting allocation checking */

#define MAGICWORD 0x6B637546

typedef struct memcheck {
    struct memcheck *next;
    struct memcheck *prev;
    int nSize;
    int magic;
    char data[1];       /* or more */
} memcheck;

memcheck *list = NULL;

int inuse = 0;
int hwm = 0;

void *Anim_Allocate( int nSize )
{
    void *res;
    memcheck *mc;

    nSize = ( nSize+3 ) & (~3);

    debugf( "malloc(%d)\n", nSize );

    res = malloc( nSize + 16 + 4 );
    mc = (memcheck*) res;

    if ( !res )
    {
        debugf( "malloc fails!\n" );
        return NULL;
    }

    if ( list )
        list->prev = mc;

    mc->next = list;
    mc->prev = NULL;
    list = mc;
    mc->magic = MAGICWORD;
    mc->nSize = nSize;
    *(int*)(mc->data + nSize) = MAGICWORD;

    inuse += nSize;
    if ( inuse > hwm )
        hwm = inuse;

    return mc->data;
}

static BOOL ValidateAddress( void *addr )
{
    _kernel_swi_regs r;
    int c;

    r.r[0] = (int)addr;
    r.r[1] = 4 + (int)addr;
    _kernel_swi_c( 0x2003A, &r, &r, &c );
    return c==0;
}

static void CheckBlock( memcheck *mc )
{
    int tail;

    if ( ((int)mc)&3 || !ValidateAddress( mc ) )
    {
        debugf( "Invalid address (0x%p) passed to CheckBlock\n", mc );
        *((int*)-1)=-1;
    }

    if ( mc->magic != MAGICWORD )
    {
        debugf( "Head magic word corrupt (0x%08x) sz=%d\n", mc->magic, mc->nSize );
        *((int*)-1)=-1;
    }

    tail = *((int*)(mc->data + mc->nSize));
    if ( tail != MAGICWORD )
    {
        debugf( "Tail magic word corrupt (0x%08x) sz=%d\n", tail,
                 mc->nSize );
        *((int*)-1)=-1;
    }
}

void Anim_CheckHeap( char *file, int line)
{
    memcheck *mc = list;

    if ( file )
        debugf( "Checking heap from %s:%d\n", file, line );

    while ( mc )
    {
        CheckBlock( mc );
        if ( mc == mc->next )
        {
            debugf( "Cycle in list! (0x%08p) sz=%d\n", mc, mc->nSize );
            *((int*)-1)=-1;
        }

        mc = mc->next;
    }
}

void Anim_Free( void *pp )
{
    void *pBlock = *(void**)pp;
    if ( pBlock )
    {
        memcheck *mc = (memcheck*)( (char*)pBlock - 16 );

        CheckBlock( mc );

        if ( mc->prev )
            mc->prev->next = mc->next;
        else
            list = mc->next;

        if ( mc->next )
            mc->next->prev = mc->prev;

        debugf( "free(%d)\n", mc->nSize );

        inuse -= mc->nSize;

        free( mc );
        *(void**)pp = NULL;
    }
}

void *Anim_Reallocate( void *pBlock, int nSize )
{
    memcheck *mc;
    void *res;
    int cpy;

    if ( !pBlock && !nSize )
        return NULL;

    if ( !pBlock )
        return Anim_Allocate( nSize );

    if ( !nSize )
    {
        Anim_Free( &pBlock );
        return NULL;
    }

    mc = (memcheck*)( (char*)pBlock - 16 );

    CheckBlock( mc );

    debugf( "realloc(%d->%d)\n", mc->nSize, nSize );

    res = Anim_Allocate( nSize );
    if ( !res )
        return NULL;

    cpy = nSize > mc->nSize ? mc->nSize : nSize;
    memcpy( res, pBlock, nSize );
    Anim_Free( &pBlock );
    return res;
}

void Anim_ListHeap( void )
{
    memcheck *mc = list;

    while ( mc )
    {
        CheckBlock(mc);
        printf( "%p: size %d\n", mc, mc->nSize );
        mc = mc->next;
    }

    printf( "memory in use %d, max was %d\n", inuse, hwm );
}

#else

void *Anim_Allocate( int nSize )
{
    return malloc( nSize );
}

void Anim_Free( void *pp )
{
    void *pBlock = *(void**)pp;
    if ( pBlock )
    {
        free( pBlock );
        *(void**)pp = NULL;
    }
}

void *Anim_Reallocate( void *pBlock, int nSize )
{
    return realloc( pBlock, nSize );
}

#endif

static anim_flexallocproc *flexalloc = NULL;
static anim_flexfreeproc *flexfree = NULL;
#if 0
static anim_flexreallocproc *flexrealloc = NULL;

void Anim_RegisterFlexAllocator( anim_flexallocproc fa,
                                 anim_flexreallocproc fr,
                                 anim_flexfreeproc ff )
{
    flexalloc = fa;
    flexrealloc = fr;
    flexfree = ff;
}
#endif

void Anim_FlexAllocate( void *pp, int nSize )
{
    if ( flexalloc )
        (*flexalloc)(pp,nSize);
    else
    {
        *(void**)pp = Anim_Allocate( nSize );
    }
}

#if 0
BOOL Anim_FlexReallocate( void *pp, int nNewSize )
{
    if ( flexrealloc )
        return (*flexrealloc)(pp,nNewSize);
    else
    {
        void *p = *(void**)pp;
        void *p2 = Anim_Reallocate( p, nNewSize );

        if ( p2 )
            *(void**)pp = p2;
        return ( p2 != NULL );
    }
}
#endif

void Anim_FlexFree( void *pp )
{
    if ( flexfree )
        (*flexfree)(pp);
    else
    {
        Anim_Free(pp);
    }
}

/* eof */
