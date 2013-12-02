/* MemFlex.h */

/* Like RiscOSLib:flex.h, but can use dynamic areas
 * (K) All Rites Reversed - Copy What You Like
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 */


#ifndef ExtrasLib_MemFlex_h
#define ExtrasLib_MemFlex_h

/*---------------------------------------------------------------------------*
 * A flex anchor.                                                            *
 * This is a pointer to your pointer to your data, so it should really be    *
 * a void**, but because of C's casting behaviour, this definition makes it  *
 * easier to do things like MemFlex_Alloc( &pSpriteArea, size )              *
 *---------------------------------------------------------------------------*/
typedef void *flex_ptr;

/*---------------------------------------------------------------------------*
 * Initialise the MemFlex system                                             *
 * Must be called before any MemFlex *or MemHeap* routine                    *
 * Pass NULL as the area name only if you don't want a dynamic area even on  *
 * a Risc PC -- MemFlex falls back automatically to using application space  *
 * on older machines.                                                        *
 *---------------------------------------------------------------------------*/
os_error *MemFlex_Initialise( const char *pDynamicAreaName );


/*---------------------------------------------------------------------------*
 * Allocate, or re-allocate, a block                                         *
 * If allocating for the first time, *anchor must be NULL. anchor, on the    *
 * other hand, must never be NULL.                                           *
 *---------------------------------------------------------------------------*/
BOOL MemFlex_Alloc( flex_ptr anchor, int size );


/*---------------------------------------------------------------------------*
 * Free a block                                                              *
 * Also writes *anchor to NULL                                               *
 *---------------------------------------------------------------------------*/
void MemFlex_Free( flex_ptr anchor );

#endif
