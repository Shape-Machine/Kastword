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
    property string passiveNotificationMessage: ""

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
        appController.setTestState(false, false)
        appController.setUsbAudioInputAvailable(true)
        appController.setAudioInputMonitoringError("")
        appController.resetMonitoringRetryCount()
        appController.audioInputId = ""
        appController.history.setAvailable(true)
        appController.history.setResetRequired(false)
        appController.history.setEntryCount(2)
        const loader = createTemporaryObject(applicationComponent, testCase)
        verify(loader)
        tryCompare(loader, "status", Loader.Ready)
        applicationWindow = loader.item
        verify(applicationWindow)
        passiveNotificationMessage = ""
        applicationWindow.passiveNotificationHandler = function(message) {
            testCase.passiveNotificationMessage = message
        }
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

    function audioInputPage() {
        const page = findChild(applicationWindow.contentItem, "audioInputPage")
        verify(page)
        return page
    }

    function historyPage() {
        const page = findChild(applicationWindow.contentItem, "historyPage")
        verify(page)
        return page
    }

    function test_missingModelOpensSetupAndDisablesDictation() {
        appController.setModelReady(false)
        tryCompare(applicationWindow, "currentView", 2)
        compare(modelPage().objectName, "modelManagerPage")
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(button)
        compare(button.enabled, false)
    }

    function test_missingAudioInputOpensAudioSetup() {
        applicationWindow.currentView = 0
        appController.requestAudioInputSetup()
        tryCompare(applicationWindow, "currentView", 3)
        compare(audioInputPage().visible, true)
    }

    function test_savedModelVerificationDoesNotOpenSetup() {
        appController.setRestoringModel(true)
        appController.setModelReady(false)
        compare(applicationWindow.currentView, 0)
        applicationWindow.openModelManager()
        compare(applicationWindow.currentView, 2)
        const models = modelPage()
        const warning = findChild(models, "modelSetupWarning")
        verify(warning)
        compare(warning.visible, true)
        verify(warning.text.indexOf("sources you trust") >= 0)

        appController.setRestoringModel(false)
        compare(applicationWindow.currentView, 2)
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
        const filter = findChild(models, "modelFilterRecommended")
        const storagePath = findChild(models, "modelStoragePath")
        const warning = findChild(models, "modelSetupWarning")
        verify(filter)
        verify(storagePath)
        verify(warning)
        compare(filter.text, "Recommended")
        compare(storagePath.Accessible.name, "Model storage path")
        storagePath.openPath()
        compare(appController.openedDirectory, "/tmp/models")
        compare(warning.visible, true)
    }

    function test_modelFiltersDefaultToRecommendedAndSeparateCapabilities() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const recommendedFilter = findChild(models, "modelFilterRecommended")
        const englishFilter = findChild(models, "modelFilterEnglish")
        const multilingualFilter = findChild(models, "modelFilterMultilingual")
        const english = findChild(models, "modelCard-base.en")
        const recommendedMultilingual = findChild(models, "modelCard-small")
        const otherMultilingual = findChild(models, "modelCard-tiny")
        const recommendedIcon = findChild(models, "recommendedModel-base.en")
        const regularIcon = findChild(models, "recommendedModel-tiny")
        verify(recommendedFilter)
        verify(englishFilter)
        verify(multilingualFilter)
        verify(english)
        verify(recommendedMultilingual)
        verify(otherMultilingual)
        verify(recommendedIcon)
        verify(regularIcon)
        compare(recommendedIcon.visible, true)
        compare(recommendedIcon.Accessible.name, "Recommended")
        compare(regularIcon.visible, false)

        compare(recommendedFilter.checked, true)
        compare(english.visible, true)
        compare(recommendedMultilingual.visible, true)
        compare(otherMultilingual.visible, false)

        multilingualFilter.clicked()
        compare(english.visible, false)
        compare(recommendedMultilingual.visible, true)
        compare(otherMultilingual.visible, true)

        englishFilter.clicked()
        compare(english.visible, true)
        compare(recommendedMultilingual.visible, false)
        compare(otherMultilingual.visible, false)
    }

    function test_modelCardsShowDistinctStatuses() {
        applicationWindow.width = 1200
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelFilterAll")
        verify(filter)
        filter.clicked()
        appController.modelManager.setModelStates("base.en", "small")
        waitForRendering(applicationWindow.contentItem)

        const active = findChild(models, "modelStatus-base.en")
        const downloaded = findChild(models, "modelStatus-small")
        const unavailable = findChild(models, "availableModelStatus-tiny")
        const activeSize = findChild(models, "modelSize-base.en")
        const downloadedSize = findChild(models, "modelSize-small")
        const unavailableSize = findChild(models, "modelSize-tiny")
        const activeCard = findChild(models, "modelCard-base.en")
        const downloadedCard = findChild(models, "modelCard-small")
        const unavailableCard = findChild(models, "modelCard-tiny")
        const unavailableFooter = findChild(models, "modelFooter-tiny")
        const activeFooter = findChild(models, "modelFooter-base.en")
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
        verify(unavailableFooter)
        verify(activeFooter)
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
        compare(activeFooter.compact, activeFooter.width < activeFooter.singleRowWidth)
        compare(activeFooter.columns, 2)
        compare(unavailable.Accessible.description, "https://example.test/tiny")
        unavailable.forceActiveFocus()
        compare(unavailable.activeFocus, true)
        compare(unavailable.showUrlTooltip, true)
        mouseClick(unavailable)
        compare(appController.copiedText, "https://example.test/tiny")
        compare(passiveNotificationMessage, "Download URL copied to clipboard.")
        compare(downloadButton.highlighted, true)
    }

    function test_modelCardFooterAdaptsAtMinimumWidth() {
        applicationWindow.width = applicationWindow.minimumWidth
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelFilterAll")
        verify(filter)
        filter.clicked()
        waitForRendering(applicationWindow.contentItem)

        const card = findChild(models, "modelCard-tiny")
        const status = findChild(models, "availableModelStatus-tiny")
        const download = findChild(models, "downloadModel-tiny")
        const footer = findChild(models, "modelFooter-tiny")
        verify(card)
        verify(status)
        verify(download)
        verify(footer)
        compare(footer.compact, footer.width < footer.singleRowWidth)
        compare(footer.columns, 1)

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
        const filter = findChild(models, "modelFilterAll")
        verify(filter)
        filter.clicked()
        appController.modelManager.setVerifyingId("tiny")

        const cancel = findChild(models, "cancelModel-tiny")
        verify(cancel)
        tryCompare(cancel, "visible", true)
        compare(cancel.text, "Cancel")
    }

    function test_partialDownloadCanBeRemoved() {
        applicationWindow.openModelManager()
        const models = modelPage()
        const filter = findChild(models, "modelFilterAll")
        verify(filter)
        filter.clicked()
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
        const historyTab = findChild(applicationWindow.contentItem, "historyTab")
        const modelsTab = findChild(applicationWindow.contentItem, "modelsTab")
        const audioInputTab = findChild(applicationWindow.contentItem, "audioInputTab")
        const settingsTab = findChild(applicationWindow.contentItem, "settingsTab")
        verify(dictationTab)
        verify(historyTab)
        verify(modelsTab)
        verify(audioInputTab)
        verify(settingsTab)
        compare(dictationTab.checked, true)
        compare(applicationWindow.height, 700)
        compare(dictationTab.text, "Dictation")
        compare(historyTab.text, "History")
        compare(modelsTab.text, "Models")
        compare(audioInputTab.text, "Audio")
        compare(settingsTab.text, "Settings")
        const navigation = findChild(applicationWindow.contentItem, "navigationPane")
        verify(navigation.width < 200)

        mouseClick(historyTab)
        tryCompare(applicationWindow, "currentView", 1)
        compare(historyTab.checked, true)
        compare(historyPage().visible, true)

        mouseClick(modelsTab)
        tryCompare(applicationWindow, "currentView", 2)
        compare(modelsTab.checked, true)
        compare(modelPage().visible, true)

        mouseClick(audioInputTab)
        tryCompare(applicationWindow, "currentView", 3)
        compare(appController.audioInputMonitoringEnabled, true)
        compare(audioInputTab.checked, true)
        compare(audioInputPage().visible, true)

        mouseClick(settingsTab)
        tryCompare(applicationWindow, "currentView", 4)
        compare(appController.audioInputMonitoringEnabled, false)
        compare(settingsTab.checked, true)
        compare(settingsPage().visible, true)
        compare(applicationWindow.pageStack.depth, 1)
    }

    function test_verticalTabsSupportArrowKeys() {
        const dictationTab = findChild(applicationWindow.contentItem, "dictationTab")
        const historyTab = findChild(applicationWindow.contentItem, "historyTab")
        const modelsTab = findChild(applicationWindow.contentItem, "modelsTab")
        const audioInputTab = findChild(applicationWindow.contentItem, "audioInputTab")
        const settingsTab = findChild(applicationWindow.contentItem, "settingsTab")
        verify(dictationTab)
        verify(historyTab)
        verify(modelsTab)
        verify(audioInputTab)
        verify(settingsTab)

        dictationTab.forceActiveFocus()
        verify(dictationTab.activeFocus)
        keyClick(Qt.Key_Down)
        verify(historyTab.activeFocus)
        keyClick(Qt.Key_Down)
        verify(modelsTab.activeFocus)
        keyClick(Qt.Key_Space)
        compare(applicationWindow.currentView, 2)

        keyClick(Qt.Key_Down)
        verify(audioInputTab.activeFocus)
        keyClick(Qt.Key_Down)
        verify(settingsTab.activeFocus)
        keyClick(Qt.Key_Down)
        verify(dictationTab.activeFocus)
        keyClick(Qt.Key_Up)
        verify(settingsTab.activeFocus)
        keyClick(Qt.Key_Space)
        compare(applicationWindow.currentView, 4)
    }

    function test_historyShowsRecentAndFullEntriesAccessibly() {
        applicationWindow.currentView = 0
        const recent = findChild(applicationWindow.contentItem, "recentHistoryPanel")
        verify(recent)
        compare(recent.visible, true)

        applicationWindow.currentView = 1
        const page = historyPage()
        const enabled = findChild(page, "historyEnabledSwitch")
        const count = findChild(page, "historyMaximumEntries")
        const age = findChild(page, "historyMaximumAgeDays")
        const clear = findChild(page, "clearHistoryButton")
        const storagePath = findChild(page, "historyStoragePath")
        verify(enabled)
        verify(count)
        verify(age)
        verify(clear)
        verify(storagePath)
        compare(enabled.checked, true)
        compare(enabled.text, "Enable history")
        verify(enabled.Accessible.description.indexOf("authenticated encryption") >= 0)
        compare(count.Accessible.name, "Maximum history entries")
        compare(age.Accessible.name, "Maximum history age in days")
        compare(clear.visible, true)
        storagePath.openPath()
        compare(appController.revealedFile, "/tmp/history.enc")
        verify(Math.abs(enabled.mapToItem(page, 0, 0).y
                        - clear.mapToItem(page, 0, 0).y) <= 4)
        const list = findChild(page, "historyList")
        list.positionViewAtIndex(0, ListView.Beginning)
        tryVerify(function() {
            return findChild(page, "historyEntryText") !== null
        })
        const entryText = findChild(page, "historyEntryText")
        verify(entryText)
        compare(entryText.Accessible.name, "Private dictation 0")
        verify(entryText.Accessible.description.indexOf("8 Aug 2026") >= 0)

        const arabicLocale = Qt.locale("ar_EG")
        const localizedAge = age.textFromValue(123, arabicLocale)
        compare(age.valueFromText(localizedAge, arabicLocale), 123)
    }

    function test_cancellingHistoryDisableKeepsSwitchEnabled() {
        applicationWindow.currentView = 1
        const enabled = findChild(historyPage(), "historyEnabledSwitch")
        const dialog = findChild(applicationWindow.contentItem, "disableHistoryDialog")
        verify(enabled)
        verify(dialog)
        compare(enabled.checked, true)

        mouseClick(enabled)
        compare(enabled.checked, true)
        compare(dialog.visible, true)
        dialog.reject()
        tryCompare(dialog, "visible", false)
        compare(enabled.checked, true)
    }

    function test_historyResetIsLimitedToUnreadableData() {
        applicationWindow.currentView = 1
        const reset = findChild(historyPage(), "resetHistoryAction")
        verify(reset)
        compare(reset.visible, false)

        appController.history.setAvailable(false)
        waitForRendering(historyPage())
        compare(reset.visible, false)

        appController.history.setResetRequired(true)
        waitForRendering(historyPage())
        compare(reset.visible, true)
    }

    function test_historyVirtualizesLargeEntryCollections() {
        appController.history.setEntryCount(10000)
        applicationWindow.currentView = 1
        const list = findChild(historyPage(), "historyList")
        verify(list)
        tryCompare(list, "count", 10000)
        const clear = findChild(historyPage(), "clearHistoryButton")
        verify(clear)
        verify(clear.visible)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        verify(list.itemAtIndex(9999) === null)
        const clearY = clear.mapToItem(historyPage(), 0, 0).y
        list.positionViewAtIndex(9999, ListView.End)
        waitForRendering(list)
        compare(clear.mapToItem(historyPage(), 0, 0).y, clearY)
        verify(clear.visible)
        verify(list.reuseItems)
    }

    function test_audioInputPageSelectsAndReportsDevices() {
        applicationWindow.openAudioInput()
        compare(appController.audioInputMonitoringEnabled, true)
        const page = audioInputPage()
        const list = findChild(page, "audioInputList")
        const none = findChild(page, "audioInputOption_0")
        const systemDefault = findChild(page, "audioInputOption_1")
        const usb = findChild(page, "audioInputOption_2")
        const status = findChild(page, "audioInputStatus")
        const level = findChild(page, "audioInputLevel")
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(list)
        verify(none)
        verify(systemDefault)
        verify(usb)
        verify(status)
        verify(level)
        verify(button)
        compare(list.Accessible.name, "Audio input device")
        compare(level.Accessible.name, "Microphone level")
        compare(none.checked, false)
        compare(systemDefault.checked, true)
        compare(usb.checked, false)
        compare(appController.audioInputId, "")
        compare(status.text, "Using Built-in Mic")

        usb.clicked()
        compare(appController.audioInputId, "usb")
        compare(usb.checked, true)
        compare(status.text, "Using USB Headset")

        appController.setUsbAudioInputAvailable(false)
        tryCompare(appController, "audioInputReady", false)
        const unavailableUsb = findChild(page, "audioInputOption_2")
        verify(unavailableUsb)
        verify(status.text.indexOf("unavailable") >= 0)
        compare(unavailableUsb.enabled, false)
        compare(unavailableUsb.checked, true)
        compare(button.enabled, false)

        appController.setUsbAudioInputAvailable(true)
        tryCompare(appController, "audioInputReady", true)
        compare(button.enabled, true)
    }

    function test_audioInputMonitoringStopsWhenWindowIsHidden() {
        applicationWindow.openAudioInput()
        compare(appController.audioInputMonitoringEnabled, true)
        applicationWindow.hide()
        tryCompare(appController, "audioInputMonitoringEnabled", false)
        applicationWindow.show()
        tryCompare(appController, "audioInputMonitoringEnabled", true)
    }

    function test_noneAudioInputDisablesDictation() {
        applicationWindow.openAudioInput()
        const page = audioInputPage()
        const list = findChild(page, "audioInputList")
        const none = findChild(page, "audioInputOption_0")
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(list)
        verify(none)
        verify(button)
        compare(none.Accessible.description, "Disable audio input")
        wait(0)
        const initialListWidth = list.width
        const initialListX = list.mapToItem(page, 0, 0).x
        none.clicked()
        wait(0)
        compare(appController.audioInputId, ":none")
        verify(appController.audioInputStatus.indexOf("No audio input is selected") >= 0)
        compare(button.enabled, false)
        compare(list.width, initialListWidth)
        compare(list.mapToItem(page, 0, 0).x, initialListX)
        const note = findChild(page, "audioInputPrivacyNote")
        verify(note)
        compare(note.text, "Audio input is disabled.")
    }

    function test_audioInputMonitoringFailureCanBeRetried() {
        applicationWindow.openAudioInput()
        const page = audioInputPage()
        const errorMessage = findChild(page, "audioInputMonitoringError")
        verify(errorMessage)
        compare(errorMessage.visible, false)

        appController.setAudioInputMonitoringError("The microphone backend failed.")
        tryCompare(errorMessage, "visible", true)
        verify(errorMessage.text.indexOf("microphone backend failed") >= 0)
        compare(errorMessage.actions.length, 1)
        errorMessage.actions[0].trigger()
        compare(appController.monitoringRetryCount, 1)
        tryCompare(errorMessage, "visible", false)
    }

    function test_audioInputPrivacyNoteReflectsRecording() {
        applicationWindow.openAudioInput()
        const note = findChild(audioInputPage(), "audioInputPrivacyNote")
        verify(note)
        verify(note.text.indexOf("not saved") >= 0)
        appController.setTestState(true, false)
        verify(note.text.indexOf("recording audio") >= 0)
        verify(note.text.indexOf("not saved") < 0)
    }

    function test_audioInputSelectionIsLockedWhileBusy() {
        applicationWindow.openAudioInput()
        const list = findChild(audioInputPage(), "audioInputList")
        verify(list)
        compare(list.enabled, true)
        appController.setTestState(true, false)
        compare(list.enabled, false)
        appController.setTestState(false, true)
        compare(list.enabled, false)
        appController.setTestState(false, false)
        compare(list.enabled, true)
    }

    function test_deviceLossKeepsRecordingStopActionEnabled() {
        appController.audioInputId = "usb"
        appController.setTestState(true, false)
        const button = findChild(applicationWindow.contentItem, "dictationButton")
        verify(button)
        compare(button.enabled, true)
        appController.setUsbAudioInputAvailable(false)
        tryCompare(appController, "audioInputReady", false)
        compare(button.enabled, true)
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
        automaticPaste.forceActiveFocus()
        verify(automaticPaste.activeFocus)
        keyClick(Qt.Key_Space)
        tryCompare(appController, "autoPaste", true)
        tryCompare(ctrlV, "visible", true)
        tryCompare(ctrlShiftV, "visible", true)
        tryCompare(shiftInsert, "visible", true)
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
