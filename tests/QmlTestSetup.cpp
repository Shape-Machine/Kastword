// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FakeAppController.h"

#include <QtQuickTest/quicktest.h>

class QmlTestSetup final : public QObject {
  Q_OBJECT

public slots:
  void qmlEngineAvailable(QQmlEngine *engine) {
    KLocalizedString::setApplicationDomain("kastword");
    engine->rootContext()->setContextProperty(QStringLiteral("appController"), &m_controller);
    KLocalization::setupLocalizedContext(engine);
  }

private:
  FakeAppController m_controller;
};

QUICK_TEST_MAIN_WITH_SETUP(kastword_qml, QmlTestSetup)

#include "QmlTestSetup.moc"
