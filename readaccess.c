//
// Created by Paul on 2/23/2018.
//

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <inttypes.h>
#include <endian.h>

#include "readlogicalvolume.h"
#include "readaccess.h"

/*********************************************************************************************
  drive access routines

  In a bootloader, these three routines are likely to access eMMC or use a FTL directly.
  For testing under Linux, we use standard i/o.

*****/

tDrive * openDrive( const char *drivePath )
{
    tDrive * drive = malloc(sizeof(tDrive));
    if (drive != NULL)
    {
        drive->sectorSize = 512;

        drive->id = open(drivePath, O_RDONLY);
        if (drive->id < 0)
        {
            Log(LOG_ERR, "unable to open \'%s\' (%d: %s)",
                drivePath, errno, strerror(errno) );
            free(drive);
            drive = NULL;
        }
        else
        {
            drive->path = strdup(drivePath);
            if (drive->path == NULL)
            {
                Log(LOG_ERR, "### unable to store the path \'%s\' (%d: %s)\n",
                    drivePath, errno, strerror(errno) );
                free(drive);
                drive = NULL;
            }
        }
    }
    return (drive);
}

ssize_t readDrive( tDrive * drive, off64_t offset, void * dest, size_t length )
{
    ssize_t result = 0;

    Log(LOG_DEBUG,"ofst:%8llx len:%8llx", offset, length);
    if (drive != NULL)
    {
        if (lseek64(drive->id, offset, SEEK_SET) < 0)
        {
            Log(LOG_ERR, "seek to offset %ll failed (%d: %s)", offset, errno, strerror(errno));
        }
        else {
            result = read(drive->id, dest, length);
            if (result < 0) {
                Log(LOG_ERR, "read @ offset %ll for %ll bytes failed (%d: %s)\n",
                    offset, length, errno, strerror(errno));
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

/****************************** end of drive access routines ********************************
*********************************************************************************************/
