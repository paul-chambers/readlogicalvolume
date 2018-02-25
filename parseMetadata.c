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
#include <libgen.h>
#include <inttypes.h>
#include <endian.h>

#include "readlogicalvolume.h"
#include "parseMetadata.h"

typedef uint32_t tHash;

typedef struct tArray {

} tArray;

typedef struct tValue {

} tValue;

typedef enum {
    unassignedNode = 0,
    childNode,
    arrayNode,
    stringNode,
    numberNode
} tNodeType;

typedef struct tNode {
    struct tNode *next;
    tHash       hash;
    byte      * key;
    tNodeType   type;
    union {
        struct tNode  * child;
        struct tArray * array;
        byte          * string;
        int64_t         number;
    };
} tNode;

static tNode * gRootNode = NULL;

byte * copyToString(byte * start, size_t length)
{
    byte * result = calloc(length + 1, sizeof(byte)); /* extra byte to add null */
    if (result != NULL)
    {
        memcpy(result, start, length);
    }
    return result;
}

tHash stringHash(byte *ptr, size_t len)
{
    tHash hash = 199999; /* seed with largish prime */

    for (int i = len; i > 0; --i)
    {
        hash = hash * 43 + *ptr++;
    }
    return hash;
}

void dumpNode( tNode * node, int depth )
{
#ifdef optDebugOutput
    int i = sizeof(kIndent) - (depth * 4);

    Log(LOG_INFO, "%s node @ %p, next @ %p", &kIndent[i], node, node->next);
    Log(LOG_INFO, "%s key = \"%s\" (hash %x)", &kIndent[i], node->key, node->hash );
    switch (node->type)
    {
    case childNode:
        Log(LOG_INFO, "%s node type = child @ %p", &kIndent[i], node->child );
        break;

    case arrayNode:
        Log(LOG_INFO, "%s node type = array", &kIndent[i] );
        break;

    case stringNode:
        Log(LOG_INFO, "%s node type = string \"%s\"", &kIndent[i], node->string );
        break;

    case numberNode:
        Log(LOG_INFO, "%s node type = number %lld", &kIndent[i], node->number );
        break;

    default:
        Log(LOG_INFO, "%s node type = unknown (%d)", &kIndent[i], node->type );
        break;
    }
    DebugOut("\n");
#endif
}

void dumpNodes(tNode * node, int depth)
{
#ifdef optDebugOutput
    while (node != NULL)
    {
        dumpNode(node, depth);
        if (node->type == childNode)
        {
            dumpNodes(node->child, depth + 1);
        }
        node = node->next;
    }
#endif
}

void parseValue(byte **ptr, size_t *length, tNode * node)
{
    byte  * start;
    byte  * str;
    enum { outsideString, insideString } state;
    int     digitsEnded;

    while (**ptr != '\n' && *length > 0)
    {
        switch (node->type)
        {
        default:
            switch (**ptr)
            {
            case ' ':
            case '\t':
                break;

            case '[':
                node->type = arrayNode;
                state = outsideString;
                break;

            case '"':
                node->type = stringNode;
                start = *ptr + 1;
                break;

            default:
                if (**ptr >= '0' && **ptr <= '9')
                {
                    digitsEnded  = 0;
                    node->type   = numberNode;
                    node->number = (**ptr - '0');
                }
                /* accumulate a number */
                break;
            }
            break;

        case arrayNode:
            switch (state)
            {
            case outsideString:
                switch (**ptr)
                {
                case '"':
                    state = insideString;
                    start = *ptr;
                    break;
                case ',':
                case ']':
                    Log(LOG_INFO, "array terminate");
                    break;
                }
                break;

            case insideString:
                switch (**ptr)
                {
                case '"':
                    state = outsideString;
                    str = copyToString(start, *ptr - start);
                    DebugOut("\n    string: \"%s\"", str);
                    free(str);
                    break;
                case '\\':
                    ++(*ptr);
                    --(*length);
                    break;
                }
                break;
            }
            break;

        case stringNode:
            if (**ptr == '"')
            {
                node->string = copyToString(start, *ptr - start);
            }
            break;

        case numberNode:
            if (!digitsEnded && **ptr >= '0' && **ptr <= '9')
            {
                node->type = numberNode;
                node->number = (node->number * 10) + (**ptr - '0');
            }
            else
            {
                digitsEnded = 1;
            }
            break;
        }
        ++(*ptr);
        --(*length);
    }
}

void parseNodes(byte **ptr, size_t *length, tNode *current)
{
    enum    { endOfLine, isKey, isComment, isValue, isArray, isChild } state;
    tHash   hash;
    byte    *kStart;
    byte    *kEnd;

    state = endOfLine;
    while ( *length > 0 )
    {
        DebugOut( "%c", **ptr);

        switch (**ptr)
        {
        case ' ':
        case '\t':
            /* ignore whitespace */
            break;

            /* if we hit a comment, consume up to next newline */
        case '#':
            state = isComment;
            Log(LOG_INFO, "\ncomment: ");
            break;

        case '=':
            state = isValue;

            current->hash = hash;
            current->key  = copyToString(kStart, kEnd - kStart + 1);
            break;

        case '{':
            state = isChild;

            current->hash  = hash;
            current->key   = copyToString(kStart, kEnd - kStart + 1);
            current->type  = childNode;
            current->child = calloc(sizeof(tNode), sizeof(byte));
            break;

        case '}':
            Log(LOG_INFO, "leaving child");
            ++(*ptr);
            --(*length);
            return; /* back to the parent */

        case '\n':
            state = endOfLine;
            break;

        default:
            if (state == isKey)
            {
                current->hash = current->hash * 43 + **ptr;
                if (kStart == NULL)
                {
                    kStart = *ptr;
                }
                kEnd = *ptr;
            }
            break;
        }

        ++(*ptr);
        --(*length);

        /* state-based processing */
        switch (state)
        {
        case endOfLine:
            if (current->type != unassignedNode)
            {   /* node has been filled in, so allocate a fresh one */
                current->next = calloc(sizeof(tNode), sizeof(byte));
                dumpNode(current,0);
                current = current->next;
            }
            /* the line begins with the key */
            state  = isKey;
            kStart = NULL;
            current->hash = 199999;
            break;

        case isComment:
            while (**ptr != '\n' && *length > 0)
            {
                ++(*ptr);
                --(*length);
            }
            state = endOfLine;
            break;

        case isValue:
            parseValue( ptr, length, current );
            break;

        case isChild:
            Log(LOG_INFO, "descend into child @ %p", current->child);
            parseNodes( ptr, length, current->child );
            break;
        }
    }
}


void parseMetadata(byte *metadata, size_t length)
{
    tNode * root = calloc(sizeof(tNode), sizeof(byte));
    gRootNode = root;

    DebugOut("\n_______________________________\n\n");
    DebugOut("%s", metadata);
    DebugOut("\n_______________________________\n\n");
    while (length > 0)
    {
        if (root != NULL)
        {
            parseNodes(&metadata, &length, root);
            root->next = calloc(sizeof(tNode), sizeof(byte));
            root = root->next;
        }
    }

    Log(LOG_DEBUG,"node dump:\n");
    dumpNodes(gRootNode,0);
}
