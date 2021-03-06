/*
    Created by Paul on 3/2/2018.
*/

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <libgen.h>

#include "readaccess.h"
#include "debug.h"

/* used for pointer validation testing */
/* set using alloca() on entry to debugInit() */
void       * gStackTop;
const char * gExecName;

void debugInit( int UNUSED(argc), char *argv[] )
{
    gStackTop = alloca( 0 );
    gExecName = basename( argv[0] );
}

#define kRowLength  32
/**
 *
 * @param displayOffset
 * @param ptr
 * @param length
 */
void dumpMemory(unsigned long displayOffset, void * ptr, size_t length)
{
    if (isValidPtr(ptr))
    {
        for (size_t i = 0; i < length; i += kRowLength)
        {
            size_t rowLength = (kRowLength < length - i) ? kRowLength : length - i;

            DebugOut("%08lx: ", displayOffset + i );

            for (size_t j = 0; j < rowLength; ++j)
            {
                DebugOut("%02x ", ((unsigned char *)ptr)[i + j]);
            }

            DebugOut("   |");
            for (size_t j = 0; j < rowLength; ++j)
            {
                char c = ((unsigned char *)ptr)[i + j];
                DebugOut("%c", (isgraph(c) || c == ' ') ? c : '.');
            }

            DebugOut("|\n");
        }
    }
}

/**
 *
 * @param drive
 * @param offset
 * @param length
 */
void dumpDisk( tDrive * drive, off64_t offset, size_t length )
{
    char * block = calloc( length, 1 );
    if ( isHeapPtr( block ) )
    {
        ssize_t rdLen = readDrive( drive, offset, block, length );
        if ( rdLen == (ssize_t) length )
        {
            dumpMemory( offset, block, length );
        }
        free( block );
    }
}

