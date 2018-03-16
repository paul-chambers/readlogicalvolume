//
// Created by Paul on 2/22/2018.
//
//
// https://github.com/libyal/libvslvm/blob/master/documentation/Logical%20Volume%20Manager%20(LVM)%20format.asciidoc
//

#ifndef READLOGICALVOLUME_LVM_H
#define READLOGICALVOLUME_LVM_H

typedef struct tLVMPVLabel
{                             /* Ofst Size Value           Description */
    byte    signature[8];     /*   0   8   "LABELONE"      Signature */
    byte    sectorNumber[8];  /*   8   8   Sector number   The sector number of the physical volume label header */
    byte    crc32[4];         /*  16   4   Checksum        CRC-32 from offset 20 to end of the physical
                                                           volume label sector */
    byte    offset[4];        /*  20   4   Data offset     The offset, in bytes, relative to the start
                                           (header size)   of the physical volume label header */
    byte    typeID[8];        /*  24   8   "LVM2 001"   Type indicator */
} tLVMPVLabel;

typedef struct tLVMDataArea
{                           /* Ofst	Size  Value	Description */
    byte    offset[8];      /*  0    8    Data area offset    Offset in bytes, relative to the start of the physical volume */
    byte    size[8];        /*  8    8    Data area size      Value in bytes */

} tLVMDataArea;

typedef struct tLVMPVHeader
{                               /* Ofst Size Value	Description */
    byte            uuid[32];   /*   0   32   Physical volume uuid  Contains a UUID stored as an ASCII string. */
    byte            size[8];    /*  32    8   Physical volume size  Volume size in bytes */
    tLVMDataArea    list[];     /*  40    …​   List of data          The last descriptor in the list is terminator
                                              area descriptors       and consists of 0-byte values. See tDiskDataArea */
                                /*   …    …​   List of metadata      The last descriptor in the list is terminator
                                              area descriptors       and consists of 0-byte values. See tDiskDataArea */
} tLVMPVHeader;

#define MDA_IGNORED      0x00000001     /* invalidated metadata - ignore */
#define MDA_INCONSISTENT 0x00000002
#define MDA_FAILED       0x00000004

typedef struct tLVMRawLocation
{                       /*Ofst Sz Value         	Description */
    byte    offset[8];  /*  0  8  Data area offset  The offset in bytes, relative to the start of the metadata area */
    byte    size[8];    /*  8  8  Data area size    Value in bytes */
    byte    crc32[4];   /* 16  4  Checksum          CRC-32 of @todo (metadata?) */
    byte    flags[4];   /* 20  4  Flags             See section: Raw location descriptor flags */
} tLVMRawLocation;

typedef struct tLVMMetadataHeader
{                                   /*Ofst Sz Value                 Description */
    byte            crc32[4];       /*  0  4  Checksum              CRC-32 from ofst 4 to end of metadata area hdr */
    byte            signature[16];  /*  4 16  " LVM2 x[5A%r0N*>"    Signature   */
    byte            version[4];     /* 20  4  1                     Version     */
    byte            offset[8];      /* 24  8  Metadata area offset  The offset in bytes, of the metadata area
                                                                    relative to the start of the physical volume */
    byte            size[8];        /* 32  8  Metadata area size    The size of the metadata area in bytes */
    tLVMRawLocation list[];         /* 40  …​  List of raw location descriptors
                                                                    The list is terminated by an entry that
                                                                    is all zero. See tRawlocation above */
} tLVMMetadataHeader;

#endif //READLOGICALVOLUME_LVM_H
