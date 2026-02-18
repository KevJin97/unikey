add_executable(gatt_test main.cpp)
target_link_libraries(gatt_test PRIVATE ${PROJECT_LIB_NAME})
install(TARGETS gatt_test DESTINATION bin)