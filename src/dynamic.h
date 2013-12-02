/* Extras:Dynamic.h */

/* C interface to Dynamic Area routines in Risc OS 3.5 and later
 * (K) All Rites Reversed - Copy What You Like
 *
 * Authors:
 *      Peter Hartley       <pdh@chaos.org.uk>
 */


#ifndef ExtrasLib_Dynamic_h
#define ExtrasLib_Dynamic_h

typedef struct DynamicArea *DynamicArea;    /* abstract type */

#define dynamicarea_NONE    ((DynamicArea)NULL)

/*---------------------------------------------------------------------------*
 * Create a new dynamic area                                                 *
 * Returns area number in *pAreaNumber, and a pointer to the start of the    *
 * new area.                                                                 *
 * Safe to call on earlier OS versions, but will return an error.            *
 *---------------------------------------------------------------------------*/
os_error *DynamicArea_Alloc( int nSize, const char *pAreaName,
                             DynamicArea *pArea, void **ppStart );

/*---------------------------------------------------------------------------*
 * Change the size of a dynamic area                                         *
 * On entry *pSize contains the amout to alter the size by; on exit it is    *
 * set to the actual number of bytes moved.                                  *
 *---------------------------------------------------------------------------*/
os_error *DynamicArea_Realloc( DynamicArea area, int *pSize );

/*---------------------------------------------------------------------------*
 * Remove a dynamic area completely                                          *
 * You probably want to call this from your atexit handler, as dynamic areas *
 * are NOT freed automatically when tasks quit. Also writes *pArea to NULL.  *
 *---------------------------------------------------------------------------*/
os_error *DynamicArea_Free( DynamicArea *pArea );

/*---------------------------------------------------------------------------*
 * Return system memory page size                                            *
 * Not really a dynamic area routine but in here for convenience.            *
 * Calls OS_ReadMemMapInfo.                                                  *
 *---------------------------------------------------------------------------*/
int MemoryPageSize( void );

#endif
