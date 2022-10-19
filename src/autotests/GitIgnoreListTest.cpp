/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021-2021 David Hallas <david@davidhallas.dk>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include <QTest>
#include <QtGlobal>

#include "../GitIgnoreList.h"
#include "../fileaccess.h"

class GitIgnoreListStub final : public GitIgnoreList
{
  public:
    QString m_fileContents;
    [[nodiscard]] QString readFile(const QString& fileName) const final
    {
        Q_UNUSED(fileName);
        return m_fileContents;
    }
};

class GitIgnoreListTest : public QObject
{
    Q_OBJECT
  private Q_SLOTS:
    void matches()
    {
        const QString testDir("dir");
        DirectoryList directoryList;
        // Empty dir doesn't match anything
        {
            GitIgnoreListStub testObject;
            testObject.enterDir(testDir, directoryList);
            QVERIFY(testObject.matches(testDir, "foo", true) == false);
        }
        // Simple .gitignore file containing wild cards
        {
            FileAccess gitignoreFile(".gitignore");
            directoryList.push_back(gitignoreFile);
            GitIgnoreListStub testObject;
            testObject.m_fileContents = QString("foo\n*.cpp\n#comment");
            testObject.enterDir(testDir, directoryList);
            QVERIFY(testObject.matches(testDir, "foo", true) == true);
            QVERIFY(testObject.matches(testDir, "FOO", true) == false);
            QVERIFY(testObject.matches(testDir, "FOO", false) == true);
            QVERIFY(testObject.matches(testDir, "file.cpp", false) == true);
            QVERIFY(testObject.matches(testDir, "file.h", false) == false);
            QVERIFY(testObject.matches(testDir, "#comment", false) == false);
        }
        // Test that matches honors the directory and any subdirectories
        {
            const QString otherTestDir("other_dir");
            const QString testSubDir("dir/sub");
            FileAccess gitignoreFile(".gitignore");
            directoryList.push_back(gitignoreFile);
            GitIgnoreListStub testObject;
            testObject.m_fileContents = QString("foo");
            testObject.enterDir(testDir, directoryList);
            QVERIFY(testObject.matches(testDir, "foo", true) == true);
            QVERIFY(testObject.matches(testSubDir, "foo", true) == true);
            QVERIFY(testObject.matches(otherTestDir, "foo", true) == false);
        }
    }
};

QTEST_MAIN(GitIgnoreListTest);

#include "GitIgnoreListTest.moc"
