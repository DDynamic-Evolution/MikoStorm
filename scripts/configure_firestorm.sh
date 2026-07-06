#!/bin/bash

###
### Constants
###

TRUE=0  # Map the shell's idea of truth to a variable for better documentation
FALSE=1

# Default AUTOBUILD_ADDRSIZE if not set by autobuild
if [ -z "$AUTOBUILD_ADDRSIZE" ] ; then
    AUTOBUILD_ADDRSIZE=`getconf LONG_BIT`
fi

#echo "DEBUG ARGS: $@"
#echo "DEBUG `pwd`"

# args ../indra
#                  <string>-DCMAKE_BUILD_TYPE:STRING=Release</string>
#                  <string>-DADDRESS_SIZE:STRING=32</string>
#                  <string>-DROOT_PROJECT_NAME:STRING=SecondLife</string>
#                  <string>-DUSE_KDU=TRUE</string>
#                  <string>-DFMODSTUDIO:BOOL=ON</string>
#                  <string>-DOPENSIM:BOOL=ON</string>
#                  <string>-DUSE_AVX_OPTIMIZATION:BOOL=OFF</string>
#                  <string>-DUSE_AVX2_OPTIMIZATION:BOOL=OFF</string>
#                  <string>-DLL_TESTS:BOOL=OFF</string>
#                  <string>-DPACKAGE:BOOL=OFF></string>
#                  <string>-DUSE_VELOPACK:BOOL=OFF></string>


###
### Global Variables
###

WANTS_CLEAN=$FALSE
WANTS_CONFIG=$FALSE
WANTS_PACKAGE=$FALSE
WANTS_VELOPACK=$FALSE
WANTS_VERSION=$FALSE
WANTS_KDU=$FALSE
WANTS_FMODSTUDIO=$FALSE
WANTS_OPENAL=$FALSE
WANTS_OPENSIM=$TRUE
WANTS_SINGLEGRID=$FALSE
WANTS_HAVOK=$FALSE
WANTS_AVX=$FALSE
WANTS_AVX2=$FALSE
WANTS_TESTBUILD=$FALSE
WANTS_TRACY=$FALSE
WANTS_BUILD=$FALSE
WANTS_CRASHREPORTING=$FALSE
WANTS_CACHE=$FALSE
TARGET_PLATFORM="linux" # linux, windows
BTYPE="Release"
CHANNEL="" # will be overwritten later with platform-specific values unless manually specified.
LL_ARGS_PASSTHRU=""
JOBS="0"
WANTS_NINJA=$FALSE
WANTS_VSCODE=$FALSE
WANTS_3D_STREAM=$TRUE
WANTS_ESPEAK_NG=$TRUE
WANTS_MCP=$TRUE
TESTBUILD_PERIOD="0"
SINGLEGRID_URI=""

###
### Helper Functions
###

