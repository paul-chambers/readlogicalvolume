//
// Created by Paul Chambers on 2/20/2018.
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
#include "parseMetadata.h"
#include "gpt.h"
#include "lvm.h"


/**** only needed for testing ****/
struct {
    const char * execName;
} g;
/**********************************/

/* placeholder */
typedef byte        tHash[64];


typedef struct {
    byte *       start;
    size_t       length;
    byte *       hash;
} tMemoryBuffer;

/************************************/

uint64_t getBE(byte * ptr, int count)
{
    uint64_t result = 0;
    for (int i = count; i > 0; --i)
    {
        result <<= 8;
        result |= *ptr++;
    }
    return (result);
}

uint64_t getLE(byte * ptr, int count)
{
    uint64_t result = 0;
    ptr += count;
    for (int i = count; i > 0; --i)
    {
        result <<= 8;
        result |= *(--ptr);
    }
    return (result);
}

uint64_t get64LE(byte * ptr)
{
    return getLE(ptr,8);
}

uint32_t get32LE(byte * ptr)
{
    return (uint32_t)getLE(ptr,4);
}

uint16_t get16LE(byte * ptr)
{
    return (uint16_t)getLE(ptr,2);
}

/***** crypto stuff *****/

int checkCRC32(uint32_t crc, byte * block, size_t length)
{
    /* calculate a CRC32 on the memory block, and check it against the provided CRC */
    /* done this way so I can be lazy and stub out this routine */
    /* TODO: implement CRC32 algo. */
    return 1;
}

int initHash(tMemoryBuffer * buffer)
{
    buffer->hash = calloc(sizeof(tHash), sizeof(byte));
    if (buffer->hash == NULL)
    {
        Log(LOG_ERR, "unable to allocate space for the hash (%d: %s)", errno, strerror(errno));
    }
    return (0);
}

int calcHash( tMemoryBuffer * buffer, byte * start, size_t length )
{
    return (0);
}


int SixteenBytesAreZero(byte *data)
{
    byte * p = data;
    unsigned int result = 0;

    for (int i = 16; i>0; --i)
    { result |= *p++; }

    return (result == 0);
}

int UUIDisLVM(byte *uuid)
{
    const byte lvmUUID[] = {0x79, 0xD3, 0xD6, 0xE6, 0x07, 0xF5, 0xC2, 0x44,
                            0xA2, 0x3C, 0x23, 0x8F, 0x2A, 0x3D, 0xF9, 0x28};

    /* for (int i = 16; i > 0; --i)
        DebugOut( "0x%02X,", *uuid++); */

    return (memcmp(uuid, lvmUUID, sizeof(lvmUUID)) == 0);
}

void displayGPTEntry( tGPTEntry *entry )
{
#ifdef optDebugOutput
    char name[37], *p;
    char uuid[40];
    byte *q;
    /* type[16];       0 (0x00)  16 bytes  Partition type GUID */
    /* thank Microsoft for the 'mixed-endian' representation */
    snprintf(uuid, sizeof(uuid),
            "%08x-%04x-%04x-%04llx-%012llx",
            get32LE(&entry->type[0]),
            get16LE(&entry->type[4]),
            get16LE(&entry->type[6]),
            getBE(&entry->type[8],2),
            getBE(&entry->type[10],6));
    Log(LOG_INFO, "      type %s", uuid );

    /* unique[16];    16 (0x10)  16 bytes  Unique partition GUID */
    /* thank Microsoft for the 'mixed-endian' representation */
    snprintf(uuid, sizeof(uuid),
            "%08x-%04x-%04x-%04llx-%012llx",
            get32LE(&entry->unique[0]),
            get16LE(&entry->unique[4]),
            get16LE(&entry->unique[6]),
            getBE(&entry->unique[8],2),
            getBE(&entry->unique[10],6));
    Log(LOG_INFO, "      UUID %s", uuid);

    /* firstLBA[8];   32 (0x20)   8 bytes  First LBA (little endian) */
    Log(LOG_INFO, "  firstLBA %lld", get64LE(entry->firstLBA));
    /* lastLBA[8];    40 (0x28)   8 bytes  Last LBA (inclusive, usually odd) */
    Log(LOG_INFO, "    lasLBA %lld", get64LE(entry->lastLBA));
    /* attributes[8]; 48 (0x30)   8 bytes  Attribute flags (e.g. bit 60 denotes read-only) */
    Log(LOG_INFO, "attributes %llx", get64LE(entry->attributes));
    /* name[72];      56 (0x38)  72 bytes  Partition name (36 UTF-16LE code units) */
    /* hack to convert UTF-16LE to plain ascii */
    p = name;
    q = entry->name;
    for (int i = 36; i > 0; --i)
    {
        *p = *q++;
        if (*p == '\0') break;
        if (*p < 32 || *p > 127 || *q != 0)
        {
            *p = '.';
        }
        ++p; ++q;
    }
    *p = '\0';
    Log(LOG_INFO, "      name %s", name);
#endif
}


