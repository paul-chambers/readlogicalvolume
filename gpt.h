//
// Created by Paul on 2/20/2018.
//

#ifndef READLOGICALVOLUME_GPT_H
#define READLOGICALVOLUME_GPT_H

/* GPT header, points to the table itself. Primary copy is at LBA 1, after the protective MBR */

typedef struct tGPTHeader
{
    byte    signature[8];           /*  0 (0x00)   8 bytes	Signature ("EFI PART", 45h 46h 49h 20h 50h 41h 52h 54h
                                                            or 0x5452415020494645ULL[a] on little-endian machines) */
    byte    revision[4];            /*  8 (0x08)   4 bytes	Revision (for GPT version 1.0 (through at least UEFI
                                                            version 2.7 (May 2017)), the value is 00h 00h 01h 00h) */
    byte    size[4];                /* 12 (0x0C)   4 bytes	Header size in little endian (in bytes,
                                                            usually 5Ch 00h 00h 00h or 92 bytes) */
    byte    crc32[4];               /* 16 (0x10)   4 bytes	CRC32/zlib of header (offset +0 up to header size) in
                                                            little endian, with this field zeroed during calculation */
    byte    zero[4];                /* 20 (0x14)   4 bytes	Reserved; must be zero */
    byte    currentLBA[8];          /* 24 (0x18)   8 bytes	Current LBA (location of this header copy) */
    byte    backupLBA[8];           /* 32 (0x20)   8 bytes	Backup LBA (location of the other header copy) */
    byte    firstAvailableLBA[8];   /* 40 (0x28)   8 bytes	First usable LBA for partitions
                                                            (primary partition table last LBA + 1) */
    byte    lastAvailableLBA[8];    /* 48 (0x30)   8 bytes	Last usable LBA (secondary partition table first LBA - 1) */
    byte    diskGUID[16];           /* 56 (0x38)  16 bytes	Disk GUID (also referred as UUID on UNIXes) */
    struct {
        byte    firstLBA[8];        /* 72 (0x48)   8 bytes	Starting LBA of array of partition entries (always 2 in
                                                            primary copy) */
        byte    count[4];           /* 80 (0x50)   4 bytes	Number of partition entries in array */
        byte    size[4];            /* 84 (0x54)   4 bytes	Size of a single partition entry (usually 80h or 128) */
    } partitionTable;
    byte    tableCRC32[4];          /* 88 (0x58)   4 bytes	CRC32/zlib of partition array in little endian */
    byte    zeroPad[];              /* 92 (0x5C)   *	    Reserved; must be zeroes for the rest of the block
                                                            (420 bytes for a sector size of 512 bytes; but can
                                                            be more with larger sector sizes) */
} tGPTHeader;

typedef struct tGPTEntry
{
    byte    type[16];               /*  0 (0x00)  16 bytes  Partition type GUID */
    byte    unique[16];             /* 16 (0x10)  16 bytes  Unique partition GUID */
    byte    firstLBA[8];            /* 32 (0x20)   8 bytes  First LBA (little endian) */
    byte    lastLBA[8];             /* 40 (0x28)   8 bytes  Last LBA (inclusive, usually odd) */
    byte    attributes[8];          /* 48 (0x30)   8 bytes  Attribute flags (e.g. bit 60 denotes read-only) */
    char    name[72];               /* 56 (0x38)  72 bytes  Partition name (36 UTF-16LE code units) */
} tGPTEntry;

#endif //READLOGICALVOLUME_GPT_H
