/**
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2020 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * 
 */

#include <QTest>
#include <qglobal.h>

#include "../FileAccessJobHandler.h"
#include "../fileaccess.h"

class FileAccessMoc: public FileAccess
{
  public:
        void setParent(FileAccessMoc* parent) { m_pParent = (FileAccess*)parent;}
        void setFileName(const QString& name) { m_name = name;}
        void setUrl(const QUrl& url) { m_url = url; }

        QString fileRelPath() const { return ((FileAccess*)this)->fileRelPath(); }
};
class FileAccessTest: public QObject
{
    Q_OBJECT;
  private Q_SLOTS:
    void testFileRelPath()
    {
        FileAccessMoc mocFile, mocRoot, mocFile2;

        mocRoot.setFileName(QLatin1String("root"));
        mocRoot.setUrl(QUrl("fish://i@0.0.0.0/root"));
        QVERIFY(mocRoot.fileRelPath().isEmpty());

        mocFile.setFileName(QLatin1String("x"));
        mocFile.setUrl(QUrl("fish://i@0.0.0.0/root/x"));
        mocFile.setParent(&mocRoot);
        QVERIFY(mocFile.fileRelPath() == "x");

        mocFile2.setFileName("y");
        mocFile2.setUrl(QUrl("fish://i@0.0.0.0/root/x/y"));
        mocFile2.setParent(&mocFile);
        QVERIFY(mocFile2.fileRelPath() == "x/y");
    }
};

QTEST_MAIN(FileAccessTest);

#include "FileAccessTest.moc"
