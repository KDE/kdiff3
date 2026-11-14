// clang-format off
/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2020 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
// clang-format on

#include <QTest>
#include <QtGlobal>

#include "../fileaccess.h"
#include "../ProgressProxy.h"
#include "FileAccessJobHandlerMoc.h"

#include <QTemporaryDir>

class FileAccessMoc: public FileAccess
{
  public:
    void setEngine(FileAccessJobHandler* handler)
    {
        if(handler != mJobHandler.get()) mJobHandler.reset(handler);
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

    void cancelledWritePreservesDestination_data()
    {
        QTest::addColumn<bool>("existingDestination");
        QTest::newRow("existing-file") << true;
        QTest::newRow("new-file") << false;
    }

    void cancelledWritePreservesDestination()
    {
        QFETCH(bool, existingDestination);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("result.txt");
        const QByteArray original("original contents");
        if(existingDestination)
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(original), original.size());
        }

        FileAccess destination(path);
        const QByteArray replacement(200000, 'x');
        signals2::scoped_connection cancel = ProgressProxy::wasCancelled.connect([] { return true; });
        QVERIFY(!destination.writeFile(replacement.constData(), replacement.size()));
        cancel.disconnect();

        QCOMPARE(QFile::exists(path), existingDestination);
        if(existingDestination)
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), original);
        }
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size(), existingDestination ? 1 : 0);
    }

    void successfulWritePreservesPermissions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("result.txt");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("old"), 3);
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto permissions = file.permissions();

        FileAccess destination(path);
        const QByteArray replacement("new contents");
        QVERIFY(destination.writeFile(replacement.constData(), replacement.size()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), replacement);
        QCOMPARE(file.permissions(), permissions);
    }

    void successfulWriteThroughSymlink()
    {
#ifdef Q_OS_WIN
        QSKIP("QFile::link creates shortcuts on Windows.");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath("target.txt");
        const QString link = dir.filePath("link.txt");
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("old"), 3);
        file.close();
        QVERIFY(QFile::link(target, link));

        FileAccess destination(link);
        const QByteArray replacement("new contents");
        QVERIFY(destination.writeFile(replacement.constData(), replacement.size()));
        QVERIFY(QFileInfo(link).isSymLink());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), replacement);
#endif
    }

    void writeEmptyFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("empty.txt");
        FileAccess destination(path);
        QVERIFY(destination.writeFile("", 0));
        QVERIFY(QFile::exists(path));
        QCOMPARE(QFileInfo(path).size(), 0);
    }

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
        QCOMPARE(fileData.prettyAbsPath(), testFile.fileName());
        QVERIFY(fileData.isValid());
        QVERIFY(fileData.isLocal());
        QVERIFY(fileData.isNormal());
        QVERIFY(fileData.isReadable());
        QVERIFY(fileData.isWritable());
        QVERIFY(fileData.getStatusText().isEmpty());
        //sanity check
        QVERIFY(!fileData.isSymLink());
        QVERIFY(!fileData.isDir());
        QVERIFY(fileData.isFile());

        testFile.write("\n\n7\n");
        testFile.close();
        char buf[4] = {0, 0, 0, 0};
        bool r = fileData.readFile(buf, 1);
        QVERIFY(r);
        QVERIFY(fileData.getStatusText().isEmpty());

        r = fileData.open(QFile::ReadOnly);
        QVERIFY(r);
        QVERIFY(fileData.getStatusText().isEmpty());
        fileData.close();

        fileData.setFile("xyz");
        QCOMPARE(fileData.fileName(), "xyz");
        r = fileData.open(QFile::ReadOnly);
        QVERIFY(!r);
        QVERIFY(!fileData.getStatusText().isEmpty());

        fileData.setFile(testFile.fileName());
        r = fileData.open(QFile::ReadOnly);
        QVERIFY(r);
        QVERIFY(fileData.getStatusText().isEmpty());
    }
};

QTEST_APPLESS_MAIN(FileAccessTest);

#include "FileAccessTest.moc"
