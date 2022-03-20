/*
 KDiff3 - Text Diff And Merge Tool

 SPDX-FileCopyrightText: 2020 Michael Reeves reeves.87@gmail.com
 SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../defmac.h"

#include <qobjectdefs.h>
#include <QTest>
#include <QtGlobal>
#include <QtTest/qtestcase.h>

class SigTest: public QObject
{
    Q_OBJECT
  public:
    bool fired = false;
   Q_SIGNALS:
     void signal();

   public Q_SLOTS:
     void slot() { fired=true;};
};

class ConnectionTest: public QObject
{
    Q_OBJECT;
  private Q_SLOTS:
    void testConnect()
    {
        SigTest s;

        chk_connect(&s, &SigTest::signal, &s, &SigTest::slot);
        Q_EMIT s.signal();
        QVERIFY(s.fired);
    }
};

QTEST_MAIN(ConnectionTest);

#include "connectiontest.moc"
