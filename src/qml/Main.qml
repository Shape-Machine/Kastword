import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    width: 560
    height: 520
    minimumWidth: 420
    minimumHeight: 420
    visible: true
    title: i18n("Kastword")

    FileDialog {
        id: modelDialog
        title: i18n("Select a Whisper model")
        nameFilters: [i18n("Whisper models (*.bin)"), i18n("All files (*)")]
        onAccepted: appController.modelPath = selectedFile.toString().replace(/^file:\/\//, "")
    }

    pageStack.initialPage: Kirigami.Page {
        title: i18n("Offline dictation")

        ColumnLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.largeSpacing

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: true
                text: appController.status
                type: appController.state === "recording" ? Kirigami.MessageType.Positive
                      : appController.status.toLowerCase().includes("could") ? Kirigami.MessageType.Error
                      : Kirigami.MessageType.Information
            }

            Controls.ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: appController.level
                indeterminate: appController.state === "transcribing"
            }

            Controls.Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 220
                Layout.preferredHeight: 64
                text: appController.state === "recording" ? i18n("Stop and transcribe")
                    : appController.state === "transcribing" ? i18n("Transcribing…")
                    : i18n("Start dictation")
                icon.name: appController.state === "recording" ? "media-playback-stop" : "audio-input-microphone"
                enabled: appController.state !== "transcribing"
                onClicked: appController.toggle()
            }

            Controls.Label {
                Layout.alignment: Qt.AlignHCenter
                text: i18n("Global shortcut: Meta+Shift+D")
                opacity: 0.7
            }

            Kirigami.FormLayout {
                Layout.fillWidth: true

                RowLayout {
                    Kirigami.FormData.label: i18n("Model:")
                    Controls.TextField {
                        Layout.fillWidth: true
                        text: appController.modelPath
                        placeholderText: i18n("Path to ggml model")
                        onEditingFinished: appController.modelPath = text
                    }
                    Controls.Button { text: i18n("Browse…"); onClicked: modelDialog.open() }
                }

                Controls.ComboBox {
                    Kirigami.FormData.label: i18n("Language:")
                    model: ["en", "auto", "de", "fr", "es", "nl", "it", "pt"]
                    currentIndex: Math.max(0, model.indexOf(appController.language))
                    onActivated: appController.language = currentText
                }

                Controls.CheckBox {
                    Kirigami.FormData.label: i18n("Output:")
                    text: i18n("Paste automatically when supported")
                    checked: appController.autoPaste
                    onToggled: appController.autoPaste = checked
                }
            }

            Controls.GroupBox {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: i18n("Last transcription")
                Controls.ScrollView {
                    anchors.fill: parent
                    Controls.TextArea {
                        text: appController.transcript
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        placeholderText: i18n("Your transcription will appear here.")
                    }
                }
            }
        }
    }
}
