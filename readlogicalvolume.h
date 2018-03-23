//
// Created by Paul on 2/20/2018.
//

#ifndef READLOGICALVOLUME_H
#define READLOGICALVOLUME_H

typedef unsigned char byte;     // old habits die hard...
typedef char tStringZ;          // zero-terminated string/char pointer

typedef struct
{
    byte * ptr;
    size_t length;
} tMemoryBlock;

typedef struct
{
    tMemoryBlock  block;
    byte  * start;
    byte  * end;
} tTextBlock;


#endif //READLOGICALVOLUME_H
