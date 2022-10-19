/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include <QTest>
#include <QtGlobal>

#include "../CvsIgnoreList.h"
#include "MocIgnoreFile.h"

class IgnoreListTestInterface: public CvsIgnoreList
{
  public:
    bool isEmpty(const QString& dir) const
    {
        return m_ignorePatterns.find(dir) == m_ignorePatterns.end();
    }

    QStringList getExactMatchList(const QString& dir) const
    {
        const auto ignorePatternsIt = m_ignorePatterns.find(dir);
        return ignorePatternsIt != m_ignorePatterns.end() ? ignorePatternsIt->second.m_exactPatterns : QStringList();
    }
};

class CvsIgnoreListTest : public QObject
{
    const QString defaultPatterns = QString::fromLatin1(". .. core RCSLOG tags TAGS RCS SCCS .make.state "
                                                        ".nse_depinfo #* .#* cvslog.* ,* CVS CVS.adm .del-* *.a *.olb *.o *.obj "
                                                        "*.so *.Z *~ *.old *.elc *.ln *.bak *.BAK *.orig *.rej *.exe _$* *$");
    Q_OBJECT
  private Q_SLOTS:
    void initTestCase()
    {
        MocIgnoreFile file;
        IgnoreListTestInterface test;
        //sanity check defaults
        QVERIFY(test.isEmpty(file.absoluteFilePath()));
        //MocIgnoreFile should be emulating a readable local file.
        QVERIFY(file.isLocal());
        QVERIFY(file.exists());
        QVERIFY(file.isReadable());
        QCOMPARE(file.fileName(), QStringLiteral(".cvsignore"));
    }

    void addEntriesFromString()
    {
        IgnoreListTestInterface test;
        QString testDir("dir");
        QString testString = ". .. core RCSLOG tags TAGS RCS SCCS .make.state";
        test.addEntriesFromString(testDir, testString);
        QVERIFY(!test.getExactMatchList(testDir).isEmpty());
        QVERIFY(test.getExactMatchList(testDir) == testString.split(' '));
    }

    void matches()
    {
        CvsIgnoreList test;

        QString testDir("dir");
        QString testString = ". .. core RCSLOG tags TAGS RCS SCCS .make.state *.so _$*";
        test.addEntriesFromString(testDir, testString);

        QVERIFY(test.matches(testDir, ".", false));
        QVERIFY(test.matches(testDir, ".", true));
        QVERIFY(!test.matches(testDir, "cores core", true));
        QVERIFY(test.matches(testDir, "core", true));
        QVERIFY(!test.matches(testDir, "Core", true));
        QVERIFY(test.matches(testDir, "Core", false));
        QVERIFY(!test.matches(testDir, "a", false));
        QVERIFY(test.matches(testDir, "core", false));
        //ends with .so
        QVERIFY(test.matches(testDir, "sdf3.so", true));
        QVERIFY(!test.matches(testDir, "sdf3.to", true));
        QVERIFY(test.matches(testDir, "*.so", true));
        QVERIFY(test.matches(testDir, "sdf4.So", false));
        QVERIFY(!test.matches(testDir, "sdf4.So", true));
        //starts with _$ddsf
        QVERIFY(test.matches(testDir, "_$ddsf", true));
        //Should only match exact strings not partial ones
        QVERIFY(!test.matches(testDir, "sdf4.so ", true));
        QVERIFY(!test.matches(testDir, " _$ddsf", true));

        testString = "*.*";
        test = CvsIgnoreList();
        test.addEntriesFromString(testDir, "*.*");

        QVERIFY(test.matches(testDir, "k.K", false));
        QVERIFY(test.matches(testDir, "*.K", false));
        QVERIFY(test.matches(testDir, "*.*", false));
        QVERIFY(!test.matches(testDir, "*+*", false));
        QVERIFY(!test.matches(testDir, "asd", false));
        //The following are matched by the above
        QVERIFY(test.matches(testDir, "a k.k", false));
        QVERIFY(test.matches(testDir, "k.k v", false));
        QVERIFY(test.matches(testDir, " k.k", false));
        QVERIFY(test.matches(testDir, "k.k ", false));

        // Test matches from a different dir
        QString otherDir("other_dir");
        QVERIFY(!test.matches(otherDir, "sdf3.so", true));
    }

    void testDefaults()
    {
        CvsIgnoreList test;
        CvsIgnoreList expected;
        MocIgnoreFile file;
        DirectoryList dirList;

        /*
            Verify default init. For this to work we must:
                1. Unset CVSIGNORE
                2. Insure no patterns are read from a .cvsignore file.
            MocCvsIgnore emulates a blank cvs file by default insuring the second condition.
        */
        test = CvsIgnoreList();
        //
        qunsetenv("CVSIGNORE");

        QString testDir("dir");
        expected.addEntriesFromString(testDir, defaultPatterns);

        test.enterDir(testDir, dirList);
        QVERIFY(test.m_ignorePatterns[testDir].m_endPatterns == expected.m_ignorePatterns[testDir].m_endPatterns);
        QVERIFY(test.m_ignorePatterns[testDir].m_exactPatterns == expected.m_ignorePatterns[testDir].m_exactPatterns);
        QVERIFY(test.m_ignorePatterns[testDir].m_startPatterns == expected.m_ignorePatterns[testDir].m_startPatterns);
        QVERIFY(test.m_ignorePatterns[testDir].m_generalPatterns == expected.m_ignorePatterns[testDir].m_generalPatterns);
    }
};

QTEST_MAIN(CvsIgnoreListTest);

#include "CvsIgnoreListTest.moc"
