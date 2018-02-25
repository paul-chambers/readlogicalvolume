cmake_minimum_required(VERSION 3.9)
project(readlogicalvolume)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable( readlogicalvolume
                readlogicalvolume.c
                readlogicalvolume.h
                readaccess.c
                readaccess.h
                parseMetadata.c
                parseMetadata.h
                gpt.h
                lvm.h )
