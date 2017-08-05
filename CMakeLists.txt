cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
project(d0_blind_id C)

option(ENABLE_RIJNDAEL "Enable build of the d0_rijndael library" ON)

set(SOURCE_COMMON
        d0.c d0.h
        )

set(BLIND_ID_SOURCES
        d0_bignum.h
        d0_blind_id.c d0_blind_id.h
        d0_iobuf.c d0_iobuf.h
        sha2.c sha2.h
        )

set(RIJNDAEL_SOURCES
        d0_rijndael.c d0_rijndael.h
        )

find_package(OpenSSL REQUIRED)
list(APPEND BLIND_ID_SOURCES d0_bignum-openssl.c)
set(BIGNUM_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS})
set(BIGNUM_LIBRARIES ${OPENSSL_LIBRARIES})

add_library(d0_blind_id SHARED ${BLIND_ID_SOURCES} ${SOURCE_COMMON})
target_include_directories(d0_blind_id PRIVATE ${BIGNUM_INCLUDE_DIRS})
target_link_libraries(d0_blind_id PRIVATE ${BIGNUM_LIBRARIES})

if (ENABLE_RIJNDAEL)
    add_library(d0_rijndael SHARED ${RIJNDAEL_SOURCES} ${SOURCE_COMMON})
endif ()

add_executable(d0_blind_bench main.c)
target_link_libraries(d0_blind_bench d0_blind_id)
