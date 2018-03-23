//
// Created by Paul Chambers on 2/20/2018.
//

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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

#ifdef optDebugOutput
void dumpGPTEntry( tGPTEntry * entry )
{
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
    Log( LOG_INFO, "      type %s", uuid );

    /* unique[16];    16 (0x10)  16 bytes  Unique partition GUID */
    /* thank Microsoft for the 'mixed-endian' representation */
    snprintf( uuid, sizeof( uuid ),
              "%08x-%04x-%04x-%04lx-%012lx",
              get32LE( &entry->unique[ 0 ] ),
              get16LE( &entry->unique[ 4 ] ),
              get16LE( &entry->unique[ 6 ] ),
              getBE( &entry->unique[ 8 ], 2 ),
              getBE( &entry->unique[ 10 ], 6 ) );
    Log( LOG_INFO, "      UUID %s", uuid );

    /* firstLBA[8];   32 (0x20)   8 bytes  First LBA (little endian) */
    Log( LOG_INFO, "  firstLBA %ld (%#lx)", get64LE( entry->firstLBA ), get64LE( entry->firstLBA ) );
    /* lastLBA[8];    40 (0x28)   8 bytes  Last LBA (inclusive, usually odd) */
    Log( LOG_INFO, "    lasLBA %ld (%#lx)", get64LE( entry->lastLBA ),  get64LE( entry->lastLBA ) );
    /* attributes[8]; 48 (0x30)   8 bytes  Attribute flags (e.g. bit 60 denotes read-only) */
    Log( LOG_INFO, "attributes %lx", get64LE( entry->attributes ) );
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
    Log( LOG_INFO, "      name %s", name );
}
#else
#define dumpGPTEntry( arg )
#endif

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
        Log( LOG_ERR, "unable to read metadata header (%d: %s)", errno, strerror( errno ) );
    }
    else
    {
        if ( memcmp( mdHeader->signature, " LVM2 x[5A%r0N*>", 16 ) == 0
            && get32LE( mdHeader->version ) == 1 )
        {
            metadata.offset = get64LE( mdHeader->offset );
            metadata.length = get64LE( mdHeader->size );

            Log( LOG_INFO, "metadata signature matched" );
            Log( LOG_INFO, "  metadata offset %8lx", metadata.offset );
            Log( LOG_INFO, "    metadata size %8lx", metadata.length );

            tLVMRawLocation * rawLoc = mdHeader->list;
            while ( !SixteenBytesAreZero( (byte *) rawLoc ) )
            {
                off64_t offset = get64LE( rawLoc->offset );
                size_t  length = get64LE( rawLoc->size );

                Log( LOG_INFO, "  offset %8lx", offset );
                Log( LOG_INFO, "    size %8lx", length );
                Log( LOG_INFO, "   crc32 %08x", get32LE( rawLoc->crc32 ) );
                Log( LOG_INFO, "   flags %08x", get32LE( rawLoc->flags ) );

                if ( (get32LE( rawLoc->flags ) & (MDA_IGNORED | MDA_INCONSISTENT | MDA_FAILED)) == 0 )
                {
                    Log( LOG_INFO, "found active metadata" );

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
                                Log(LOG_ERR, "unable to read metadata text" );
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
#if 0
    tDiskBlock * block = metadataList;
    result->block.length = 0;
    while (block->length != 0)
    {
        result->block.length += block->length;
    }
    result->block.ptr = malloc( result->block.length );

    block = metadataList;
    while (block->length != 0)
    {
        /* grab the active metadata */
        ssize_t rdLen = readDrive( drive,
                                   metadataBase + block->offset,
                                   result->block.ptr + block->offset,
                                   block->length );
        if ( rdLen != (ssize_t) result->block.length )
        {
            Log( LOG_ERR, "unable to read metadata area (%d: %s)", errno, strerror( errno ) );
        }
        else
        {
#ifdef optDebugOutput
            DebugOut( "\n_______________________________\n\n" );
            fwrite( result->block.ptr, result->block.length, 1, stderr );
            DebugOut( "\n_______________________________\n\n" );
#endif
        }

    }
#endif
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
            Log( LOG_ERR, "unable to read pvLabel area (%d: %s)", errno, strerror( errno ) );
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
                    Log( LOG_INFO, "found pvLabel in the %s sector", ordinal[ i ] );
#endif
                    tLVMPVHeader * pvHeader = (tLVMPVHeader *) ((byte *) label + get32LE( label->offset ));
                    Log( LOG_INFO, "PV UUID is %32s", pvHeader->uuid );
                    size_t pvSize = get64LE( pvHeader->size );
                    Log( LOG_INFO, "PV size is %ld", pvSize );

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
                        Log( LOG_INFO, "    data area: offset %lx, %ld bytes", list->offset, list->length );
                        ++list;
                        ++dataArea;
                    }

                    return blockList;
                }
                label = (tLVMPVLabel *) ((byte *) label + drive->sectorSize);
            }
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
                Log( LOG_ERR, "Unable to read GPT header (%d: %s)", errno, strerror( errno ) );
            }
            else
            {
                uint32_t crc32 = get32LE( gptHeader->crc32 );
                memset( gptHeader->crc32, 0, sizeof( gptHeader->crc32 ) );

                if ( memcmp( gptHeader->signature, "EFI PART", 8 ) != 0
                    || get32LE( gptHeader->revision ) != 0x00010000
                    || !checkCRC32( crc32, (byte *) gptHeader, get32LE(gptHeader->size) ) )
                {
                    Log( LOG_INFO, "signature, revision or CRC is incorrect" );
                }
                else
                {
                    Log( LOG_INFO, "signature, revision & CRC are correct" );
                    Log( LOG_INFO, " Partition first LBA = %ld", get64LE( gptHeader->partitionTable.firstLBA ) );
                    Log( LOG_INFO, "     Partition Count = %d",  get32LE( gptHeader->partitionTable.count ) );
                    Log( LOG_INFO, "Partition Entry Size = %d",  get32LE( gptHeader->partitionTable.size ) );

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
                            Log( LOG_ERR, "Unable to read partition table (%d: %s)", errno, strerror( errno ) );
                        }
                        else
                        {
                            Log( LOG_INFO, "read of partition table successful" );
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
                                    Log( LOG_INFO, "found LVM PV partition" );

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
 * below is a skeleton to use for testing. Wouldn't be used in a bootloader
 */

/**
 *
 * @param output
 */
void usage( FILE * output )
{
    fprintf( output, "### usage: %s <drive path> <logical volume label>\n", gExecName );
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
                        tMemoryBuffer * buffer = readLogicalVolume( drive, argv[2], metadataTree );
                        if ( isValidPtr( buffer ) )
                        {
                            Log( LOG_INFO, "memory buffer @ %p", (void *) buffer );
                            Log( LOG_INFO, "        start = %p", (void *) buffer->start );
                            Log( LOG_INFO, "       length = %ld (0x%lx)", buffer->length, buffer->length );
                            Log( LOG_INFO, "         hash @ %p", (void *) buffer->hash );
                        }
                    }
                }
            }
        }
    }

    closeDrive( drive );

    exit( 0 );
}
