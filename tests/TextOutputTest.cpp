// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <KLocalizedString>
#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

Q_DECLARE_METATYPE(TextOutput::PasteMethod)

namespace {
QString createHelper(const QTemporaryDir &directory, const QString &name, const QByteArray &body) {
  const QString path = directory.filePath(name);
  QFile helper(path);
  if (!helper.open(QIODevice::WriteOnly) || helper.write(body) != body.size())
    return {};
  helper.close();
  if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                       QFileDevice::ExeOwner))
    return {};
  return path;
}
} // namespace

class TextOutputTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void copiesTranscriptionToAvailableClipboards();
  void forgetsOnlyMatchingClipboardText();
  void reportsPasteHelperFailures();
  void deliversThroughX11Helper();
  void deliversThroughWaylandHelper();
  void reportsMissingAutomaticPasteHelper();
  void cancelsX11PasteWhenFocusChanges();
  void pastesOnX11WhenFocusIsUnchanged();
  void choosesPasteMethod_data();
  void choosesPasteMethod();
  void usesExpectedX11Arguments();
  void usesRegularClipboardShortcutOnWayland();
};

void TextOutputTest::initTestCase() { KLocalizedString::setApplicationDomain("kastword"); }

void TextOutputTest::copiesTranscriptionToAvailableClipboards() {
  const QString transcription = QStringLiteral("Kastword clipboard regression test");
  TextOutput output;

  QCOMPARE(output.deliver(transcription, false), QStringLiteral("Copied to clipboard."));
  QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Clipboard), transcription);
  if (QGuiApplication::clipboard()->supportsSelection())
    QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Selection), transcription);
}

void TextOutputTest::forgetsOnlyMatchingClipboardText() {
  TextOutput output;
  QClipboard *clipboard = QGuiApplication::clipboard();
  const QString transcription = QStringLiteral("private transcription");
  output.deliver(transcription, false);

  clipboard->setText(QStringLiteral("new clipboard text"), QClipboard::Clipboard);
  output.forget(transcription);
  QCOMPARE(clipboard->text(QClipboard::Clipboard), QStringLiteral("new clipboard text"));

  clipboard->setText(transcription, QClipboard::Clipboard);
  output.forget(transcription);
  QVERIFY(clipboard->text(QClipboard::Clipboard).isEmpty());
}

void TextOutputTest::reportsPasteHelperFailures() {
  TextOutput output;
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  output.startPaste(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("exit 7")},
                    QStringLiteral("unexpected success"));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Automatic paste helper failed with exit code 7."));

  output.startPaste(QStringLiteral("/missing/kastword-paste-helper"), {},
                    QStringLiteral("unexpected success"));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Automatic paste helper could not be started."));

  output.startPaste(QStringLiteral("/bin/sh"),
                    {QStringLiteral("-c"), QStringLiteral("kill -SEGV $$")},
                    QStringLiteral("unexpected success"));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(), QStringLiteral("Automatic paste helper crashed."));
}

void TextOutputTest::deliversThroughX11Helper() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QVERIFY(
      !createHelper(directory, QStringLiteral("xdotool"),
                    QByteArrayLiteral(
                        "#!/bin/sh\nif [ \"$1\" = getwindowfocus ]; then echo 42; fi\nexit 0\n"))
           .isEmpty());
  const QByteArray oldPath = qgetenv("PATH");
  const QByteArray oldSession = qgetenv("XDG_SESSION_TYPE");
  const auto restore = qScopeGuard([oldPath, oldSession] {
    qputenv("PATH", oldPath);
    qputenv("XDG_SESSION_TYPE", oldSession);
  });
  qputenv("PATH", directory.path().toUtf8());
  qputenv("XDG_SESSION_TYPE", QByteArrayLiteral("x11"));
  TextOutput output;
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  QCOMPARE(output.deliver(QStringLiteral("x11 text"), true),
           QStringLiteral("Copied to clipboard; automatic paste scheduled."));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Pasted into the focused application."));
}

void TextOutputTest::deliversThroughWaylandHelper() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QVERIFY(
      !createHelper(directory, QStringLiteral("ydotool"), QByteArrayLiteral("#!/bin/sh\nexit 0\n"))
           .isEmpty());
  const QByteArray oldPath = qgetenv("PATH");
  const QByteArray oldSession = qgetenv("XDG_SESSION_TYPE");
  const auto restore = qScopeGuard([oldPath, oldSession] {
    qputenv("PATH", oldPath);
    qputenv("XDG_SESSION_TYPE", oldSession);
  });
  qputenv("PATH", directory.path().toUtf8());
  qputenv("XDG_SESSION_TYPE", QByteArrayLiteral("wayland"));
  TextOutput output;
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  QCOMPARE(output.deliver(QStringLiteral("wayland text"), true),
           QStringLiteral("Copied to clipboard; automatic paste scheduled."));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Sent paste to the focused application."));
}

