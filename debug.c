/*
    Created by Paul on 3/2/2018.
*/

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "readaccess.h"

/* used for pointer validation testing */
/* set using alloca() on entry to main() */
void * gStackTop;

#define kRowLength  32
/**
 *
 * @param ptr
 * @param length
 */
void dumpMemory(char * ptr, size_t length)
{
    if (isValidPtr(ptr))
    {
        for (size_t i = 0; i < length; i += kRowLength)
        {
            size_t rowLength = (kRowLength < length - i) ? kRowLength : length - i;

            DebugOut("%p: ", &ptr[i]);

            for (size_t j = 0; j < kRowLength; ++j)
            {
                DebugOut("%02x ", ptr[i + j]);
            }

            DebugOut("   |");
            for (size_t j = 0; j < rowLength; ++j)
            {
                char c = ptr[i + j];
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
void dumpDisk(tDrive * drive, off64_t offset, size_t length)
{
    char * ptr;
    char * block = calloc(length, 1);
    if ( isHeapPtr(block) )
    {
        ssize_t rdLen = readDrive(drive, offset, block, length);
        if (rdLen == (ssize_t)length)
        {
            for (size_t i = 0; i < length; i += kRowLength )
            {
                size_t rowLength = (kRowLength < length - i) ? kRowLength : length - i;

                DebugOut("%08lx: ", offset + i);

                ptr = &block[i];
                for (int j = 0; j < kRowLength; ++j)
                {
                    DebugOut( "%02x ", *ptr++);
                }

                ptr = &block[i];
                DebugOut("   |");
                for (size_t j = 0; j < rowLength; ++j)
                {
                    DebugOut( "%c", (isgraph(*ptr) || *ptr == ' ') ? *ptr : '.');
                    ++ptr;
                }

                DebugOut("|\n");
            }
        }
        free(block);
    }
}