void readMetadata(tDrive * drive, tGPTEntry * entry, tDataArea * metadataArea)
{

    /* the metadata area is a large circular buffer (1MB, typically)
     * only a small section is current at any point in time.
     * So we only load the header, and walk the 'rawlocation' list
     * looking for the active section, and only read that into memory   */
    size_t requestLength;
    ssize_t rdLen;
    byte signature[] = {' ','L','V','M','2',' ','x','[','5','A','%','r','0','N','*','>'};

    requestLength = 65536;
    tMetadataHeader * mdHeader = calloc(requestLength, sizeof(byte));
    off64_t partitionOffset = get64LE(entry->firstLBA) * drive->sectorSize;

    rdLen = readDrive(drive,
                      partitionOffset + get64LE(metadataArea->offset),
                      mdHeader,
                      requestLength);
    if (rdLen == requestLength)
    {
        if (memcmp(mdHeader->signature, signature, sizeof(signature)) == 0
            && get32LE(mdHeader->version) == 1 )
        {
            off64_t metadataBase = get64LE(mdHeader->offset);
            size_t  metadataSize = get64LE(mdHeader->size);

            Log(LOG_INFO, "metadata signature matched");
            Log(LOG_INFO, "  metadata offset %8llx", metadataBase);
            Log(LOG_INFO, "    metadata size %8llx", metadataSize);

            tRawLocation * rawLoc = mdHeader->list;
            while (!SixteenBytesAreZero((byte *)rawLoc))
            {
                if (get32LE(rawLoc->flags) != RAW_LOCN_IGNORED)
                {
                    off64_t offset = get64LE(rawLoc->offset);
                    size_t requestLength = get64LE(rawLoc->size);
                    Log(LOG_INFO,"found active metadata");
                    Log(LOG_INFO,"  offset %8llx", offset);
                    Log(LOG_INFO,"    size %8llx", requestLength);
                    Log(LOG_INFO,"   crc32 %08x",  get32LE(rawLoc->crc32));
                    Log(LOG_INFO,"   flags %08x",  get32LE(rawLoc->flags));

                    /* grab the active metadata */
                    byte * metadata = calloc(requestLength, sizeof(byte));
                    if ( metadata != NULL )
                    {
                        ssize_t rdLen = readDrive( drive,
                                                   partitionOffset + metadataBase + offset,
                                                   metadata,
                                                   requestLength);
                        if (rdLen == requestLength)
                        {
                            parseMetadata(metadata, requestLength);
                        }
                    }
                }
                ++rawLoc;
            }
        }
    }

/*    for (int i = 0; i < 16; ++i)
    {
        DebugOut( "\'%c\',", mdHeader->signature[i]);
    } */

}

int readPhysicalVolume(tDrive * drive, tGPTEntry * entry)
{
    tPVLabel  * p;
    tPVLabel  * q;
    tPVHeader * header;
    tDataArea * dataArea;
    tDataArea * metadataArea;
    size_t      readRequest;
    ssize_t     rdlen;
#ifdef optDebugOutput
    const char *ordinal[] = {"first", "second", "third", "fourth"};
#endif

    displayGPTEntry(entry);

    /* technically we only need four, but read a couple more in case someone
     * gets creative and their structures extend beyond one sector  */
    readRequest = 6 * drive->sectorSize;
    p = (tPVLabel *)calloc(readRequest, sizeof(byte));

    if (p != NULL)
    {
        rdlen = readDrive(drive, get64LE(entry->firstLBA) * drive->sectorSize, p, readRequest);
        if (rdlen != readRequest)
        {
            Log(LOG_ERR,"unable to read pvLabel");
        }
        else
        {
            q = p;
            for (int i = 0; i < 4; ++i)
            {
                if ( (memcmp(q->signature, "LABELONE", 8) == 0)
                  && (memcmp(q->typeID,"LVM2 001",8) == 0)
                  && checkCRC32( get32LE(q->crc32), (byte*)q + 20, drive->sectorSize - 20))
                {
                    Log(LOG_INFO, "found pvLabel in the %s sector", ordinal[i]);
                    header = (tPVHeader *)((byte *)q + get32LE(q->offset));
                    Log(LOG_INFO, "PV UUID is %32s", header->uuid );
                    Log(LOG_INFO, "PV size is %lld", get64LE(header->size) );
                    dataArea = header->list;
                    while (!SixteenBytesAreZero((byte *) dataArea)) {
                        Log(LOG_INFO, "    data area: offset %llx, %lld bytes",
                            get64LE(dataArea->offset), get64LE(dataArea->size));
                        ++dataArea;
                    }
                    ++dataArea;
                    metadataArea = dataArea;
                    while (!SixteenBytesAreZero((byte *) dataArea)) {
                        Log(LOG_INFO, "metadata area: offset %llx, %lld bytes",
                            get64LE(dataArea->offset), get64LE(dataArea->size));
                        readMetadata(drive, entry, dataArea);
                        ++dataArea;
                    }
                    dataArea = metadataArea;
                }
                q = (tPVLabel *)((byte *)q + drive->sectorSize);
            }
        }
    }
}

