//
// Created by Paul Chambers on 2/20/2018.
//

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
//#include <sys/file.h>
#include <string.h>
//#include <ctype.h>
#include <inttypes.h>
#include <endian.h>
#include <errno.h>

#include "readlogicalvolume.h"
#include "readaccess.h"
#include "debug.h"
#include "stringHash.h"
#include "parseMetadata.h"
#include "gpt.h"
#include "lvm.h"




/************************************/
/**
 * @param ptr    starting address of value to be converted
 * @param count  number of bytes to be converted
 * @return the value, assuming the value is big-endian in memory
 */
uint64_t getBE( byte * ptr, int count )
{
    uint64_t  result = 0;
    for ( int i      = count; i > 0; --i )
    {
        result <<= 8;
        result |= *ptr++;
    }
    return (result);
}

/**
 *
 * @param ptr    starting address of value to be converted
 * @param count  number of bytes to be converted
 * @return the value, assuming the value is little-endian in memory
 */
uint64_t getLE( byte * ptr, int count )
{
    uint64_t result = 0;
    ptr += count;
    for ( int i = count; i > 0; --i )
    {
        result <<= 8;
        result |= *(--ptr);
    }
    return (result);
}

uint64_t get64LE( byte * ptr )
{
    return getLE( ptr, 8 );
}

uint32_t get32LE( byte * ptr )
{
    return (uint32_t) getLE( ptr, 4 );
}

uint16_t get16LE( byte * ptr )
{
    return (uint16_t) getLE( ptr, 2 );
}

/**
 *
 * @param data
 * @return
 */
int SixteenBytesAreZero( byte * data )
{
    byte * p = data;
    unsigned int result = 0;

    for ( int i = 16; i > 0; --i )
    {
        result |= *p++;
    }

    return (result == 0);
}

int UUIDisLVM( byte * uuid )
{
    const byte lvmUUID[] = { 0x79, 0xD3, 0xD6, 0xE6, 0x07, 0xF5, 0xC2, 0x44,
                             0xA2, 0x3C, 0x23, 0x8F, 0x2A, 0x3D, 0xF9, 0x28 };

    /* for (int i = 16; i > 0; --i)
        DebugOut( "0x%02X,", *uuid++); */

    return (memcmp( uuid, lvmUUID, sizeof( lvmUUID ) ) == 0);
}

/* debug only */

/**
 * for debugging purposes only.
 * @param entry
 */

void dumpGPTEntry( tGPTEntry * entry )
{
#ifdef optDebugOutput
    char name[37];
    char * p;
    char uuid[40];
    char * q;
    /* type[16];       0 (0x00)  16 bytes  Partition type GUID */
    /* thank Microsoft for the 'mixed-endian' representation */
    snprintf( uuid, sizeof( uuid ),
              "%08x-%04x-%04x-%04lx-%012lx",
              get32LE( &entry->type[ 0 ] ),
              get16LE( &entry->type[ 4 ] ),
              get16LE( &entry->type[ 6 ] ),
              getBE( &entry->type[ 8 ], 2 ),
              getBE( &entry->type[ 10 ], 6 ) );
    LogInfo( "      type %s", uuid );

    /* unique[16];    16 (0x10)  16 bytes  Unique partition GUID */
    /* thank Microsoft for the 'mixed-endian' representation */
    snprintf( uuid, sizeof( uuid ),
              "%08x-%04x-%04x-%04lx-%012lx",
              get32LE( &entry->unique[ 0 ] ),
              get16LE( &entry->unique[ 4 ] ),
              get16LE( &entry->unique[ 6 ] ),
              getBE( &entry->unique[ 8 ], 2 ),
              getBE( &entry->unique[ 10 ], 6 ) );
    LogInfo( "      UUID %s", uuid );

    /* firstLBA[8];   32 (0x20)   8 bytes  First LBA (little endian) */
    LogInfo( "  firstLBA %ld (%#lx)", get64LE( entry->firstLBA ), get64LE( entry->firstLBA ) );
    /* lastLBA[8];    40 (0x28)   8 bytes  Last LBA (inclusive, usually odd) */
    LogInfo( "    lasLBA %ld (%#lx)", get64LE( entry->lastLBA ),  get64LE( entry->lastLBA ) );
    /* attributes[8]; 48 (0x30)   8 bytes  Attribute flags (e.g. bit 60 denotes read-only) */
    LogInfo( "attributes %lx", get64LE( entry->attributes ) );
    /* name[72];      56 (0x38)  72 bytes  Partition name (36 UTF-16LE code units) */
    /* hack to convert UTF-16LE to plain ascii */
    p = name;
    q = entry->name;
    for ( int i = 36; i > 0; --i )
    {
        *p = *q++;
        if ( *p == '\0' )
        {
            break;
        }
        if ( *p < 32 || (*p & 0x80) || *q != 0 )
        {
            *p = '.';
        }
        ++p;
        ++q;
    }
    *p = '\0';
    LogInfo( "      name %s", name );
#endif
}

