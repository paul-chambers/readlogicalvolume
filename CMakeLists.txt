cmake_minimum_required(VERSION 3.9)
project(readlogicalvolume)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG -fsanitize=address)

include_directories(.)
include_directories( SYSTEM /usr/include )

add_executable( readlogicalvolume
                readlogicalvolume.c readlogicalvolume.h
                debug.c debug.h
                gpt.h lvm.h
                readaccess.c readaccess.h
                parseMetadata.c parseMetadata.h
                stringHash.c stringHash.h)
