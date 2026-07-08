# -*- cmake -*-

project(llaudio)

include(00-Common)
include(LLAudio)
include(FMODSTUDIO)
include(OPENAL)
include(LLCommon)
include(LLWindow)
include(CURL)
include(OpenSSL)
include(NGHTTP2)

set(llaudio_SOURCE_FILES
    llaudioengine.cpp
    lllistener.cpp
    llaudiodecodemgr.cpp
    llvorbisencode.cpp
    )

set(llaudio_HEADER_FILES
    CMakeLists.txt

    llaudioengine.h
    lllistener.h
    llaudiodecodemgr.h
    llvorbisencode.h
    llwindgen.h
    )

if (TARGET ll::fmodstudio)
    list(APPEND llaudio_SOURCE_FILES
         llaudioengine_fmodstudio.cpp
         lllistener_fmodstudio.cpp
         llstreamingaudio_fmodstudio.cpp
         llmultichanneldownmix.cpp
         llstereoupmix.cpp
         lllitehrtfdsp.cpp
         llfftconvolver.cpp
         llirloader.cpp
         llvenuereverbdsp.cpp
          )
    
    find_path(OPUS_INCLUDE_DIR "opus.h" PATHS
        ${CMAKE_BINARY_DIR}/packages/include
        ${CMAKE_BINARY_DIR}/packages/include/webrtc/third_party/opus/src/include
        NO_DEFAULT_PATH)
    find_library(OPUS_LIBRARY opus PATHS
        ${CMAKE_BINARY_DIR}/packages/lib/release
        ${CMAKE_BINARY_DIR}/packages/lib/debug
        NO_DEFAULT_PATH)
    
    if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
        list(APPEND llaudio_SOURCE_FILES fmod_codec_ogg.cpp)
        target_compile_definitions(llaudio PRIVATE LL_OPUS_FOUND=1)
    else()
        message(STATUS "opus not found - excluding fmod_codec_ogg.cpp")
    endif()

     list(APPEND llaudio_HEADER_FILES
          llaudioengine_fmodstudio.h
          lllistener_fmodstudio.h
          llstreamingaudio_fmodstudio.h
          llmultichanneldownmix.h
          llstereoupmix.h
          lllitehrtfdsp.h
          llfftconvolver.h
          llirloader.h
          llvenuereverbdsp.h
          )
    
    if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
        list(APPEND llaudio_HEADER_FILES fmod_codec_ogg.h)
    endif()

    if (USE_3D_STREAM)
        list(APPEND llaudio_SOURCE_FILES
             llpositionalstream.cpp
             llpositionalstreamstereo.cpp
             llpositionalstreammulti.cpp
             llstream3durlresolve.cpp
             llmetadatatags.cpp
             )

        list(APPEND llaudio_HEADER_FILES
             llpositionalstream.h
             llpositionalstreamstereo.h
             llpositionalstreammulti.h
             llstream3durlresolve.h
             llmetadatatags.h
             )
    endif ()

    if (LL_DULLAHAN_AUDIO_CALLBACK)
        list(APPEND llaudio_SOURCE_FILES llmediaaudiostream.cpp)
        list(APPEND llaudio_HEADER_FILES llmediaaudiostream.h)
    endif ()
endif ()

if (TARGET ll::openal)
  list(APPEND llaudio_SOURCE_FILES
    llaudioengine_openal.cpp
    lllistener_openal.cpp
    )
  
  list(APPEND llaudio_HEADER_FILES
    llaudioengine_openal.h
    lllistener_openal.h
    )
endif ()

list(APPEND llaudio_SOURCE_FILES ${llaudio_HEADER_FILES})

add_library (llaudio ${llaudio_SOURCE_FILES})
target_include_directories( llaudio  INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries( llaudio
        llcommon
        llmath
        llmessage
        llfilesystem
        ll::vorbis
    )

if( TARGET ll::openal )
    target_link_libraries( llaudio ll::openal )
endif()
if( TARGET ll::fmodstudio )
    target_link_libraries( llaudio ll::fmodstudio )
    if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
        target_include_directories(llaudio PRIVATE ${OPUS_INCLUDE_DIR})
        target_link_libraries(llaudio ${OPUS_LIBRARY})
    else()
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(OPUS opus)
            if(OPUS_FOUND)
                target_include_directories(llaudio PRIVATE ${OPUS_INCLUDE_DIRS})
                target_link_libraries(llaudio ${OPUS_LIBRARIES})
            endif()
        endif()
    endif()
endif()
if( TARGET ll::libcurl )
    target_link_libraries( llaudio ll::libcurl ll::openssl ll::nghttp2 )
endif()
