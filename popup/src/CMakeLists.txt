add_library(nabu-popup-model STATIC
    NotificationModel.h
    NotificationModel.cpp
    IconProvider.h
    IconProvider.cpp
)

target_include_directories(nabu-popup-model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nabu-popup-model PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::DBus)
