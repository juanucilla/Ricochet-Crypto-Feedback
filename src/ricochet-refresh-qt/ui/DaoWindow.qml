import QtQuick 2.15
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Dialogs 1.3

ApplicationWindow {
    id: daoWindow
    title: "group-ricochet · DAO locale sperimentale"
    width: 760; height: 680
    minimumWidth: 650; minimumHeight: 600
    property var pendingCommand: ({})
    function prepare(command) { pendingCommand = command; confirmation.open() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 12
        Label { text: "DAO · Crediti interni di prova"; font.pixelSize: 22 }
        Label { text: "Solo servizio locale. Non denaro, non blockchain, non ancora Tor." }
        Label { text: daoClient.state.actor ? "Identità: " + daoClient.state.actor + " · Ruolo: " + daoClient.state.role : "Non connesso" }
        RowLayout {
            Button { text: "Aggiorna"; enabled: !daoClient.busy; onClicked: daoClient.refresh() }
            Button { text: "Riprova stessa operazione"; enabled: !daoClient.busy; onClicked: daoClient.retry() }
        }
        Label { text: daoClient.message; wrapMode: Text.Wrap; Layout.fillWidth: true }
        TableView {
            Layout.fillWidth: true; Layout.preferredHeight: 150
            model: daoClient.state.assets || []
            TableViewColumn { role: "id"; title: "Credito"; width: 100 }
            TableViewColumn { role: "name"; title: "Nome"; width: 180 }
            TableViewColumn { role: "balance"; title: "Il tuo saldo"; width: 110 }
            TableViewColumn { role: "supply"; title: "Emessi"; width: 100 }
            TableViewColumn { role: "cap"; title: "Limite"; width: 100 }
        }
        GridLayout {
            columns: 2
            Label { text: "Credito (ID)" }
            TextField { id: asset; placeholderText: "DAO" }
            Label { text: "Destinatario accreditato" }
            ComboBox { id: recipient; model: daoClient.state.members || [] }
            Label { text: "Quantità (unità intere)" }
            TextField { id: amount; placeholderText: "10"; validator: RegExpValidator { regExp: /[1-9][0-9]{0,14}/ } }
        }
        RowLayout {
            enabled: !daoClient.busy && amount.acceptableInput && asset.text.length > 0 && !!daoClient.state.actor
            Button { text: "Trasferisci"; onClicked: prepare({kind:"transfer", asset:asset.text, to:recipient.currentText, amount:Number(amount.text)}) }
            Button { text: "Brucia i miei crediti"; onClicked: prepare({kind:"burn", asset:asset.text, amount:Number(amount.text)}) }
            Button { text: "Emetti (admin)"; visible: daoClient.state.role === "issuer"; onClicked: prepare({kind:"mint", asset:asset.text, to:recipient.currentText, amount:Number(amount.text)}) }
        }
        RowLayout {
            visible: daoClient.state.role === "issuer"
            TextField { id: name; placeholderText: "Nome nuovo credito" }
            TextField { id: cap; placeholderText: "Limite totale"; validator: RegExpValidator { regExp: /[1-9][0-9]{0,14}/ } }
            Button { text: "Crea credito"; enabled: !daoClient.busy && cap.acceptableInput && name.text.length > 0 && asset.text.length > 0; onClicked: prepare({kind:"create", asset:asset.text, name:name.text, cap:Number(cap.text)}) }
        }
        Label { text: "Governance: integrazione LiquidFeedback in corso; voto e deleghe non attivi."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { text: "Ultime operazioni personali" }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            model: daoClient.state.history || []
            delegate: Text { width: ListView.view.width; height: implicitHeight + 8; wrapMode: Text.Wrap; text: "#" + modelData.seq + " · " + modelData.created + "\n" + modelData.body }
        }
    }
    Dialog {
        id: confirmation
        title: "Conferma operazione di prova"
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        Label { text: JSON.stringify(daoWindow.pendingCommand); wrapMode: Text.Wrap; width: 520 }
        onAccepted: daoClient.submit(daoWindow.pendingCommand)
    }
}
