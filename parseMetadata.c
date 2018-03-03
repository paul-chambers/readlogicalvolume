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
#include <syslog.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <inttypes.h>
#include <endian.h>

#include "readlogicalvolume.h"
#include "debug.h"
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
    tHash        hash;
    tStringZ     * key;
    tNodeType    type;
    union
    {
        struct tNode * child;
        struct tNode * list;
        tStringZ     * string;
        int64_t      integer;
    };
} tNode;

static tNode * gRootNode = NULL;

typedef struct
{
    char   * ptr;
    char   * start;
    char   * end;
    size_t length;
} tBuffer;

tBuffer * newBuffer( char * start, size_t length )
{
    tBuffer * result;
    result = calloc( sizeof( tBuffer ), 1 );
    if ( isHeapPtr( result ) )
    {
        result->ptr    = start;
        result->start  = start;
        result->end    = start;
        result->length = length;
    }
    return result;
}

/***************************************************************/

void dumpNodes( tNode * node, int depth );

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

/**
 *
 * @param node
 * @param depth
 */
void dumpNodes( tNode * node, int depth )
{
#ifdef optDebugOutput
    while ( node != NULL )
    {
        dumpNode( node, depth );
        switch ( node->type )
        {
        case childNode:
            dumpNodes( node->child, depth + 1 );
            break;

        case listNode:
            dumpNodes( node->list, depth + 1 );
            break;

        default:
            /* nothing needed for other types */
            break;
        }
        node = node->next;
    }
#endif
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
int getNextChar( tBuffer * buf )
{
    int result = EOF;

    if ( buf->length > 0 )
    {
        result = *buf->ptr++;
        --buf->length;
    }
    return result;
}

int getPreviousChar( tBuffer * buf )
{
    int result = EOF;

    if ( buf->length > 0 )
    {
        result = *(--buf->ptr);
        ++buf->length;
    }
    return result;

}

void setStringStart( tBuffer * buf )
{
    buf->start = buf->ptr;
}

void setStringEnd( tBuffer * buf )
{
    buf->end = buf->ptr - 1;
}

long lenString( tBuffer * buf )
{
    return (buf->end - buf->start);
}

char * dupString( tBuffer * buf )
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

tNode * getKeyHash( tHash hash, tNode * root )
{
    tNode * node;
    tNode * result;

    node = root;
    if ( node == NULL )
    {
        node = gRootNode;
    }

    while ( node != NULL && hash != node->hash )
    {
        switch ( node->type )
        {
        case childNode:
            if ( node->child != NULL )
            {
                result = getKeyHash( hash, node->child );
                if ( result != NULL )
                {
                    return result;
                }
            }
            break;

        case listNode:
            if ( node->list != NULL )
            {
                result = getKeyHash( hash, node->list );
                if ( result != NULL )
                {
                    return result;
                }
            }
            break;

        default:
            /* nothing needed for other types */
            break;
        }

        node = node->next;
    }
    return node;
}

tNode * getKeyPath( char * keyPath, tNode * root )
{
    tNode      * result;
    char * start;
    char * end = keyPath;

    result = root;
    if ( result == NULL )
    {
        result = gRootNode;
    }

    if ( keyPath[ 0 ] == '/' )
    {
        result = gRootNode;
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

/****************************************************************************/

/**
 *
 * @param node
 * @param buf
 * @return
 */
tNode * parseList( tNode * node, tBuffer * buf )
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

tNode * parseString( tNode * node, tBuffer * buf )
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

tNode * parseInteger( tNode * node, tBuffer * buf )
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

tNode * parseValue( tNode * node, tBuffer * buf )
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

tNode * parseChild( tNode * node, tBuffer * buf )
{
    int   c;
    tNode * result = NULL;
    int   getKey;

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

void parseMetadata( char * metadata, size_t length )
{
    tNode * root;
    tNode * logicalVolume;

    DebugOut( "\n_______________________________\n\n" );
    fwrite( metadata, length, 1, stderr );
    DebugOut( "\n_______________________________\n\n" );

    tBuffer * buf = newBuffer( metadata, length );

    root = newNode( NULL );
    if ( isValidPtr( root ) )
    {
        gRootNode   = root;
        root->key   = "root_node";
        root->hash  = hashString( root->key );
        root->type  = childNode;
        root->child = parseChild( root, buf );

        DebugOut( "\n" );
        Log( LOG_DEBUG, "######## node dump ########\n" );
        dumpNodes( gRootNode, 0 );

        logicalVolume = getKeyPath( "logical_volumes/kernel", NULL );

        DebugOut( "\n" );
        Log( LOG_DEBUG, "######## kernel node ########\n" );
        dumpNodes( logicalVolume, 0 );
    }
}