showUsage()
{
    echo
    echo "Usage: "
    echo "========================"
    echo
    echo "  --clean                  : Remove past builds & configuration"
    echo "  --config                 : Generate a new architecture-specific config"
    echo "  --build                  : Build MikoStorm"
    echo "  --version                : Update version number"
    echo "  --chan  [Release|Beta|Private]   : Private is the default"
    echo "  --btype [Release|RelWithDebInfo] : Release is default, whether to use symbols"
    echo "  --kdu                    : Build with KDU"
    echo "  --package                : Build installer"
    echo "  --velopack               : Build with velopack (Overrides --package)"
    echo "  --no-package             : Build without installer (Overrides --package)"
    echo "  --fmodstudio             : Build with FMOD Studio"
    echo "  --openal                 : Build with OpenAL"
  echo "  --3dstream               : Build with 3D Stream (PandaView) support (default)"
  echo "  --no-3dstream            : Build without 3D Stream (PandaView) support"
  echo "  --espeak                 : Build with eSpeak-NG TTS support (default)"
  echo "  --no-espeak              : Build without eSpeak-NG TTS support"
  echo "  --mcp                    : Build with MCP support (default)"
  echo "  --no-mcp                 : Build without MCP support"
    echo "  --opensim                : Build with OpenSim support (Disables Havok features)"
    echo "  --no-opensim             : Build without OpenSim support (Overrides --opensim)"
    echo "  --singlegrid <login_uri> : Build for single grid usage (Requires --opensim)"
    echo "  --havok                  : Build with Havok support (Disables OpenSim support)"
    echo "  --avx                    : Build with Advanced Vector Extensions"
    echo "  --avx2                   : Build with Advanced Vector Extensions 2"
    echo "  --tracy                  : Build with Tracy Profiler support"
    echo "  --crashreporting         : Build with crash reporting enabled (Windows only)"
    echo "  --testbuild <days>       : Create time-limited test build (build date + <days>)"
    echo "  --platform <platform>    : Build for specified platform (linux | windows)"
    echo "  --jobs <num>             : Build with <num> jobs in parallel (Linux only)"
    echo "  --ninja                  : Build using Ninja"
    echo "  --vscode                 : Exports compile commands for VSCode (Linux only)"
    echo "  --compiler-cache         : Try to detect and use compiler cache (needs also --ninja for Windows)"
    echo
    echo "All arguments not in the above list will be passed through to LL's configure/build."
    echo
}

