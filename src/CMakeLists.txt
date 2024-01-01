add_executable(test.exe main.cpp)
target_include_directories(test.exe PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test.exe PRIVATE Ecad Threads::Threads Ceres::ceres dl)