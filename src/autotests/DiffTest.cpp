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
#include <qttestglobal.h>

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
        tempFile.open();
        tempFile.write(u8"//\n //\n     \t\n  D//   \t\n");
        tempFile.close();

        auto file = FileAccess(tempFile.fileName());
        QVERIFY(file.exists());
        QVERIFY(file.isFile());
        QVERIFY(file.isReadable());

        FileAccess faRec = FileAccess(tempFile.fileName());
        QVERIFY(faRec.isLocal());
        QVERIFY(faRec.exists());
        QVERIFY(faRec.isFile());
        QVERIFY(faRec.size() > 0);

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
        QCOMPARE(simData.getSizeLines(), 4);
        QCOMPARE(simData.getSizeBytes(), file.size());

        const QVector<LineData>* lineData = simData.getLineDataForDisplay();
        QVERIFY(lineData != nullptr);
        QCOMPARE(lineData->size() - 1, 4);
        QVERIFY(!(*lineData)[0].whiteLine());
        QVERIFY((*lineData)[0].isSkipable());
        QVERIFY((*lineData)[0].isPureComment());

        QVERIFY(!(*lineData)[1].whiteLine());
        QVERIFY(!(*lineData)[1].isPureComment());
        QVERIFY((*lineData)[1].isSkipable());

        QVERIFY((*lineData)[2].whiteLine());
        QVERIFY(!(*lineData)[2].isPureComment());
        QVERIFY((*lineData)[2].isSkipable());

        QVERIFY(!(*lineData)[3].whiteLine());
        QVERIFY(!(*lineData)[3].isPureComment());
        QVERIFY(!(*lineData)[3].isSkipable());
   }

};

QTEST_MAIN(DiffTest);

#include "DiffTest.moc"
