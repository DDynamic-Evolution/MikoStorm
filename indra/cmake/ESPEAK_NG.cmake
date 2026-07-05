include_guard()

if (USE_ESPEAK_NG)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(USE_MBROLA OFF CACHE BOOL "" FORCE)
  set(USE_LIBPCAUDIO OFF CACHE BOOL "" FORCE)
  set(USE_SPEECHPLAYER OFF CACHE BOOL "" FORCE)
  set(USE_KLATT ON CACHE BOOL "" FORCE)
  set(USE_ASYNC ON CACHE BOOL "" FORCE)
  set(COMPILE_INTONATIONS ON CACHE BOOL "" FORCE)
  set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)

  add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/../espeak-ng
    ${CMAKE_BINARY_DIR}/espeak-ng)

  add_library(ll::espeak-ng ALIAS espeak-ng)
  target_compile_definitions(espeak-ng INTERFACE LL_ESPEAK_NG=1)
  target_compile_options(espeak-ng PRIVATE -Wno-error)
  target_compile_options(espeak-ng-bin PRIVATE -Wno-error)

  set(ESPEAK_NG_DATA_DIR ${CMAKE_BINARY_DIR}/espeak-ng/espeak-ng-data
    CACHE PATH "Path to compiled espeak-ng-data directory")
else()
  set( USE_ESPEAK_NG "OFF")
endif ()
