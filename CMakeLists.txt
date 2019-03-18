cmake_minimum_required (VERSION 2.6)

macro(use_cxx11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif ()
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_cxx11)
use_cxx11()

macro(use_c99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 99)
  endif ()
endmacro(use_c99)
use_c99()

# ==================
# Define Our Project
# ==================

set (My_Project_Title "C-Template")
set (My_Project_Description "Boilerplate c project with cmake support, CuTest unit testing, and more.")
set (My_Project_Author "Fletcher T. Penney")
set (My_Project_Revised_Date "2017-01-16")
set (My_Project_Version_Major 1)
set (My_Project_Version_Minor 0)
set (My_Project_Version_Patch 5)

set (My_Project_Version "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")

set (My_Project_Copyright_Date "2015-2017")
set (My_Project_Copyright "Copyright © ${My_Project_Copyright_Date} ${My_Project_Author}.")

string(TOUPPER ${My_Project_Title} My_Project_Title_Caps  )
string(REGEX REPLACE " " "_" My_Project_Title_Caps ${My_Project_Title_Caps} )

project (${My_Project_Title})


# =========================
# Build Submodules (if any)
# =========================

# add_subdirectory(submodules/foo)


# =================
# Configure Project
# =================

# Search for included files here
include_directories( ${PROJECT_SOURCE_DIR}/src )
include_directories( ${PROJECT_SOURCE_DIR}/test )
include_directories(${PROJECT_BINARY_DIR})


# =================
# Macro Definitions
# =================

MACRO(ADD_PUBLIC_HEADER target filename)
	# Add filename to public_header_files list, flag it as
	# public header for libraries and OS X Frameworks

	# This will work for creating one library/framework with public headers
	# per project. If you need more than one, you will need to customize
	# the workflow as appropriate, since there is only one
	# public_header_files list.

	SET_TARGET_PROPERTIES(${target} PROPERTIES PUBLIC_HEADER ${filename})

	LIST(APPEND public_header_files ${filename})

	SET_SOURCE_FILES_PROPERTIES(
		${filename}
		PROPERTIES
		MACOSX_PACKAGE_LOCATION
		PUBLIC_HEADER
	)

	# Set Xcode project to configure public header location to allow
	# use when this project is used in another workspace.
	# NOTE: You must manually add a "Headers" build phase and add
	# the desired public headers to that list for Xcode to use them.
	#
	# TODO: If anyone knows how to automate that in Cmake, I would be very
	# grateful!!

	SET_TARGET_PROPERTIES(${target} PROPERTIES
		XCODE_ATTRIBUTE_PUBLIC_HEADERS_FOLDER_PATH
		"include/$(TARGET_NAME)"
	)

	SET_TARGET_PROPERTIES(${target} PROPERTIES
		XCODE_ATTRIBUTE_PRIVATE_HEADERS_FOLDER_PATH
		"$(PUBLIC_HEADERS_FOLDER_PATH)/Private"
	)

	# Set Xcode target to include settings for OS X and iOS

	SET_TARGET_PROPERTIES(${target} PROPERTIES
		XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS
		"macos iphonesimulator iphoneos"
	)

	SET_TARGET_PROPERTIES(${target} PROPERTIES
		XCODE_ATTRIBUTE_VALID_ARCHITECTURES
		"x86_64 i386 armv6 armv7 armv7s arm64"
	)

ENDMACRO(ADD_PUBLIC_HEADER)


# The target should be an OS X Bundle with a PList

MACRO(MAKE_TARGET_BUNDLE targetname)

	set_target_properties(
		${targetname}
		PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST
		${PROJECT_SOURCE_DIR}/templates/plist.in
	)

ENDMACRO(MAKE_TARGET_BUNDLE)


MACRO(ADD_LINKED_FRAMEWORK frame)

	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGES} -framework ${frame}")

ENDMACRO(ADD_LINKED_FRAMEWORK)


# ======================
# Process Template Files
# ======================

file(READ ${PROJECT_SOURCE_DIR}/LICENSE.txt My_Project_License)

string(REGEX REPLACE "\n" "\n\t" My_Project_License_Indent ${My_Project_License})

string(REGEX REPLACE "\"" "\\\\\"" My_Project_License_Escaped ${My_Project_License_Indent})

