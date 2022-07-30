set(PROJECT_BINARY_DIR ..)
set(PROJECT_LIBRARY_DIR ..)

include_directories(h)

add_library(posixfio
	STATIC posixfio.cpp ../posixfio_tl.cpp )
set_target_properties(
	posixfio PROPERTIES
	VERSION "${PROJECT_VERSION}"
	SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

install(TARGETS posixfio DESTINATION lib)
install(FILES
	"${CMAKE_SOURCE_DIR}/include/win32/posixfio.hpp"
	"${CMAKE_SOURCE_DIR}/include/win32/posixfio_tl.hpp"
	DESTINATION include )
