//
// Created by Paul on 2/26/2018.
//
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "readlogicalvolume.h"
#include "debug.h"
#include "stringHash.h"

tHash hashString( const tStringZ * ptr )
{
    tHash hash = 0; /* seed with largish prime */
    if ( isValidPtr( ptr ) )
    {
        hash = 199999; /* seed with largish prime */
        while ( *ptr != '\0' )
        {
            /* The 'djb2' string hash function */
            hash = (hash << 5) + hash + *ptr++;
        }
    }
    return hash;
}

tHash hashBytes( const char * ptr, size_t len )
{
    tHash hash = 0; /* seed with largish prime */
    if ( isValidPtr( ptr ) )
    {
        hash = 199999; /* seed with largish prime */
        while ( len > 0 )
        {
            /* The 'djb2' string hash function */
            hash = (hash << 5) + hash + *ptr++;
            --len;
        }
    }
    return hash;
}

#if 0
/* only used to generate header file defining the hash values for known strings */
const char *knownStrings[] = {
    "root_node",
    "id",
    "seqno",
    "format",
    "status",
    "RESIZEABLE",
    "READ",
    "WRITE",
    "flags",
    "extent_size",
    "max_lv",
    "max_pv",
    "metadata_copies",
    "physical_volumes",
    "id",
    "device",
    "status",
    "ALLOCATABLE",
    "flags",
    "dev_size",
    "pe_start",
    "pe_count",
    "contents",
    "version",
    "description",
    "creation_host",
    "creation_time",
    NULL
};

void generateHashHeader(FILE *output)
{
    int i = 0;
    const char **string;

    string = &knownStrings[0];
    while (string != NULL)
    {
        fprintf( output, "#define kHash_%-20s %#x", *string, hashString( *string ) );
        ++string;
    }
}
#endif