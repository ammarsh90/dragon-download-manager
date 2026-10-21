set(TARGET_NAME DragonBrowserHost)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
qt_standard_project_setup()
add_executable(${TARGET_NAME} launcher.cpp ${CMAKE_SOURCE_DIR}/src/resources_win.rc)
target_compile_definitions(${TARGET_NAME} PRIVATE UNICODE)
target_link_libraries(${TARGET_NAME} PRIVATE Qt::Core Qt::Network)
install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
if(WIN32)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/windows/dragon-host-chromium.json
        ${CMAKE_CURRENT_SOURCE_DIR}/windows/dragon-host-firefox.json
        ${CMAKE_SOURCE_DIR}/web-extension/setup/windows/register-browser.ps1
        DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(DragonBrowserExtension ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/web-extension/extension/make.py
        --output-directory ${CMAKE_BINARY_DIR}/browser-extension
        --archive-directory ${CMAKE_BINARY_DIR}/browser-addons
    COMMENT "Package Dragon browser integration")
install(DIRECTORY ${CMAKE_BINARY_DIR}/browser-extension/ DESTINATION ${CMAKE_INSTALL_PREFIX}/browser-extension)
