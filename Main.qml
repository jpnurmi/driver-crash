import QtQuick

Window {
    id: root
    property bool crashRequested: false

    width: 960
    height: 600
    visible: true
    title: "Sentry GPU crash test"
    color: "#111827"

    Repeater {
        model: 80
        Rectangle {
            required property int index
            x: (index % 10) * 96 + 18
            y: Math.floor(index / 10) * 64 + 70
            width: 42
            height: 42
            radius: 8
            color: Qt.hsla(index / 80, 0.7, 0.55, 0.8)
            RotationAnimator on rotation {
                from: 0
                to: 360
                duration: 1500 + index * 35
                loops: Animation.Infinite
            }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 20
        color: "white"
        font.pixelSize: 20
        text: "Sentry GPU crash test"
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 220
        height: 48
        radius: 6
        color: mouse.pressed ? "#991b1b" : "#dc2626"

        Text {
            anchors.centerIn: parent
            color: "white"
            text: "Crash GPU driver"
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            onClicked: root.crashRequested = true
        }
    }
}
