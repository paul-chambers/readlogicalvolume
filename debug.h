//
// Created by Paul on 3/2/2018.
//

#ifndef READLOGICALVOLUME_DEBUG_H
#define READLOGICALVOLUME_DEBUG_H

#ifdef __GNUC__
  #define UNUSED(param) UNUSED_ ## param __attribute__((__unused__))
#else
  #define UNUSED(param) UNUSED_ ## param
#endif

/* used for pointer validation testing */
/* set using alloca() on entry to main() */
extern void * gStackTop;

/* scrutinise pointers a little harder than just checking they're not NULL */
#undef optDebugPointers

#ifdef  optDebugPointers
/* check it's not null, and below the upper reaches of the stack */
#define isValidPtr( ptr )   (ptr != NULL && ptr < gStackTop)
/* check it's not NULL, is longword aligned, and below the current value of brk (top of C heap) */
#define isHeapPtr( ptr )    (ptr != NULL && (void *)ptr < sbrk(0))
#define LogIfBadPtr( ptr )  { if (!isValidPtr(ptr)) Log(LOG_DEBUG,"bad pointer %p", ptr); }
#else
#define isValidPtr( ptr )   ((ptr) != NULL)
#define isHeapPtr( ptr )    ((ptr) != NULL)
#define LogIfBadPtr( ptr )  {}
#endif

/* enable debugging output - pretty verbose */
#define optDebugOutput

#ifdef  optDebugOutput
#define Log( level, msg, ... )  fprintf( stderr, "### %15s [%3d] " msg "\n", __func__, __LINE__, ##__VA_ARGS__ )
#define DebugOut( msg, ... )    fprintf(stderr, msg, ##__VA_ARGS__)
#else
#define Log( level, msg, ... )
#define DebugOut( msg, ... )
#endif

/* used for indenting debug output */
extern const char kIndent[];

void dumpMemory(char * ptr, size_t length);

#endif //READLOGICALVOLUME_DEBUG_H