void TextOutputTest::reportsMissingAutomaticPasteHelper() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray oldPath = qgetenv("PATH");
  const QByteArray oldSession = qgetenv("XDG_SESSION_TYPE");
  const auto restore = qScopeGuard([oldPath, oldSession] {
    qputenv("PATH", oldPath);
    qputenv("XDG_SESSION_TYPE", oldSession);
  });
  qputenv("PATH", directory.path().toUtf8());
  qputenv("XDG_SESSION_TYPE", QByteArrayLiteral("wayland"));
  TextOutput output;

  QCOMPARE(output.deliver(QStringLiteral("manual text"), true),
           QStringLiteral("Copied to clipboard; install ydotool for automatic paste."));
}

void TextOutputTest::cancelsX11PasteWhenFocusChanges() {
  int reads = 0;
  TextOutput output([&reads](const QString &) {
    ++reads;
    return QString::number(reads);
  });
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  output.scheduleX11Paste(QStringLiteral("/bin/true"));

  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Automatic paste was cancelled because focus changed."));
  QCOMPARE(reads, 2);
}

void TextOutputTest::pastesOnX11WhenFocusIsUnchanged() {
  int reads = 0;
  TextOutput output([&reads](const QString &) {
    ++reads;
    return QStringLiteral("42");
  });
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  output.scheduleX11Paste(QStringLiteral("/bin/true"));

  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Pasted into the focused application."));
  QCOMPARE(reads, 2);
}

void TextOutputTest::choosesPasteMethod_data() {
  QTest::addColumn<bool>("autoPaste");
  QTest::addColumn<QString>("session");
  QTest::addColumn<bool>("xdotoolAvailable");
  QTest::addColumn<bool>("ydotoolAvailable");
  QTest::addColumn<TextOutput::PasteMethod>("expected");

  using Method = TextOutput::PasteMethod;
  QTest::newRow("disabled") << false << QStringLiteral("wayland") << true << true
                            << Method::ClipboardOnly;
  QTest::newRow("x11 helper") << true << QStringLiteral("x11") << true << false << Method::Xdotool;
  QTest::newRow("x11 missing") << true << QStringLiteral("X11") << false << true
                               << Method::ClipboardOnly;
  QTest::newRow("wayland helper") << true << QStringLiteral("wayland") << false << true
                                  << Method::Ydotool;
  QTest::newRow("wayland missing")
      << true << QStringLiteral("wayland") << true << false << Method::ClipboardOnly;
  QTest::newRow("unknown session")
      << true << QStringLiteral("unknown") << false << true << Method::Ydotool;
}

void TextOutputTest::choosesPasteMethod() {
  QFETCH(bool, autoPaste);
  QFETCH(QString, session);
  QFETCH(bool, xdotoolAvailable);
  QFETCH(bool, ydotoolAvailable);
  QFETCH(TextOutput::PasteMethod, expected);

  QCOMPARE(TextOutput::choosePasteMethod(autoPaste, session, xdotoolAvailable, ydotoolAvailable),
           expected);
}

void TextOutputTest::usesExpectedX11Arguments() {
  QCOMPARE(TextOutput::x11PasteArguments(),
           QStringList({QStringLiteral("key"), QStringLiteral("--clearmodifiers"),
                        QStringLiteral("shift+Insert")}));
}

void TextOutputTest::usesRegularClipboardShortcutOnWayland() {
  const QStringList arguments = TextOutput::waylandPasteArguments();

  // Ctrl+Shift+V makes Konsole read the regular clipboard updated by Kastword and Klipper.
  QCOMPARE(arguments,
           QStringList({QStringLiteral("key"), QStringLiteral("29:1"), QStringLiteral("42:1"),
                        QStringLiteral("47:1"), QStringLiteral("47:0"), QStringLiteral("42:0"),
                        QStringLiteral("29:0")}));
  // KEY_INSERT=110 would make Konsole read the primary selection, which may contain stale text.
  QVERIFY(!arguments.contains(QStringLiteral("110:1")));
}

QTEST_MAIN(TextOutputTest)
#include "TextOutputTest.moc"
