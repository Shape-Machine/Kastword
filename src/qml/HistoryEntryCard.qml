// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

Controls.Frame {
    id: root

    property string entryIdentifier: ""
    property string timestampText: ""
    property string transcriptText: ""
    property string timestampDescription: ""
    property string copyLabel: ""
    property string deleteLabel: ""
    property string deleteDescription: ""
    property bool showDelete: false
    property bool deleteEnabled: true
    signal copyRequested(string text)
    signal deleteRequested(string entryId)

    implicitHeight: contentLayout.implicitHeight + topPadding + bottomPadding

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent

        Controls.Label {
            Layout.fillWidth: true
            text: root.timestampText
            opacity: 0.7
        }

        Controls.Label {
            objectName: "historyEntryText"
            Layout.fillWidth: true
            text: root.transcriptText
            wrapMode: Text.Wrap
            maximumLineCount: 4
            elide: Text.ElideRight
            Accessible.name: root.transcriptText
            Accessible.description: root.timestampDescription
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            Controls.ToolButton {
                text: root.copyLabel
                icon.name: "edit-copy"
                display: Controls.AbstractButton.TextBesideIcon
                onClicked: root.copyRequested(root.transcriptText)
            }

            Controls.ToolButton {
                visible: root.showDelete
                enabled: root.deleteEnabled
                text: root.deleteLabel
                icon.name: "edit-delete"
                display: Controls.AbstractButton.TextBesideIcon
                onClicked: root.deleteRequested(root.entryIdentifier)
                Accessible.description: root.deleteDescription
            }
        }
    }
}