tTextBlock * readMetadata( tDrive * drive, tDiskBlock * metadataList )
{
    tTextBlock * result = NULL;
    ssize_t      rdLen;
    tDiskBlock   metadata;

    size_t mdHeaderLength = sizeof(tLVMMetadataHeader) + 32 * sizeof(tLVMRawLocation);
    tLVMMetadataHeader * mdHeader = calloc( mdHeaderLength + sizeof(tLVMRawLocation), 1 );

    rdLen = readDrive( drive, metadataList->offset, mdHeader, mdHeaderLength );

    if ( rdLen != (ssize_t) mdHeaderLength )
    {
        LogError( "unable to read metadata header (%d: %s)", errno, strerror( errno ) );
    }
    else
    {
        if ( memcmp( mdHeader->signature, " LVM2 x[5A%r0N*>", 16 ) == 0
            && get32LE( mdHeader->version ) == 1 )
        {
            metadata.offset = get64LE( mdHeader->offset );
            metadata.length = get64LE( mdHeader->size );

            LogInfo( "metadata signature matched" );
            LogInfo( "  metadata offset %8lx", metadata.offset );
            LogInfo( "    metadata size %8lx", metadata.length );

            tLVMRawLocation * rawLoc = mdHeader->list;
            while ( !SixteenBytesAreZero( (byte *) rawLoc ) )
            {
                off64_t offset = get64LE( rawLoc->offset );
                size_t  length = get64LE( rawLoc->size );

                LogInfo( "  offset %8lx", offset );
                LogInfo( "    size %8lx", length );
                LogInfo( "   crc32 %08x", get32LE( rawLoc->crc32 ) );
                LogInfo( "   flags %08x", get32LE( rawLoc->flags ) );

                if ( (get32LE( rawLoc->flags ) & (MDA_IGNORED | MDA_INCONSISTENT | MDA_FAILED)) == 0 )
                {
                    LogInfo( "found active metadata" );

                    result = calloc( sizeof( tTextBlock ), 1 );

                    if ( isValidPtr( result ) )
                    {
                        result->block.length = length;
                        result->block.ptr    = calloc( length, sizeof( byte ) );
                        if ( isValidPtr( result->block.ptr ) )
                        {
                            rdLen = readDrive( drive, metadata.offset + offset, result->block.ptr, length );
                            if ( rdLen != (ssize_t)length )
                            {
                                LogError( "unable to read metadata text" );
                            }
                            else
                            {
                                DebugOut( "\n_______________________________\n\n" );
                                fwrite( result->block.ptr, result->block.length, 1, stderr );
                                DebugOut( "\n_______________________________\n\n" );
                            }
                        }
                    }
                }
                ++rawLoc;
            }
        }
    }
    if ( result == NULL )
    {
        LogError( "unable to find an active metadata area" );
    }

    return result;
}


/**
   Dig out the list that points to the metadata area.
   the metadata area is a large circular buffer (1MB, typically)
   only a small section is current at any point in time. So we
   only load the header, and walk the 'rawlocation' list looking
   for the active section, and only read that into memory
   @param partition structure
   @return data area pointing to the active metadata area
*/

