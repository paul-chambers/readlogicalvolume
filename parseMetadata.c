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
        struct tNode * child;
        struct tNode * array;
        byte         * string;
        int64_t        number;
    };
} tNode;

static tNode * gRootNode = NULL;

byte * copyToString(byte * start, size_t length)
{
    byte * result = calloc(length + 1, 1); /* extra byte to guarantee a trailing null */
    if (result != NULL && length > 0)
    {
        memcpy(result, start, length);
    }
    return result;
}

tHash hashString(byte *ptr, size_t len)
{
    tHash hash = 199999; /* seed with largish prime */

    for (int i = len; i > 0; --i)
    {
        /* The 'djb2' string hash function */
        hash = (hash << 5) + hash + *ptr++;
    }
    return hash;
}

void setNodeKey(tNode * node, byte * keyStart, size_t length)
{
    node->key  = copyToString(keyStart, length);
    node->hash = hashString(keyStart, length);
}

void dumpNodes(tNode * node, int depth);

void dumpNode(tNode * node, int depth)
{
#ifdef optDebugOutput
    int i = sizeof(kIndent) - (depth * 4);

    Log(LOG_INFO, "%s node @ %p, next @ %p", &kIndent[i], node, node->next);
    Log(LOG_INFO, "%s key = \"%s\" (hash %x)", &kIndent[i], node->key, node->hash );
    switch (node->type)
    {
    case childNode:
        Log(LOG_INFO, "%s node type = child @ %p", &kIndent[i], node->child );
        dumpNodes(node->child, depth + 1);
        break;

    case arrayNode:
        Log(LOG_INFO, "%s node type = array", &kIndent[i] );
        dumpNodes(node->array, depth + 1);
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
        node = node->next;
    }
#endif
}


tNode * parseArray(byte **ptr, size_t *length)
{
    tNode * result;
    tNode * node;
    tNode * previous;
    enum { outsideString, insideString } state;
    byte  * start;

    state = outsideString;

    result = NULL;
    while (**ptr != '\n' && *length > 0)
    {
        DebugOut("%c", **ptr);

        switch (state)
        {
        case outsideString:
            switch (**ptr)
            {
            case '"':
                state = insideString;
                start = *ptr + 1;
                break;

            case ',':
                Log(LOG_INFO, "array separator");
                break;

            case ']':
                Log(LOG_INFO, "array terminator");
                return result;
            }
            break;

        case insideString:
            switch (**ptr)
            {
            case '"':
                state = outsideString;

                if (result == NULL)
                {
                    result = calloc(sizeof(tNode), 1);
                    previous = result;
                    node     = result;
                }
                else
                {
                    node->next = calloc(sizeof(tNode), 1);
                    previous = node;
                    node     = node->next;
                }

                setNodeKey(node, start, *ptr - start);
                node->type   = stringNode;
                node->string = node->key;
                break;

            case '\\':
                ++(*ptr);
                --(*length);
                break;
            }
            break;
        }

        ++(*ptr);
        --(*length);
    }
}

void parseValue(byte **ptr, size_t *length, tNode * node)
{
    byte  * start;
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
            node->array = parseArray(ptr, length);
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
                node->type   = numberNode;
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
    enum    { endOfLine, isKey, isComment, isValue, isArray, isChild, endOfChild } state;
    tNode   * previous;
    byte    * kStart;
    byte    * kEnd;

    current->type  = childNode;
    current->child = calloc(sizeof(tNode), 1);
    previous = current;
    current = current->child;

    state = endOfLine;
    while ( *length > 0 )
    {
        /* state-based processing */
        switch (state)
        {
        case endOfLine:
            /* each line begins with a key */
            kStart = NULL;
            state  = isKey;
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
            setNodeKey(current, kStart, kEnd - kStart + 1);

            parseValue( ptr, length, current );

            current->next = calloc(sizeof(tNode), 1);
            previous = current;
            current = current->next;
            break;

        case isChild:
            current->type = childNode;
            setNodeKey(current, kStart, kEnd - kStart + 1);

            parseNodes( ptr, length, current );

            current->next = calloc(sizeof(tNode), 1);
            previous = current;
            current = current->next;
            break;

        case endOfChild:
            /* discard the empty node at the end */
            previous->next = NULL;
            free(current);
            return;
        }

        switch (**ptr)
        {
        case ' ':
        case '\t':
            /* ignore whitespace */
            break;

        case '\n':
            state = endOfLine;
            break;

            /* if we hit a comment, consume up to next newline */
        case '#':
            state = isComment;
            break;

        case '=':
            state = isValue;
            break;

        case '{':
            state = isChild;
            break;

        case '}':
            state = endOfChild;
            break; /* back to the parent */

        default:
            if (state == isKey)
            {
                /* everything else is part of the key */
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
    }
}


void parseMetadata(byte *metadata, size_t length)
{
    tNode * root;

    DebugOut("\n_______________________________\n\n");
    DebugOut("%s", metadata);
    DebugOut("\n_______________________________\n\n");

    root = calloc(sizeof(tNode), 1);
    if (root != NULL)
    {
        gRootNode = root;
        setNodeKey(root, "root node", 9);
        parseNodes(&metadata, &length, root);

        Log(LOG_DEBUG,"######## node dump ########\n");
        dumpNodes(gRootNode,0);
    }
}
