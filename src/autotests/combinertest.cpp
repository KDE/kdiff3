/*
 KDiff3 - Text Diff And Merge Tool

 SPDX-FileCopyrightText: 2020 Michael Reeves reeves.87@gmail.com
 SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#include <qglobal.h>

#include "../combiners.h"

class CombinertestTest: public QObject
{
    Q_OBJECT;

  private:
    bool yes() { return true; }
    bool no() { return false;}

  private Q_SLOTS:
    void testOrCombiner()
    {
        boost::signals2::signal<bool(), or> test1;
        std::list<boost::signals2::scoped_connection> connections;

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));

        QVERIFY(test1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));

        QVERIFY(!test1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));

        QVERIFY(test1());
        connections.clear();
    }

    void testAndCombiner()
    {
        boost::signals2::signal<bool(), and> test1;
        std::list<boost::signals2::scoped_connection> connections;

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));

        QVERIFY(!test1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::no, this)));

        QVERIFY(!test1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::yes, this)));

        QVERIFY(test1());
        connections.clear();
    }
};

QTEST_MAIN(CombinertestTest);

#include "combinertest.moc"
