/*
    Once we've navigated the on-disk structures, we land at metadata
    which is in a text format. It's a nested 'key = value' style format
    not JSON or XML.

    The parsing of this is different from the more normal style of binary
    structures handled in readlogicalvolume.c, so it's split out here for
    clarity.

     Created by Paul on 2/23/2018.
*/

#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <endian.h>

#include "readlogicalvolume.h"
#include "debug.h"
#include "readaccess.h"
#include "stringHash.h"
#include "parseMetadata.h"

const char kIndent[] =
/*              12345678901234567890 */
               "                    "
               "                    "
               "                    "
               "                    "
               "                    "
               "                    "
               "                    "
               "                    "
               "                    ";


tTextBlock * newBuffer( byte * start, size_t length )
{
    tTextBlock * result;
    result = calloc( sizeof( tTextBlock ), 1 );
    if ( isHeapPtr( result ) )
    {
        result->block.ptr    = start;
        result->block.length = length;
        result->start        = start;
        result->end          = start;
    }
    return result;
}

/***************************************************************/

tNode * newNode( tNode * node )
{
    if ( isValidPtr( node ) )
    {
        node->next = calloc( sizeof( tNode ), 1 );
        // dumpNode( node, 0 );
        return isValidPtr( node->next ) ? node->next : node;
    }
    else
    {
        return calloc( sizeof( tNode ), 1 );
    }
}

/* patterned after fgetc */
int getNextChar( tTextBlock * buf )
{
    int result = EOF;

    if ( buf->block.length > 0 )
    {
        result = *buf->block.ptr++;
        --buf->block.length;
    }
    return result;
}

int getPreviousChar( tTextBlock * buf )
{
    int result = EOF;

    if ( buf->block.length > 0 )
    {
        result = *(--buf->block.ptr);
        ++buf->block.length;
    }
    return result;

}

void setStringStart( tTextBlock * buf )
{
    buf->start = buf->block.ptr;
}

void setStringEnd( tTextBlock * buf )
{
    buf->end = buf->block.ptr - 1;
}

long lenString( tTextBlock * buf )
{
    return (buf->end - buf->start);
}

char * dupString( tTextBlock * buf )
{
    char * result = NULL;

    if ( lenString( buf ) > 0 )
    {
        result = calloc( lenString( buf ) + 1, 1 ); /* extra byte to guarantee a trailing null */
        if ( isHeapPtr( result ) )
        {
#if 0
            Log(LOG_DEBUG, "string:");
            fprintf(stderr, "\"");
            fwrite(buf->start, buf->end - buf->start - 1, 1, stderr);
            fprintf(stderr, "\"\n");
#endif
            memcpy( result, buf->start, lenString( buf ) );
        }
    }

    return result;
}

/* node traversal functions */

/**
   callback that can be invoked for the root node provided, and each node below it
 */
typedef tNode * (*tNodeCallback)( tNode * node, int depth, int index, void * cbData );

/**
 * recursive inner function used by forEachNode() to traverse the node tree
 * invokes the callback on each node, depth-first, with some context, until
 * the callback returns something other than NULL, which is then returned.
 *
 * @param node      the starting point
 * @param depth     how many levels down we've recursed
 * @param callback  callback that is passed the current node and some context
 * @param cbData    opaque pointer passed through to callback for its use
 * @return either NULL, or an non-null value returned by the callback function
 */
tNode * forEachNodeRecurse( tNode * node, int depth, tNodeCallback callback, void * cbData )
{
    tNode * result;
    tNode * child;
    int     index = 0;

    while ( node != NULL )
    {
        result = (*callback)( node, depth, index, cbData );

        if (result != NULL )
        {
            /* unwind */
            return result;
        }

        switch ( node->type )
        {
        case childNode:
            child = node->child;
            break;

        case listNode:
            child = node->list;
            break;

        default:
            /* no recursion needed for other types */
            child = NULL;
            break;
        }

        if ( child != NULL )
        {
            result = forEachNodeRecurse( child, depth + 1, callback, cbData );
            if ( result != NULL )
            {
                return result;
            }
        }

        node = node->next;
        ++index;
    }

    return node;
}


