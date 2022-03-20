/*
 KDiff3 - Text Diff And Merge Tool

 SPDX-FileCopyrightText: 2020 Michael Reeves reeves.87@gmail.com
 SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#include <QtGlobal>

#include "../combiners.h"

class CombinertestTest: public QObject
{
    Q_OBJECT;

  private:
    QString nonEmpty2(){ return "test 2";}

    QString nonEmpty1(){ return "test 1";}
    QString emptyString(){ return "";}
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

    void testFirstNonEmpty()
    {
        boost::signals2::signal<QString (), FirstNonEmpty<QString>> test1;
        std::list<boost::signals2::scoped_connection> connections;

        //Just in case someone moneys with the test functions
        QVERIFY(emptyString().isEmpty());
        QVERIFY(!nonEmpty1().isEmpty());
        QVERIFY(!nonEmpty1().isEmpty());

        connections.clear();
        QVERIFY(test1().isEmpty());

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty1, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty2, this)));

        QCOMPARE(test1(), nonEmpty1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty1, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty2, this)));

        QCOMPARE(test1(), nonEmpty1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty1, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty2, this)));

        QCOMPARE(test1(), nonEmpty1());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty2, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::nonEmpty1, this)));

        QCOMPARE(test1(), nonEmpty2());
        connections.clear();

        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));
        connections.push_back(test1.connect(boost::bind(&CombinertestTest::emptyString, this)));

        QCOMPARE(test1(), emptyString());
        connections.clear();
    }
};

QTEST_MAIN(CombinertestTest);

#include "combinertest.moc"
