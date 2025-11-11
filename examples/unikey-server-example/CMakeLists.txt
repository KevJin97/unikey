add_executable(unikey_server unikey_server_example.cpp)
target_link_libraries(unikey_server PRIVATE ${PROJECT_LIB_NAME})
install(TARGETS unikey_server DESTINATION bin)