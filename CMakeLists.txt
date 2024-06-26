cmake_minimum_required(VERSION 3.15)
project(100000PHOTOS)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the MSYS2 compiler paths
set(CMAKE_C_COMPILER "C:/msys64/ucrt64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/ucrt64/bin/g++.exe")

# Provide the path to the ZLIB library and include directory
set(ZLIB_LIBRARY "C:/msys64/ucrt64/lib/libz.a")
set(ZLIB_INCLUDE_DIR "C:/msys64/ucrt64/include")

# Provide the path to the OpenSSL library and include directory
set(OPENSSL_LIBRARY_CRYPTO "C:/msys64/ucrt64/lib/libcrypto.a")
set(OPENSSL_LIBRARY_SSL "C:/msys64/ucrt64/lib/libssl.a")
set(OPENSSL_INCLUDE_DIR "C:/msys64/ucrt64/include")

# Add your source file
add_executable(100000PHOTOS APP.cpp)

# Include directories for the header files
target_include_directories(100000PHOTOS PRIVATE ${ZLIB_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

# Link against the libraries
target_link_libraries(100000PHOTOS PRIVATE ${ZLIB_LIBRARY} ${OPENSSL_LIBRARY_CRYPTO} ${OPENSSL_LIBRARY_SSL})