getArgs()
# $* = the options passed in from main
{
    if [ $# -gt 0 ]; then
      while       getoptex "clean build config version package velopack no-package fmodstudio openal ninja vscode compiler-cache jobs: platform: kdu opensim no-opensim 3dstream no-3dstream espeak no-espeak mcp no-mcp singlegrid: havok avx avx2 tracy crashreporting testbuild: help chan: btype:" "$@" ; do

          #ensure options are valid
          if [  -z "$OPTOPT"  ] ; then
                showUsage
                exit 1
          fi

          case "$OPTOPT" in
          clean)          WANTS_CLEAN=$TRUE;;
          config)         WANTS_CONFIG=$TRUE;;
          espeak)         WANTS_ESPEAK_NG=$TRUE;;
          no-espeak)      WANTS_ESPEAK_NG=$FALSE;;
          mcp)            WANTS_MCP=$TRUE;;
          no-mcp)         WANTS_MCP=$FALSE;;
          version)        WANTS_VERSION=$TRUE;;
          chan)           CHANNEL="$OPTARG";;
          btype)          if [ \( "$OPTARG" == "Release" \) -o \( "$OPTARG" == "RelWithDebInfo" \) ] ; then
                            BTYPE="$OPTARG"
                          fi
                          ;;
          kdu)            WANTS_KDU=$TRUE;;
          fmodstudio)     WANTS_FMODSTUDIO=$TRUE;;
          openal)         WANTS_OPENAL=$TRUE;;
          3dstream)       WANTS_3D_STREAM=$TRUE;;
          no-3dstream)    WANTS_3D_STREAM=$FALSE;;
          opensim)        WANTS_OPENSIM=$TRUE;;
          no-opensim)     WANTS_OPENSIM=$FALSE;;
          singlegrid)     WANTS_SINGLEGRID=$TRUE
                          SINGLEGRID_URI="$OPTARG"
                          ;;
          havok)          WANTS_HAVOK=$TRUE
                          WANTS_OPENSIM=$FALSE
                          ;;
          avx)            WANTS_AVX=$TRUE;;
          avx2)           WANTS_AVX2=$TRUE;;
          tracy)          WANTS_TRACY=$TRUE;;
          crashreporting) WANTS_CRASHREPORTING=$TRUE;;
          testbuild)      WANTS_TESTBUILD=$TRUE
                          TESTBUILD_PERIOD="$OPTARG"
                          ;;
          package)        WANTS_PACKAGE=$TRUE;;
          velopack)       WANTS_PACKAGE=$TRUE
                          WANTS_VELOPACK=$TRUE;;
          no-package)     WANTS_PACKAGE=$FALSE;;
          build)          WANTS_BUILD=$TRUE;;
          platform)       TARGET_PLATFORM="$OPTARG";;
          jobs)           JOBS="$OPTARG";;
          ninja)          WANTS_NINJA=$TRUE;;
          vscode)         WANTS_VSCODE=$TRUE;;
          compiler-cache) WANTS_CACHE=$TRUE;;

          help)           showUsage && exit 0;;

          -*)             showUsage && exit 1;;
          *)              showUsage && exit 1;;
          esac

      done
      shift $[OPTIND-1]
      if [ $OPTIND -le 1 ] ; then
          showUsage && exit 1
      fi
    fi

    if [ $WANTS_CLEAN -ne $TRUE ] && [ $WANTS_CONFIG -ne $TRUE ] && \
       [ $WANTS_VERSION -ne $TRUE ] && [ $WANTS_BUILD -ne $TRUE ] && \
       [ $WANTS_PACKAGE -ne $TRUE ] ; then
        # the user didn't say what to do, so assume he wants to do a basic rebuild
        WANTS_CONFIG=$TRUE
        WANTS_BUILD=$TRUE
        WANTS_VERSION=$TRUE
    fi

    LOG="`pwd`/logs/build_$TARGET_PLATFORM.log"
    if [ -r "$LOG" ] ; then
        rm -f `basename "$LOG"`/* #(remove old logfiles)
    fi
}

function b2a()
{
  if [ $1 -eq $TRUE ] ; then
    echo "true"
  else
    echo "false"
  fi
}

function getoptex()
{
  let $# || return 1
  local optlist="${1#;}"
  let OPTIND || OPTIND=1
  [ $OPTIND -lt $# ] || return 1
  shift $OPTIND
  if [ "$1" != "-" -a "$1" != "${1#-}" ]
  then OPTIND=$[OPTIND+1]; if [ "$1" != "--" ]
  then
        local o
        o="-${1#-$OPTOFS}"
        for opt in ${optlist#;}
        do
          OPTOPT="${opt%[;.:]}"
          unset OPTARG
          local opttype="${opt##*[^;:.]}"
          [ -z "$opttype" ] && opttype=";"
          if [ ${#OPTOPT} -gt 1 ]
          then # long-named option
                case $o in
                  "--$OPTOPT")
                        if [ "$opttype" != ":" ]; then return 0; fi
                        OPTARG="$2"
                        if [ -z "$OPTARG" ];
                        then # error: must have an agrument
                          let OPTERR && echo "$0: error: $OPTOPT must have an argument" >&2
                          OPTARG="$OPTOPT";
                          OPTOPT="?"
                          return 1;
                        fi
                        OPTIND=$[OPTIND+1] # skip option's argument
                        return 0
                  ;;
                  "--$OPTOPT="*)
                        if [ "$opttype" = ";" ];
                        then  # error: must not have arguments
                          let OPTERR && echo "$0: error: $OPTOPT must not have arguments" >&2
                          OPTARG="$OPTOPT"
                          OPTOPT="?"
                          return 1
                        fi
                        OPTARG=${o#"--$OPTOPT="}
                        return 0
                  ;;
                esac
          else # short-named option
                case "$o" in
                  "-$OPTOPT")
                        unset OPTOFS
                        [ "$opttype" != ":" ] && return 0
                        OPTARG="$2"
                        if [ -z "$OPTARG" ]
                        then
                          echo "$0: error: -$OPTOPT must have an argument" >&2
                          OPTARG="$OPTOPT"
                          OPTOPT="?"
                          return 1
                        fi
                        OPTIND=$[OPTIND+1] # skip option's argument
                        return 0
                  ;;
                  "-$OPTOPT"*)
                        if [ $opttype = ";" ]
                        then # an option with no argument is in a chain of options
                          OPTOFS="$OPTOFS?" # move to the next option in the chain
                          OPTIND=$[OPTIND-1] # the chain still has other options
                          return 0
                        else
                          unset OPTOFS
                          OPTARG="${o#-$OPTOPT}"
                          return 0
                        fi
                  ;;
                esac
          fi
        done
        #echo "$0: error: invalid option: $o"
    LL_ARGS_PASSTHRU="$LL_ARGS_PASSTHRU $o"
    return 0
    #showUsage
    #exit 1
  fi; fi
  OPTOPT="?"
  unset OPTARG
  return 1
}

function optlistex
{
  local l="$1"
  local m # mask
  local r # to store result
  while [ ${#m} -lt $[${#l}-1] ]; do m="$m?"; done # create a "???..." mask
  while [ -n "$l" ]
  do
        r="${r:+"$r "}${l%$m}" # append the first character of $l to $r
        l="${l#?}" # cut the first charecter from $l
        m="${m#?}"  # cut one "?" sign from m
        if [ -n "${l%%[^:.;]*}" ]
        then # a special character (";", ".", or ":") was found
          r="$r${l%$m}" # append it to $r
          l="${l#?}" # cut the special character from l
          m="${m#?}"  # cut one more "?" sign
        fi
  done
  echo $r
}

function getopt()
{
  local optlist=`optlistex "$1"`
  shift
  getoptex "$optlist" "$@"
  return $?
}

###
###  Main Logic
###


getArgs $*
if [ ! -d `dirname "$LOG"` ] ; then
        mkdir -p `dirname "$LOG"`
fi

echo -e "configure_firestorm.sh" > "$LOG"
echo -e "       PLATFORM: $TARGET_PLATFORM"                                    | tee -a "$LOG"
echo -e "            KDU: `b2a $WANTS_KDU`"                                    | tee -a "$LOG"
echo -e "     FMODSTUDIO: `b2a $WANTS_FMODSTUDIO`"                             | tee -a "$LOG"
echo -e "         OPENAL: `b2a $WANTS_OPENAL`"                                 | tee -a "$LOG"
echo -e "        OPENSIM: `b2a $WANTS_OPENSIM`"                                | tee -a "$LOG"
if [ $WANTS_SINGLEGRID -eq $TRUE ] ; then
    echo -e "     SINGLEGRID: `b2a $WANTS_SINGLEGRID` ($SINGLEGRID_URI)"       | tee -a "$LOG"
else
    echo -e "     SINGLEGRID: `b2a $WANTS_SINGLEGRID`"                         | tee -a "$LOG"
fi
echo -e "          HAVOK: `b2a $WANTS_HAVOK`"                                  | tee -a "$LOG"
echo -e "            AVX: `b2a $WANTS_AVX`"                                    | tee -a "$LOG"
echo -e "           AVX2: `b2a $WANTS_AVX2`"                                   | tee -a "$LOG"
echo -e "          TRACY: `b2a $WANTS_TRACY`"                                  | tee -a "$LOG"
echo -e "      3D STREAM: `b2a $WANTS_3D_STREAM`"                              | tee -a "$LOG"
echo -e "     ESPEAK NG: `b2a $WANTS_ESPEAK_NG`"                              | tee -a "$LOG"
echo -e "          MCP: `b2a $WANTS_MCP`"                                      | tee -a "$LOG"
echo -e " CRASHREPORTING: `b2a $WANTS_CRASHREPORTING`"                         | tee -a "$LOG"
if [ $WANTS_TESTBUILD -eq $TRUE ] ; then
    echo -e "      TESTBUILD: `b2a $WANTS_TESTBUILD` ($TESTBUILD_PERIOD days)" | tee -a "$LOG"
else
    echo -e "      TESTBUILD: `b2a $WANTS_TESTBUILD`"                          | tee -a "$LOG"
fi
echo -e "        PACKAGE: `b2a $WANTS_PACKAGE`"                                | tee -a "$LOG"
echo -e "       VELOPACK: `b2a $WANTS_VELOPACK`"                               | tee -a "$LOG"
echo -e "          CLEAN: `b2a $WANTS_CLEAN`"                                  | tee -a "$LOG"
echo -e "          BUILD: `b2a $WANTS_BUILD`"                                  | tee -a "$LOG"
echo -e "         CONFIG: `b2a $WANTS_CONFIG`"                                 | tee -a "$LOG"
echo -e "          NINJA: `b2a $WANTS_NINJA`"                                  | tee -a "$LOG"
echo -e "         VSCODE: `b2a $WANTS_VSCODE`"                                 | tee -a "$LOG"
echo -e " COMPILER CACHE: `b2a $WANTS_CACHE`"                                  | tee -a "$LOG"
echo -e "       PASSTHRU: $LL_ARGS_PASSTHRU"                                   | tee -a "$LOG"
echo -e "          BTYPE: $BTYPE"                                              | tee -a "$LOG"
if [ $TARGET_PLATFORM == "linux" ] ; then
    echo -e "           JOBS: $JOBS"                                           | tee -a "$LOG"
fi
echo -e "       Logging to $LOG"

if [ $TARGET_PLATFORM == "windows" ]
then
    if [ -z "${AUTOBUILD_VSVER}" ]
    then
        echo "AUTOBUILD_VSVER not set, this can lead to Autobuild picking a higher VS version than desired."
        echo "If you see this happen you should set the variable to e.g. 150 for Visual Studio 2017."
    fi

    echo "Setting environment variables for Visual Studio..."
    if [ "$OSTYPE" = "cygwin" ] ; then
        export AUTOBUILD_EXEC="$(cygpath -u $AUTOBUILD)"
    fi
    if [ -z "$AUTOBUILD_EXEC" ]
    then
        export AUTOBUILD_EXEC=`which autobuild`
    fi

    # load autobuild provided shell functions and variables
    eval "$("$AUTOBUILD_EXEC" source_environment)"
    # vsvars is needed for determing path to VS runtime redist files in Copy3rdPartyLibs.cmake
    load_vsvars
fi

if [ -z "$AUTOBUILD_VARIABLES_FILE" ]
then
    echo "AUTOBUILD_VARIABLES_FILE not set."
    echo "In order to run autobuild it needs to be set to point to a correct variables file."
    exit 1
fi

if [ $TARGET_PLATFORM == "windows" ] ; then
    FIND=/usr/bin/find
else
    FIND=find
fi


CHANNEL_SIMPLE="$CHANNEL"
if [ -z $CHANNEL ] ; then
    CHANNEL="private-`hostname`"
else
    CHANNEL=`echo $CHANNEL | sed -e "s/[^a-zA-Z0-9\-]*//g"` # strip out difficult characters from channel
