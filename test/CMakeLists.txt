include_directories(..)
include_directories(.)

add_library(test-tools STATIC
	test_tools.cpp)

add_executable(posixfio-test posixfio-test.cpp)
target_link_libraries(posixfio-test
	test-tools posixfio)

add_executable(posixfio-tl-test posixfio-tl-test.cpp)
target_link_libraries(posixfio-tl-test
	test-tools posixfio)
