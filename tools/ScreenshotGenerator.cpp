// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FakeAppController.h"
#include "ScreenshotPlatform.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QImageReader>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <array>
#include <cstdio>

namespace {
constexpr int screenshotWidth = 760;
constexpr int screenshotHeight = 520;

struct ScreenshotView {
  int index;
  const char *fileName;
};

constexpr std::array views = {
    ScreenshotView{0, "01-offline-dictation.png"},
    ScreenshotView{1, "02-speech-models.png"},
    ScreenshotView{2, "03-audio-input.png"},
    ScreenshotView{3, "04-settings.png"},
};

void waitForRendering() {
  QEventLoop waitLoop;
  QTimer::singleShot(100, &waitLoop, &QEventLoop::quit);
  waitLoop.exec();
}

bool saveView(QQuickWindow *window, const QDir &outputDirectory, const ScreenshotView &view) {
  if (!window->setProperty("currentView", view.index))
    return false;
  window->requestUpdate();
  waitForRendering();

  const QImage image = window->grabWindow();
  const qreal scale = window->devicePixelRatio();
  const QSize expectedSize(qRound(screenshotWidth * scale), qRound(screenshotHeight * scale));
  if (image.size() != expectedSize)
    return false;

  const QString path = outputDirectory.filePath(QString::fromLatin1(view.fileName));
  if (!image.save(path, "PNG"))
    return false;

  QImageReader reader(path, "PNG");
  return reader.canRead() && reader.size() == expectedSize;
}
} // namespace

int main(int argc, char **argv) {
  qputenv("LANGUAGE", "en");
  qputenv("LC_ALL", "C.UTF-8");

  QApplication application(argc, argv);
  QApplication::setApplicationName(QStringLiteral("kastword-screenshot-generator"));
  QLocale::setDefault(QLocale::c());
  KLocalizedString::setApplicationDomain("kastword");

  const QString platformName = QApplication::platformName();
  if (!isNativeDesktopPlatform(platformName)) {
    std::fprintf(stderr,
                 "Screenshot capture requires an active graphical Plasma session; the Qt platform "
                 "is '%s'. Unset QT_QPA_PLATFORM and try again.\n",
                 platformName.toLocal8Bit().constData());
    return 2;
  }

  if (application.arguments().size() != 2) {
    std::fputs("Usage: kastword_screenshot_generator OUTPUT_DIRECTORY\n", stderr);
    return 2;
  }

  const QDir outputDirectory(application.arguments().constLast());
  if (!outputDirectory.exists()) {
    std::fputs("Screenshot output directory does not exist.\n", stderr);
    return 2;
  }

  FakeAppController controller;
  auto *modelManager = qobject_cast<FakeModelManager *>(controller.modelManager());
  if (modelManager)
    modelManager->setModelStates(QStringLiteral("base.en"), QStringLiteral("small"));

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
  KLocalization::setupLocalizedContext(&engine);
  engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
  if (engine.rootObjects().size() != 1) {
    std::fputs("Could not load the Kastword interface.\n", stderr);
    return 1;
  }

  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
  if (!window) {
    std::fputs("Kastword did not create a window.\n", stderr);
    return 1;
  }
  window->setWidth(screenshotWidth);
  window->setHeight(screenshotHeight);
  window->show();
  waitForRendering();

  for (const ScreenshotView &view : views) {
    if (!saveView(window, outputDirectory, view)) {
      std::fprintf(stderr, "Could not capture %s.\n", view.fileName);
      return 1;
    }
  }

  window->close();
  QCoreApplication::processEvents();
  return 0;
}
