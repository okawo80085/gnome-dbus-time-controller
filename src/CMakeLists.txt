project(gnomeDbusTimeService)

add_custom_target(generate-dbus-adaptors ALL)

add_custom_command(
        TARGET generate-dbus-adaptors
        PRE_BUILD
        COMMAND dbusxx-xml2cpp
        ARGS ${CMAKE_SOURCE_DIR}/src/gnome-dbus-time-service.xml --adaptor=${CMAKE_SOURCE_DIR}/src/gnome-dbus-time-service-glue.hpp
)

file(
        GLOB_RECURSE
        SRC
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        # "${CMAKE_SOURCE_DIR}/src/utils/*.cpp"
        # "${CMAKE_SOURCE_DIR}/src/utils/*.hpp"
)
# list(FILTER SRC EXCLUDE REGEX "<pattern>")


add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} ${DBUS_LDFLAGS_OTHER})
target_link_libraries(${PROJECT_NAME} ${DBUS_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /bin
        PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE OWNER_WRITE OWNER_READ)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/files/gnomeDbusTimeService/
        DESTINATION /etc/sv/${PROJECT_NAME})
install(FILES ${CMAKE_SOURCE_DIR}/files/gnomeDbusTimeService/run
        DESTINATION /etc/sv/${PROJECT_NAME}
        PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE OWNER_WRITE OWNER_READ)
install(FILES ${CMAKE_SOURCE_DIR}/files/gnomeDbusTimeService/log/run
        DESTINATION /etc/sv/${PROJECT_NAME}/log
        PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE OWNER_WRITE OWNER_READ)