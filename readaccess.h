//
// Created by Paul on 2/23/2018.
//

#ifndef READLOGICALVOLUME_READACCESS_H
#define READLOGICALVOLUME_READACCESS_H

typedef off64_t     tLogicalBlockAddress;

typedef struct {
    int          id;
    const char * path;
    size_t       sectorSize;
} tDrive;

tDrive * openDrive( const char *drivePath );
ssize_t  readDrive( tDrive * drive, off64_t offset, void * dest, size_t length );
void     closeDrive( tDrive * drive );

#endif //READLOGICALVOLUME_READACCESS_H
