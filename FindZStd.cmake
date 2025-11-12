# [AK] Find ZStd
# Find the native ZStd includes and library.
#
# ZSTD_INCLUDE_DIR - Where to find zstd.h.
# ZSTD_LIBRARY     - The library file to use for ZStd.
# ZSTD_FOUND       - True if ZStd found.

IF ( ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY )
	# Already in cache, be silent.
	SET( ZSTD_FIND_QUIETLY TRUE )
ENDIF ( ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY )

FIND_PATH( ZSTD_INCLUDE_DIR NAMES zstd.h )

FIND_LIBRARY( ZSTD_LIBRARY NAMES libzstd_static )
MARK_AS_ADVANCED( CLEAR ZSTD_LIBRARY ZSTD_INCLUDE_DIR )

# Handle the QUIETLY and REQUIRED arguments and set ZSTD_FOUND to TRUE if 
# all listed variables are TRUE.
INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( ZStd DEFAULT_MSG ZSTD_LIBRARY ZSTD_INCLUDE_DIR )
