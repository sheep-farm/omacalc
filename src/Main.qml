import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: 400
    height: 620
    minimumWidth: 300
    minimumHeight: 470
    visible: true
    title: "Omacalc"

    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: backend.themeBackground
    readonly property color inkColor: backend.themeForeground
    // Every hardcoded size in the interface is expressed at the 400 × 620
    // design size; resizing the window scales the whole face with it. The
    // desktop text scale is folded into the default window size below, and
    // runtime changes resize the window proportionally so the face re-flows
    // live along with the rest of the desktop.
    readonly property real uiScale: Math.min(width / 400, height / 620)
    property real appliedTextScale: backend.textScale

    Connections {
        target: backend

        function onTextScaleChanged() {
            var factor = backend.textScale / win.appliedTextScale;
            win.appliedTextScale = backend.textScale;
            if (win.visibility === Window.Windowed) {
                win.width = Math.round(win.width * factor);
                win.height = Math.round(win.height * factor);
            }
        }
    }

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }
    readonly property color mutedColor: mixColors(pageColor, inkColor, 0.5)

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * uiScale));
    }

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: backend.themeAccent
    color: pageColor

    Shortcut {
        sequence: "Ctrl+C"
        context: Qt.ApplicationShortcut
        onActivated: backend.copyResult()
    }

    Shortcut {
        sequence: "Ctrl+?"
        context: Qt.ApplicationShortcut
        onActivated: shortcutsDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Dialog {
        id: shortcutsDialog
        modal: true
        title: "Keyboard shortcuts"
        standardButtons: Dialog.Close
        anchors.centerIn: parent
        contentItem: Label {
            text: "0-9 . + - * / %  Enter digits and operators\nEnter or =  Calculate\nBackspace  Delete last digit\nEscape  Clear\nS  Toggle sign\nCtrl+C  Copy result\nCtrl+?  Shortcuts"
            lineHeight: 1.5
        }
    }

    Item {
        id: face
        anchors.fill: parent
        anchors.margins: win.scaledSize(20)
        focus: true

        Keys.onPressed: function(event) {
            if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                return;

            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                    || event.text === "=") {
                backend.pressKey("=");
            } else if (event.key === Qt.Key_Backspace) {
                backend.pressKey("backspace");
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Delete) {
                backend.pressKey("clear");
            } else if (event.text === "," || event.text === ".") {
                backend.pressKey(".");
            } else if (event.text === "s" || event.text === "S") {
                backend.pressKey("sign");
            } else if (/^[0-9+\-*/%]$/.test(event.text)) {
                backend.pressKey(event.text);
            } else {
                return;
            }
            event.accepted = true;
        }

        // The menu icon from the design: three right-aligned strokes.
        Item {
            id: menuButton
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: win.scaledSize(6)
            anchors.rightMargin: win.scaledSize(6)
            width: win.scaledSize(24)
            height: win.scaledSize(18)

            Repeater {
                model: [
                    { y: 0.0, length: 1.0 },
                    { y: 0.5, length: 0.66 },
                    { y: 1.0, length: 1.0 }
                ]

                Rectangle {
                    anchors.right: parent.right
                    y: Math.round(modelData.y * (menuButton.height - height))
                    width: Math.round(menuButton.width * modelData.length)
                    height: Math.max(2, win.scaledSize(2))
                    radius: height / 2
                    color: menuHitArea.containsMouse ? win.inkColor : win.mutedColor
                }
            }

            MouseArea {
                id: menuHitArea
                anchors.fill: parent
                anchors.margins: -win.scaledSize(8)
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: appMenu.popup(menuButton,
                                         menuButton.width - appMenu.width,
                                         menuButton.height + win.scaledSize(8))
            }

            Menu {
                id: appMenu
                onClosed: face.forceActiveFocus()

                MenuItem {
                    text: "Copy Result"
                    onTriggered: backend.copyResult()
                }

                MenuItem {
                    text: "Keyboard Shortcuts"
                    onTriggered: shortcutsDialog.open()
                }
            }
        }

        Item {
            id: displayArea
            anchors.top: menuButton.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: divider.top
            anchors.leftMargin: win.scaledSize(8)
            anchors.rightMargin: win.scaledSize(8)
            anchors.bottomMargin: win.scaledSize(24)

            Text {
                id: expressionText
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: win.scaledSize(16)
                text: backend.expression
                color: win.mutedColor
                elide: Text.ElideLeft
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(21)
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                horizontalAlignment: Text.AlignRight
                text: backend.display
                color: win.inkColor
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: win.scaledSize(22)
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(76)
            }
        }

        Rectangle {
            id: divider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: keypad.top
            anchors.bottomMargin: win.scaledSize(22)
            height: 1
            color: win.mixColors(win.pageColor, win.inkColor, 0.16)
        }

        GridLayout {
            id: keypad
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.round(parent.height * 0.62)
            columns: 4
            rowSpacing: win.scaledSize(12)
            columnSpacing: win.scaledSize(12)

            Repeater {
                model: [
                    { label: "AC", key: "clear", kind: "number" },
                    { label: "±", key: "sign", kind: "number" },
                    { label: "%", key: "%", kind: "number" },
                    { label: "÷", key: "÷", kind: "operator" },
                    { label: "7", key: "7", kind: "number" },
                    { label: "8", key: "8", kind: "number" },
                    { label: "9", key: "9", kind: "number" },
                    { label: "×", key: "×", kind: "operator" },
                    { label: "4", key: "4", kind: "number" },
                    { label: "5", key: "5", kind: "number" },
                    { label: "6", key: "6", kind: "number" },
                    { label: "−", key: "-", kind: "operator" },
                    { label: "1", key: "1", kind: "number" },
                    { label: "2", key: "2", kind: "number" },
                    { label: "3", key: "3", kind: "number" },
                    { label: "+", key: "+", kind: "operator" },
                    { label: "0", key: "0", kind: "number" },
                    { label: ".", key: ".", kind: "number" },
                    { label: "", key: "backspace", kind: "number", icon: "backspace" },
                    { label: "=", key: "=", kind: "equals" }
                ]

                CalcButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    label: modelData.label
                    keyValue: modelData.key
                    kind: modelData.kind
                    iconName: modelData.icon === undefined ? "" : modelData.icon
                    pageColor: win.pageColor
                    inkColor: win.inkColor
                    onActivated: backend.pressKey(keyValue)
                }
            }
        }
    }

    // Remember the last windowed geometry rather than whatever the window
    // happens to measure at teardown: a maximized window reports screen-sized
    // dimensions, and the close sequence hides the window before destruction,
    // so neither the live geometry nor the final visibility can be trusted.
    property rect normalGeometry: Qt.rect(x, y, width, height)
    property bool wasMaximized: false

    function trackNormalGeometry() {
        if (visibility === Window.Windowed)
            normalGeometry = Qt.rect(x, y, width, height);
    }

    onXChanged: trackNormalGeometry()
    onYChanged: trackNormalGeometry()
    onWidthChanged: trackNormalGeometry()
    onHeightChanged: trackNormalGeometry()

    onVisibilityChanged: {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            wasMaximized = true;
        else if (visibility === Window.Windowed)
            wasMaximized = false;
    }

    Component.onCompleted: {
        var geometry = backend.windowGeometry();
        if (geometry.valid) {
            x = geometry.x;
            y = geometry.y;
            width = geometry.width;
            height = geometry.height;
            if (geometry.maximized) showMaximized();
        } else {
            // First run: open at the design size, grown by the desktop text scale.
            width = Math.round(400 * backend.textScale);
            height = Math.round(620 * backend.textScale);
        }
    }

    Component.onDestruction: backend.saveWindowGeometry(
        normalGeometry.x, normalGeometry.y,
        normalGeometry.width, normalGeometry.height, wasMaximized)
}
