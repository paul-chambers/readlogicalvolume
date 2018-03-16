//
// Created by Paul on 2/23/2018.
//

#ifndef READLOGICALVOLUME_PARSEMETADATA_H
#define READLOGICALVOLUME_PARSEMETADATA_H

typedef enum
{
    childNode = 1,
    listNode,
    stringNode,
    integerNode
} tNodeType;

typedef struct tNode
{
    struct tNode * next;
    tHash          hash;
    tStringZ     * key;
    tNodeType      type;
    union
    {
        struct tNode * child;
        struct tNode * list;
        tStringZ     * string;
        int64_t        integer;
    };
} tNode;

/* built from the metadata node tree */

typedef struct tPhysicalVolume {
    struct tPhysicalVolume * next;
    tDrive                 * drive;
    char                   * name;
    char                   * id;
    char                   * dev;
    size_t   extentSize;    /* in bytes */
    uint64_t devSize;
    tExtent  peStart;   /* start of the region containing extents, in sectors from the beginning of the partition */
    long     peCount;
} tPhysicalVolume;

typedef struct tStripe {
    tStringZ        * pvName;
    tPhysicalVolume * physicalVolume;
    tExtent           startExtent;
} tStripe;

typedef struct tLogicalVolumeSegment {
    tExtent   startExtent;
    long      extentCount;
    long      stripeCount;
    tStripe * stripes;
} tLogicalVolumeSegment;

tNode         * parseMetadata( tTextBlock * metadata );
tMemoryBuffer * readLogicalVolume( tDrive * drive, const char * lvName, tNode * root );

#endif //READLOGICALVOLUME_PARSEMETADATA_H
