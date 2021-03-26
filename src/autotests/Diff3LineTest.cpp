/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "../diff.h"

#include <QTest>
#include <QObject>

class Diff3LineTest: public QObject
{
    Q_OBJECT;
  private Q_SLOTS:
    void initTestCase()
    {
        Diff3LineList diffList;
        Diff3Line     entry;

        QVERIFY(diffList.empty());
        QVERIFY(!entry.isEqualAB());
        QVERIFY(!entry.isEqualBC());
        QVERIFY(!entry.isEqualAC());
        QVERIFY(!entry.isWhiteLine(e_SrcSelector::A));
        QVERIFY(!entry.isWhiteLine(e_SrcSelector::B));
        QVERIFY(!entry.isWhiteLine(e_SrcSelector::C));
    }

    void calcDiffTest()
    {
        Diff3LineList diff3List;
        /*
            Start with something simple. This diff list indicates one different line fallowed by three equal lines
            *This was generated with a two-way compare.

            Not all functions in Diff3Line will work since there is no data actually loaded.
        */
        DiffList diffList = {{0, 1, 1}, {3, 0, 0}};

        diff3List.calcDiff3LineListUsingAB(&diffList);
        QCOMPARE(diff3List.size(), 4);

        Diff3LineList::const_iterator entry = diff3List.begin();
        QCOMPARE(entry->getLineA(), 0);
        QCOMPARE(entry->getLineB(), 0);
        QCOMPARE(entry->getLineC(), LineRef::invalid);
        QVERIFY(!entry->isEqualAB());
        QVERIFY(!entry->isEqualAC());
        QVERIFY(!entry->isEqualBC());
        ++entry;

        QCOMPARE(entry->getLineA(), 1);
        QCOMPARE(entry->getLineB(), 1);
        QCOMPARE(entry->getLineC(), LineRef::invalid);
        QVERIFY(entry->isEqualAB());
        QVERIFY(!entry->isEqualAC());
        QVERIFY(!entry->isEqualBC());
        ++entry;

        QCOMPARE(entry->getLineA(), 2);
        QCOMPARE(entry->getLineB(), 2);
        QCOMPARE(entry->getLineC(), LineRef::invalid);
        QVERIFY(entry->isEqualAB());
        QVERIFY(!entry->isEqualAC());
        QVERIFY(!entry->isEqualBC());
        ++entry;

        QCOMPARE(entry->getLineA(), 3);
        QCOMPARE(entry->getLineB(), 3);
        QCOMPARE(entry->getLineC(), LineRef::invalid);
        QVERIFY(entry->isEqualAB());
        QVERIFY(!entry->isEqualAC());
        QVERIFY(!entry->isEqualBC());
        ++entry;
    }
};

QTEST_MAIN(Diff3LineTest);

#include "Diff3LineTest.moc"