string(REGEX REPLACE "\n" "\\\\n\"\\\\\n\"" My_Project_License_Literal ${My_Project_License_Escaped})


configure_file (
	"${PROJECT_SOURCE_DIR}/templates/template.c.in"
	"${PROJECT_BINARY_DIR}/template.c"
)

configure_file (
	"${PROJECT_SOURCE_DIR}/templates/template.h.in"
	"${PROJECT_BINARY_DIR}/template.h"
)

# Update the project README, to pull in new version #, etc.
configure_file (
	"${PROJECT_SOURCE_DIR}/templates/README.md.in"
	"${CMAKE_CURRENT_LIST_DIR}/README.md"
)

configure_file (
	"${PROJECT_SOURCE_DIR}/templates/version.h.in"
	"${PROJECT_BINARY_DIR}/version.h"
)


# ============
# Source Files
# ============

# src_files are the primary files, and will be included in doxygen documentation
set(src_files
#	src/foo.c
)

# Primary header files, also for doxygen documentation
set(header_files
)

# Public headers, will be installed in 'include'
# Do not manually add files here, use the ADD_PUBLIC_HEADER() macro
set(public_header_files
)

# Utility source files will not be included in doxygen
set(src_utility_files
#	src/d_string.c
)

set(header_utility_files
#	src/d_string.h
	${PROJECT_BINARY_DIR}/version.h
)

# Generate doxygen configuration file
string(REPLACE ";" " " doxygen_src_files "${src_files}" )
string(REPLACE ";" " " doxygen_header_files "${header_files}" )

configure_file (
	"${PROJECT_SOURCE_DIR}/templates/doxygen.conf.in"
	"${PROJECT_BINARY_DIR}/doxygen.conf"
)


# ===========================================
# Build Test Suite with CuTest (unit testing)
# ===========================================

# from http://stackoverflow.com/questions/25199677/how-to-detect-if-current-scope-has-a-parent-in-cmake
get_directory_property(hasParent PARENT_DIRECTORY)

if(hasParent)
	# Don't create "run_tests" when we're a sub-project for something else
