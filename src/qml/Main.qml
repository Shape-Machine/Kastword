// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later
// qmllint disable unqualified
// KLocalizedQmlContext and AppController intentionally expose context properties.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    objectName: "mainWindow"
    width: 480
    height: 280
    minimumWidth: 380
    minimumHeight: 280
    visible: appController.modelSetupRequired
    title: i18n("Kastword")

    function selectModel(url) {
        appController.setModelUrl(url)
    }

    function openModelManager() {
        if (!root.pageStack.currentItem || root.pageStack.currentItem.objectName !== "modelManagerPage")
            root.pageStack.push(modelManagerPage)
    }

    property string pendingRemovalId: ""

    Connections {
        target: appController
        function onModelSetupRequested() {
            root.show()
            root.openModelManager()
        }
    }

    Component.onCompleted: {
        if (appController.modelSetupRequired)
            root.openModelManager()
    }

    FileDialog {
        id: modelDialog
        title: i18n("Select a Whisper model")
        nameFilters: [i18n("Whisper models (*.bin)"), i18n("All files (*)")]
        onAccepted: root.selectModel(selectedFile)
    }

    MessageDialog {
        id: removeDialog
        title: i18n("Remove speech model?")
        text: i18n("The model file will be deleted from this user account.")
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: {
            appController.removeModel(root.pendingRemovalId)
            root.pendingRemovalId = ""
        }
        onRejected: root.pendingRemovalId = ""
    }

    Component {
        id: modelManagerPage

        Kirigami.ScrollablePage {
            objectName: "modelManagerPage"
            title: i18n("Speech models")

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                Kirigami.InlineMessage {
                    objectName: "modelSetupWarning"
                    Layout.fillWidth: true
                    visible: !appController.modelReady
                        && !appController.modelManager.verificationPending
                    type: Kirigami.MessageType.Warning
                    text: i18n("Choose and download a model before using dictation.")
                }

                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: i18n("Models are downloaded only when you request them. Transcription remains offline after the download.")
                }

                Controls.ComboBox {
                    id: modelLanguageFilter
                    objectName: "modelLanguageFilter"
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "code"
                    model: [
                        { text: i18n("Recommended"), code: "recommended" },
                        { text: i18n("English-only models"), code: "english" },
                        { text: i18n("Multilingual models"), code: "multilingual" },
                        { text: i18n("All models"), code: "all" }
                    ]
                    Accessible.name: i18n("Filter speech models")
                }

                Repeater {
                    model: appController.modelManager.models

                    Controls.Frame {
                        required property var modelData
                        objectName: "modelCard-" + modelData.id
                        Layout.fillWidth: true
                        visible: modelLanguageFilter.currentValue === "all"
                            || (modelLanguageFilter.currentValue === "recommended" && modelData.recommended)
                            || (modelLanguageFilter.currentValue === "english" && modelData.englishOnly)
                            || (modelLanguageFilter.currentValue === "multilingual" && !modelData.englishOnly)
                        implicitHeight: visible ? modelDetails.implicitHeight + padding * 2 : 0

                        ColumnLayout {
                            id: modelDetails
                            anchors.fill: parent

                            RowLayout {
                                Layout.fillWidth: true

                                Controls.Label {
                                    Layout.fillWidth: true
                                    font.bold: true
                                    text: modelData.name + (modelData.recommended ? i18n(" — Recommended") : "")
                                }

                                Controls.Label {
                                    text: modelData.sizeText
                                }
                            }

                            Controls.Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: i18n("%1 · %2 · %3", modelData.languageText, modelData.speed, modelData.accuracy)
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

                            RowLayout {
                                Layout.alignment: Qt.AlignRight

                                Controls.Button {
                                    objectName: "cancelModel-" + modelData.id
                                    visible: modelData.downloading || modelData.verifying
                                    text: i18n("Cancel")
                                    onClicked: appController.modelManager.cancel()
                                }

                                Controls.Button {
                                    visible: !modelData.installed
                                    enabled: !appController.modelManager.busy
                                    text: i18n("Download")
                                    onClicked: appController.modelManager.download(modelData.id)
                                    Accessible.name: i18n("Download %1", modelData.name)
                                }

                                Controls.Button {
                                    visible: modelData.installed && !modelData.active
                                    enabled: !appController.modelManager.busy
                                    text: i18n("Use")
                                    onClicked: appController.modelManager.selectModel(modelData.id)
                                    Accessible.name: i18n("Use %1", modelData.name)
                                }

                                Controls.Label {
                                    visible: modelData.active
                                    text: i18n("In use")
                                }

                                Controls.Button {
                                    visible: modelData.installed
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

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: appController.modelManager.error.length > 0
                    type: Kirigami.MessageType.Error
                    text: appController.modelManager.error
                }

                Controls.Label {
                    Layout.fillWidth: true
                    visible: appController.modelManager.status.length > 0
                    wrapMode: Text.Wrap
                    text: appController.modelManager.status
                }

                Controls.Button {
                    text: i18n("Use an existing model…")
                    enabled: !appController.modelManager.busy
                    onClicked: modelDialog.open()
                    Accessible.description: i18n("Select an existing compatible Whisper model file")
                }

                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    opacity: 0.7
                    text: i18n("Model storage: %1", appController.modelManager.storagePath)
                }
            }
        }
    }

    Component {
        id: settingsPage

        Kirigami.ScrollablePage {
            title: i18n("Settings")

            Kirigami.FormLayout {
                width: parent.width

                RowLayout {
                    Kirigami.FormData.label: i18n("Model:")

                    Controls.Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: appController.modelReady ? appController.modelPath : i18n("No model selected")
                    }

                    Controls.Button {
                        objectName: "manageModelsButton"
                        text: i18n("Manage…")
                        onClicked: root.pageStack.push(modelManagerPage)
                        Accessible.name: i18n("Manage speech models")
                    }
                }

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
                    Kirigami.FormData.label: i18n("Output:")
                    text: i18n("Paste automatically")
                    checked: appController.autoPaste
                    onToggled: appController.autoPaste = checked
                }

                Kirigami.InlineMessage {
                    Kirigami.FormData.isSection: true
                    Layout.fillWidth: true
                    visible: autoPasteCheckBox.checked
                    type: Kirigami.MessageType.Warning
                    text: i18n("Automatic paste sends keystrokes to the focused application after a short delay. On Wayland, Kastword cannot verify that focus stayed unchanged.")
                }

                Kirigami.InlineMessage {
                    Kirigami.FormData.isSection: true
                    Layout.fillWidth: true
                    visible: true
                    type: Kirigami.MessageType.Information
                    text: i18n("Only open Whisper models from sources you trust. Models are parsed inside Kastword.")
                }

                Controls.Label {
                    Kirigami.FormData.label: i18n("Shortcut:")
                    text: appController.shortcutText
                }
            }
        }
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        title: i18n("Offline dictation")
        actions: [
            Kirigami.Action {
                text: i18n("Settings")
                icon.name: "settings-configure"
                onTriggered: root.pageStack.push(settingsPage)
            }
        ]

        ColumnLayout {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Kirigami.Units.largeSpacing

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

            Controls.Button {
                objectName: "dictationButton"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                highlighted: true
                text: appController.recording ? i18n("Stop dictation")
                    : appController.transcribing ? i18n("Transcribing…")
                    : i18n("Start dictation")
                icon.name: appController.recording ? "media-playback-stop" : "audio-input-microphone"
                enabled: appController.modelReady && !appController.transcribing
                onClicked: appController.toggle()
                Accessible.name: text
                Accessible.description: appController.recording
                    ? i18n("Stop recording and begin local transcription")
                    : appController.transcribing
                    ? i18n("Local transcription is in progress")
                    : !appController.modelReady
                    ? i18n("Choose a speech model before starting dictation")
                    : i18n("Begin recording audio for local transcription")
            }

            Controls.ProgressBar {
                objectName: "dictationProgress"
                Layout.fillWidth: true
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
                Layout.alignment: Qt.AlignHCenter
                visible: appController.idle
                text: i18n("Global shortcut: %1", appController.shortcutText)
                opacity: 0.7
            }

            Controls.GroupBox {
                id: transcriptPanel
                Layout.fillWidth: true
                visible: appController.transcript.length > 0
                title: i18n("Last transcription")

                ColumnLayout {
                    anchors.fill: parent

                    Controls.ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 6

                        Controls.TextArea {
                            objectName: "transcriptText"
                            text: appController.transcript
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            Accessible.name: i18n("Last transcription")
                            Accessible.description: i18n("Read-only text from the most recent dictation")
                        }
                    }

                    Controls.Button {
                        Layout.alignment: Qt.AlignRight
                        text: i18n("Clear transcription")
                        icon.name: "edit-clear-history"
                        onClicked: appController.forgetTranscript()
                        Accessible.name: text
                        Accessible.description: i18n("Remove the transcription from Kastword and matching current clipboards")
                    }
                }
            }
        }
    }
}
