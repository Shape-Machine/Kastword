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
    width: 480
    height: 280
    minimumWidth: 380
    minimumHeight: 280
    visible: false
    title: i18n("Kastword")

    FileDialog {
        id: modelDialog
        title: i18n("Select a Whisper model")
        nameFilters: [i18n("Whisper models (*.bin)"), i18n("All files (*)")]
        onAccepted: appController.modelPath = selectedFile.toString().replace(/^file:\/\//, "")
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
                        Layout.fillWidth: true
                        text: appController.modelPath
                        placeholderText: i18n("Path to ggml model")
                        onEditingFinished: appController.modelPath = text
                    }

                    Controls.Button {
                        text: i18n("Browse…")
                        onClicked: modelDialog.open()
                    }
                }

                Controls.ComboBox {
                    Kirigami.FormData.label: i18n("Language:")
                    Layout.fillWidth: true
                    model: ["en", "auto", "de", "fr", "es", "nl", "it", "pt"]
                    currentIndex: Math.max(0, model.indexOf(appController.language))
                    onActivated: appController.language = currentText
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

    pageStack.initialPage: Kirigami.Page {
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
                Layout.fillWidth: true
                visible: true
                text: appController.status
                type: appController.recording ? Kirigami.MessageType.Positive
                      : Kirigami.MessageType.Information
            }

            Controls.Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                highlighted: true
                text: appController.recording ? i18n("Stop dictation")
                    : appController.transcribing ? i18n("Transcribing…")
                    : i18n("Start dictation")
                icon.name: appController.recording ? "media-playback-stop" : "audio-input-microphone"
                enabled: !appController.transcribing
                onClicked: appController.toggle()
            }

            Controls.ProgressBar {
                Layout.fillWidth: true
                visible: appController.recording || appController.transcribing
                from: 0
                to: 1
                value: appController.level
                indeterminate: appController.transcribing
            }

            Controls.Label {
                Layout.alignment: Qt.AlignHCenter
                visible: appController.idle
                text: i18n("Global shortcut: %1").arg(appController.shortcutText)
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
                            text: appController.transcript
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                        }
                    }

                    Controls.Button {
                        Layout.alignment: Qt.AlignRight
                        text: i18n("Clear transcription")
                        icon.name: "edit-clear-history"
                        onClicked: appController.forgetTranscript()
                    }
                }
            }
        }
    }
}
