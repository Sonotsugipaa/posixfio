set(PROJECT_BINARY_DIR ..)
set(PROJECT_LIBRARY_DIR ..)

include_directories(h)

add_library(posixfio
	SHARED posixfio.cpp)
add_compile_definitions(POSIXFIO_UNIX)
set_target_properties(
	posixfio PROPERTIES
	VERSION "${PROJECT_VERSION}"
	SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

install(TARGETS posixfio DESTINATION lib)
install(FILES "${CMAKE_SOURCE_DIR}/include/posixfio.hpp" DESTINATION include)
