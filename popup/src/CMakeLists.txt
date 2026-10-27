add_library(nabu-popup-model STATIC
    NotificationModel.h
    NotificationModel.cpp
    IconProvider.h
    IconProvider.cpp
)

target_include_directories(nabu-popup-model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nabu-popup-model PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::DBus)

qt_add_executable(nabu-popup main.cpp)

target_compile_definitions(nabu-popup PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

qt_add_qml_module(nabu-popup
    URI Sanar.Nabu
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Card.qml
        qml/ActionButton.qml
)

target_link_libraries(nabu-popup PRIVATE nabu-popup-model SanarStyle::sanar-style)

install(TARGETS nabu-popup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PROJECT_SOURCE_DIR}/resources/org.nuros.sanar.nabu-popup.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${PROJECT_SOURCE_DIR}/resources/org.nuros.sanar.nabu-popup.desktop
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart)
