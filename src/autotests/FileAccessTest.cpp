/**
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2020 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * 
 */

#include <QTest>
#include <qglobal.h>

#include "../fileaccess.h"
#include "FileAccessJobHandlerMoc.h"

class FileAccessMoc: public FileAccess
{
  public:
    void setEngine(FileAccessJobHandler* handler)
    {
        if(handler != mJobHandler) delete mJobHandler, mJobHandler = handler;
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
        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocRoot));
        mocFile2.setEngine(new FileAccessJobHandlerMoc(&mocRoot));

        //Check remote url.
        mocRoot.setFileName(QLatin1String("root"));
        mocRoot.setUrl(QUrl("fish://i@0.0.0.0/root"));
        QCOMPARE(mocRoot.fileRelPath(), "");

        mocFile.setFileName(QLatin1String("x"));
        mocFile.setUrl(QUrl("fish://i@0.0.0.0/root/x"));
        mocFile.setParent(&mocRoot);
        QCOMPARE(mocFile.fileRelPath(), "x");

        mocFile2.setFileName("y");
        mocFile2.setUrl(QUrl("fish://i@0.0.0.0/root/x/y"));
        mocFile2.setParent(&mocFile);
        QCOMPARE(mocFile2.fileRelPath(), "x/y");
    }

    void testUrl()
    {
        FileAccessMoc mocFile;

        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));

        mocFile.setFileName(QLatin1String("root"));
        mocFile.setUrl(QUrl("fish://i@0.0.0.0/root"));
        QCOMPARE(mocFile.url(), QUrl("fish://i@0.0.0.0/root"));
    }

    void testIsLocal()
    {
        FileAccessMoc mocFile;

        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));

        QVERIFY(!FileAccess::isLocal(QUrl("fish://i@0.0.0.0/root")));
        QVERIFY(FileAccess::isLocal(QUrl("file:///dds/root")));
        QVERIFY(FileAccess::isLocal(QUrl("/dds/root")));
        //Check remote url.
        mocFile.setFileName(QLatin1String("root"));
        mocFile.setUrl(QUrl("fish://i@0.0.0.0/root"));
        QVERIFY(!mocFile.isLocal());

        //Check local file.
        mocFile.setFileName(QLatin1String("root"));
        mocFile.setUrl(QUrl("file:///dds/root"));
        QVERIFY(mocFile.isLocal());
        //Path only
        mocFile.setFileName(QLatin1String("root"));
        mocFile.setUrl(QUrl("/dds/root"));
        QVERIFY(mocFile.isLocal());
    }
    
    void testAbsolutePath()
    {
#ifndef Q_OS_WIN
        const QString expected = "/dds/root";
#else
        const QString expected = "C:/dds/root";
#endif // !Q_OS_WIN
        FileAccessMoc mocFile;
        mocFile.setEngine(new FileAccessJobHandlerMoc(&mocFile));

        const QUrl url = QUrl("fish://i@0.0.0.0/root");

        QCOMPARE(FileAccess::prettyAbsPath(url), url.toDisplayString());
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("file:///dds/root")), expected);
        QCOMPARE(FileAccess::prettyAbsPath(QUrl("/dds/root")), expected);

        mocFile.setFile(url);
        QCOMPARE(mocFile.prettyAbsPath(url), url.toDisplayString());

        mocFile.setFile(QUrl("file:///dds/root"));
        QCOMPARE(mocFile.prettyAbsPath(), expected);

        mocFile.setFile(QUrl("/dds/root"));
        QCOMPARE(mocFile.prettyAbsPath(), expected);
    }
};

QTEST_APPLESS_MAIN(FileAccessTest);

#include "FileAccessTest.moc"
