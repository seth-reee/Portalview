import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1040; height: 650
    minimumWidth: 760; minimumHeight: 540
    visible: true
    title: "Portalview"
    color: theme.colors.background
    font.pixelSize: 13
    palette.window: theme.colors.background
    palette.base: theme.colors.lighter_background
    palette.button: theme.colors.lighter_background
    palette.text: theme.colors.foreground
    palette.windowText: theme.colors.foreground
    palette.buttonText: theme.colors.foreground
    palette.highlight: theme.colors.selection
    palette.highlightedText: theme.colors.foreground
    palette.placeholderText: theme.colors.light_foreground
    property string selectedId: ""
    onClosing: close => {
        if (tray.canHide) {
            close.accepted = false
            root.hide()
        }
    }
    function showMainMenu() {
        root.showNormal()
        root.raise()
        root.requestActivate()
    }
    Connections {
        target: tray
        function onOpenRequested() { root.showMainMenu() }
        function onConnectionRequested(id) {
            root.showMainMenu()
            root.selectedGroup = null
            search.clear()
            root.selectedId = id
            root.connectSelected()
        }
    }
    property var selectedGroup: null
    property var groupNames: [...new Set(manager.connections.map(e => e.group || "").filter(g => g.length > 0))].sort((a, b) => a.localeCompare(b))
    property var groupOptions: [{label: "All groups", value: null}, {label: "Ungrouped", value: ""}].concat(groupNames.map(g => ({label: g, value: g})))
    onGroupNamesChanged: {
        if (selectedGroup && !groupNames.includes(selectedGroup)) selectedGroup = null
    }
    property var selected: {
        for (let entry of manager.connections) if (entry.id === selectedId) return entry
        return null
    }
    function edit(entry) {
        editor.entryId = entry ? entry.id : ""
        nameField.text = entry ? entry.name : ""
        groupField.editText = entry ? (entry.group || "") : (selectedGroup || "")
        hostField.text = entry ? entry.host : ""
        userField.text = entry ? entry.username : ""
        domainField.text = entry ? entry.domain : ""
        portField.value = entry ? entry.port : 3389
        clipboard.checked = entry ? entry.clipboard : true
        fullscreen.checked = entry ? entry.fullscreen : false
        performance.checked = entry ? !!entry.performanceMode : false
        trust.checked = entry ? entry.trustFirst : false
        editor.open(); nameField.forceActiveFocus()
    }
    function connectSelected() {
        if (!selected || selected.active) return
        passwordDialog.connectionId = selected.id
        passwordDialog.connectionName = selected.name
        passwordField.clear(); passwordDialog.open(); passwordField.forceActiveFocus()
    }
    component ActionButton: Button {
        id: control
        leftPadding: 12; rightPadding: 12; topPadding: 8; bottomPadding: 8
        background: Rectangle {
            radius: 6
            color: control.down ? theme.colors.selection : theme.colors.lighter_background
            border.width: 1
            border.color: control.hovered || control.activeFocus ? theme.colors.accent : theme.colors.muted
            opacity: control.enabled ? 1 : 0.5
        }
    }
    component EntryField: TextField {
        id: control
        padding: 9
        background: Rectangle {
            radius: 6; color: theme.colors.lighter_background
            border.color: control.activeFocus ? theme.colors.accent : theme.colors.muted
        }
    }
    component GroupComboBox: ComboBox {
        id: control
        implicitHeight: 36
        leftPadding: 10; rightPadding: 30
        palette.button: theme.colors.lighter_background
        palette.base: theme.colors.lighter_background
        palette.mid: theme.colors.selection
        palette.dark: theme.colors.foreground
        background: Rectangle {
            radius: 6
            color: control.down ? theme.colors.selection : theme.colors.lighter_background
            border.color: control.hovered || control.activeFocus ? theme.colors.accent : theme.colors.muted
        }
        indicator: Label {
            x: control.width - width - 10
            anchors.verticalCenter: parent.verticalCenter
            text: "⌄"; font.pixelSize: 18
            color: theme.colors.foreground
        }
        delegate: ItemDelegate {
            required property int index
            width: control.width - 10
            height: 36
            text: control.textAt(index)
            highlighted: control.highlightedIndex === index
            contentItem: Label {
                text: parent.text
                color: theme.colors.foreground
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: parent.highlighted || parent.hovered ? theme.colors.selection : "transparent"
            }
        }
        popup: Popup {
            y: control.height + 4
            width: control.width
            padding: 5
            implicitHeight: Math.min(menuList.contentHeight + 10, 280)
            contentItem: ListView {
                id: menuList
                clip: true
                model: control.delegateModel
                currentIndex: control.highlightedIndex
                ScrollBar.vertical: ScrollBar {}
            }
            background: Rectangle {
                radius: 6
                color: theme.colors.lighter_background
                border.color: theme.colors.muted
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        RowLayout {
            Image {
                source: "qrc:/icons/portalview.png"
                sourceSize.width: 42; sourceSize.height: 42
                Layout.preferredWidth: 42; Layout.preferredHeight: 42
                fillMode: Image.PreserveAspectFit
            }
            Label { text: "Portalview"; font.pixelSize: 24; font.bold: true }
            Item { Layout.fillWidth: true }
            CheckBox {
                text: "System Tray Icon"
                checked: tray.enabled
                onToggled: tray.enabled = checked
                ToolTip.visible: hovered
                ToolTip.text: "Keep Portalview and its sessions running when the main window closes."
            }
            ActionButton { text: "About"; onClicked: about.open() }
        }
        Label { text: "Manage connections to your remote desktops." }
        RowLayout {
            EntryField { id: search; Layout.fillWidth: true; placeholderText: "Search connections…" }
            GroupComboBox {
                id: groupFilter
                objectName: "groupFilter"
                Layout.preferredWidth: 180
                model: root.groupOptions
                textRole: "label"
                currentIndex: root.groupOptions.findIndex(g => g.value === root.selectedGroup)
                onActivated: root.selectedGroup = root.groupOptions[index].value
                Accessible.name: "Filter by group"
            }
            ActionButton { text: "Add connection"; onClicked: root.edit(null) }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: theme.colors.background; radius: 6; border.color: theme.colors.muted
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 1; spacing: 0
                Rectangle {
                    Layout.fillWidth: true; height: 39; color: theme.colors.lighter_background
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                        Label { text: "Name"; Layout.fillWidth: true; Layout.preferredWidth: 240 }
                        Label { text: "Group"; Layout.fillWidth: true; Layout.preferredWidth: 150 }
                        Label { text: "Host"; Layout.fillWidth: true; Layout.preferredWidth: 280 }
                        Label { text: "Username"; Layout.fillWidth: true; Layout.preferredWidth: 200 }
                        Label { text: "Session"; Layout.preferredWidth: 110 }
                    }
                }
                ListView {
                    id: list
                    objectName: "connectionList"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: manager.connections.filter(e => (root.selectedGroup === null || (e.group || "") === root.selectedGroup) && (e.name + " " + e.host + " " + e.username + " " + (e.group || "")).toLowerCase().includes(search.text.toLowerCase()))
                    onModelChanged: {
                        if (!model.some(e => e.id === root.selectedId)) root.selectedId = ""
                    }
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: list.width; height: 46
                        color: root.selectedId === modelData.id ? theme.colors.selection : (index % 2 ? theme.colors.lighter_background : theme.colors.background)
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                            Label { text: modelData.name; elide: Text.ElideRight; Layout.fillWidth: true; Layout.preferredWidth: 240 }
                            Label { text: modelData.group || "Ungrouped"; elide: Text.ElideRight; Layout.fillWidth: true; Layout.preferredWidth: 150 }
                            Label { text: modelData.host + ":" + modelData.port; elide: Text.ElideRight; Layout.fillWidth: true; Layout.preferredWidth: 280 }
                            Label { text: (modelData.domain ? modelData.domain + "\\" : "") + modelData.username; elide: Text.ElideRight; Layout.fillWidth: true; Layout.preferredWidth: 200 }
                            Label { text: modelData.active ? "Open" : "Closed"; color: modelData.active ? theme.colors.accent : theme.colors.light_foreground; Layout.preferredWidth: 110 }
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: mouse => {
                                root.selectedId = parent.modelData.id
                                if (mouse.button === Qt.RightButton) entryMenu.popup()
                            }
                            onDoubleClicked: mouse => {
                                if (mouse.button === Qt.LeftButton) {
                                    root.selectedId = parent.modelData.id
                                    root.connectSelected()
                                }
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: manager.connections.length ? "No matching connections." : "Add a connection to get started."
                        color: theme.colors.light_foreground
                    }
                }
            }
        }
        RowLayout {
            Label { text: list.count + " / " + manager.connections.length + " connections"; color: theme.colors.light_foreground }
            Item { Layout.fillWidth: true }
            ActionButton { text: "Edit"; enabled: !!root.selected; onClicked: root.edit(root.selected) }
            ActionButton { text: "Delete…"; enabled: !!root.selected && !root.selected.active; onClicked: deletion.open() }
            ActionButton { text: "Disconnect"; enabled: !!root.selected && root.selected.active; onClicked: manager.disconnectFrom(root.selectedId) }
            ActionButton { text: "Connect"; enabled: !!root.selected && !root.selected.active; onClicked: root.connectSelected() }
        }
        Label { text: manager.message || "Ready"; Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.colors.light_foreground }
    }
    Menu {
        id: entryMenu
        objectName: "entryContextMenu"
        implicitWidth: 180
        padding: 5
        background: Rectangle {
            radius: 6; color: theme.colors.lighter_background
            border.color: theme.colors.muted
        }
        MenuItem {
            id: editMenuItem
            objectName: "editConnectionMenuItem"
            implicitWidth: 170
            implicitHeight: 36
            text: "Edit…"
            enabled: !!root.selected
            onTriggered: root.edit(root.selected)
            contentItem: Label {
                text: editMenuItem.text
                color: theme.colors.foreground
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: editMenuItem.highlighted ? theme.colors.selection : "transparent"
            }
        }
    }
    Dialog {
        id: editor
        property string entryId: ""
        objectName: "connectionEditor"
        anchors.centerIn: parent; implicitWidth: 480; modal: true
        height: Math.min(implicitHeight, root.height - 40)
        title: entryId ? "Edit connection" : "Add connection"
        contentItem: ScrollView {
            implicitHeight: editorForm.implicitHeight
            contentWidth: availableWidth
            ColumnLayout {
                id: editorForm
                width: parent.width; spacing: 10
            Label { text: "Name" }
            EntryField { id: nameField; Layout.fillWidth: true; placeholderText: "Office workstation" }
            Label { text: "Group (optional)" }
            GroupComboBox {
                id: groupField
                objectName: "groupEditor"
                Layout.fillWidth: true
                editable: true
                model: root.groupNames
                currentIndex: -1
                onActivated: editText = currentText
                Accessible.name: "Group"
            }
            Label { text: "Choose a group or type a new name. Leave blank for Ungrouped."; Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.colors.light_foreground }
            RowLayout {
                ColumnLayout { Layout.fillWidth: true; Label { text: "Host" } EntryField { id: hostField; Layout.fillWidth: true; placeholderText: "Hostname or IP address" } }
                ColumnLayout { Label { text: "Port" } SpinBox { id: portField; from: 1; to: 65535; value: 3389; editable: true } }
            }
            Label { text: "Username" }
            EntryField { id: userField; Layout.fillWidth: true }
            Label { text: "Domain (optional)" }
            EntryField { id: domainField; Layout.fillWidth: true }
            CheckBox { id: clipboard; text: "Share clipboard" }
            CheckBox { id: fullscreen; text: "Open fullscreen" }
            Switch { id: performance; text: "Performance mode" }
            Label {
                text: "Uses modem connection settings and 16-bit color; disables wallpaper, themes, font smoothing, composition, full window dragging and menu animations. Applies on the next connection."
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.colors.light_foreground
            }
            CheckBox { id: trust; text: "Trust certificate on first connection" }
            Label { text: "When enabled, FreeRDP remembers the first certificate and rejects changes. Otherwise a trusted certificate is required."; Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.colors.light_foreground }
            Label { text: manager.message; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Item { Layout.fillWidth: true }
                ActionButton { text: "Cancel"; onClicked: editor.close() }
                ActionButton {
                    text: "Save"
                    onClicked: {
                        if (manager.save({id: editor.entryId, name: nameField.text, group: groupField.editText, host: hostField.text, port: portField.value, username: userField.text, domain: domainField.text, clipboard: clipboard.checked, fullscreen: fullscreen.checked, performanceMode: performance.checked, trustFirst: trust.checked})) editor.close()
                    }
                }
            }
        }
        }
    }
    Dialog {
        id: passwordDialog
        property string connectionId: ""
        property string connectionName: ""
        objectName: "passwordDialog"
        anchors.centerIn: parent; implicitWidth: 400; modal: true; title: "Connect to " + connectionName
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Label { text: "Password" }
            EntryField { id: passwordField; Layout.fillWidth: true; echoMode: TextInput.Password; onAccepted: passwordDialog.accept() }
            Label { text: "Passwords are used for this session only."; color: theme.colors.light_foreground }
        }
        onAccepted: { manager.connectTo(connectionId, passwordField.text); passwordField.clear() }
        onRejected: passwordField.clear()
    }
    Dialog {
        id: deletion; anchors.centerIn: parent; implicitWidth: 400; modal: true; title: "Delete connection?"
        objectName: "deleteDialog"
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label { wrapMode: Text.Wrap; text: "Delete “" + (root.selected ? root.selected.name : "") + "”?" }
        onAccepted: { if (manager.remove(root.selectedId)) root.selectedId = "" }
    }
    Dialog {
        id: about; anchors.centerIn: parent; implicitWidth: 580; height: 440; modal: true; title: "About Portalview"
        objectName: "aboutDialog"
        padding: 20
        contentItem: ColumnLayout {
            spacing: 10
            Image {
                source: "qrc:/icons/portalview.png"
                sourceSize.width: 72; sourceSize.height: 72
                Layout.preferredWidth: 72; Layout.preferredHeight: 72
                fillMode: Image.PreserveAspectFit
            }
            Label { text: "Portalview " + Qt.application.version; font.pixelSize: 23; font.bold: true }
            Label { text: "A remote desktop connection manager for Omarchy." }
            Label {
                text: "Created by seth-reee · <a href=\"https://github.com/seth-reee\">GitHub profile</a>"
                textFormat: Text.RichText
                linkColor: theme.colors.accent
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
            Label { text: "MIT License · Copyright © 2026 seth-reee" }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                background: Rectangle {
                    color: theme.colors.lighter_background
                    border.color: theme.colors.muted; radius: 6
                }
                TextArea {
                    objectName: "aboutLicenseText"
                    text: manager.licenseText
                    readOnly: true; selectByMouse: true
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    color: theme.colors.foreground
                    padding: 10
                    background: null
                }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                ActionButton { text: "Close"; onClicked: about.close() }
            }
        }
    }
}
