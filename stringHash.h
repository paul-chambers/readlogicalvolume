//
// Created by Paul on 2/26/2018.
//

#ifndef READLOGICALVOLUME_STRINGHASH_H
#define READLOGICALVOLUME_STRINGHASH_H

#include "readlogicalvolume.h"
typedef unsigned long tHash;

int checkCRC32( uint32_t crc, byte * ptr, size_t length );
void crc32( const void * data, size_t n_bytes, uint32_t * crc );
tHash hashString( const tStringZ * ptr );
tHash hashBytes( const char * ptr, size_t len );

#endif //READLOGICALVOLUME_STRINGHASH_H
