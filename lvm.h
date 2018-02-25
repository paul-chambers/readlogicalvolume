//
// Created by Paul on 2/22/2018.
//
//
// https://github.com/libyal/libvslvm/blob/master/documentation/Logical%20Volume%20Manager%20(LVM)%20format.asciidoc
//

#ifndef READLOGICALVOLUME_LVM_H
#define READLOGICALVOLUME_LVM_H

typedef struct tPVLabel
{                             /* Ofst Size Value           Description */
    byte    signature[8];     /*   0   8   "LABELONE"      Signature */
    byte    sectorNumber[8];  /*   8   8   Sector number   The sector number of the physical volume label header */
    byte    crc32[4];         /*  16   4   Checksum        CRC-32 from offset 20 to end of the physical
                                                           volume label sector */
    byte    offset[4];        /*  20   4   Data offset     The offset, in bytes, relative to the start
                                           (header size)   of the physical volume label header */
    byte    typeID[8];        /*  24   8   "LVM2 001"   Type indicator */
} tPVLabel;

typedef struct tDataArea
{                           /* Ofst	Size  Value	Description */
    byte    offset[8];      /*  0    8    Data area offset    Offset in bytes, relative to the start of the physical volume */
    byte    size[8];        /*  8    8    Data area size      Value in bytes */

} tDataArea;

typedef struct tPVHeader
{                           /* Ofst Size Value	Description */
    byte        uuid[32];   /*   0   32   Physical volume uuid   Contains a UUID stored as an ASCII string. */
    byte        size[8];    /*  32    8   Physical volume size   volume size in bytes */
    tDataArea   list[];     /*  40    …​   List of data           The last descriptor in the list is terminator
                                           area descriptors      and consists of 0-byte values. See tDataArea */
                            /*   …    …​   List of metadata      The last descriptor in the list is terminator
                                           area descriptors      and consists of 0-byte values. See tDataArea */
} tPVHeader;


#define RAW_LOCN_IGNORED    0x00000001  /* invalidated metadata - ignore */

typedef struct tRawLocation
{                       /* Ofst	Size  Value         	Description */
    byte    offset[8];  /*   0   8    Data area offset  The offset in bytes, relative to the start of the metadata area */
    byte    size[8];    /*   8   8    Data area size    Value in bytes */
    byte    crc32[4];   /*  16   4    Checksum          CRC-32 of TODO (metadata?) */
    byte    flags[4];   /*  20   4    Flags             See section: Raw location descriptor flags */
} tRawLocation;

typedef struct tMetadataHeader
{                           /* Ofst	Size Value                      Description */
    byte    crc32[4] ;      /*   0   4   Checksum                   CRC-32 from ofst 4 to end of metadata area hdr */
    byte    signature[16];  /*   4  16   "\x20LVM2\x20x[5A%r0N*>"   Signature   */
    byte    version[4];     /*  20   4   1                          Version     */
    byte    offset[8];      /*  24   8   Metadata area offset       The offset in bytes, of the metadata area
                                                                    relative to the start of the physical volume */
    byte    size[8];        /*  32   8   Metadata area size         The size of the metadata area in bytes */
    tRawLocation  list[];   /*  40   …​   List of raw location descriptors
                                                                    The last descriptor in the list is terminator
                                                                    and consists of 0-byte values. See tRawlocation */
} tMetadataHeader;

#endif //READLOGICALVOLUME_LVM_H
