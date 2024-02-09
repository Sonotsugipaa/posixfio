set(PROJECT_BINARY_DIR ..)
set(PROJECT_LIBRARY_DIR ..)

set(POSIXFIO_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/win32")

add_library(posixfio STATIC
	posixfio.cpp
	../posixfio_tl.cpp )

target_compile_definitions(posixfio PUBLIC POSIXFIO_WIN32)

if(POSIXFIO_LOCAL)
	target_include_directories(posixfio PUBLIC ${POSIXFIO_INCLUDE_DIR})
else()
	target_include_directories(posixfio PRIVATE ${POSIXFIO_INCLUDE_DIR})
endif(POSIXFIO_LOCAL)

target_compile_definitions(posixfio PUBLIC POSIXFIO_WIN32)

set_target_properties(
	posixfio PROPERTIES
	VERSION "${PROJECT_VERSION}"
	SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}" )

if(NOT POSIXFIO_LOCAL)
	install(TARGETS posixfio DESTINATION lib)
	install(FILES
		"${POSIXFIO_INCLUDE_DIR}/posixfio_compat_constants.hpp"
		"${POSIXFIO_INCLUDE_DIR}/posixfio.hpp"
		"${POSIXFIO_INCLUDE_DIR}/posixfio_tl.hpp"
		DESTINATION include )
endif(NOT POSIXFIO_LOCAL)