/**
 * invokes a callback on the root node provided, and each node below it, until
 * the callback returns something other than NULL, which is then returned.
 *
 * @param node      the starting point
 * @param depth     how many levels down we've recursed
 * @param callback  callback that is passed the current node and some context
 * @param cbData    opaque pointer passed through to callback for its use
 * @return either NULL, or an non-null value returned by the callback function
 */

tNode * forEachNode( tNode * root, tNodeCallback callback, void * cbData )
{
    tNode * result;

    /* process the root node first */
    result = (*callback)( root, 0, 0, cbData );
    if (result == NULL)
    {
        switch (root->type)
        {
        case listNode:
            result = forEachNodeRecurse( root->list, 1, callback, cbData );
            break;

        case childNode:
            result = forEachNodeRecurse( root->child, 1, callback, cbData );
            break;

        default:
            break;
        }
    }
    return result;
}

tNode * hashMatchCallback(tNode * node, int UNUSED(depth), int UNUSED(index), void * cbData)
{
    if (node->hash == (tHash)cbData)
    {
        return node;
    }
    return NULL;
}

tNode * getKeyHash( tHash hash, tNode * root )
{
    return forEachNode( root, hashMatchCallback, (void *)hash );
}

tNode * getKeyPath( const char * keyPath, tNode * root )
{
    tNode * result;
    const char  * start;
    const char  * end = keyPath;

    result = root;

    if ( keyPath[ 0 ] == '/' )
    {
        ++end;
    }

    while ( result != NULL && *end != '\0' )
    {
        start = end;
        while ( *end != '/' && *end != '\0' )
        {
            ++end;
        }
        result = getKeyHash( hashBytes( start, end - start ), result );
        if ( *end == '/' )
        {
            ++end;
        }
    }
    return result;
}


void dumpPhysicalVolume( tPhysicalVolume * pv )
{
    Log( LOG_INFO, "pv @ %p, next @ %p", pv, pv->next );
    Log( LOG_INFO, "      drive @ %p",   pv->drive );
    Log( LOG_INFO, "       name = \"%s\"",  pv->name );
    Log( LOG_INFO, "         id = \"%s\"",  pv->id  );
    Log( LOG_INFO, "        dev = \"%s\"",  pv->dev );
    Log( LOG_INFO, " extentSize = %ld", pv->extentSize );
    Log( LOG_INFO, "    devSize = %ld", pv->devSize );
    Log( LOG_INFO, "    peStart = %ld", pv->peStart );
    Log( LOG_INFO, "    peCount = %ld", pv->peCount );
}

void dumpSegment( tLogicalVolumeSegment * segment )
{
#ifdef optDebugOutput
    Log( LOG_INFO, "  start extent = %ld", segment->startExtent );
    Log( LOG_INFO, "  extent count = %ld", segment->extentCount );
    Log( LOG_INFO, "  stripe count = %ld", segment->stripeCount );
    Log( LOG_INFO, "  stripes @ %p",  segment->stripes );
    for ( int j = 0; j < segment->stripeCount; ++j )
    {
        Log( LOG_INFO, "    stripe[%d]         pvName = \"%s\"", j, segment->stripes[j].pvName );
        Log( LOG_INFO, "    stripe[%d] physicalVolume @ %p \"%s\"",
             j,
             segment->stripes[j].physicalVolume,
             segment->stripes[j].physicalVolume->name );
        Log( LOG_INFO, "    stripe[%d]   start extent = %ld",    j, segment->stripes[j].startExtent );
    }
#endif
}

/**
 *
 * @param node
 * @param depth
 */
