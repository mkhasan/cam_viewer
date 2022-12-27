# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.10.2)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds it for you.
# Gradle automatically packages shared libraries with your APK.

#include_directories($ENV{HOME}/extension/RailRobot/FFmpeg/FFmpeg-Development-Kit-master/ffmpeg)
#include_directories($ENV{HOME}/external/lib/android/libyuv-android-master/jni/include)
include_directories($ENV{HOME}/extension/RailRobot/FFmpeg/FFmpeg-Development-Kit-master/ffmpeg)
include_directories($ENV{HOME}/extension/libyuv-android/jni/include)

set (JNI_DIR "myLibs")

set (TEST_T "$ENV{HOME}/external/lib/libyuv-android-master/jni/include")
message ("Home is ${TEST_T}")
add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
             src/main/cpp/native-lib.cpp src/main/cpp/queue.c src/main/cpp/player.c src/main/cpp/decode.c src/main/cpp/convert.cpp)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.




find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )





add_library( avutil
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       avutil


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libavutil.so)

add_library( swresample
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       swresample


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libswresample.so)



add_library( avcodec
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       avcodec


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libavcodec.so)


add_library( avformat
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       avformat


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libavformat.so)



add_library( swscale
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       swscale


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libswscale.so)


add_library( avfilter
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       avfilter


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libavfilter.so)


add_library( avdevice
             SHARED
             IMPORTED )


set_target_properties( # Specifies the target library.
                       avdevice


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libavdevice.so)


add_library( yuv
             SHARED
             IMPORTED )

set_target_properties( # Specifies the target library.
                       yuv


                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       #imported-lib/src/${ANDROID_ABI}/libimported-lib.so
                      ${CMAKE_SOURCE_DIR}/src/main/${JNI_DIR}/${ANDROID_ABI}/libyuv.so)


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib



                       avutil

                       swresample

                       avcodec

                       avformat

                       swscale

                       avfilter

                       avdevice

                       android

                       yuv


                       # Links the target library to the log library
                       # included in the NDK.

                       ${log-lib} )


