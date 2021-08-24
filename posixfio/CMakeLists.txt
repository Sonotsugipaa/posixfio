include_directories(h)

add_library(posixfio SHARED posixfio.cpp)

install(TARGETS posixfio DESTINATION lib)
install(FILES "../include/posixfio.hpp" DESTINATION include)