tDiskBlock * readPhysicalVolumeLabel( tDrive * drive )
{
    /* the PV label is in one of the first four sectors of the partition, usually the second one */
    size_t pvLabelRdLen = 4 * drive->sectorSize;

    tLVMPVLabel * label = calloc( sizeof( byte ), pvLabelRdLen );

    if ( isValidPtr( label ) )
    {
        ssize_t rdlen = readDrive( drive, 0, label, pvLabelRdLen );
        if ( rdlen != (ssize_t) pvLabelRdLen )
        {
            LogError( "unable to read pvLabel area (%d: %s)", errno, strerror( errno ) );
        }
        else
        {
            for ( int i = 0; i < 4; ++i )
            {
                if ( (memcmp( label->signature, "LABELONE", 8 ) == 0)
                    && (memcmp( label->typeID,  "LVM2 001", 8 ) == 0)
                    && checkCRC32( get32LE( label->crc32 ), (byte *) label + 20, drive->sectorSize - 20 ) )
                {
#ifdef optDebugOutput
                    const char * ordinal[] = {"first", "second", "third", "fourth"};
                    LogInfo( "found pvLabel in the %s sector", ordinal[ i ] );
#endif
                    tLVMPVHeader * pvHeader = (tLVMPVHeader *) ((byte *) label + get32LE( label->offset ));
                    LogInfo( "PV UUID is %32s", pvHeader->uuid );
                    size_t pvSize = get64LE( pvHeader->size );
                    LogInfo( "PV size is %ld", pvSize );

                    tLVMDataArea * dataArea = pvHeader->list;
                    /* skip over the data list. we want the metadata list that follows it. */
                    while ( !SixteenBytesAreZero( (byte *) dataArea ) )
                    {
                        ++dataArea;
                    }
                    /* skip over the data list terminator. metadata list follows immediately after */
                    ++dataArea;

                    int count = 0;
                    tLVMDataArea * mdaList = dataArea;
                    while ( !SixteenBytesAreZero( (byte *) dataArea ) )
                    {
                        ++count;
                        ++dataArea;
                    }

                    /* Now we know how large a list to create, add one entry for a trailing null */
                    tDiskBlock * blockList = calloc( sizeof( tDiskBlock ), count + 1 );

                    tDiskBlock * list = blockList;
                    dataArea = mdaList;
                    while ( !SixteenBytesAreZero( (byte *) dataArea ) )
                    {
                        list->offset = get64LE( dataArea->offset );
                        list->length = get64LE( dataArea->size );
                        LogInfo( "    data area: offset %lx, %ld bytes", list->offset, list->length );
                        ++list;
                        ++dataArea;
                    }

                    return blockList;
                }
                label = (tLVMPVLabel *) ((byte *) label + drive->sectorSize);
            }
            LogError("no Physical Volume Label was found");
        }
    }
    return NULL;
}


/**
 * start off by walking the GPT, looking for partitions marked as LVM
 * If found, pass them to readPhysicalVolumeLabel()
 * @todo check the GPT CRC32 values, and if they're bad, try the backup copy
 * @param  drive
 * @return the first LVM partition in the drive's GPT
 */
