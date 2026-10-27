import QtQuick
import Sanar.Style

Rectangle {
    id: root

    property string text

    signal clicked

    implicitWidth: label.implicitWidth + 32
    implicitHeight: 36
    radius: height / 2
    color: hover.hovered ? Qt.alpha(Theme.onSurface, 0.08) : "transparent"

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: Theme.primary
        font.pixelSize: 14
        font.weight: Font.Medium
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: root.clicked()
    }
}