else()
	set(test_files
		test/CuTest.c
		test/CuTest.h
		${PROJECT_BINARY_DIR}/AllTests.c
	)

	if (DEFINED TEST)
		add_definitions(-DTEST)

		add_executable(run_tests
			${test_files}
			${src_files}
			${header_files}
			${src_utility_files}
			${header_utility_files}
		)

		# Process source files to look for tests to run
		add_custom_command (
			OUTPUT ${PROJECT_BINARY_DIR}/AllTests.c
			COMMAND sh ${PROJECT_SOURCE_DIR}/test/make-tests.sh ${PROJECT_SOURCE_DIR}/src/*.c > ${PROJECT_BINARY_DIR}/AllTests.c
		)

		enable_testing()

		add_test( test ${PROJECT_BINARY_DIR}/run_tests)

		# valgrind memory testing
		find_program (MEMORYCHECK_COMMAND valgrind)
		SET (MEMORYCHECK_COMMAND_OPTIONS --leak-check=full --error-exitcode=1)

		add_test( memory_test ${MEMORYCHECK_COMMAND} ${MEMORYCHECK_COMMAND_OPTIONS} ${PROJECT_BINARY_DIR}/run_tests)

	endif()
endif()


# =======================
# Configure for Target OS
# =======================

# OS X Builds
if (APPLE)

	# Configure backwards-compatible support (if your project allows it)
	SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.4" CACHE STRING "Deployment target for OSX" FORCE)

	# Compile for x86_64 and i386.  ppc no longer supported
	if(CMAKE_BUILD_TYPE MATCHES "Release")
		SET (CMAKE_OSX_ARCHITECTURES x86_64;i386)
	endif(CMAKE_BUILD_TYPE MATCHES "Release")

	# Use PackageMaker for installers?
	if (DEFINED ZIP)
		set (CPACK_GENERATOR ZIP)
	else (DEFINED ZIP)
		set (CPACK_GENERATOR PackageMaker)
	endif (DEFINED ZIP)

endif (APPLE)

# Windows Builds
if (WIN32)

	# Use NSIS to generate installers?
	if (DEFINED ZIP)
		set (CPACK_GENERATOR ZIP)
	else (DEFINED ZIP)
		set (CPACK_GENERATOR NSIS)
	endif (DEFINED ZIP)

# Linux Builds (not cross-compiling for Windows)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

	# Create zip archive
	set (CPACK_GENERATOR ZIP)

	# Statically link libraries -- might make the binary slightly more
	# compatible across Linux distributions, for example
	#
	# You may wish to disable this.
	#

	set (CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set (BUILD_SHARED_LIBRARIES OFF)
	set (CMAKE_EXE_LINKER_FLAGS "-static")

	# Some libraries need to be linked on some Linux builds
	if (DEFINED TEST)
		# target_link_libraries(run_tests m)
	endif (DEFINED TEST)

endif (WIN32)


set (CMAKE_CXX_FLAGS "-static-libstdc++ ${CMAKE_CXX_FLAGS}")

# Create a command-line app?
file(GLOB SOURCES src/*.cpp src/*.c src/sha/sha1.c)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

# find_package(OpenSSL REQUIRED)

if (NOT DEFINED TEST)
    # add_executable(hcinfo_reader_test
    #     test/hcinfo_reader_test.c
    #     ${SOURi686|x86-64)-w64-mingw32-gccCES}
    # )
    add_executable(kdcracker
        kdcracker.cpp
        ${SOURCES}
    )
    # add_executable(test/hc_convertor_test
    #     test/hccvt_test.c
    #     ${SOURCES}
    # )
    # add_executable(test/hckill_test
    #     test/hckill_test.c
    #     ${SOURCES}
    # )
    # add_executable(test/fifo_test
    #     test/fifo_test.c
    #     ${SOURCES}
    # )
    # add_executable(test/hc_log_test
    #     test/hclog_test.c
    #     ${SOURCES}
    # )
    add_executable(ini_infor
        ini_infor.cpp
        ${SOURCES}
    )
    # add_executable(test/ini_test
    #     test/ini_test.c
    #     ${SOURCES}
    # )
    # add_executable(test/sysbasic_test
    #     test/sysbasic_test.c
    #     ${SOURCES}
    # )

    # add_executable(test/pptp_test
    #     test/pptp_test.c
    #     ${SOURCES}
    # )
    # TARGET_LINK_LIBRARIES(test/pptp_test ${OPENSSL_SSL_LIBRARY})
    # TARGET_LINK_LIBRARIES(test/pptp_test ${OPENSSL_CRYPTO_LIBRARY})

    # TARGET_LINK_LIBRARIES(ini_infor ${OPENSSL_SSL_LIBRARY})
    # TARGET_LINK_LIBRARIES(ini_infor ${OPENSSL_CRYPTO_LIBRARY})
    
    # TARGET_LINK_LIBRARIES(test/hckill_test ${OPENSSL_SSL_LIBRARY})
    # TARGET_LINK_LIBRARIES(test/hckill_test ${OPENSSL_CRYPTO_LIBRARY})
    # TARGET_LINK_LIBRARIES(kdcracker ${OPENSSL_SSL_LIBRARY})
    # TARGET_LINK_LIBRARIES(kdcracker ${OPENSSL_CRYPTO_LIBRARY})
    # TARGET_LINK_LIBRARIES(test/pptp_test ssl)
    # TARGET_LINK_LIBRARIES(test/pptp_test crypto)
    # LINK_DIRECTORIES("/usr/local/include/collectc")
    # TARGET_LINK_LIBRARIES(test/ini_test collectc)
    # TARGET_LINK_LIBRARIES(ini_infor collectc)
    # TARGET_LINK_LIBRARIES(test/sysbasic_test collectc)
    # add_executable(main
    #     src/main.c
    #     src/d_string.c
    #     src/d_string.h
    #     ${header_files}
    # )

    # Link the library to the app?
    # target_link_libraries(main libFOO)
endif()


set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${My_Project_Description}")
set (CPACK_PACKAGE_VENDOR "${My_Project_Author}")
set (CPACK_PACKAGE_VERSION "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")
set (CPACK_PACKAGE_VERSION_MAJOR "${My_Project_Version_Major}")
set (CPACK_PACKAGE_VERSION_MINOR "${My_Project_Version_Minor}")
set (CPACK_PACKAGE_VERSION_PATCH "My_Project_Version_Patch")

set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-Mac-${CPACK_PACKAGE_VERSION}")
else (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}")
endif (APPLE)

# Use some default install locations (if appropriate)
set(CPACK_SET_DESTDIR true)
if (WIN32)
	set (CPACK_INSTALL_PREFIX "")
	set (CPACK_NSIS_MODIFY_PATH ON)
else (WIN32)
	set (CPACK_INSTALL_PREFIX /usr/local)
endif (WIN32)

set (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT})

include (CPack)


set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${My_Project_Description}")
set (CPACK_PACKAGE_VENDOR "${My_Project_Author}")
set (CPACK_PACKAGE_VERSION "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")
set (CPACK_PACKAGE_VERSION_MAJOR "${My_Project_Version_Major}")
set (CPACK_PACKAGE_VERSION_MINOR "${My_Project_Version_Minor}")
set (CPACK_PACKAGE_VERSION_PATCH "My_Project_Version_Patch")

set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-Mac-${CPACK_PACKAGE_VERSION}")
else (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}")
endif (APPLE)

# Use some default install locations (if appropriate)
set(CPACK_SET_DESTDIR true)
if (WIN32)
	set (CPACK_INSTALL_PREFIX "")
	set (CPACK_NSIS_MODIFY_PATH ON)
else (WIN32)
	set (CPACK_INSTALL_PREFIX /usr/local)
endif (WIN32)

set (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT})

include (CPack)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${My_Project_Description}")
set (CPACK_PACKAGE_VENDOR "${My_Project_Author}")
set (CPACK_PACKAGE_VERSION "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")
set (CPACK_PACKAGE_VERSION_MAJOR "${My_Project_Version_Major}")
set (CPACK_PACKAGE_VERSION_MINOR "${My_Project_Version_Minor}")
set (CPACK_PACKAGE_VERSION_PATCH "My_Project_Version_Patch")

set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-Mac-${CPACK_PACKAGE_VERSION}")
else (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}")
endif (APPLE)

# Use some default install locations (if appropriate)
set(CPACK_SET_DESTDIR true)
if (WIN32)
	set (CPACK_INSTALL_PREFIX "")
	set (CPACK_NSIS_MODIFY_PATH ON)
else (WIN32)
	set (CPACK_INSTALL_PREFIX /usr/local)
endif (WIN32)

set (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT})

include (CPack)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${My_Project_Description}")
set (CPACK_PACKAGE_VENDOR "${My_Project_Author}")
set (CPACK_PACKAGE_VERSION "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")
set (CPACK_PACKAGE_VERSION_MAJOR "${My_Project_Version_Major}")
set (CPACK_PACKAGE_VERSION_MINOR "${My_Project_Version_Minor}")
set (CPACK_PACKAGE_VERSION_PATCH "My_Project_Version_Patch")

set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-Mac-${CPACK_PACKAGE_VERSION}")
else (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}")
endif (APPLE)

# Use some default install locations (if appropriate)
set(CPACK_SET_DESTDIR true)
if (WIN32)
	set (CPACK_INSTALL_PREFIX "")
	set (CPACK_NSIS_MODIFY_PATH ON)
else (WIN32)
	set (CPACK_INSTALL_PREFIX /usr/local)
endif (WIN32)

set (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT})

include (CPack)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${My_Project_Description}")
set (CPACK_PACKAGE_VENDOR "${My_Project_Author}")
set (CPACK_PACKAGE_VERSION "${My_Project_Version_Major}.${My_Project_Version_Minor}.${My_Project_Version_Patch}")
set (CPACK_PACKAGE_VERSION_MAJOR "${My_Project_Version_Major}")
set (CPACK_PACKAGE_VERSION_MINOR "${My_Project_Version_Minor}")
set (CPACK_PACKAGE_VERSION_PATCH "My_Project_Version_Patch")

set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")

if (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-Mac-${CPACK_PACKAGE_VERSION}")
else (APPLE)
	set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}")
endif (APPLE)

# Use some default install locations (if appropriate)
set(CPACK_SET_DESTDIR true)
if (WIN32)
	set (CPACK_INSTALL_PREFIX "")
	set (CPACK_NSIS_MODIFY_PATH ON)
else (WIN32)
	set (CPACK_INSTALL_PREFIX /usr/local)
endif (WIN32)

set (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT})

include (CPack)