tDrive * readGPT( tDrive * drive )
{
    if ( isValidPtr( drive ) )
    {
        tGPTHeader * gptHeader = malloc( sizeof( tGPTHeader ) );
        if ( isHeapPtr( gptHeader ) )
        {
            ssize_t rdLen = readDrive( drive, drive->sectorSize, gptHeader, sizeof( tGPTHeader ) );
            if ( rdLen != sizeof( tGPTHeader ) )
            {
                LogError( "Unable to read GPT header (%d: %s)", errno, strerror( errno ) );
            }
            else
            {
                uint32_t crc32 = get32LE( gptHeader->crc32 );
                memset( gptHeader->crc32, 0, sizeof( gptHeader->crc32 ) );
                if ( !(memcmp( gptHeader->signature, "EFI PART", 8 ) == 0
                    && get32LE( gptHeader->revision ) == 0x00010000
                    && checkCRC32( crc32, (byte *) gptHeader, drive->sectorSize )) )
                {
                    LogInfo( "signature incorrect" );
                }
                else
                {
                    LogInfo( "signature & revision are correct" );
                    LogInfo( " Partition first LBA = %ld", get64LE( gptHeader->partitionTable.firstLBA ) );
                    LogInfo( "     Partition Count = %d",  get32LE( gptHeader->partitionTable.count ) );
                    LogInfo( "Partition Entry Size = %d",  get32LE( gptHeader->partitionTable.size ) );

                    size_t tableLength = get32LE( gptHeader->partitionTable.count )
                                       * get32LE( gptHeader->partitionTable.size );
                    tGPTEntry * gptTable = calloc( tableLength, 1 );

                    if ( isHeapPtr( gptTable ) )
                    {
                        off64_t gptTableOffset = get64LE( gptHeader->partitionTable.firstLBA ) * drive->sectorSize;
                        setPartition( drive, 0, gptTableOffset + tableLength );
                        rdLen = readDrive( drive, gptTableOffset, gptTable, tableLength );
                        if ( rdLen != (ssize_t) tableLength )
                        {
                            LogError( "Unable to read partition table (%d: %s)", errno, strerror( errno ) );
                        }
                        else
                        {
                            LogInfo( "read of partition table successful" );
                            tGPTEntry * entry = gptTable;
                            size_t  entrySize = get32LE( gptHeader->partitionTable.size );
                            for ( int count   = get32LE( gptHeader->partitionTable.count ); count > 0; --count )
                            {
                                if ( SixteenBytesAreZero( entry->type ) )
                                {
                                    break;
                                }
                                if ( UUIDisLVM( entry->type ) )
                                {
                                    LogInfo( "found LVM PV partition" );

                                    setPartition( drive,
                                                  get64LE( entry->firstLBA ) * drive->sectorSize,
                                                  (get64LE( entry->lastLBA ) - get64LE( entry->firstLBA ))
                                                      * drive->sectorSize );

                                    dumpGPTEntry( entry );
                                }
                                entry = (tGPTEntry *) ((byte *) entry + entrySize);
                            }
                        }
                    }
                }
            }
        }
    }
    return drive;
}


/*
 * below is a skeleton to use for testing. Wouldn't use this in a bootloader
 */

/**
 *
 * @param output
 */
void usage( FILE * output )
{
    fprintf( output, "### error: %s <drive path> <logical volume label>\n", gExecName );
}

void writeMemoryBuffer( tMemoryBlock * buffer, const char * lvName )
{
    char filename[256];
    snprintf( filename, sizeof( filename ), "%s.bin", lvName );
    int fd = creat( filename, S_IRUSR | S_IRGRP );
    if ( fd == -1 )
    {
        LogError( "unable to open file \"%s\" (%d: %s)", filename, errno, strerror( errno ) );
    }
    else
    {
        LogInfo( " writing memory block @ %p to \'%s\'", buffer, filename );
        write( fd, buffer->ptr, buffer->length );
        close( fd );
    }
}

/**
 *
 * @param argc
 * @param argv
 * @return
 */
int main( int argc, char * argv[] )
{
    debugInit( argc, argv );

    if ( argc < 3 )
    {
        usage( stderr );
        exit( -1 );
    }

    tDrive * drive = openDrive( argv[1] );
    if ( isValidPtr( drive ) )
    {
        tDrive * result = readGPT( drive );
        if (isValidPtr(result))
        {
            tDiskBlock * metadataArea = readPhysicalVolumeLabel( drive );
            if ( isValidPtr(metadataArea) )
            {
                tTextBlock * metadata = readMetadata( drive, metadataArea );
                if ( isValidPtr(metadata) )
                {
                    tNode * metadataTree = parseMetadata( metadata );
                    if ( isValidPtr(metadataTree) )
                    {
                        tMemoryBlock * buffer = readLogicalVolume( drive, argv[2], metadataTree );
                        if ( isValidPtr( buffer ) )
                        {
                            LogInfo( "memory block @ %p", (void *) buffer );
                            LogInfo( "     pointer = %p", (void *) buffer->ptr );
                            LogInfo( "      length = %ld (0x%lx)", buffer->length, buffer->length );

                            writeMemoryBuffer( buffer, argv[2] );
                        }
                    }
                }
            }
        }
    }

    closeDrive( drive );

    exit( 0 );
}
