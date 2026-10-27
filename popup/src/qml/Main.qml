import QtQuick
import Sanar.Style

Window {
    id: root

    readonly property int margin: 16
    readonly property int cardWidth: 380
    readonly property int maxCards: 5

    width: cardWidth + margin * 2
    height: column.implicitHeight + margin * 2
    x: Screen.virtualX + Screen.width - width
    y: Screen.virtualY
    visible: Notifications.count > 0
    title: "Notifications"
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus

    Column {
        id: column
        x: root.margin
        y: root.margin
        width: root.cardWidth
        spacing: 8

        Repeater {
            model: Notifications

            delegate: Card {
                required property int index

                width: column.width
                visible: index < root.maxCards
            }
        }
    }
}