fi
CHANNEL="MikoStorm-$CHANNEL"

if [ \( $WANTS_CLEAN -eq $TRUE \) -a \( $WANTS_BUILD -eq $FALSE \) ] ; then
    echo "Cleaning $TARGET_PLATFORM...."

    if [ $TARGET_PLATFORM == "windows" ] ; then
        rm -rf build-vc${AUTOBUILD_VSVER:-150}-${AUTOBUILD_ADDRSIZE}
        mkdir -p build-vc${AUTOBUILD_VSVER:-150}-${AUTOBUILD_ADDRSIZE}/logs

    elif [ $TARGET_PLATFORM == "linux" ] ; then
        if [ "${AUTOBUILD_ADDRSIZE}" == "64" ]
        then
           rm -rf build-linux-x86_64/*
           mkdir -p build-linux-x86_64/logs
        else
           rm -rf build-linux-i686/*
           mkdir -p build-linux-i686/logs
        fi
    fi
fi

if [ \( $WANTS_VERSION -eq $TRUE \) -o \( $WANTS_CONFIG -eq $TRUE \) ] ; then
    echo "Versioning..."
    fullVer=`cat indra/newview/VIEWER_VERSION_FS.txt`
    gitHash=`git describe --always --exclude '*'`
    echo "Channel: ${CHANNEL}"
    echo "Version: ${fullVer} [${gitHash}]"
    GITHASH=-DVIEWER_VERSION_GITHASH=\"${gitHash}\"
fi

if [ $WANTS_CONFIG -eq $TRUE ] ; then
    echo "Configuring $TARGET_PLATFORM..."

    if [ $WANTS_KDU -eq $TRUE ] ; then
        KDU="-DUSE_KDU:BOOL=ON"
    else
        KDU="-DUSE_KDU:BOOL=OFF"
    fi
    if [ $WANTS_FMODSTUDIO -eq $TRUE ] ; then
        FMODSTUDIO="-DUSE_FMODSTUDIO:BOOL=ON"
    else
        FMODSTUDIO="-DUSE_FMODSTUDIO:BOOL=OFF"
    fi
    if [ $WANTS_OPENAL -eq $TRUE ] ; then
        OPENAL="-DOPENAL:BOOL=ON"
    else
        OPENAL="-DOPENAL:BOOL=OFF"
    fi
    if [ $WANTS_OPENSIM -eq $TRUE ] ; then
        OPENSIM="-DOPENSIM:BOOL=ON"
    else
        OPENSIM="-DOPENSIM:BOOL=OFF"
    fi
    if [ $WANTS_SINGLEGRID -eq $TRUE ] ; then
        SINGLEGRID="-DSINGLEGRID:BOOL=ON -DSINGLEGRID_URI:STRING=$SINGLEGRID_URI"
    else
        SINGLEGRID="-DSINGLEGRID:BOOL=OFF"
    fi
    if [ $WANTS_HAVOK -eq $TRUE ] ; then
        HAVOK="-DHAVOK_TPV:BOOL=ON"
    else
        HAVOK="-DHAVOK_TPV:BOOL=OFF"
    fi
    if [ $WANTS_AVX -eq $TRUE ] ; then
        AVX_OPTIMIZATION="-DUSE_AVX_OPTIMIZATION:BOOL=ON"
    else
        AVX_OPTIMIZATION="-DUSE_AVX_OPTIMIZATION:BOOL=OFF"
    fi
    if [ $WANTS_AVX2 -eq $TRUE ] ; then
        AVX2_OPTIMIZATION="-DUSE_AVX2_OPTIMIZATION:BOOL=ON"
    else
        AVX2_OPTIMIZATION="-DUSE_AVX2_OPTIMIZATION:BOOL=OFF"
    fi
    if [ $WANTS_TRACY -eq $TRUE ] ; then
        TRACY_PROFILER="-DUSE_TRACY:BOOL=ON"
    else
        TRACY_PROFILER="-DUSE_TRACY:BOOL=OFF"
    fi
    if [ $WANTS_3D_STREAM -eq $TRUE ] ; then
        USE_3D_STREAM="-DUSE_3D_STREAM:BOOL=ON"
    else
        USE_3D_STREAM="-DUSE_3D_STREAM:BOOL=OFF"
    fi
    if [ $WANTS_ESPEAK_NG -eq $TRUE ] ; then
        ESPEAK_NG="-DUSE_ESPEAK_NG:BOOL=ON"
    else
        ESPEAK_NG="-DUSE_ESPEAK_NG:BOOL=OFF"
    fi
    if [ $WANTS_MCP -eq $TRUE ] ; then
        USE_MCP="-DUSE_MCP:BOOL=ON"
    else
        USE_MCP="-DUSE_MCP:BOOL=OFF"
    fi

    if [ $WANTS_TESTBUILD -eq $TRUE ] ; then
        TESTBUILD="-DTESTBUILD:BOOL=ON -DTESTBUILDPERIOD:STRING=$TESTBUILD_PERIOD"
    else
        TESTBUILD="-DTESTBUILD:BOOL=OFF"
    fi
    if [ $WANTS_PACKAGE -eq $TRUE ] ; then
        PACKAGE="-DPACKAGE:BOOL=ON"
        # Also delete easy-to-copy resource files, insuring that we properly refresh resoures from the source tree
        if [ -d skins ] ; then
            echo "Removing select previously packaged resources, they will refresh at build time"
            for subdir in skins app_settings fs_resources ; do
                for resourcedir in `$FIND . -type d -name $subdir` ; do
                    rm -rf $resourcedir ;
                done
            done
        fi
        if [ $WANTS_VELOPACK -eq $TRUE ] ; then
            VELOPACK="-DUSE_VELOPACK:BOOL=ON"
        else
            VELOPACK="-DUSE_VELOPACK:BOOL=OFF"
        fi
    else
        PACKAGE="-DPACKAGE:BOOL=OFF"
        VELOPACK="-DUSE_VELOPACK:BOOL=OFF"
    fi
    if [ $WANTS_CRASHREPORTING -eq $TRUE ] ; then
        if [ $TARGET_PLATFORM == "windows" ] ; then
            BUILD_DIR=`cygpath -w $(pwd)`
        else
            BUILD_DIR=`pwd`
        fi
        # This name is consumed by indra/newview/CMakeLists.txt
        if [ $TARGET_PLATFORM == "linux" ] ; then
            VIEWER_SYMBOL_FILE="${BUILD_DIR}/newview/mikostorm-symbols-${TARGET_PLATFORM}-${AUTOBUILD_ADDRSIZE}.tar.bz2"
        else
            VIEWER_SYMBOL_FILE="${BUILD_DIR}/newview/$BTYPE/mikostorm-symbols-${TARGET_PLATFORM}-${AUTOBUILD_ADDRSIZE}.tar.bz2"
        fi
        CRASH_REPORTING="-DRELEASE_CRASH_REPORTING=ON"
        if [ ! -z $CHANNEL_SIMPLE ]
        then
            CRASH_REPORTING="$CRASH_REPORTING -DUSE_BUGSPLAT=On -DBUGSPLAT_DB=mikostorm_"`echo $CHANNEL_SIMPLE | tr [:upper:] [:lower:] | sed -e 's/x64//' | sed 's/[^A-Za-z0-9]//g'`
        fi
    else
        CRASH_REPORTING="-DRELEASE_CRASH_REPORTING:BOOL=OFF"
    fi

    CHANNEL="-DVIEWER_CHANNEL:STRING=$CHANNEL"

    #make sure log directory exists.
    if [ ! -d "logs" ] ; then
        echo "Creating logging dir `pwd`/logs"
        mkdir -p "logs"
    fi

    CMAKE_ARCH=""

    if [ \( $TARGET_PLATFORM == "linux" \) ] ; then
        TARGET="Unix Makefiles"
        if [ $WANTS_VSCODE -eq $TRUE ] ; then
            VSCODE_FLAGS="-DCMAKE_EXPORT_COMPILE_COMMANDS=On"
            ROOT_DIR=$(dirname $(dirname $(readlink -f $0)))

            if [ -d ${ROOT_DIR}/vscode_template/ ]
            then
                test -d "${ROOT_DIR}/.vscode" || mkdir "${ROOT_DIR}/.vscode"
                cp -n "${ROOT_DIR}/vscode_template/"* "${ROOT_DIR}/.vscode/"
            fi
        fi
    elif [ \( $TARGET_PLATFORM == "windows" \) ] ; then
        TARGET="${AUTOBUILD_WIN_CMAKE_GEN}"
        if [ $AUTOBUILD_ADDRSIZE == 32 ]
        then
            CMAKE_ARCH="-A Win32"
        fi
        UNATTENDED="-DUNATTENDED=ON"
    fi

    if [ $WANTS_NINJA -eq $TRUE ] ; then
        TARGET="Ninja"
    fi

    CACHE_OPT=""

    if [ $WANTS_CACHE -eq $TRUE ]
    then
        if [ `which ccache 2>/dev/null` ]
        then
            echo "Found ccache"
            CACHE_OPT="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
        fi
        if [ `which buildcache 2>/dev/null` ]
        then
            echo "Found buildcache"
            CACHE_OPT="-DCMAKE_C_COMPILER_LAUNCHER=buildcache -DCMAKE_CXX_COMPILER_LAUNCHER=buildcache"
        fi
    fi

    # Source fs-build-variables for LL_BUILD
    if [ -n "$AUTOBUILD_VARIABLES_FILE" ] && [ -f "$AUTOBUILD_VARIABLES_FILE" ]
    then
        source "$AUTOBUILD_VARIABLES_FILE"
        conv_dir="$(dirname "$AUTOBUILD_VARIABLES_FILE")"
        if [ -f "$conv_dir/convenience" ]
        then
            source "$conv_dir/convenience" "$BTYPE"
            export LL_BUILD
        fi
    fi

    mkdir -p build-linux-x86_64
    cmake -G "$TARGET" $CMAKE_ARCH -B build-linux-x86_64 -S $PWD/indra $CHANNEL ${GITHASH} $FMODSTUDIO $OPENAL $KDU $OPENSIM $SINGLEGRID $HAVOK $AVX_OPTIMIZATION $AVX2_OPTIMIZATION $TRACY_PROFILER $USE_3D_STREAM $ESPEAK_NG $USE_MCP $TESTBUILD $PACKAGE $VELOPACK \
          $UNATTENDED -DLL_TESTS:BOOL=OFF -DADDRESS_SIZE:STRING=$AUTOBUILD_ADDRSIZE -DCMAKE_BUILD_TYPE:STRING=$BTYPE $CACHE_OPT \
          $CRASH_REPORTING -DVIEWER_SYMBOL_FILE:STRING="${VIEWER_SYMBOL_FILE:-}" $LL_ARGS_PASSTHRU ${VSCODE_FLAGS:-} | tee "$LOG"
    configure_status=${PIPESTATUS[0]}

    # Check the return code of the build command
    if [ $configure_status -ne 0 ]; then
        echo "Configure failed!"
        exit 1
    fi    
fi
if [ $WANTS_BUILD -eq $TRUE ] ; then
    echo "Building $TARGET_PLATFORM..."
    build_status=0
    if [ $TARGET_PLATFORM == "linux" ] ; then
        if [ $JOBS == "0" ] ; then
            JOBS=`cat /proc/cpuinfo | grep processor | wc -l`
            echo $JOBS
        fi
        if [ $WANTS_NINJA -eq $TRUE ] ; then
            ninja -C build-linux-x86_64 -j $JOBS | tee -a "$LOG"
            build_status=${PIPESTATUS[0]}
        else
            make -C build-linux-x86_64 -j $JOBS | tee -a "$LOG"
            build_status=${PIPESTATUS[0]}
        fi
    elif [ $TARGET_PLATFORM == "windows" ] ; then
        # VS2026+ now uses .slnx so determine which one exists
        if [ -f "MikoStorm.slnx" ]; then
          SOLUTION="MikoStorm.slnx"
        elif [ -f "MikoStorm.sln" ]; then
          SOLUTION="MikoStorm.sln"
        else
          echo "Build failed! No MikoStorm.slnx or MikoStorm.sln found"
          exit 1
        fi
        msbuild.exe "$SOLUTION" -p:Configuration=${BTYPE} -flp:LogFile="logs\\MikoStormBuild_win-${AUTOBUILD_ADDRSIZE}.log" \
            -flp1:"errorsonly;LogFile=logs\\MikoStormBuild_win-${AUTOBUILD_ADDRSIZE}.err" -p:Platform=${AUTOBUILD_WIN_VSPLATFORM} -t:Build -p:useenv=true \
            -verbosity:normal -toolsversion:Current -p:"VCBuildAdditionalOptions= /incremental"
        build_status=$?
    fi
    # Check the return code of the build command
    if [ $build_status -ne 0 ]; then
        echo "Build failed!"
        exit 1
    fi    
fi
echo "finished"
exit 0
