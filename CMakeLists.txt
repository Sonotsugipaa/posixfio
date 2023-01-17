# Declare the project

	cmake_minimum_required(VERSION 3.20)
	project(posixfio
		VERSION "0.3.0"
		DESCRIPTION "Simple C++ wrapper around basic POSIX file IO functions." )


# Set the project's structure

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})


# Set warning levels

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options(-Wall -Wextra -Wpedantic)
	elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		# add_compile_options(\\Wall) # How...?
	else()
		message(WARNING "Toolchain \"${CMAKE_CXX_COMPILER_ID}\" not recognized by the project")
	endif()


# Define the C++ standard to use

	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED True)


# Setup sources

	# Uncomment the next command to disable exception handling, and use C-style
	# error management.
	# If the library is to be installed and not used as a simple CMake module,
	# it is unadvisable to do so due to inconsistent symbols; depending on
	# the chosen error management method, the source file will use a different
	# namespace, and projects that disable exception handling need to make sure
	# that the `POSIXFIO_NOTHROW` macro is defined before `posixfio.hpp` is
	# included.
	#
	# Simply put: don't disable exceptions unless you really need to,
	# and assume that POSIX-like functions always return non-negative numbers.
	# Unless the negative number does not signal an error.
	#
	# add_compile_definitions(POSIXFIO_NOTHROW)

	if(UNIX)
		add_subdirectory(posixfio/unix)
	elseif(WIN32)
		add_subdirectory(posixfio/win32)
	else()
		message(FATAL_ERROR "Unknown platform")
	endif()


# Add unit test module

	add_subdirectory(test)


# Setup `apcfConfig.cmake` holy fuck this is copypasty

if(NOT POSIXFIO_LOCAL)
	add_library(posixfio::posixfio ALIAS posixfio)

	include(GNUInstallDirs)
	install(
		TARGETS posixfio
		EXPORT posixfioTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} )

	install(
		EXPORT posixfioTargets
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/posixfio
		NAMESPACE posixfio::posixfio
		FILE posixfioTargets.cmake )

	include(CMakePackageConfigHelpers)
	configure_package_config_file(
		"Config.cmake.in"
		"posixfioConfig.cmake"
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/posixfio
		PATH_VARS CMAKE_INSTALL_LIBDIR )

	write_basic_package_version_file(
		${CMAKE_CURRENT_BINARY_DIR}/posixfioConfigVersion.cmake
		VERSION "${PROJECT_VERSION}"
		COMPATIBILITY SameMajorVersion )

	install(
		FILES
			"${CMAKE_CURRENT_BINARY_DIR}/posixfioConfig.cmake"
			"${CMAKE_CURRENT_BINARY_DIR}/posixfioConfigVersion.cmake"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/posixfio" )
endif(NOT POSIXFIO_LOCAL)
