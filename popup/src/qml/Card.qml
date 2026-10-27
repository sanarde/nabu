import QtQuick
import QtQuick.Layouts
import Sanar.Style

Rectangle {
    id: root

    required property int notificationId
    required property string appName
    required property string appIcon
    required property string summary
    required property string body
    required property string imagePath
    required property var actionKeys
    required property var actionLabels
    required property int urgency

    readonly property bool critical: urgency === 2
    readonly property bool hasDefault: actionKeys.indexOf("default") >= 0
    readonly property string picture: imagePath.length > 0 ? imagePath : appIcon

    function pictureSource(name) {
        if (name.length === 0)
            return ""
        if (name.startsWith("/"))
            return "file://" + name
        if (name.startsWith("file:"))
            return name
        return "image://icon/" + name
    }

    implicitHeight: content.implicitHeight + 32
    radius: 24
    color: critical ? Theme.errorContainer : Theme.surfaceHigh
    border.width: critical ? 0 : 1
    border.color: Theme.outlineVariant

    RowLayout {
        id: content
        x: 16
        y: 16
        width: parent.width - 32
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: 20
            color: root.critical ? Theme.onErrorContainer : Theme.primaryContainer

            Image {
                id: picture
                anchors.centerIn: parent
                width: 24
                height: 24
                source: root.pictureSource(root.picture)
                sourceSize: Qt.size(48, 48)
                fillMode: Image.PreserveAspectFit
                visible: status === Image.Ready
            }

            Icon {
                anchors.centerIn: parent
                visible: !picture.visible
                name: root.critical ? "error" : "info"
                filled: true
                color: root.critical ? Theme.errorContainer : Theme.onPrimaryContainer
                font.pixelSize: 22
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    visible: root.appName.length > 0
                    text: root.appName
                    color: root.critical ? Theme.onErrorContainer : Theme.onSurfaceVariant
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.summary
                    color: root.critical ? Theme.onErrorContainer : Theme.onSurface
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.body.length > 0
                    text: root.body
                    textFormat: Text.PlainText
                    color: root.critical ? Theme.onErrorContainer : Theme.onSurfaceVariant
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                }

                TapHandler {
                    onTapped: root.hasDefault
                        ? Notifications.invoke(root.notificationId, "default")
                        : Notifications.dismiss(root.notificationId)
                }
            }

            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: -16
                spacing: 8
                visible: root.actionKeys.length > (root.hasDefault ? 1 : 0)

                Repeater {
                    model: root.actionKeys

                    delegate: ActionButton {
                        required property int index
                        required property string modelData

                        visible: modelData !== "default"
                        text: root.actionLabels[index]
                        onClicked: Notifications.invoke(root.notificationId, modelData)
                    }
                }
            }
        }

        IconButton {
            Layout.alignment: Qt.AlignTop
            implicitWidth: 32
            implicitHeight: 32
            icon: "close"
            iconColor: root.critical ? Theme.onErrorContainer : Theme.onSurfaceVariant
            iconSize: 20
            label: "Close"
            onClicked: Notifications.dismiss(root.notificationId)
        }
    }
}
