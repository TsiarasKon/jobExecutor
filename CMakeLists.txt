cmake_minimum_required(VERSION 3.8)
project(jobExecutor)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c worker.c trie.c postinglist.c util.c lists.c lists.h)
add_executable(jobExecutor ${SOURCE_FILES})