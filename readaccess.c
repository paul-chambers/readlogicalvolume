//
// Created by Paul on 2/23/2018.
//

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/file.h>
#include <string.h>
#include <endian.h>
#include <sys/stat.h>
#include <errno.h>

#include "debug.h"

/*********************************************************************************************
  drive access routines

  In a bootloader, these three routines are likely to access eMMC or use a FTL directly.
  For testing under Linux, we use standard i/o.

*****/

tDrive * openDrive( const char *drivePath )
{
    tDrive * drive = calloc( sizeof(tDrive), 1 );
    if (drive != NULL)
    {
        drive->sectorSize = 512;

        drive->id = open(drivePath, O_RDONLY);
        if (drive->id < 0)
        {
            LogError( "unable to open \'%s\' (%d: %s)",
                drivePath, errno, strerror(errno) );
            free(drive);
            drive = NULL;
        }
        else
        {
            struct stat st;
            if ( fstat( drive->id, &st) < 0 )
            {
                LogError( "unable to get the size of the drive (%d: %s)", errno, strerror(errno) );
            }
            else
            {
                drive->partition.start  = 0;
                drive->partition.length = 4096;
                LogInfo( "drive size %ld (%.2f MB)",
                     drive->partition.length,
                     drive->partition.length / 1048576.0 );

                    drive->path = strdup(drivePath);
                if (drive->path == NULL)
                {
                    LogError( "### unable to store the path \'%s\' (%d: %s)\n",
                        drivePath, errno, strerror(errno) );
                    free(drive);
                    drive = NULL;
                }
            }
        }
    }
    return (drive);
}

void setPartition( tDrive * drive, off64_t offset, size_t length )
{
    drive->partition.start  = offset;
    drive->partition.length = length;
    LogInfo( "partition start %ld (%#lx), size %ld (%.2f MB)",
         drive->partition.start,  drive->partition.start,
         drive->partition.length, drive->partition.length / 1048576.0 );
}

ssize_t readDrive( tDrive * drive, off64_t offset, void * dest, size_t length )
{
    ssize_t result = 0;

    LogInfo( "readDrive( offset %#lx, %ld (%#lx) bytes)", offset, length, length );
    if ( drive != NULL )
    {
        if ( (offset + length) > drive->partition.length )
        {
            LogError( "read requested past the end of partition (%ld + %ld > %ld)",
                 offset, length, drive->partition.start + drive->partition.length );
        }
        else
        {
            if ( lseek64( drive->id, drive->partition.start + offset, SEEK_SET ) < 0 )
            {
                LogError( "seek to offset %lu failed (%d: %s)",
                     drive->partition.start + offset, errno, strerror( errno ) );
            }
            else
            {
                result = read( drive->id, dest, length );
                if ( result < 0 )
                {
                    LogError( "read @ offset %lu for %lu bytes failed (%d: %s)\n",
                         offset, length, errno, strerror( errno ) );
                }
            }
        }
    }
    return (result);
}

void closeDrive( tDrive * drive )
{
    if (drive != NULL)
    {
        if (drive->id >= 0)
        {
            close(drive->id);
            drive->id = -1;
        }
    }
}

/*******************************************************************************************\
|                               end of drive access routines                                |
\*******************************************************************************************/
