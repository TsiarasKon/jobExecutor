cmake_minimum_required(VERSION 3.8)
project(jobExecutor)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c interface.c trie.c postinglist.c pairingheap.c doc_struct.h)
add_executable(jobExecutor ${SOURCE_FILES})