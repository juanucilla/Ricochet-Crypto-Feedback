import QtQuick 2.2
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtQuick.Window 2.2

ApplicationWindow {
    id: groupsWindow
    title: "group-ricochet · " + qsTr("Private groups")
    width: 920; height: 650
    minimumWidth: 720; minimumHeight: 500
    visible: false
    property string selected: ""
    property var current: ({})
    property var timeline: []
    property string replyId: ""
    property string replyPreview: ""
    property bool switching: false
    property int lastCount: 0
    function refresh() {
        current = groupManager.details(selected)
        timeline = groupManager.messages(selected, search.text)
        if (visible && active && selected.length) groupManager.markRead(selected)
    }
    function selectGroup(id) {
        draftTimer.stop()
        if (selected.length) groupManager.setDraft(selected, composer.text)
        switching = true
        selected = id
        current = groupManager.details(id)
        composer.text = current.draft || ""
        replyId = ""; replyPreview = ""
        search.text = ""
        switching = false
        refresh()
        messageList.positionViewAtEnd()
    }
    function submit() {
        if (groupManager.send(selected, composer.text, replyId)) {
            composer.text = ""; replyId = ""; replyPreview = ""
            messageList.positionViewAtEnd()
        }
    }
    onActiveChanged: if (active) refresh()
    onVisibleChanged: if (visible) refresh()
    onClosing: {
        draftTimer.stop()
        if (selected.length) groupManager.setDraft(selected, composer.text)
    }
    Connections {
        target: groupManager
        function onChanged() {
            var followLatest = messageList.atYEnd
            groupsWindow.refresh()
            if (followLatest) Qt.callLater(function() { messageList.positionViewAtEnd() })
        }
        function onNotification(title, body) {
            if (!groupsWindow.active) groupsWindow.alert(0)
        }
    }
    SystemPalette { id: colors }
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        ColumnLayout {
            width: 250; Layout.minimumWidth: 210; Layout.maximumWidth: 330
            Label { text: qsTr("Private groups"); font.bold: true; Layout.margins: 12 }
            Button { text: qsTr("+ New group"); Layout.margins: 8; onClicked: createWindow.show() }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                model: groupManager.groups
                delegate: Rectangle {
                    width: ListView.view.width; height: 76
                    color: selected === modelData.id ? colors.highlight : colors.window
                    Column {
                        anchors.fill: parent; anchors.margins: 10; spacing: 5
                        Label { width: parent.width; text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; font.bold: true }
                        Label { text: modelData.state + (modelData.unread ? " · " + modelData.unread + " " + qsTr("unread") : "") + (modelData.muted ? " · " + qsTr("muted") : "") }
                    }
                    MouseArea { anchors.fill: parent; onClicked: selectGroup(modelData.id) }
                }
            }
            Label {
                Layout.fillWidth: true; Layout.margins: 10; wrapMode: Text.WordWrap
                text: qsTr("Experimental · All members must use group-ricochet and be mutual accepted contacts.")
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            RowLayout {
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: current.title || qsTr("Choose a group"); textFormat: Text.PlainText; font.bold: true; elide: Text.ElideRight }
                Button { text: qsTr("Members"); enabled: selected.length > 0; onClicked: membersWindow.show() }
                CheckBox { text: qsTr("Mute"); checked: current.muted || false; enabled: selected.length > 0; onClicked: groupManager.setMuted(selected, checked) }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: groupManager.error; visible: text.length > 0; color: "#b33131" }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                visible: selected.length > 0 && current.state !== "active"
                text: current.state === "invited" ? qsTr("Invitation: review the members before accepting. Your onion identity will be visible to everyone.")
                    : current.state === "waiting" ? qsTr("Waiting for every participant to accept. The owner must be online to complete setup.")
                    : qsTr("This fixed-roster group is closed. Its history remains local.")
            }
            RowLayout {
                visible: current.state === "invited"
                Button { text: qsTr("Review and accept"); onClicked: membersWindow.show() }
                Button { text: qsTr("Decline…"); onClicked: closeConfirmation.show() }
            }
            TextField { id: search; Layout.fillWidth: true; placeholderText: qsTr("Search this group locally…"); onTextChanged: groupsWindow.refresh() }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                ListView {
                    id: messageList
                    clip: true; spacing: 8
                    model: timeline
                    delegate: Rectangle {
                        width: messageList.width - 12
                        height: content.height + 20
                        radius: 8
                        color: modelData.mine ? "#d9eaf7" : "#eeeeee"
                        Column {
                            id: content
                            spacing: 6
                            anchors.left: parent.left; anchors.right: parent.right
                            anchors.top: parent.top; anchors.margins: 10
                            Label {
                                width: parent.width; elide: Text.ElideMiddle
                                text: groupManager.displayName(modelData.author)
                                textFormat: Text.PlainText; font.bold: true; color: "#222222"
                            }
                            Label {
                                visible: !!modelData.replyTo
                                width: parent.width; elide: Text.ElideRight
                                text: modelData.replyPreview || ""
                                textFormat: Text.PlainText; color: "#555555"
                            }
                            TextEdit {
                                width: parent.width; height: contentHeight; text: modelData.text
                                textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true
                                wrapMode: TextEdit.Wrap; color: "#171717"
                            }
                            RowLayout {
                                width: parent.width
                                Label { text: new Date(modelData.time * 1000).toLocaleTimeString(Qt.locale(), "HH:mm"); color: "#555555" }
                                Label {
                                    visible: modelData.mine
                                    text: qsTr("Delivered %1/%2").arg(modelData.deliveryCount).arg(modelData.recipientCount) + (modelData.expired && modelData.deliveryCount < modelData.recipientCount ? " · " + qsTr("retry expired") : "")
                                    color: "#555555"
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: qsTr("Reply"); enabled: current.state === "active"
                                    onClicked: { replyId = modelData.id; replyPreview = modelData.text.substring(0, 100); composer.forceActiveFocus() }
                                }
                            }
                        }
                    }
                }
            }
            Button { text: qsTr("↓ Latest messages"); Layout.alignment: Qt.AlignRight; visible: !messageList.atYEnd; onClicked: messageList.positionViewAtEnd() }
            RowLayout {
                visible: replyId.length > 0
                Label { Layout.fillWidth: true; text: replyPreview; textFormat: Text.PlainText; elide: Text.ElideRight }
                Button { text: qsTr("Cancel reply"); onClicked: { replyId = ""; replyPreview = "" } }
            }
            TextArea {
                id: composer
                Layout.fillWidth: true; Layout.preferredHeight: 90
                enabled: current.state === "active"
                textFormat: TextEdit.PlainText
                onTextChanged: if (!switching && selected.length) draftTimer.restart()
                Keys.onPressed: {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                        event.accepted = true; groupsWindow.submit()
                    }
                }
            }
            Timer { id: draftTimer; interval: 500; onTriggered: groupManager.setDraft(selected, composer.text) }
            RowLayout {
                Label { text: composer.text.length + "/500 · Enter " + qsTr("send") + " · Shift+Enter " + qsTr("new line") }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Retry delivery"); enabled: current.state === "active"; onClicked: groupManager.retry() }
                Button { text: qsTr("Send"); enabled: current.state === "active" && composer.text.trim().length > 0 && composer.text.length <= 500; onClicked: groupsWindow.submit() }
            }
        }
    }
    Window {
        id: createWindow
        title: qsTr("Create a private group"); width: 520; height: 480
        property var selectedPeers: []
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Choose a name and 1–11 accepted contacts. Everyone must already know every other member. All participants see the roster.") }
            TextField { id: groupTitle; Layout.fillWidth: true; placeholderText: qsTr("Group name"); maximumLength: 80 }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                ListView {
                    model: groupManager.contacts
                    delegate: CheckBox {
                        width: parent.width
                        text: modelData.name + " · " + modelData.id.substring(9, 19) + "…"
                        checked: createWindow.selectedPeers.indexOf(modelData.id) >= 0
                        onClicked: {
                            var peers = createWindow.selectedPeers.slice()
                            var index = peers.indexOf(modelData.id)
                            if (checked && index < 0) peers.push(modelData.id)
                            if (!checked && index >= 0) peers.splice(index, 1)
                            createWindow.selectedPeers = peers
                        }
                    }
                }
            }
            Label { text: qsTr("%1 contacts selected").arg(createWindow.selectedPeers.length) }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: groupManager.error; textFormat: Text.PlainText }
            Button {
                text: qsTr("Create and invite")
                onClicked: {
                    var id = groupManager.create(groupTitle.text, createWindow.selectedPeers)
                    if (id.length) { selectGroup(id); createWindow.close(); groupTitle.text = ""; createWindow.selectedPeers = [] }
                }
            }
        }
    }
    Window {
        id: membersWindow
        title: qsTr("Group members and consent"); width: 600; height: 420
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16
            Label { text: current.title || ""; textFormat: Text.PlainText; font.bold: true }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Your onion identity and the member list are shared with everyone in this group. Messages travel directly over Tor. New messages require every member's consent. In this preview, leaving closes the whole group.") }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                TextEdit { text: (current.members || []).join("\n"); textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere }
            }
            Label { Layout.fillWidth: true; text: qsTr("Owner: %1").arg(current.owner || ""); textFormat: Text.PlainText; elide: Text.ElideMiddle }
            RowLayout {
                Button { visible: current.state === "invited"; text: qsTr("Accept and share my identity"); onClicked: if (groupManager.accept(selected)) membersWindow.close() }
                Button { visible: current.state !== "closed"; text: qsTr("Close this group for everyone…"); onClicked: closeConfirmation.show() }
                Button { text: qsTr("Done"); onClicked: membersWindow.close() }
            }
        }
    }
    Window {
        id: closeConfirmation
        title: qsTr("Close group?"); width: 420; height: 180
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Leaving or declining closes this fixed-roster group for everyone once the owner receives the request. Local history is retained. Continue?") }
            RowLayout {
                Button { text: qsTr("Cancel"); onClicked: closeConfirmation.close() }
                Button { text: qsTr("Close group"); onClicked: { groupManager.leave(selected); closeConfirmation.close(); membersWindow.close() } }
            }
        }
    }
}
