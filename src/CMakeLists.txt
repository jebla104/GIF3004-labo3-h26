cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE $ENV{HOME}/arm-cross-comp-env/rpi-zero-w-toolchain.cmake)
project(SETR_TP3)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

SET_SOURCE_FILES_PROPERTIES(jpgd.cpp decodeur.c PROPERTIES LANGUAGE CXX )
set(SOURCE_DECODEUR allocateurMemoire.c commMemoirePartagee.c jpgd.cpp utils.c decodeur.c)
set(SOURCE_COMPOSITEUR allocateurMemoire.c commMemoirePartagee.c utils.c compositeur.c)
set(SOURCE_REDIMENSIONNEUR allocateurMemoire.c commMemoirePartagee.c utils.c redimensionneur.c)
set(SOURCE_FILTREUR allocateurMemoire.c commMemoirePartagee.c utils.c filtreur.c)
set(SOURCE_CONVERTISSEURGRIS allocateurMemoire.c commMemoirePartagee.c utils.c convertisseurgris.c)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -Ofast -funroll-loops -funsafe-math-optimizations -floop-block -flto")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -Ofast -funroll-loops -funsafe-math-optimizations -floop-block -flto")


SET(GCC_WARNING_FLAGS "-Wall -Wextra -Wpedantic -Wduplicated-cond -Wlogical-op -Wnull-dereference -Wshadow")
add_definitions("${GCC_WARNING_FLAGS}")

add_executable(decodeur ${SOURCE_DECODEUR})
target_link_libraries(decodeur rt Threads::Threads)

add_executable(compositeur ${SOURCE_COMPOSITEUR})
target_link_libraries(compositeur rt Threads::Threads m)

add_executable(redimensionneur ${SOURCE_REDIMENSIONNEUR})
target_link_libraries(redimensionneur rt Threads::Threads m)

add_executable(filtreur ${SOURCE_FILTREUR})
target_link_libraries(filtreur rt Threads::Threads m)

add_executable(convertisseur ${SOURCE_CONVERTISSEURGRIS})
target_link_libraries(convertisseur rt Threads::Threads m)