int readGPT( tDrive * drive )
{
    tGPTHeader    * gptHeader;
    tGPTEntry     * gptTable;
    unsigned long   tableLength;
    ssize_t         result;
    tGPTEntry     * entry;
    size_t          length;
    if (drive != NULL)
    {
        gptHeader = malloc(sizeof(tGPTHeader));
        if (gptHeader != NULL)
        {
            readDrive(drive, drive->sectorSize, gptHeader, sizeof(tGPTHeader));
            uint32_t crc32 = get32LE(gptHeader->crc32);
            memset(gptHeader->crc32, 0, sizeof(gptHeader->crc32));
            if (   memcmp(gptHeader->signature, "EFI PART", 8) == 0
                && get32LE(gptHeader->revision) == 0x00010000
                && checkCRC32(crc32, (byte *)gptHeader, drive->sectorSize) )
            {
                Log(LOG_INFO, "signature & revision are correct");
                Log(LOG_INFO, " Partition first LBA %lld", get64LE(gptHeader->partitionTable.firstLBA));
                Log(LOG_INFO, "     Partition Count %d",   get32LE(gptHeader->partitionTable.count));
                Log(LOG_INFO, "Partition Entry Size %d",   get32LE(gptHeader->partitionTable.size));

                tableLength = get32LE(gptHeader->partitionTable.count)
                              * get32LE(gptHeader->partitionTable.size);
                gptTable = calloc(tableLength, sizeof(byte));

                if (gptTable != NULL)
                {
                    result = readDrive(drive,
                                       get64LE(gptHeader->partitionTable.firstLBA) * drive->sectorSize,
                                       gptTable,
                                       tableLength);
                    if (result == tableLength)
                    {
                        Log(LOG_INFO, "read of partition table successful");
                        entry = gptTable;
                        length = get32LE(gptHeader->partitionTable.size);
                        for (int i = get32LE(gptHeader->partitionTable.count); i > 0; --i)
                        {
                            if (SixteenBytesAreZero(entry->type)) break;
                            if (UUIDisLVM(entry->type))
                            {
                                Log(LOG_INFO,"found LVM partition");
                                readPhysicalVolume(drive, entry);
                            }
                            entry = (tGPTEntry *)((byte *)entry + length);
                        }
                    }
                }
            }
            else Log(LOG_INFO, "signature incorrect");
        }
    }
}

tMemoryBuffer * readVolume( tDrive * drive, const char * lvName )
{
    byte *  start;
    size_t  length;

    tMemoryBuffer * buffer = malloc(sizeof(tMemoryBuffer));

    if (buffer != NULL)
    {
        buffer->start  = NULL;
        buffer->length = 0;
        /* see if we cna find lvName among the logical volumes */
        /* found it, let's allocate the buffer and hash storage */
        initHash(buffer);
        /* populate the buffer by reading in each extent */
        /* pass the extent we just read successfully to the hash calculation */
        // calcHash(buffer, start, length);
    }
    return (buffer);
}

/*
 * below is a skeleton to use for testing. Wouldn't be used in a bootloader
 */

void usage(FILE *output)
{
    fprintf( output,
             "### error: %s <drive path> <volume label>\n",
             g.execName );
}

int main(int argc, char *argv[])
{
    tDrive        * drive  = NULL;
    tMemoryBuffer * buffer = NULL;

    g.execName = basename(argv[0]);

    if (argc < 3)
    {
        usage(stderr);
        exit(-1);
    }

    drive = openDrive(argv[1]);
    if (drive != NULL)
    {
        readGPT(drive);
        buffer = readVolume(drive, argv[2]);
        if (buffer != NULL)
        {
            Log( LOG_INFO, "memory buffer @ %p", (void *)buffer);
            Log( LOG_INFO, "        start = %p", (void *)buffer->start);
            Log( LOG_INFO, "       length = %d (0x%x)", buffer->length, buffer->length);
            Log( LOG_INFO, "         hash @ %p", (void *)buffer->hash);
        }
    }
    closeDrive(drive);

    exit(0);
}