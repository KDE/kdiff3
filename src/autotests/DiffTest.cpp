/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "../diff.h"
#include "../SourceData.h"

#include <QString>
#include <QTest>

#include <qglobal.h>

class DiffTest: public QObject
{
    Q_OBJECT;
  private:
    QTemporaryFile  tempFile;
    SourceData      simData;
    QSharedPointer<Options>  defualtOptions = QSharedPointer<Options>::create();

  private Q_SLOTS:
    void initTestCase()
    {
        //Check assumed default values.
        QVERIFY(simData.isEmpty());
        QVERIFY(simData.isValid());
        QVERIFY(!simData.hasData());
        QVERIFY(simData.isText());
        QVERIFY(simData.getBuf() == nullptr);
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isIncompleteConversion());
        QVERIFY(simData.getErrors().isEmpty());
        //Setup default options for.
        simData.setOptions(defualtOptions);
    }
    void testWhiteLineComment()
    {
        //First chech LineData itself. Not much to check here only whiteLine does actual work.
        auto line = QSharedPointer<QString>::create(u8"/*\n");
        auto dataRec = LineData(line, 0, line->length(), 0, true);

        QVERIFY(dataRec.isPureComment());
        QVERIFY(!dataRec.whiteLine());

        line = QSharedPointer<QString>::create(u8"     \t\n");
        dataRec = LineData(line, 0, line->length(), 6, false);

        QVERIFY(!dataRec.isPureComment());
        QVERIFY(dataRec.whiteLine());

        line = QSharedPointer<QString>::create(u8"  /*   \t\n");
        dataRec = LineData(line, 0, line->length(), 2, false);

        QVERIFY(!dataRec.isPureComment());
        QVERIFY(!dataRec.whiteLine());

        tempFile.open();
        tempFile.write(u8"/*\n */\n     \t\n  /*   \t\n");

        auto file = FileAccess(tempFile.fileName());
        QVERIFY(file.exists());
        QVERIFY(file.isFile());
        QVERIFY(file.isReadable());

        simData.setFilename(tempFile.fileName());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(!simData.hasData()); //setFileName should not load data.

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QVERIFY(simData.getEncoding() != nullptr);
   }

};

QTEST_MAIN(DiffTest);

#include "DiffTest.moc"
