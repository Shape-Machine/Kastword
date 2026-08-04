// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
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
        appController.setRestoringModel(false)
        appController.setModelReady(true)
        const loader = createTemporaryObject(applicationComponent, testCase)
        verify(loader)
        tryCompare(loader, "status", Loader.Ready)
        applicationWindow = loader.item
        verify(applicationWindow)
        applicationWindow.visible = true
        waitForRendering(applicationWindow.contentItem)
        appController.setTestState(false, false)
    }

    function modelPage() {
        const page = findChild(applicationWindow.contentItem, "modelManagerPage")
        verify(page)
        return page
    }

    function settingsPage() {
        const page = findChild(applicationWindow.contentItem, "settingsPage")
        verify(page)
        return page
    }

    function test_missingModelOpensSetupAndDisablesDictation() {
        appController.setModelReady(false)
        tryCompare(applicationWindow, "currentView", 1)
        compare(modelPage().objectName, "modelManagerPage")
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(button)
        compare(button.enabled, false)
    }

    function test_savedModelVerificationDoesNotOpenSetup() {
        appController.setRestoringModel(true)
        appController.setModelReady(false)
        compare(applicationWindow.currentView, 0)
        applicationWindow.openModelManager()
        compare(applicationWindow.currentView, 1)
        const models = modelPage()
        const warning = findChild(models, "modelSetupWarning")
        verify(warning)
        compare(warning.visible, false)

        appController.setRestoringModel(false)
        compare(applicationWindow.currentView, 1)
    }

    function cleanup() {
        appController.modelManager.setVerifyingId("")
        appController.modelManager.setPartialId("")
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

    function test_modelReadinessDoesNotHideExplicitlyShownWindow() {
        compare(applicationWindow.visible, true)
        appController.setModelReady(false)
        appController.setModelReady(true)
        compare(applicationWindow.visible, true)
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

    function test_activitySlotKeepsStableHeight() {
        const slot = findChild(applicationWindow.contentItem, "activitySlot")
        verify(slot)
        const idleHeight = slot.height

        appController.setTestState(true, false)
        compare(slot.height, idleHeight)

        appController.setTestState(false, true)
        compare(slot.height, idleHeight)
    }

    function test_modelControlsHaveAccessibleNames() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelLanguageFilter")
        const warning = findChild(models, "externalModelWarning")
        verify(filter)
        verify(warning)
        compare(filter.Accessible.name, "Filter speech models")
        compare(warning.visible, true)
    }

    function test_modelFiltersDefaultToRecommendedAndSeparateCapabilities() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelLanguageFilter")
        const english = findChild(models, "modelCard-base.en")
        const recommendedMultilingual = findChild(models, "modelCard-small")
        const otherMultilingual = findChild(models, "modelCard-tiny")
        verify(filter)
        verify(english)
        verify(recommendedMultilingual)
        verify(otherMultilingual)

        compare(filter.currentValue, "recommended")
        compare(english.visible, true)
        compare(recommendedMultilingual.visible, true)
        compare(otherMultilingual.visible, false)

        filter.currentIndex = filter.indexOfValue("multilingual")
        compare(english.visible, false)
        compare(recommendedMultilingual.visible, true)
        compare(otherMultilingual.visible, true)

        filter.currentIndex = filter.indexOfValue("english")
        compare(english.visible, true)
        compare(recommendedMultilingual.visible, false)
        compare(otherMultilingual.visible, false)
    }

    function test_verificationCanBeCancelled() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelLanguageFilter")
        verify(filter)
        filter.currentIndex = filter.indexOfValue("all")
        appController.modelManager.setVerifyingId("tiny")

        const cancel = findChild(models, "cancelModel-tiny")
        verify(cancel)
        tryCompare(cancel, "visible", true)
        compare(cancel.text, "Cancel")
    }

    function test_partialDownloadCanBeRemoved() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelLanguageFilter")
        verify(filter)
        filter.currentIndex = filter.indexOfValue("all")
        appController.modelManager.setPartialId("tiny")

        const partial = findChild(models, "partialModel-tiny")
        const remove = findChild(models, "removeModel-tiny")
        verify(partial)
        verify(remove)
        tryCompare(partial, "visible", true)
        compare(partial.text, "Partial download: 42 MiB")
        compare(remove.visible, true)
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

    function test_verticalTabsSwitchViewsInOneWindow() {
        const dictationTab = findChild(applicationWindow.contentItem, "dictationTab")
        const modelsTab = findChild(applicationWindow.contentItem, "modelsTab")
        const settingsTab = findChild(applicationWindow.contentItem, "settingsTab")
        verify(dictationTab)
        verify(modelsTab)
        verify(settingsTab)
        compare(dictationTab.checked, true)

        modelsTab.clicked()
        compare(applicationWindow.currentView, 1)
        compare(modelsTab.checked, true)
        compare(modelPage().visible, true)

        settingsTab.clicked()
        compare(applicationWindow.currentView, 2)
        compare(settingsTab.checked, true)
        compare(settingsPage().visible, true)
        compare(applicationWindow.pageStack.depth, 1)
    }

    function test_navigationAdaptsToMinimumWidth() {
        const dictationTab = findChild(applicationWindow.contentItem, "dictationTab")
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        const navigation = findChild(applicationWindow.contentItem, "navigationPane")
        verify(dictationTab)
        verify(button)
        verify(navigation)

        applicationWindow.width = applicationWindow.minimumWidth
        tryCompare(applicationWindow, "width", applicationWindow.minimumWidth)
        tryCompare(navigation, "compact", true)
        compare(dictationTab.display, Controls.AbstractButton.IconOnly)
        verify(button.width <= applicationWindow.pageStack.currentItem.width)
    }

    function test_settingsCanModifyGlobalShortcut() {
        applicationWindow.openSettings()
        const editor = findChild(settingsPage(), "shortcutEditor")
        verify(editor)
        compare(editor.Accessible.name, "Global dictation shortcut")

        editor.keySequence = "Meta+Shift+X"
        editor.keySequenceModified()
        compare(appController.shortcutText, "Meta+Shift+X")
    }

    function test_compactTranscriptActionsAreAccessible() {
        const page = applicationWindow.pageStack.currentItem
        const preview = findChild(applicationWindow.contentItem, "transcriptText")
        const copy = findChild(applicationWindow.contentItem, "copyTranscriptButton")
        const expand = findChild(applicationWindow.contentItem, "expandTranscriptButton")
        const clear = findChild(applicationWindow.contentItem, "clearTranscriptButton")
        const dialog = findChild(applicationWindow.contentItem, "transcriptDialog")
        verify(preview)
        verify(copy)
        verify(expand)
        verify(clear)
        verify(dialog)
        compare(preview.maximumLineCount, 3)
        compare(copy.display, Controls.AbstractButton.TextBesideIcon)
        compare(expand.display, Controls.AbstractButton.TextBesideIcon)
        compare(clear.display, Controls.AbstractButton.TextBesideIcon)
        compare(copy.Accessible.name, "Copy")
        compare(expand.Accessible.name, "Show full transcription")
        compare(clear.Accessible.name, "Clear transcription")
        for (const action of [copy, expand, clear]) {
            const topLeft = action.mapToItem(page, 0, 0)
            const bottomRight = action.mapToItem(page, action.width, action.height)
            verify(topLeft.x >= 0)
            verify(topLeft.y >= 0)
            verify(bottomRight.x <= page.width)
            verify(bottomRight.y <= page.height)
        }

        expand.clicked()
        tryCompare(dialog, "visible", true)
    }

    function test_compactLayoutKeepsPrimaryActionReachable() {
        const page = applicationWindow.pageStack.currentItem
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(page)
        verify(button)
        verify(page.height >= button.y + button.height)
        verify(button.width <= page.width)
    }
}
