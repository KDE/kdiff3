/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2020 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include <QTest>
#include <QtGlobal>

#include "../fileaccess.h"
#include "FileAccessJobHandlerMoc.h"

class FileAccessMoc: public FileAccess
{
  public:
    void setEngine(FileAccessJobHandler* handler)
    {
        if(handler != mJobHandler.data()) mJobHandler.reset(handler);
    }

    void setParent(FileAccessMoc* parent)
    {
        m_pParent = (FileAccess*)parent;
        m_baseDir = parent->m_baseDir;
    }
    void setFileName(const QString& name) { m_name = name; }
    void setUrl(const QUrl& url) { m_url = url; }

    void loadData() override {}
};

class FileAccessTest: public QObject
{
    Q_OBJECT;

  private Q_SLOTS:

    void testFileRelPath()
    {
        FileAccessMoc mocFile, mocRoot, mocFile2;

        mocRoot.setEngine(new FileAccessJobHandlerMoc(&mocRoot));
        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));
        mocFile2.setEngine(new FileAccessJobHandlerMoc(&mocFile2));

        //Check remote url.
        mocRoot.setFile(u8"fish://i@0.0.0.0/root");
        QCOMPARE(mocRoot.fileRelPath(), u8"");

        mocFile.setFile(u8"fish://i@0.0.0.0/root/x");
        mocFile.setParent(&mocRoot);
        QCOMPARE(mocFile.fileRelPath(), QStringLiteral("x"));

        mocFile2.setFile("fish://i@0.0.0.0/root/x/y");
        mocFile2.setParent(&mocFile);
        QCOMPARE(mocFile2.fileRelPath(), QStringLiteral("x/y"));
    }

    void testUrl()
    {
#ifndef Q_OS_WIN
        const QString expected = "/dds/root";
        const QString expected2 = "file:///dds/root";
#else
        const QString expected = "C:/dds/root";
        const QString expected2 = "file:///C:/dds/root";
#endif // !Q_OS_WIN
        FileAccessMoc mocFile;

        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));

        //Sanity check FileAccess::url by directly setting the url.
        mocFile.setFileName(u8"root");
        mocFile.setUrl(QUrl("fish://i@0.0.0.0/root"));
        QCOMPARE(mocFile.url(), QUrl("fish://i@0.0.0.0/root"));

        //Now check that setFile does what its supposed to
        mocFile.setFile(QUrl("fish://i@0.0.0.0/root"));
        QCOMPARE(mocFile.url(), QUrl("fish://i@0.0.0.0/root"));

        mocFile.setFile(QUrl(expected));
        QCOMPARE(mocFile.url(), QUrl(expected));
        //Try the QString version
        mocFile.setFile("fish://i@0.0.0.0/root");
        QCOMPARE(mocFile.url(), QUrl("fish://i@0.0.0.0/root"));

        mocFile.setFile(expected);
        QCOMPARE(mocFile.url(), QUrl(expected2));
    }

    void testIsLocal()
    {
        FileAccessMoc mocFile;

        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));

        QVERIFY(!FileAccess::isLocal(QUrl("fish://i@0.0.0.0/root")));
        QVERIFY(FileAccess::isLocal(QUrl("file:///dds/root")));
        QVERIFY(FileAccess::isLocal(QUrl("/dds/root")));
        //Check remote url.
        mocFile.setFile("fish://i@0.0.0.0/root");
        QVERIFY(!mocFile.isLocal());

        //Check local file.
        mocFile.setFile(QStringLiteral("file:///dds/root"));
        QVERIFY(mocFile.isLocal());
        //Path only
        mocFile.setFile(QStringLiteral("/dds/root"));
        QVERIFY(mocFile.isLocal());
    }

    void testAbsolutePath()
    {
#ifndef Q_OS_WIN
        const QString expected = "/dds/root";
#else
        const QString expected = "c:/dds/root";
#endif // !Q_OS_WIN
        FileAccessMoc mocFile;
        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));

        const QUrl url = QUrl("fish://i@0.0.0.0/root");

        QCOMPARE(FileAccess::prettyAbsPath(url), url.toDisplayString());
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("file:///dds/root")).toLower(), expected);
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("/dds/root")).toLower(), expected);
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("c:/dds/root")).toLower(), "c:/dds/root");
        //work around for bad path in windows drop event urls. (Qt 5.15.2 affected)
#ifndef Q_OS_WIN
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("file:///c:/dds/root")).toLower(), "/c:/dds/root");
#else
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("file:///c:/dds/root")).toLower(), "c:/dds/root");
#endif // !Q_OS_WIN

        mocFile.setFile(url);
        QCOMPARE(mocFile.prettyAbsPath(), url.toDisplayString());

        mocFile.setFile(QUrl("file:///dds/root"));
        QCOMPARE(mocFile.prettyAbsPath().toLower(), expected);

        mocFile.setFile(QUrl("/dds/root"));
        QCOMPARE(mocFile.prettyAbsPath().toLower(), expected);

        mocFile.setFile(QStringLiteral("file:///dds/root"));
        QCOMPARE(mocFile.prettyAbsPath().toLower(), expected);

        mocFile.setFile(QStringLiteral("/dds/root"));
        QCOMPARE(mocFile.prettyAbsPath().toLower(), expected);
    }

    void liveTest()
    {
        QTemporaryFile testFile;
        QVERIFY(testFile.open());
        FileAccess fileData(testFile.fileName());

        QVERIFY(fileData.isValid());
        QVERIFY(fileData.isLocal());
        QVERIFY(fileData.isNormal());
        QVERIFY(fileData.isReadable());
        QVERIFY(fileData.isWritable());
        //sanity check
        QVERIFY(!fileData.isSymLink());
        QVERIFY(!fileData.isDir());
        QVERIFY(fileData.isFile());
    }
};

QTEST_APPLESS_MAIN(FileAccessTest);

#include "FileAccessTest.moc"
