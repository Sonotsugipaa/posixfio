include_directories(..)
include_directories(.)

add_library(test-tools STATIC
	test_tools.cpp)


if(NOT POSIXFIO_LOCAL)
	if(UNIX)
		include_directories("${PROJECT_SOURCE_DIR}/include/unix")
	elseif(WIN32)
		include_directories("${PROJECT_SOURCE_DIR}/include/win32")
	endif()
endif(NOT POSIXFIO_LOCAL)

add_executable(posixfio-test posixfio-test.cpp)
target_link_libraries(posixfio-test
	test-tools posixfio)

add_executable(posixfio-tl-test posixfio-tl-test.cpp)
target_link_libraries(posixfio-tl-test
	test-tools posixfio)
