//
// Created by Paul on 2/20/2018.
//

#ifndef READLOGICALVOLUME_H
#define READLOGICALVOLUME_H

typedef unsigned char byte;     // old habits die hard...
typedef char tStringZ;          // zero-terminated string/char pointer


typedef struct
{
    byte * start;
    size_t length;
    byte   hash[64];
} tMemoryBuffer;

typedef struct
{
    byte * ptr;
    size_t length;
} tBlock;

typedef struct
{
    tBlock  block;
    byte  * start;
    byte  * end;
} tTextBlock;


#endif //READLOGICALVOLUME_H