void dumpNode( tNode * node, int depth )
{
#ifdef optDebugOutput
    char scratch[256];
    int i = sizeof( kIndent ) - (depth * 4) - 1;

    if ( node != NULL )
    {
        switch ( node->type )
        {
        case childNode:
            snprintf(scratch, sizeof(scratch), "child @ %p", node->child);
            break;

        case listNode:
            snprintf(scratch, sizeof(scratch), "list @ %p", node->list);
            break;

        case stringNode:
            snprintf(scratch, sizeof(scratch), "\"%s\"", node->string);
            break;

        case integerNode:
            snprintf(scratch, sizeof(scratch), "%ld", node->integer);
            break;

        default:
            snprintf(scratch, sizeof(scratch), "unknown type (%d)", node->type);
            break;
        }
        Log( LOG_INFO, "%s | node @ %10p, next @ %10p, (hash %016lx) \"%s\" = %s",
             &kIndent[ i ], node, node->next, node->hash, node->key, scratch );
    }
    else
    {
        Log( LOG_INFO, "%s node is (nil)", &kIndent[ i ] );
    }
#endif
}

#ifdef optDebugOutput
tNode * dumpNodeCallback( tNode * node, int depth, int UNUSED(index), void * UNUSED(cbData) )
{
    dumpNode( node, depth );
    return NULL;
}
#endif

/**
 *
 * @param node
 * @param depth
 */
void dumpNodeTree( tNode * node )
{
#ifdef optDebugOutput
    forEachNode( node, dumpNodeCallback, NULL );
#endif
}

void dumpMemoryBuffer( tMemoryBuffer * buffer, const char * lvName )
{
    char filename[256];
    snprintf( filename, sizeof( filename ), "%s.bin", lvName );
    int fd = creat( filename, S_IRUSR | S_IRGRP );
    if ( fd == -1 )
    {
        Log( LOG_ERR, "unable to open file \"%s\" (%d: %s)", filename, errno, strerror( errno ) );
    }
    else
    {
        write( fd, buffer->start, buffer->length );
        close( fd );
    }
}

/****************************************************************************/

/**
 *
 * @param node
 * @param buf
 * @return
 */
tNode * parseList( tNode * node, tTextBlock * buf )
{
    tNode   * result;

    enum
    {
        outside,
        insideString,
        insideInteger
    }       state;

    enum
    {
        noElement,
        integerElement,
        stringElement
    }       elementType;

    int64_t integer;
    int     c;

    state       = outside;
    elementType = noElement;
    result      = NULL;

    /* arrays can span lines, so ignore newlines */
    while ( (c = getNextChar( buf )) != EOF )
    {
        switch ( state )
        {
        case outside:
            switch ( c )
            {
            case '"':
                setStringStart( buf );
                elementType = stringElement;
                state       = insideString;
                break;

            case ',':
            case ']':
                if ( elementType != noElement )
                {
                    node = newNode( node );
                    if ( result == NULL ) { result = node; }

                    /* set the node */
                    switch ( elementType )
                    {
                    case integerElement:
                        node->type    = integerNode;
                        node->integer = integer;
                        node->key     = "integer";
                        break;

                    case stringElement:
                        node->type   = stringNode;
                        node->string = dupString( buf );
                        node->key    = node->string;    /* makes it easier to test for presence of node */
                        break;

                    default:
                        /* just to keep the compiler happy */
                        break;
                    }
                    node->hash = hashString( node->key );
                }

                if ( c == ']' )
                {
                    return result;
                }

                elementType = noElement;
                break;

            default:
                if ( isdigit( c ) )
                {
                    state       = insideInteger;
                    elementType = integerElement;
                    integer     = (c - '0');
                }
                break;
            }
            break;

        case insideString:
            if ( c == '"' )
            {
                setStringEnd( buf );
                elementType = stringElement;
                state       = outside;
            }
            break;

        case insideInteger:
            if ( isdigit( c ) )
            {
                integer = (integer * 10) + (c - '0');
            }
            else
            {
                getPreviousChar( buf );
                state = outside;
            }
            break;
        }
    }
    return result;
}

