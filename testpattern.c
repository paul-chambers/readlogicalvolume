//
// Created by Paul on 3/9/2018.
//

#include <stdlib.h>
#include <stdio.h>

int main( int argc, char *argv[] )
{
    unsigned long sectorCount;
    unsigned long index;

    sectorCount = 1;
    if (argc > 1)
    {
        sectorCount = atoi( argv[ 1 ] ) * 1024 * (1024/512);
    }
    index = 0;

    while ( index < sectorCount )
    {
        for ( int i = 32; i > 0; --i )
        {
            /*                1234567890123456  */
            fprintf( stdout, "%15lu\n", index  );
        }
        ++index;
    }
}
