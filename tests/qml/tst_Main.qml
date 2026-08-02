// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "Main"
    when: windowShown

    property var applicationWindow

    Component {
        id: applicationComponent

        Loader {
            source: "qrc:/Main.qml"
        }
    }

    function init() {
        const loader = createTemporaryObject(applicationComponent, testCase)
        verify(loader)
        tryCompare(loader, "status", Loader.Ready)
        applicationWindow = loader.item
        verify(applicationWindow)
        applicationWindow.visible = true
        waitForRendering(applicationWindow.contentItem)
        appController.setTestState(false, false)
    }

    function cleanup() {
        if (applicationWindow) {
            applicationWindow.visible = false
            wait(50)
            applicationWindow.close()
        }
        applicationWindow = null
    }

    function test_modelUrlReachesCppBoundary() {
        applicationWindow.selectModel("file:///tmp/My%20Model%25-%E6%97%A5%E6%9C%AC%E8%AA%9E.bin")
        compare(appController.modelPath, "/tmp/My Model%-日本語.bin")
    }

    function test_accessibleStateDescriptions() {
        const status = findChild(applicationWindow.contentItem, "statusMessage")
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        const progress = findChild(applicationWindow.contentItem, "dictationProgress")
        verify(status)
        verify(button)
        verify(progress)

        compare(status.Accessible.name, "Dictation status")
        compare(button.Accessible.name, "Start dictation")
        compare(progress.Accessible.name, "Microphone level")
        verify(progress.Accessible.description.length > 0)

        appController.setTestState(false, true)
        tryCompare(progress, "indeterminate", true)
        compare(button.Accessible.name, "Transcribing…")
        compare(progress.Accessible.name, "Transcription progress")
        compare(status.Accessible.description, "Transcribing")
    }

    function test_modelControlsHaveAccessibleNames() {
        applicationWindow.pageStack.currentItem.actions[0].trigger()
        const modelPath = findChild(applicationWindow.contentItem, "modelPathField")
        const browse = findChild(applicationWindow.contentItem, "modelBrowseButton")
        verify(modelPath)
        verify(browse)
        compare(modelPath.Accessible.name, "Whisper model path")
        compare(browse.Accessible.name, "Browse for Whisper model")
    }

    function test_primaryActionSupportsKeyboard() {
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(button)
        button.forceActiveFocus()
        verify(button.activeFocus)

        const previousCount = appController.toggleCount
        keyClick(Qt.Key_Space)
        compare(appController.toggleCount, previousCount + 1)
    }

    function test_scrollableLayoutKeepsPrimaryActionReachable() {
        const page = applicationWindow.pageStack.currentItem
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(page)
        verify(button)
        verify(page.contentHeight >= button.y + button.height)
        verify(button.width <= page.width)
    }
}