tNode * parseString( tNode * node, tTextBlock * buf )
{
    int c;

    setStringStart( buf );
    do
    {
        c = getNextChar( buf );

    } while ( c != EOF && c != '"' );
    setStringEnd( buf );

    node->type   = stringNode;
    node->string = dupString( buf );

    return node;
}

tNode * parseInteger( tNode * node, tTextBlock * buf )
{
    int64_t integer = 0;
    int     c;

    while ( (c = getNextChar( buf )) != EOF )
    {
        if ( isdigit( c ) )
        {
            integer = (integer * 10) + (c - '0');
        }
        else
        {
            // fprintf(stderr, "\ninteger: %lld\n", integer);
            getPreviousChar( buf );
            break;
        }
    }
    node->type    = integerNode;
    node->integer = integer;

    return node;
}

tNode * parseValue( tNode * node, tTextBlock * buf )
{
    int c;

    while ( (c = getNextChar( buf )) != EOF )
    {
        switch ( c )
        {
        case ' ':
        case '\t':
            /* skip whitespace */
            break;

        case '\n':
            getPreviousChar( buf );
            return node;

        case '[':
            node->type = listNode;
            node->list = parseList( node, buf );
            node->next = NULL;
            return node;

        case '"':
            return parseString( node, buf );

        default:
            if ( isdigit( c ) )
            {
                /* back up one character, otherwise we strip the first digit */
                getPreviousChar( buf );

                return parseInteger( node, buf );
            }
            break;
        }
    }
    return node;
}

tNode * parseChild( tNode * node, tTextBlock * buf )
{
    tNode * result = NULL;
    int     getKey;
    int     c;

    getKey = 1;
    while ( (c = getNextChar( buf )) != EOF )
    {
        // dumpMemory( (unsigned long)buf->ptr - 1, buf->ptr - 1, buf->length < 32 ? buf->length : 32);

        switch ( c )
        {
        case '#':
            // dumpMemory( (unsigned long)buf->ptr, buf->ptr, buf->length < 96 ? buf->length : 96 );
            setStringStart( buf );
            do { c = getNextChar( buf ); } while ( c != EOF && c != '\n' );
            setStringEnd( buf );
            // Log(LOG_INFO, "comment: \"%s\"", dupString( buf ));
            break;

        case '\n':
            do { c = getNextChar( buf ); } while ( c != EOF && (c == '\n' || c == '\t' || c == ' ') );
            getPreviousChar( buf );
            getKey = 1;
            break;

        case '=':
            /* parseValue() figures out if it's a integer, string or list */
            node = parseValue( node, buf );
            break;

        case '{':
            node->type  = childNode;
            node->child = parseChild( node, buf );
            node->next  = NULL;
            break;

        case '}':
            return result;

        default:
            if ( getKey )
            {
                getPreviousChar( buf );
                setStringStart( buf );
                do {
                    c = getNextChar( buf );

                } while ( isalnum( c ) || c == '_' );
                setStringEnd( buf );

                /* ignore zero-length keys, e.g. comment lines */
                if ( lenString( buf ) > 0)
                {
                    node = newNode( node );
                    if ( result == NULL )
                    {
                        result = node;
                    }

                    node->key  = dupString( buf );
                    node->hash = hashString( node->key );
                }
            }
        }
    }
    return (result);
}


tNode * parseMetadata( tTextBlock * metadata )
{
    tNode * root;

    root = newNode( NULL );
    if ( isValidPtr( root ) )
    {
        root->key   = "root_node";
        root->hash  = hashString( root->key );
        root->type  = childNode;
        root->child = parseChild( root, metadata );
        root->next  = NULL;

        DebugOut( "\n" );
        Log( LOG_DEBUG, "######## node dump ########\n" );
        dumpNodeTree( root );
    }
    return root;
}


#define kHash_id        0x000000000cfb66ec
#define kHash_device    0x0000eaeb4d97cacf
#define kHash_dev_size  0x03e752f51209acb8
#define kHash_pe_start  0x03e7536bf0e35441
#define kHash_pe_count  0x03e7536befbf62dc

