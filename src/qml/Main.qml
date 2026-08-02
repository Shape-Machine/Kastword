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
    visible: false
    title: i18n("Kastword")

    function selectModel(url) {
        appController.setModelUrl(url)
    }

    FileDialog {
        id: modelDialog
        title: i18n("Select a Whisper model")
        nameFilters: [i18n("Whisper models (*.bin)"), i18n("All files (*)")]
        onAccepted: root.selectModel(selectedFile)
    }

    Component {
        id: settingsPage

        Kirigami.ScrollablePage {
            title: i18n("Settings")

            Kirigami.FormLayout {
                width: parent.width

                RowLayout {
                    Kirigami.FormData.label: i18n("Model:")

                    Controls.TextField {
                        objectName: "modelPathField"
                        Layout.fillWidth: true
                        text: appController.modelPath
                        placeholderText: i18n("Path to ggml model")
                        onEditingFinished: appController.modelPath = text
                        Accessible.name: i18n("Whisper model path")
                    }

                    Controls.Button {
                        objectName: "modelBrowseButton"
                        text: i18n("Browse…")
                        onClicked: modelDialog.open()
                        Accessible.name: i18n("Browse for Whisper model")
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
                enabled: !appController.transcribing
                onClicked: appController.toggle()
                Accessible.name: text
                Accessible.description: appController.recording
                    ? i18n("Stop recording and begin local transcription")
                    : appController.transcribing
                    ? i18n("Local transcription is in progress")
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
