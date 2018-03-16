//
// Created by Paul on 2/23/2018.
//

#ifndef READLOGICALVOLUME_READACCESS_H
#define READLOGICALVOLUME_READACCESS_H

typedef off64_t     tExtent;

typedef struct tPartition {
    off64_t     start;
    size_t      length;
} tPartition;

typedef struct {
    int          id;
    const char * path;
    size_t       sectorSize;
    tPartition   partition;
} tDrive;


typedef struct tDiskBlock {
    off64_t     offset;
    size_t      length;
} tDiskBlock;

tDrive *  openDrive( const char *drivePath );
void   setPartition( tDrive * drive, off64_t offset, size_t length );
ssize_t   readDrive( tDrive * drive, off64_t offset, void * dest, size_t length );
void     closeDrive( tDrive * drive );

#endif //READLOGICALVOLUME_READACCESS_H