/**
 *
 * @param node
 * @param depth
 * @param index
 * @param cbData
 * @return
 */
tNode *physVolCallback( tNode * node, int depth, int index, void * cbData )
{
    tPhysicalVolume * pv = (tPhysicalVolume *)cbData;

    switch (depth)
    {
    case 1:
        if (index == 0)
        {
            if ( node->type == childNode )
            {
                pv->name = strdup( node->key );
            }
        }
        else
        {
            /** @todo handle multiple physical volumes */
            Log(LOG_ERR,"multiple physical volumes - not currently supported");
        }
        break;

    case 2:
        switch (node->hash )
        {
        case kHash_id:
            if (node->type == stringNode)
            {
                pv->id = strdup( node->string );
            }
            break;

        case kHash_device:
            if (node->type == stringNode)
            {
                pv->dev = strdup( node->string );
            }
            break;

        case kHash_dev_size:
            if (node->type == integerNode)
            {
                pv->devSize = node->integer;
            }
            break;

        case kHash_pe_start:
            if (node->type == integerNode)
            {
                pv->peStart = node->integer;
            }
            break;

        case kHash_pe_count:
            if (node->type == integerNode)
            {
                pv->peCount = node->integer;
            }
            break;

        default:
            break;
        }
    }
    return NULL;
}


/* depth = 1 */
#define kHash_segment_count 0x69d828731cbe319a
/* depth = 2 */
#define kHash_start_extent  0x8766b8e0de292584
#define kHash_extent_count  0x7dadb102604275ff
#define kHash_type          0x0000003739774241
#define kHash_stripe_count  0x87697ace17d5787e
#define kHash_stripes       0x001e4859a5efeb29
/* depth = 3 */
/* stripe pairs themselves: (physical volume name, starting extent) */

/**
 *
 * @param node
 * @param depth
 * @param index
 * @param cbData
 * @return
 */

tNode * logVolCallback( tNode * node, int depth, int index, void * cbData )
{
    static int seg;
    static int stripeCount;
    tLogicalVolumeSegment * segment = (tLogicalVolumeSegment *)cbData;

    switch (depth)
    {
    case 1: /* is it a segment? which one? */
        if (memcmp(node->key, "segment", 7) == 0
            && node->type == childNode)
        {
            seg = atoi(&node->key[7]) - 1;
        }
        else seg = 0;
        break;

    case 2: /* attributes of this segment */
        if (seg >= 0)
        {
            switch ( node->hash )
            {
            case kHash_start_extent:
                if ( node->type == integerNode )
                {
                    segment[ seg ].startExtent = node->integer;
                }
                break;

            case kHash_extent_count:
                if ( node->type == integerNode )
                {
                    segment[ seg ].extentCount = node->integer;
                }
                break;

            case kHash_stripe_count:
                if ( node->type == integerNode )
                {
                    stripeCount = node->integer;
                    segment[ seg ].stripeCount = stripeCount;
                    segment[ seg ].stripes = calloc( sizeof(tStripe), stripeCount );
                }
                break;
            }
        }
        break;

    case 3: /* array of stripes (usually 1 stripe) */
        if ( (index & 1) == 0 )
        {
            if ( node->type == stringNode )
            {
                segment[ seg ].stripes[ index / 2 ].pvName = strdup( node->string );
            }
        }
        else
        {
            /* starting extent */
            if ( node->type == integerNode )
            {
                if ( index / 2 < stripeCount )
                {
                    segment[ seg ].stripes[ index / 2 ].startExtent = node->integer;
                }
            }
        }
        break;
    }
    return NULL;
}

