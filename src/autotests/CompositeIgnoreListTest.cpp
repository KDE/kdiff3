// clang-format off
/*
 * This file is part of KDiff3
 *
 * SPDX-FileCopyrightText: 2021-2021 David Hallas, david@davidhallas.dk
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include "../CompositeIgnoreList.h"

#include <QTest>
#include <QtGlobal>

class IgnoreListMock final : public IgnoreList {
public:
    mutable unsigned callCount = 0;
    bool match = false;
    void enterDir(const QString&, const DirectoryList&) final {}
    [[nodiscard]] bool matches(const QString& dir, const QString& text, bool bCaseSensitive) const final {
        Q_UNUSED(dir);
        Q_UNUSED(text);
        Q_UNUSED(bCaseSensitive);
        ++callCount;
        return match;
    }
};

class CompositeIgnoreListTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMatches()
    {
        CompositeIgnoreList testObject;
        // Verify that with no ignore list handlers added, matches returns false
        QVERIFY(testObject.matches(QString(), QString(), false) == false);
        auto firstIgnoreList = std::make_unique<IgnoreListMock>();
        auto firstIgnoreListPtr = firstIgnoreList.get();
        testObject.addIgnoreList(std::move(firstIgnoreList));
        // Verify that the added ignore list is called, and the return value of CompositeIgnoreList is that
        // of the added ignore list class
        QVERIFY(testObject.matches(QString(), QString(), false) == firstIgnoreListPtr->match);
        QVERIFY(firstIgnoreListPtr->callCount == 1);
        firstIgnoreListPtr->match = true;
        QVERIFY(testObject.matches(QString(), QString(), false) == firstIgnoreListPtr->match);
        QVERIFY(firstIgnoreListPtr->callCount == 2);
        // Verify that CompositeIgnoreList calls each added ignore list class and returns true the first time
        // a match is made
        auto secondIgnoreList = std::make_unique<IgnoreListMock>();
        auto secondIgnoreListPtr = secondIgnoreList.get();
        testObject.addIgnoreList(std::move(secondIgnoreList));
        auto thirdIgnoreList = std::make_unique<IgnoreListMock>();
        auto thirdIgnoreListPtr = thirdIgnoreList.get();
        testObject.addIgnoreList(std::move(thirdIgnoreList));
        firstIgnoreListPtr->callCount = 0;
        firstIgnoreListPtr->match = false;
        secondIgnoreListPtr->match = true;
        thirdIgnoreListPtr->match = false;
        QVERIFY(testObject.matches(QString(), QString(), false) == true);
        QVERIFY(firstIgnoreListPtr->callCount == 1);
        QVERIFY(secondIgnoreListPtr->callCount == 1);
        QVERIFY(thirdIgnoreListPtr->callCount == 0);
    }
};

QTEST_MAIN(CompositeIgnoreListTest);

#include "CompositeIgnoreListTest.moc"
