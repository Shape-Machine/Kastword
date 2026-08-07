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

    SignalSpy {
        id: passiveNotificationSpy
        target: applicationWindow
        signalName: "passiveNotificationShown"
    }

    Component {
        id: applicationComponent

        Loader {
            source: "qrc:/Main.qml"
        }
    }

    function init() {
        appController.setRestoringModel(false)
        appController.setModelReady(true)
        appController.setShortcutChangeAccepted(true)
        appController.setShortcut("Meta+Z")
        appController.autoPaste = false
        appController.pasteShiftInsert = true
        appController.pasteCtrlV = false
        appController.pasteCtrlShiftV = false
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
        appController.modelManager.setModelStates("", "")
        appController.copyText("")
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
        const recommendedIcon = findChild(models, "recommendedModel-base.en")
        const regularIcon = findChild(models, "recommendedModel-tiny")
        verify(filter)
        verify(english)
        verify(recommendedMultilingual)
        verify(otherMultilingual)
        verify(recommendedIcon)
        verify(regularIcon)
        compare(recommendedIcon.visible, true)
        compare(recommendedIcon.Accessible.name, "Recommended")
        compare(regularIcon.visible, false)

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

    function test_modelCardsShowDistinctStatuses() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelLanguageFilter")
        verify(filter)
        filter.currentIndex = filter.indexOfValue("all")
        appController.modelManager.setModelStates("base.en", "small")

        const active = findChild(models, "modelStatus-base.en")
        const downloaded = findChild(models, "modelStatus-small")
        const unavailable = findChild(models, "availableModelStatus-tiny")
        const activeSize = findChild(models, "modelSize-base.en")
        const downloadedSize = findChild(models, "modelSize-small")
        const unavailableSize = findChild(models, "modelSize-tiny")
        const activeCard = findChild(models, "modelCard-base.en")
        const downloadedCard = findChild(models, "modelCard-small")
        const unavailableCard = findChild(models, "modelCard-tiny")
        const useButton = findChild(models, "useModel-small")
        const downloadButton = findChild(models, "downloadModel-tiny")
        const activeIndicator = findChild(models, "activeModelIndicator-base.en")
        const downloadedIndicator = findChild(models, "activeModelIndicator-small")
        const unavailableIndicator = findChild(models, "activeModelIndicator-tiny")
        verify(active)
        verify(downloaded)
        verify(unavailable)
        verify(activeSize)
        verify(downloadedSize)
        verify(unavailableSize)
        verify(activeCard)
        verify(downloadedCard)
        verify(unavailableCard)
        verify(useButton)
        verify(downloadButton)
        verify(activeIndicator)
        verify(downloadedIndicator)
        verify(unavailableIndicator)
        tryCompare(active, "text", "In use")
        compare(active.font.bold, true)
        compare(active.opacity, 1)
        compare(activeSize.text, "· 141 MiB")
        compare(activeSize.opacity, 1)
        compare(activeIndicator.visible, true)
        const activePosition = active.mapToItem(activeCard, 0, 0)
        verify(Number.isFinite(activePosition.x))
        verify(Number.isFinite(activePosition.y))
        verify(activePosition.x > activeIndicator.width)
        compare(downloaded.text, "Downloaded")
        compare(downloaded.font.bold, false)
        compare(downloaded.opacity, 1)
        compare(downloadedSize.text, "· 141 MiB")
        compare(downloadedSize.opacity, 1)
        compare(downloadedIndicator.visible, false)
        const downloadedIcon = findChild(models, "installedModelStatusIcon-small")
        verify(downloadedIcon)
        compare(downloadedIcon.source.toString(), "media-playback-start")
        compare(useButton.highlighted, true)
        compare(unavailable.text, "Available for download")
        compare(unavailable.font.bold, false)
        compare(unavailable.opacity, 0.7)
        compare(unavailableSize.text, "· 141 MiB")
        compare(unavailableSize.opacity, 0.7)
        compare(unavailableIndicator.visible, false)
        compare(unavailable.Accessible.description, "https://example.test/tiny")
        unavailable.forceActiveFocus()
        compare(unavailable.activeFocus, true)
        compare(unavailable.showUrlTooltip, true)
        passiveNotificationSpy.clear()
        mouseClick(unavailable)
        compare(appController.copiedText, "https://example.test/tiny")
        compare(passiveNotificationSpy.count, 1)
        compare(passiveNotificationSpy.signalArguments[0][0],
                "Download URL copied to clipboard.")
        compare(downloadButton.highlighted, true)
        const downloadPosition = downloadButton.mapToItem(unavailableCard, 0, 0)
        verify(downloadPosition.x + downloadButton.width
               >= unavailableCard.width - unavailableCard.padding * 2)
        verify(downloadPosition.y + downloadButton.height
               <= unavailableCard.height - unavailableCard.padding)
    }

    function test_modelCardFooterAdaptsAtMinimumWidth() {
        applicationWindow.width = applicationWindow.minimumWidth
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelLanguageFilter")
        verify(filter)
        filter.currentIndex = filter.indexOfValue("all")
        waitForRendering(applicationWindow.contentItem)

        const card = findChild(models, "modelCard-tiny")
        const status = findChild(models, "availableModelStatus-tiny")
        const download = findChild(models, "downloadModel-tiny")
        verify(card)
        verify(status)
        verify(download)

        const statusPosition = status.mapToItem(card, 0, 0)
        const downloadPosition = download.mapToItem(card, 0, 0)
        verify(downloadPosition.y > statusPosition.y)
        verify(downloadPosition.x >= card.padding)
        verify(downloadPosition.x + download.width <= card.width - card.padding)
        verify(downloadPosition.y + download.height <= card.height - card.padding)
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

        mouseClick(modelsTab)
        tryCompare(applicationWindow, "currentView", 1)
        compare(modelsTab.checked, true)
        compare(modelPage().visible, true)

        mouseClick(settingsTab)
        tryCompare(applicationWindow, "currentView", 2)
        compare(settingsTab.checked, true)
        compare(settingsPage().visible, true)
        compare(applicationWindow.pageStack.depth, 1)
    }

    function test_verticalTabsSupportArrowKeys() {
        const dictationTab = findChild(applicationWindow.contentItem, "dictationTab")
        const modelsTab = findChild(applicationWindow.contentItem, "modelsTab")
        const settingsTab = findChild(applicationWindow.contentItem, "settingsTab")
        verify(dictationTab)
        verify(modelsTab)
        verify(settingsTab)

        dictationTab.forceActiveFocus()
        verify(dictationTab.activeFocus)
        keyClick(Qt.Key_Down)
        verify(modelsTab.activeFocus)
        keyClick(Qt.Key_Space)
        compare(applicationWindow.currentView, 1)

        keyClick(Qt.Key_Down)
        verify(settingsTab.activeFocus)
        keyClick(Qt.Key_Down)
        verify(dictationTab.activeFocus)
        keyClick(Qt.Key_Up)
        verify(settingsTab.activeFocus)
        keyClick(Qt.Key_Space)
        compare(applicationWindow.currentView, 2)
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

    function test_settingsConfiguresAutomaticPasteShortcuts() {
        applicationWindow.openSettings()
        const automaticPaste = findChild(settingsPage(), "autoPasteCheckBox")
        const ctrlV = findChild(settingsPage(), "pasteCtrlVCheckBox")
        const ctrlShiftV = findChild(settingsPage(), "pasteCtrlShiftVCheckBox")
        const shiftInsert = findChild(settingsPage(), "pasteShiftInsertCheckBox")
        verify(automaticPaste)
        verify(ctrlV)
        verify(ctrlShiftV)
        verify(shiftInsert)

        compare(ctrlV.visible, false)
        compare(ctrlShiftV.visible, false)
        compare(shiftInsert.visible, false)
        mouseClick(automaticPaste)
        compare(ctrlV.visible, true)
        compare(ctrlShiftV.visible, true)
        compare(shiftInsert.visible, true)
        compare(ctrlV.checked, false)
        compare(ctrlShiftV.checked, false)
        compare(shiftInsert.checked, true)

        appController.pasteCtrlV = true
        tryCompare(appController, "pasteCtrlV", true)
        appController.pasteCtrlShiftV = true
        tryCompare(appController, "pasteCtrlShiftV", true)
        tryCompare(ctrlV, "checked", true)
        tryCompare(ctrlShiftV, "checked", true)
        compare(shiftInsert.checked, true)
    }

    function test_settingsReportsRejectedGlobalShortcut() {
        applicationWindow.openSettings()
        const editor = findChild(settingsPage(), "shortcutEditor")
        const error = findChild(settingsPage(), "shortcutError")
        verify(editor)
        verify(error)
        appController.setShortcutChangeAccepted(false)

        editor.keySequence = "Meta+Shift+Y"
        editor.keySequenceModified()

        compare(appController.shortcutText, "Meta+Z")
        compare(editor.keySequence, appController.shortcut)
        compare(error.visible, true)
        verify(error.text.indexOf("Choose another shortcut") >= 0)

        appController.setShortcutChangeAccepted(true)
        appController.setShortcut("Meta+Shift+W")
        compare(editor.keySequence, appController.shortcut)
        compare(error.visible, false)
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
