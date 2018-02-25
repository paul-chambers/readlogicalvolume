//
// Created by Paul on 2/20/2018.
//

#ifndef READLOGICALVOLUME_H
#define READLOGICALVOLUME_H

typedef unsigned char byte;

#define optDebugOutput

#ifdef  optDebugOutput
#define Log( level, msg, ... )   \
        fprintf( stderr, "\n### %20s [%3d] " msg, __func__, __LINE__, ##__VA_ARGS__ )
#define DebugOut( msg, ... ) \
        fprintf(stderr, msg, ##__VA_ARGS__)
#else
#define Log( level, msg, ... )
#define DebugOut( msg, ... )
#endif

/* used for indenting debug output */
static const char kIndent[] = "                                                                          ";

#endif //READLOGICALVOLUME_H
