include_directories(..)
include_directories(.)

add_library(test-tools STATIC
	test_tools.cpp)

add_executable(posixfio-test posixfio-test.cpp)
target_link_libraries(posixfio-test
	test-tools posixfio)
