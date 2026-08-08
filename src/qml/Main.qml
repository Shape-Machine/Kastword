// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later
// qmllint disable unqualified
// KLocalizedQmlContext and AppController intentionally expose context properties.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls

Kirigami.ApplicationWindow {
    id: root
    objectName: "mainWindow"
    width: 760
    height: 700
    minimumWidth: 380
    minimumHeight: 400
    visible: false
    title: i18n("Kastword")

    function selectModel(url) {
        appController.setModelUrl(url)
    }

    function openModelManager() {
        root.currentView = 2
    }

    function openSettings() {
        root.currentView = 4
    }

    function openAudioInput() {
        root.currentView = 3
    }

    function updateAudioInputMonitoring() {
        appController.setAudioInputMonitoringEnabled(root.visible && root.currentView === 3)
    }

    function copyModelUrl(url) {
        appController.copyText(url)
        const message = i18n("Download URL copied to clipboard.")
        root.passiveNotificationHandler(message)
    }

    property string pendingRemovalId: ""
    property string pendingHistoryId: ""
    property int currentView: 0
    property bool shortcutChangeFailed: false
    property var passiveNotificationHandler: function(message) {
        root.showPassiveNotification(message, "short")
    }

    onVisibleChanged: updateAudioInputMonitoring()
    onCurrentViewChanged: updateAudioInputMonitoring()

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    Connections {
        target: appController
        function onModelSetupRequested() {
            root.show()
            root.openModelManager()
        }
        function onAudioInputSetupRequested() {
            root.show()
            root.openAudioInput()
        }
        function onShortcutChanged() {
            root.shortcutChangeFailed = false
        }
    }

    Component.onCompleted: {
        if (appController.modelSetupRequired) {
            root.show()
            root.openModelManager()
        }
    }

    FileDialog {
        id: modelDialog
        title: i18n("Select a Whisper model")
        nameFilters: [i18n("Whisper models (*.bin)"), i18n("All files (*)")]
        onAccepted: root.selectModel(selectedFile)
    }

    Kirigami.PromptDialog {
        id: removeDialog
        title: i18n("Remove speech model?")
        subtitle: i18n("The model file will be deleted from this user account.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: Kirigami.Action {
            text: i18n("Remove")
            icon.name: "edit-delete"
            onTriggered: {
                appController.removeModel(root.pendingRemovalId)
                root.pendingRemovalId = ""
                removeDialog.close()
            }
        }
        onRejected: root.pendingRemovalId = ""
    }

    Kirigami.Dialog {
        id: transcriptDialog
        objectName: "transcriptDialog"
        title: i18n("Last transcription")
        preferredWidth: Kirigami.Units.gridUnit * 24
        standardButtons: Kirigami.Dialog.Close

        Controls.ScrollView {
            implicitWidth: Kirigami.Units.gridUnit * 22
            implicitHeight: Math.min(fullTranscriptText.contentHeight
                                     + fullTranscriptText.topPadding
                                     + fullTranscriptText.bottomPadding,
                                     Kirigami.Units.gridUnit * 16)

            Controls.TextArea {
                id: fullTranscriptText
                text: appController.transcript
                readOnly: true
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                Accessible.name: i18n("Full transcription")
            }
        }
    }

    Kirigami.PromptDialog {
        id: disableHistoryDialog
        objectName: "disableHistoryDialog"
        title: i18n("Disable dictation history?")
        subtitle: i18n("You can keep the encrypted history for later, or delete it together with its KDE Wallet key.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                objectName: "disableKeepHistoryAction"
                text: i18n("Disable and keep")
                icon.name: "document-save"
                onTriggered: {
                    appController.disableHistory(false)
                    disableHistoryDialog.close()
                }
            },
            Kirigami.Action {
                objectName: "disableDeleteHistoryAction"
                text: i18n("Disable and delete")
                icon.name: "edit-delete"
                onTriggered: {
                    appController.disableHistory(true)
                    disableHistoryDialog.close()
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: clearHistoryDialog
        title: i18n("Clear all dictation history?")
        subtitle: i18n("All entries in Kastword's encrypted history will be deleted. This cannot remove copies retained by other applications or backups.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: Kirigami.Action {
            text: i18n("Clear all")
            icon.name: "edit-clear-history"
            onTriggered: {
                appController.history.clear()
                clearHistoryDialog.close()
            }
        }
    }

    Kirigami.PromptDialog {
        id: resetHistoryDialog
        title: i18n("Reset encrypted history?")
        subtitle: i18n("The unreadable encrypted file and its KDE Wallet key will be deleted. Existing entries cannot be recovered afterward.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: Kirigami.Action {
            text: i18n("Reset and delete")
            icon.name: "edit-delete"
            onTriggered: {
                appController.disableHistory(true)
                resetHistoryDialog.close()
            }
        }
    }

    Kirigami.PromptDialog {
        id: deleteHistoryEntryDialog
        title: i18n("Delete this history entry?")
        subtitle: i18n("The selected entry will be removed from Kastword's encrypted history.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: Kirigami.Action {
            text: i18n("Delete")
            icon.name: "edit-delete"
            onTriggered: {
                appController.history.removeEntry(root.pendingHistoryId)
                root.pendingHistoryId = ""
                deleteHistoryEntryDialog.close()
            }
        }
        onRejected: root.pendingHistoryId = ""
    }

    Component {
        id: historyPage

        Kirigami.Page {
            objectName: "historyPage"
            title: i18n("History")

            footer: FooterContainer {
                id: historyFooter
                objectName: "historyFooter"
                visible: appController.history.status.length > 0

                contentItem: ColumnLayout {
                    width: historyFooter.availableWidth
                    Kirigami.InlineMessage {
                        objectName: "historyStatus"
                        Layout.fillWidth: true
                        visible: true
                        type: appController.history.available ? Kirigami.MessageType.Information
                                                              : Kirigami.MessageType.Error
                        text: appController.history.status
                        actions: Kirigami.Action {
                            objectName: "resetHistoryAction"
                            visible: appController.history.resetRequired
                            text: i18n("Reset encrypted history")
                            icon.name: "edit-clear-history"
                            onTriggered: resetHistoryDialog.open()
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    Layout.fillWidth: true

                    Controls.Switch {
                        objectName: "historyEnabledSwitch"
                        text: i18n("Enable history")
                        checked: appController.history.enabled
                        checkable: false
                        enabled: !appController.history.busy
                        onClicked: {
                            if (appController.history.enabled)
                                disableHistoryDialog.open()
                            else
                                appController.enableHistory()
                        }
                        Accessible.description: i18n("Store transcription text locally using authenticated encryption and a key protected by KDE Wallet")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Controls.Button {
                        objectName: "clearHistoryButton"
                        visible: appController.history.enabled && historyList.count > 0
                        enabled: !appController.history.busy
                        text: i18n("Clear all history")
                        icon.name: "edit-clear-history"
                        onClicked: clearHistoryDialog.open()
                    }
                }

                ListView {
                    id: historyList
                    objectName: "historyList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: appController.history.enabled ? appController.history.entryModel : null
                    spacing: Kirigami.Units.smallSpacing
                    clip: true
                    reuseItems: true

                header: ColumnLayout {
                    width: historyList.width
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.FormLayout {
                        Layout.fillWidth: true
                        enabled: appController.history.enabled && !appController.history.busy

                        Controls.SpinBox {
                            objectName: "historyMaximumEntries"
                            Kirigami.FormData.label: i18n("Maximum entries:")
                            from: 1
                            to: 10000
                            value: appController.history.maximumEntries
                            onValueModified: appController.history.maximumEntries = value
                            Accessible.name: i18n("Maximum history entries")
                        }

                        Controls.SpinBox {
                            objectName: "historyMaximumAgeDays"
                            Kirigami.FormData.label: i18n("Maximum age (days):")
                            from: 1
                            to: 3650
                            value: appController.history.maximumAgeDays
                            textFromValue: function(value, locale) {
                                return Number(value).toLocaleString(locale, "f", 0)
                            }
                            valueFromText: function(text, locale) {
                                return Number.fromLocaleString(locale, text)
                            }
                            onValueModified: appController.history.maximumAgeDays = value
                            Accessible.name: i18n("Maximum history age in days")
                        }

                        RowLayout {
                            Kirigami.FormData.label: i18n("History storage:")
                            Layout.fillWidth: true

                            Controls.TextField {
                                objectName: "historyStoragePath"
                                Layout.fillWidth: true
                                readOnly: true
                                selectByMouse: true
                                text: appController.history.storagePath
                                Accessible.name: i18n("Encrypted history storage path")
                            }

                            Controls.ToolButton {
                                objectName: "openHistoryStorageButton"
                                icon.name: "document-open-folder"
                                onClicked: appController.revealHistoryStorage()
                                Accessible.name: i18n("Show history file in file manager")
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: Accessible.name
                            }
                        }
                    }

                    Controls.Label {
                        Layout.fillWidth: true
                        visible: appController.history.enabled && historyList.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        text: i18n("No saved dictations yet")
                        opacity: 0.7
                    }
                }

                delegate: HistoryEntryCard {
                    required property string entryId
                    required property string createdText
                    required property string text
                    width: historyList.width
                    entryIdentifier: entryId
                    timestampText: createdText
                    transcriptText: text
                    timestampDescription: i18n("Dictation from %1", createdText)
                    copyLabel: i18n("Copy")
                    deleteLabel: i18n("Delete")
                    deleteDescription: i18n("Delete the dictation from encrypted history")
                    showDelete: true
                    deleteEnabled: !appController.history.busy
                    onCopyRequested: function(entryText) {
                        appController.copyText(entryText)
                    }
                    onDeleteRequested: function(id) {
                        root.pendingHistoryId = id
                        deleteHistoryEntryDialog.open()
                    }
                }
                }
            }
        }
    }

    Component {
        id: modelManagerPage

        Kirigami.ScrollablePage {
            id: modelsPage
            objectName: "modelManagerPage"
            title: i18n("Models")
            property string selectedFilter: "recommended"

            header: Controls.Pane {
                padding: Kirigami.Units.largeSpacing

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Controls.TabButton {
                            objectName: "modelFilterRecommended"
                            Layout.fillWidth: true
                            text: i18n("Recommended")
                            checked: modelsPage.selectedFilter === "recommended"
                            checkable: false
                            onClicked: modelsPage.selectedFilter = "recommended"
                        }
                        Controls.TabButton {
                            objectName: "modelFilterEnglish"
                            Layout.fillWidth: true
                            text: i18n("English")
                            checked: modelsPage.selectedFilter === "english"
                            checkable: false
                            onClicked: modelsPage.selectedFilter = "english"
                        }
                        Controls.TabButton {
                            objectName: "modelFilterMultilingual"
                            Layout.fillWidth: true
                            text: i18n("Multilingual")
                            checked: modelsPage.selectedFilter === "multilingual"
                            checkable: false
                            onClicked: modelsPage.selectedFilter = "multilingual"
                        }
                        Controls.TabButton {
                            objectName: "modelFilterAll"
                            Layout.fillWidth: true
                            text: i18n("All")
                            checked: modelsPage.selectedFilter === "all"
                            checkable: false
                            onClicked: modelsPage.selectedFilter = "all"
                        }
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true

                        RowLayout {
                            Kirigami.FormData.label: i18n("Model storage:")
                            Layout.fillWidth: true

                            Controls.TextField {
                                objectName: "modelStoragePath"
                                Layout.fillWidth: true
                                readOnly: true
                                selectByMouse: true
                                text: appController.modelManager.storagePath
                                Accessible.name: i18n("Model storage path")
                            }

                            Controls.ToolButton {
                                objectName: "openModelStorageButton"
                                icon.name: "document-open-folder"
                                onClicked: appController.openModelStorage()
                                Accessible.name: i18n("Open model storage folder")
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: Accessible.name
                            }
                        }
                    }
                }
            }

            footer: FooterContainer {
                id: modelsFooter
                objectName: "modelsFooter"

                contentItem: ColumnLayout {
                    width: modelsFooter.availableWidth
                    Kirigami.InlineMessage {
                        objectName: "modelSetupWarning"
                        Layout.fillWidth: true
                        visible: true
                        type: appController.modelManager.error.length > 0
                            ? Kirigami.MessageType.Error
                            : !appController.modelReady && !appController.modelManager.verificationPending
                            ? Kirigami.MessageType.Warning
                            : Kirigami.MessageType.Information
                        text: appController.modelManager.error.length > 0
                            ? appController.modelManager.error
                            : !appController.modelReady && !appController.modelManager.verificationPending
                            ? i18n("Choose and download a model before using dictation.")
                            : appController.modelManager.status.length > 0
                            ? appController.modelManager.status
                            : i18n("Only open Whisper models from sources you trust. Models are parsed inside Kastword.")
                    }
                }
            }

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                Repeater {
                    model: appController.modelManager.models

                    Controls.Frame {
                        id: modelCard
                        required property var modelData
                        objectName: "modelCard-" + modelData.id
                        Layout.fillWidth: true
                        visible: modelsPage.selectedFilter === "all"
                            || (modelsPage.selectedFilter === "recommended" && modelData.recommended)
                            || (modelsPage.selectedFilter === "english" && modelData.englishOnly)
                            || (modelsPage.selectedFilter === "multilingual" && !modelData.englishOnly)
                        implicitHeight: visible
                            ? modelDetails.implicitHeight + padding * 2 + Kirigami.Units.smallSpacing
                            : 0
                        padding: Kirigami.Units.smallSpacing

                        Rectangle {
                            objectName: "activeModelIndicator-" + modelData.id
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(2, Kirigami.Units.smallSpacing / 2)
                            height: Math.max(0, parent.height - Kirigami.Units.smallSpacing * 2)
                            radius: width / 2
                            color: Kirigami.Theme.highlightColor
                            visible: modelData.active
                        }

                        ColumnLayout {
                            id: modelDetails
                            anchors.fill: parent
                            anchors.leftMargin: modelCard.padding + Kirigami.Units.smallSpacing
                            anchors.rightMargin: modelCard.padding + Kirigami.Units.smallSpacing
                            anchors.topMargin: modelCard.padding
                            anchors.bottomMargin: modelCard.padding + Kirigami.Units.smallSpacing
                            spacing: Kirigami.Units.smallSpacing

                            RowLayout {
                                Layout.fillWidth: true

                                Controls.Label {
                                    Layout.fillWidth: true
                                    font.bold: true
                                    text: modelData.name
                                }

                                Kirigami.Icon {
                                    id: recommendedIcon
                                    objectName: "recommendedModel-" + modelData.id
                                    visible: modelData.recommended
                                    source: "emblem-favorite"
                                    implicitWidth: Kirigami.Units.iconSizes.small
                                    implicitHeight: implicitWidth
                                    Accessible.role: Accessible.StaticText
                                    Accessible.name: i18n("Recommended")

                                    HoverHandler {
                                        id: recommendedHover
                                    }

                                    Controls.ToolTip.visible: recommendedHover.hovered
                                    Controls.ToolTip.text: i18n("Recommended")
                                }
                            }

                            Controls.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: i18n("%1 · %2 · %3", modelData.languageText, modelData.speed, modelData.accuracy)
                            }

                            Controls.Label {
                                objectName: "partialModel-" + modelData.id
                                Layout.fillWidth: true
                                visible: modelData.partial && !modelData.downloading
                                text: i18n("Partial download: %1", modelData.partialSizeText)
                            }

                            Controls.ProgressBar {
                                Layout.fillWidth: true
                                visible: modelData.downloading
                                from: 0
                                to: 1
                                value: appController.modelManager.progress
                                Accessible.name: i18n("Model download progress")
                            }

                            Controls.ProgressBar {
                                Layout.fillWidth: true
                                visible: modelData.verifying
                                indeterminate: true
                                Accessible.name: i18n("Model verification progress")
                                Accessible.description: i18n("The model is being verified")
                            }

                            Item {
                                id: modelFooter
                                objectName: "modelFooter-" + modelData.id
                                Layout.fillWidth: true
                                readonly property real singleRowWidth: modelStatusRow.implicitWidth
                                    + modelActionsRow.implicitWidth + Kirigami.Units.smallSpacing
                                readonly property bool compact: width < singleRowWidth
                                readonly property int columns: compact ? 1 : 2
                                implicitHeight: compact
                                    ? modelStatusRow.implicitHeight + Kirigami.Units.smallSpacing
                                      + modelActionsRow.implicitHeight
                                    : Math.max(modelStatusRow.implicitHeight,
                                               modelActionsRow.implicitHeight)

                                RowLayout {
                                    id: modelStatusRow
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    spacing: Kirigami.Units.smallSpacing

                                    Kirigami.Icon {
                                        objectName: "installedModelStatusIcon-" + modelData.id
                                        visible: modelData.installed
                                        source: modelData.active ? "media-record" : "media-playback-start"
                                        implicitWidth: Kirigami.Units.iconSizes.small
                                        implicitHeight: implicitWidth
                                        Accessible.ignored: true
                                    }

                                    Controls.Label {
                                        objectName: "modelStatus-" + modelData.id
                                        visible: modelData.installed
                                        text: modelData.active ? i18n("In use") : i18n("Downloaded")
                                        font.bold: modelData.active
                                    }

                                    Controls.Button {
                                        objectName: "availableModelStatus-" + modelData.id
                                        readonly property bool showUrlTooltip: hovered || activeFocus
                                        visible: !modelData.installed
                                        flat: true
                                        text: i18n("Available for download")
                                        icon.name: "system-software-install"
                                        display: Controls.AbstractButton.TextBesideIcon
                                        opacity: 0.7
                                        onClicked: root.copyModelUrl(modelData.url)
                                        Accessible.description: modelData.url
                                        Controls.ToolTip.visible: showUrlTooltip
                                        Controls.ToolTip.text: modelData.url
                                    }

                                    Controls.Label {
                                        objectName: "modelSize-" + modelData.id
                                        text: "· " + modelData.sizeText
                                        opacity: modelData.installed ? 1 : 0.7
                                    }
                                }

                                RowLayout {
                                    id: modelActionsRow
                                    anchors.right: parent.right
                                    anchors.top: modelFooter.compact ? modelStatusRow.bottom : parent.top
                                    anchors.topMargin: modelFooter.compact ? Kirigami.Units.smallSpacing : 0
                                    spacing: Kirigami.Units.smallSpacing

                                    Controls.Button {
                                        objectName: "cancelModel-" + modelData.id
                                        visible: modelData.downloading || modelData.verifying
                                        text: i18n("Cancel")
                                        onClicked: appController.modelManager.cancel()
                                    }

                                    Controls.Button {
                                        objectName: "downloadModel-" + modelData.id
                                        visible: !modelData.installed
                                        enabled: !appController.modelManager.busy
                                        highlighted: true
                                        text: i18n("Download")
                                        onClicked: appController.modelManager.download(modelData.id)
                                        Accessible.name: i18n("Download %1", modelData.name)
                                    }

                                    Controls.Button {
                                        objectName: "useModel-" + modelData.id
                                        visible: modelData.installed && !modelData.active
                                        enabled: !appController.modelManager.busy
                                        highlighted: true
                                        text: i18n("Use")
                                        onClicked: appController.modelManager.selectModel(modelData.id)
                                        Accessible.name: i18n("Use %1", modelData.name)
                                    }

                                    Controls.Button {
                                        objectName: "removeModel-" + modelData.id
                                        visible: modelData.installed || modelData.partial
                                        enabled: !appController.modelManager.busy && appController.idle
                                        text: i18n("Remove")
                                        onClicked: {
                                            root.pendingRemovalId = modelData.id
                                            removeDialog.open()
                                        }
                                        Accessible.name: i18n("Remove %1", modelData.name)
                                    }
                                }
                            }
                        }
                    }
                }

                Controls.Button {
                    text: i18n("Use an existing model…")
                    enabled: !appController.modelManager.busy
                    onClicked: modelDialog.open()
                    Accessible.description: i18n("Select an existing compatible Whisper model file")
                }

            }
        }
    }

    Component {
        id: audioInputPage

        Kirigami.ScrollablePage {
            objectName: "audioInputPage"
            title: i18n("Audio")

            footer: FooterContainer {
                id: audioFooter
                objectName: "audioFooter"

                contentItem: ColumnLayout {
                    width: audioFooter.availableWidth
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.InlineMessage {
                        objectName: "audioInputStatus"
                        Layout.fillWidth: true
                        visible: true
                        type: appController.audioInputReady ? Kirigami.MessageType.Positive
                                                             : Kirigami.MessageType.Warning
                        text: appController.audioInputStatus
                        Accessible.name: i18n("Audio input status")
                        Accessible.description: text
                    }

                    Kirigami.InlineMessage {
                        objectName: "audioInputMonitoringError"
                        Layout.fillWidth: true
                        visible: appController.audioInputMonitoringError.length > 0
                        type: Kirigami.MessageType.Error
                        text: i18n("Input level monitoring failed: %1",
                                   appController.audioInputMonitoringError)
                        Accessible.name: i18n("Input level monitoring failed")
                        Accessible.description: text
                        actions: Kirigami.Action {
                            text: i18n("Retry")
                            icon.name: "view-refresh"
                            onTriggered: appController.retryAudioInputMonitoring()
                        }
                    }
                }
            }

            ColumnLayout {
                width: Math.min(parent.width, Kirigami.Units.gridUnit * 44)
                anchors.horizontalCenter: parent.horizontalCenter

                Controls.Label {
                    text: i18n("Microphone:")
                }

                ColumnLayout {
                    id: audioInputList
                    objectName: "audioInputList"
                    Layout.fillWidth: true
                    enabled: appController.audioInputSelectionEnabled
                    Accessible.name: i18n("Audio input device")
                    Accessible.description: i18n("Choose a microphone or follow the system default")

                    Controls.ButtonGroup {
                        id: audioInputGroup
                    }

                    Repeater {
                        model: appController.audioInputs

                        Controls.RadioDelegate {
                            required property var modelData
                            required property int index

                            objectName: "audioInputOption_" + index
                            Layout.fillWidth: true
                            text: modelData.name
                            enabled: modelData.available
                            checked: modelData.id === appController.audioInputId
                            Controls.ButtonGroup.group: audioInputGroup
                            onClicked: appController.audioInputId = modelData.id
                            Accessible.description: modelData.id === ":none"
                                ? i18n("Disable audio input")
                                : modelData.available
                                ? i18n("Use %1 for dictation", modelData.name)
                                : i18n("This microphone is unavailable")
                        }
                    }
                }

                Controls.Label {
                    text: i18n("Input level:")
                }

                Controls.ProgressBar {
                    objectName: "audioInputLevel"
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: appController.level
                    Accessible.name: i18n("Microphone level")
                    Accessible.description: i18n("Current level from the selected microphone")
                }

                Controls.Label {
                    objectName: "audioInputPrivacyNote"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: appController.audioInputId === ":none"
                        ? i18n("Audio input is disabled.")
                        : appController.recording
                        ? i18n("Dictation is recording audio for local transcription.")
                        : i18n("Level monitoring only; audio is not saved. A specific microphone is never replaced automatically if it becomes unavailable.")
                }
            }
        }
    }

    Component {
        id: settingsPage

        Kirigami.ScrollablePage {
            objectName: "settingsPage"
            title: i18n("Settings")

            footer: FooterContainer {
                id: settingsFooter
                objectName: "settingsFooter"
                visible: autoPasteCheckBox.checked || root.shortcutChangeFailed

                contentItem: ColumnLayout {
                    width: settingsFooter.availableWidth
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.InlineMessage {
                        objectName: "autoPasteWarning"
                        Layout.fillWidth: true
                        visible: autoPasteCheckBox.checked
                        type: Kirigami.MessageType.Warning
                        text: i18n("Automatic paste sends each selected shortcut to the focused application after a short delay. On Wayland, Kastword cannot verify that focus stayed unchanged.")
                    }

                    Kirigami.InlineMessage {
                        objectName: "shortcutError"
                        Layout.fillWidth: true
                        visible: root.shortcutChangeFailed
                        type: Kirigami.MessageType.Error
                        text: i18n("The global shortcut could not be changed. Choose another shortcut.")
                    }
                }
            }

            Kirigami.FormLayout {
                width: parent.width

                Controls.ComboBox {
                    id: languageComboBox
                    objectName: "languageComboBox"
                    Kirigami.FormData.label: i18n("Language:")
                    Layout.fillWidth: true
                    model: appController.availableLanguages
                    textRole: "name"
                    valueRole: "code"
                    currentIndex: Math.max(0, indexOfValue(appController.language))
                    onActivated: appController.language = currentValue
                    Accessible.name: i18n("Dictation language")
                    Accessible.description: i18n("Language supported by the selected Whisper model")
                }

                Controls.SpinBox {
                    Kirigami.FormData.label: i18n("Maximum recording:")
                    from: 1
                    to: 60
                    value: appController.recordingLimitMinutes
                    textFromValue: function(value) { return i18np("%1 minute", "%1 minutes", value) }
                    onValueModified: appController.recordingLimitMinutes = value
                }

                Controls.CheckBox {
                    id: autoPasteCheckBox
                    objectName: "autoPasteCheckBox"
                    Kirigami.FormData.label: i18n("Output:")
                    text: i18n("Paste automatically")
                    checked: appController.autoPaste
                    onToggled: appController.autoPaste = checked
                }

                Controls.CheckBox {
                    objectName: "pasteCtrlVCheckBox"
                    Kirigami.FormData.label: i18n("Paste shortcuts:")
                    text: i18n("Ctrl+V")
                    visible: autoPasteCheckBox.checked
                    checked: appController.pasteCtrlV
                    onToggled: appController.pasteCtrlV = checked
                }

                Controls.CheckBox {
                    objectName: "pasteCtrlShiftVCheckBox"
                    text: i18n("Ctrl+Shift+V")
                    visible: autoPasteCheckBox.checked
                    checked: appController.pasteCtrlShiftV
                    onToggled: appController.pasteCtrlShiftV = checked
                }

                Controls.CheckBox {
                    objectName: "pasteShiftInsertCheckBox"
                    text: i18n("Shift+Insert")
                    visible: autoPasteCheckBox.checked
                    checked: appController.pasteShiftInsert
                    onToggled: appController.pasteShiftInsert = checked
                }

                KQuickControls.KeySequenceItem {
                    id: shortcutEditor
                    objectName: "shortcutEditor"
                    Kirigami.FormData.label: i18n("Shortcut:")
                    multiKeyShortcutsAllowed: false
                    onKeySequenceModified: {
                        root.shortcutChangeFailed = !appController.setShortcut(keySequence)
                        if (root.shortcutChangeFailed)
                            keySequence = appController.shortcut
                    }
                    Accessible.name: i18n("Global dictation shortcut")

                    Binding {
                        target: shortcutEditor
                        property: "keySequence"
                        value: appController.shortcut
                    }
                }

            }
        }
    }

    Component {
        id: dictationPage

        Kirigami.Page {
            title: i18n("Dictation")

            footer: FooterContainer {
                id: dictationFooter
                objectName: "dictationFooter"

                contentItem: ColumnLayout {
                    width: dictationFooter.availableWidth
                    Kirigami.InlineMessage {
                        objectName: "statusMessage"
                        Layout.fillWidth: true
                        visible: true
                        text: appController.status
                        type: appController.recording ? Kirigami.MessageType.Positive
                                                       : Kirigami.MessageType.Information
                        Accessible.name: i18n("Dictation status")
                        Accessible.description: appController.status
                    }
                }
            }

            ColumnLayout {
                id: mainContent
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Kirigami.Units.largeSpacing

            Controls.Button {
                objectName: "dictationButton"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                highlighted: true
                text: appController.recording ? i18n("Stop dictation")
                    : appController.transcribing ? i18n("Transcribing…")
                    : i18n("Start dictation")
                icon.name: appController.recording ? "media-playback-stop" : "audio-input-microphone"
                enabled: appController.dictationActionEnabled
                onClicked: appController.toggle()
                Accessible.name: text
                Accessible.description: appController.recording
                    ? i18n("Stop recording and begin local transcription")
                    : appController.transcribing
                    ? i18n("Local transcription is in progress")
                    : !appController.modelReady
                    ? i18n("Choose a speech model before starting dictation")
                    : !appController.audioInputReady
                    ? i18n("Choose an available microphone before starting dictation")
                    : i18n("Begin recording audio for local transcription")
            }

            Item {
                id: activitySlot
                objectName: "activitySlot"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(dictationProgress.implicitHeight,
                                                 shortcutLabel.implicitHeight)

                Controls.ProgressBar {
                    id: dictationProgress
                    objectName: "dictationProgress"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: appController.recording || appController.transcribing
                    from: 0
                    to: 1
                    value: appController.level
                    indeterminate: appController.transcribing
                    Accessible.name: appController.transcribing
                        ? i18n("Transcription progress")
                        : i18n("Microphone level")
                    Accessible.description: appController.transcribing
                        ? i18n("Local transcription is in progress")
                        : i18n("Current microphone input level")
                }

                Controls.Label {
                    id: shortcutLabel
                    anchors.centerIn: parent
                    visible: appController.idle
                    text: i18n("Global shortcut: %1", appController.shortcutText)
                    opacity: 0.7
                }
            }

            ColumnLayout {
                id: transcriptPanel
                Layout.fillWidth: true
                visible: appController.transcript.length > 0
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    Layout.fillWidth: true
                    font.bold: true
                    text: i18n("Last transcription")
                }

                Controls.Label {
                    objectName: "transcriptText"
                    Layout.fillWidth: true
                    text: appController.transcript
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    Accessible.name: i18n("Last transcription")
                    Accessible.description: i18n("Preview of the most recent dictation")
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Kirigami.Units.smallSpacing

                    Controls.ToolButton {
                        objectName: "copyTranscriptButton"
                        icon.name: "edit-copy"
                        text: i18n("Copy")
                        display: Controls.AbstractButton.TextBesideIcon
                        onClicked: appController.copyTranscript()
                        Accessible.name: text
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: text
                    }

                    Controls.ToolButton {
                        objectName: "expandTranscriptButton"
                        icon.name: "view-fullscreen"
                        text: i18n("View")
                        display: Controls.AbstractButton.TextBesideIcon
                        onClicked: transcriptDialog.open()
                        Accessible.name: i18n("Show full transcription")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: Accessible.name
                    }

                    Controls.ToolButton {
                        objectName: "clearTranscriptButton"
                        icon.name: "edit-clear"
                        text: i18n("Clear")
                        display: Controls.AbstractButton.TextBesideIcon
                        onClicked: appController.forgetTranscript()
                        Accessible.name: i18n("Clear transcription")
                        Accessible.description: i18n("Remove the transcription from Kastword and matching current clipboards")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.text: Accessible.name
                    }
                }
            }

            ColumnLayout {
                id: recentHistoryPanel
                objectName: "recentHistoryPanel"
                Layout.fillWidth: true
                visible: appController.history.enabled
                    && appController.history.recentEntries.length > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Label {
                        Layout.fillWidth: true
                        font.bold: true
                        text: i18n("Recent history")
                    }
                    Controls.ToolButton {
                        text: i18n("View all")
                        onClicked: root.currentView = 1
                        Accessible.name: i18n("View all dictation history")
                    }
                }

                Repeater {
                    model: appController.history.recentEntries
                    HistoryEntryCard {
                        required property var modelData
                        Layout.fillWidth: true
                        entryIdentifier: modelData.id || ""
                        timestampText: modelData.createdText
                        transcriptText: modelData.text
                        timestampDescription: i18n("Dictation from %1", modelData.createdText)
                        copyLabel: i18n("Copy")
                        onCopyRequested: function(entryText) {
                            appController.copyText(entryText)
                        }
                    }
                }
            }
            }
        }
    }

    pageStack.initialPage: Kirigami.Page {
        id: mainPage
        padding: 0

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Controls.Pane {
                id: navigationPane
                objectName: "navigationPane"
                readonly property bool compact: mainPage.width < Math.max(680,
                                                                           Kirigami.Units.gridUnit * 35)

                Layout.fillHeight: true
                Layout.preferredWidth: compact ? Kirigami.Units.gridUnit * 3
                                               : Kirigami.Units.gridUnit * 8
                padding: Kirigami.Units.smallSpacing

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing

                    Controls.ButtonGroup {
                        id: navigationGroup
                    }

                    Controls.TabButton {
                        id: dictationTab
                        objectName: "dictationTab"
                        readonly property string label: i18n("Dictation")
                        Layout.fillWidth: true
                        text: navigationPane.compact ? "" : label
                        icon.name: "audio-input-microphone"
                        Accessible.name: label
                        display: navigationPane.compact ? Controls.AbstractButton.IconOnly
                                                        : Controls.AbstractButton.TextBesideIcon
                        checked: root.currentView === 0
                        Controls.ButtonGroup.group: navigationGroup
                        KeyNavigation.up: settingsTab
                        KeyNavigation.down: historyTab
                        onClicked: root.currentView = 0
                        Controls.ToolTip.visible: hovered && navigationPane.compact
                        Controls.ToolTip.text: label
                    }

                    Controls.TabButton {
                        id: historyTab
                        objectName: "historyTab"
                        readonly property string label: i18n("History")
                        Layout.fillWidth: true
                        text: navigationPane.compact ? "" : label
                        icon.name: "view-history"
                        Accessible.name: label
                        display: navigationPane.compact ? Controls.AbstractButton.IconOnly
                                                        : Controls.AbstractButton.TextBesideIcon
                        checked: root.currentView === 1
                        Controls.ButtonGroup.group: navigationGroup
                        KeyNavigation.up: dictationTab
                        KeyNavigation.down: modelsTab
                        onClicked: root.currentView = 1
                        Controls.ToolTip.visible: hovered && navigationPane.compact
                        Controls.ToolTip.text: label
                    }

                    Controls.TabButton {
                        id: modelsTab
                        objectName: "modelsTab"
                        readonly property string label: i18n("Models")
                        Layout.fillWidth: true
                        text: navigationPane.compact ? "" : label
                        icon.name: "system-software-install"
                        Accessible.name: label
                        display: navigationPane.compact ? Controls.AbstractButton.IconOnly
                                                        : Controls.AbstractButton.TextBesideIcon
                        checked: root.currentView === 2
                        Controls.ButtonGroup.group: navigationGroup
                        KeyNavigation.up: historyTab
                        KeyNavigation.down: audioInputTab
                        onClicked: root.currentView = 2
                        Controls.ToolTip.visible: hovered && navigationPane.compact
                        Controls.ToolTip.text: label
                    }

                    Controls.TabButton {
                        id: audioInputTab
                        objectName: "audioInputTab"
                        readonly property string label: i18n("Audio")
                        Layout.fillWidth: true
                        text: navigationPane.compact ? "" : label
                        icon.name: "audio-input-microphone"
                        Accessible.name: label
                        display: navigationPane.compact ? Controls.AbstractButton.IconOnly
                                                        : Controls.AbstractButton.TextBesideIcon
                        checked: root.currentView === 3
                        Controls.ButtonGroup.group: navigationGroup
                        KeyNavigation.up: modelsTab
                        KeyNavigation.down: settingsTab
                        onClicked: root.currentView = 3
                        Controls.ToolTip.visible: hovered && navigationPane.compact
                        Controls.ToolTip.text: label
                    }

                    Controls.TabButton {
                        id: settingsTab
                        objectName: "settingsTab"
                        readonly property string label: i18n("Settings")
                        Layout.fillWidth: true
                        text: navigationPane.compact ? "" : label
                        icon.name: "preferences-system"
                        Accessible.name: label
                        display: navigationPane.compact ? Controls.AbstractButton.IconOnly
                                                        : Controls.AbstractButton.TextBesideIcon
                        checked: root.currentView === 4
                        Controls.ButtonGroup.group: navigationGroup
                        KeyNavigation.up: audioInputTab
                        KeyNavigation.down: dictationTab
                        onClicked: root.currentView = 4
                        Controls.ToolTip.visible: hovered && navigationPane.compact
                        Controls.ToolTip.text: label
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillHeight: true
            }

            StackLayout {
                objectName: "mainViewStack"
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentView

                Loader {
                    sourceComponent: dictationPage
                }

                Loader {
                    sourceComponent: historyPage
                }

                Loader {
                    sourceComponent: modelManagerPage
                }

                Loader {
                    sourceComponent: audioInputPage
                }

                Loader {
                    sourceComponent: settingsPage
                }
            }
        }
    }
}
