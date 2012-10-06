#! /bin/sh
#
# Turns Makefile.am.in into a Makefile.am which can be processed by
# automake. This is necessary because automake cannot build a list
# of source files dynamically.

# directories which contain sources for the sync engine
ENGINE_SOURCES="sysync DB_interfaces sysync_SDK/Sources Transport_interfaces/engine platform_adapters"

# files needed exclusively for libsynthesissdk.a
cat > SDK_FILES <<EOF
enginemodulebridge.cpp
enginemodulebridge.h
target_options.h
san.cpp
san.h
timeutil.cpp
timeutil.h
UI_util.cpp
UI_util.h
EOF

# files needed exclusively for libsynthesissdk.so
# They should not go into libsynthesissdk.a because the more complete set of
# code is available in the engine
cat > SDK_EXCLUDE_FILES <<EOF
stringutil.cpp
stringutil.h
EOF

# The distinction between client and server files is not
# important and even likely to be wrong/incomplete. Right now,
# all of these files are compiled into libsynthesis and only
# kept out of libsynthesisdk.

# files needed exclusively for the client engine
cat >CLIENT_FILES <<EOF
binfile.cpp
binfileimplds.cpp
binfileimplclient.cpp
binfilebase.cpp
engineclientbase.cpp
syncclient.cpp
syncclientbase.cpp
EOF

# files needed exclusively for the server engine
cat > SERVER_FILES <<EOF
admindata.cpp
admindata.h
dbitem.cpp
dbitem.h
enginesessiondispatch.cpp
syncserver.cpp
EOF

# files not needed anywhere at the moment:
# - some files are simply not used
# - because Mac OS X has its own build system,
#   compiling on it with autotools is not supported
cat > EXTRA_FILES <<EOF
clientprovisioning_inc.cpp
.*_tables_inc.cpp
syncsessiondispatch.cpp
platform_thread.cpp
enginestubs.c
platform_adapters/macosx
EOF

# files to be included in libsynthesis
cat EXTRA_FILES SDK_FILES SDK_EXCLUDE_FILES> EXCLUDE_FILES
LIBSYNTHESIS_SOURCES=`find ${ENGINE_SOURCES} \
     syncapps/clientEngine_custom \
     syncapps/serverEngine_custom \
     sysync_SDK/DB_Interfaces/text_db \
     \( -name '*.cpp' -o -name '*.[ch]' \) |
    grep -v -E -f EXCLUDE_FILES |
    sort`
LIBSYNTHESIS_SOURCES=`echo $LIBSYNTHESIS_SOURCES`

# files to be included in both libsynthesis and libsynthesissdk;
# necessary when building as shared libraries with these files not
# being exposed by libsynthesis
cat SERVER_FILES CLIENT_FILES EXTRA_FILES > EXCLUDE_FILES
LIBSYNTHESISSDK_SOURCES_BOTH=`find sysync_SDK/Sources \
     \( -name '*.cpp' -o -name '*.c' \) |
    grep -v -E -f EXCLUDE_FILES |
    sort`
LIBSYNTHESISSDK_SOURCES_BOTH=`echo $LIBSYNTHESISSDK_SOURCES_BOTH`

# files only needed in libsynthesissdk
cat SERVER_FILES CLIENT_FILES EXTRA_FILES > EXCLUDE_FILES
LIBSYNTHESISSDK_SOURCES_ONLY=`find sysync_SDK/Sources \
     \( -name '*.cpp' -o -name '*.c' \) |
    grep -E -f SDK_FILES |
    grep -v -E -f EXCLUDE_FILES |
    sort`
LIBSYNTHESISSDK_SOURCES_ONLY=`echo $LIBSYNTHESISSDK_SOURCES_ONLY`

# files needed in libsmltk
LIBSMLTK_SOURCES=`find syncml_tk \
     \( -name '*.cpp' -o -name '*.[ch]' \) \
     \! \( -path syncml_tk/src/sml/\*/palm/\* -o \
           -path syncml_tk/src/sml/\*/win/\* \) |
     sort`
LIBSMLTK_SOURCES=`echo $LIBSMLTK_SOURCES`

# header files required for using libsynthesissdk,
# with "synthesis/" prefix
LIBSYNTHESISSDK_HEADERS=`find sysync_SDK/Sources -name '*.h' | sed -e 's;.*/;synthesis/;' | sort`
LIBSYNTHESISSDK_HEADERS=`echo $LIBSYNTHESISSDK_HEADERS`

sed -e "s;@LIBSYNTHESIS_SOURCES@;$LIBSYNTHESIS_SOURCES;" \
    -e "s;@LIBSYNTHESISSDK_SOURCES_BOTH@;$LIBSYNTHESISSDK_SOURCES_BOTH;" \
    -e "s;@LIBSYNTHESISSDK_SOURCES_SDK_ONLY@;$LIBSYNTHESISSDK_SOURCES_ONLY;" \
    -e "s;@LIBSMLTK_SOURCES@;$LIBSMLTK_SOURCES;" \
    -e "s;@LIBSYNTHESISSDK_HEADERS@;$LIBSYNTHESISSDK_HEADERS;" \
    Makefile.am.in >Makefile.am