tMemoryBuffer * readLogicalVolume( tDrive * drive, const char * lvName, tNode * root )
{
    tPhysicalVolume * physicalVolume;
    tLogicalVolumeSegment * segments;

    physicalVolume = calloc( sizeof(tPhysicalVolume),1 );
    physicalVolume->drive = drive;

    tNode * extentSizeNode = getKeyPath( "extent_size", root );
    if ( isValidPtr(extentSizeNode) && extentSizeNode->type == integerNode )
    {
        physicalVolume->extentSize = extentSizeNode->integer * drive->sectorSize;
        DebugOut( "\n" );
        Log( LOG_INFO, "extents are %ld KB long", physicalVolume->extentSize / 1024 );
    }

    tNode * physicalVolumes = getKeyPath( "physical_volumes", root );

    DebugOut( "\n" );
    Log( LOG_DEBUG, "######## physical volumes ########\n" );
    dumpNodeTree( physicalVolumes );

    if ( isValidPtr(physicalVolumes) && physicalVolumes->type == childNode )
    {
        forEachNode( physicalVolumes, physVolCallback, (void *)physicalVolume );
        dumpPhysicalVolume(physicalVolume);
    }

    tNode * logicalVolume = getKeyPath( "logical_volumes", root );
    logicalVolume = getKeyPath( lvName, logicalVolume );

    DebugOut( "\n" );
    Log( LOG_DEBUG, "######## logical volume ########\n" );
    dumpNodeTree( logicalVolume );

    tNode * segmentCountNode = getKeyPath( "segment_count", logicalVolume );
    int segmentCount = 0;
    if ( isValidPtr( segmentCountNode ) && segmentCountNode->type == integerNode )
    {
        segmentCount = segmentCountNode->integer;
    }
    if ( segmentCount > 0 )
    {
        if (segmentCount == 1)
            Log( LOG_INFO, "there is one segment");
        else
            Log( LOG_INFO, "there are %d segments", segmentCount);

        segments = calloc( sizeof(tLogicalVolumeSegment), segmentCount );
        forEachNode( logicalVolume, logVolCallback, segments );
        for (int i = 0; i < segmentCount; ++i)
        {
            for (int j = 0; j < segments[i].stripeCount; ++j)
            {
                tPhysicalVolume * pv = physicalVolume;
                while ( isValidPtr(pv) )
                {
                    if ( strcmp( segments[i].stripes[j].pvName, pv->name ) == 0 )
                    {
                        segments[i].stripes[j].physicalVolume = pv;
                        break;
                    }
                    pv = pv->next;
                }
            }
        }

#ifdef optDebugOutput
        for ( int i = 0; i < segmentCount; ++i )
        {
            Log( LOG_INFO, "segment %d", i + 1 );
            dumpSegment( &segments[i] );
        }
#endif
    }

    /* now we have enough information to actually read the segments into memory */

    tMemoryBuffer * buffer = malloc( sizeof( tMemoryBuffer ) );

    /** @todo this code assumes one stripe per segment */
    if ( isHeapPtr( buffer ) )
    {
        buffer->length = 0;
        for ( int i = 0; i < segmentCount; ++i )
        {
            buffer->length += segments[ i ].extentCount * segments[ i ].stripes->physicalVolume->extentSize;
        }
        buffer->start  = malloc( buffer->length );
        if ( isValidPtr( buffer->start ) )
        {
            for ( int i = 0; i < segmentCount; ++i )
            {
                tStripe * stripe = segments[ i ].stripes;
                physicalVolume = stripe->physicalVolume;

                off64_t   extentSize = physicalVolume->extentSize;
                off64_t   offset     = (physicalVolume->peStart * physicalVolume->drive->sectorSize)
                                     + (stripe->startExtent * extentSize);

                tBlock    destBlock;
                destBlock.ptr    = &buffer->start[ segments[i].startExtent * extentSize ];
                destBlock.length = segments[i].extentCount * extentSize;

                Log(LOG_INFO, "extentSize = %ld (%1.2f MB)", extentSize, extentSize/1048576.0 );
                Log(LOG_INFO, "    offset = %ld (%ld extents)", offset , offset / extentSize);

                readDrive( stripe->physicalVolume->drive, offset, destBlock.ptr, destBlock.length );
            }

            dumpMemoryBuffer( buffer, lvName );
        }
    }

    return (buffer);
}