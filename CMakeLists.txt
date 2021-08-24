# Declare the project

	cmake_minimum_required(VERSION 3.20)
	project(posixfio
		VERSION "0.1.0"
		DESCRIPTION "Lightweight wrapper around basic POSIX file IO functions.")


# Set the project's structure

	set(PROJECT_SOURCE_DIR .)
	set(PROJECT_BINARY_DIR .)

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

	include_directories(include)
	link_directories(lib)
	link_directories(bin)


# Set warning levels

	if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
		add_compile_options(-Wall -Wextra -Wpedantic)
	elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
		add_compile_options(\Wall)
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
	# include.
	#
	# add_compile_definitions(POSIXFIO_NOTHROW)

	add_subdirectory(posixfio)


# Add unit test module

	add_subdirectory(test)
