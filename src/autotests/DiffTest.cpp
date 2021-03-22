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
    QTemporaryFile  testFile;
    FileAccess      file;
    QSharedPointer<Options>  defualtOptions = QSharedPointer<Options>::create();

  private Q_SLOTS:
    void initTestCase()
    {
        SourceData simData;

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
        //write out test file
        testFile.open();
        testFile.write(u8"//\n //\n     \t\n  D//   \t\n");
        testFile.close();
        //Verify that we actcually wrote the test file.
        file = FileAccess(testFile.fileName());
        QVERIFY(file.exists());
        QVERIFY(file.isFile());
        QVERIFY(file.isReadable());
        QVERIFY(file.isLocal());
        QVERIFY(file.size() > 0);
        //Sanity check essential functions. Failure of these makes further testing pointless.
        simData.setFilename(testFile.fileName());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(!simData.hasData());
        QCOMPARE(simData.getFilename(), testFile.fileName());

        simData.setFilename("");
        QVERIFY(simData.isEmpty());
        QVERIFY(!simData.hasData());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(simData.getFilename().isEmpty());
    }

    /*
        Check basic ablity to read data in.
    */
    void testRead()
    {
        SourceData simData;
        simData.setOptions(defualtOptions);

        simData.setFilename(testFile.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QVERIFY(simData.getEncoding() != nullptr);
        QCOMPARE(simData.getSizeLines(), 4);
        QCOMPARE(simData.getSizeBytes(), file.size());
    }

    /*
        This validates that LineData records are being setup with the correct data.
        Only size and raw content are checked here.
    */
    void testLineData()
    {
        SourceData simData;
        simData.setOptions(defualtOptions);
        simData.setFilename(testFile.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);

        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getSizeLines(), 4);
        QCOMPARE(simData.getSizeBytes(), file.size());

        const QVector<LineData>* lineData = simData.getLineDataForDisplay();
        //Verify LineData is being setup correctly.
        QVERIFY(lineData != nullptr);
        QCOMPARE(lineData->size() - 1, 4);

        QVERIFY((*lineData)[0].getBuffer() != nullptr);
        QCOMPARE((*lineData)[0].size(), 2);
        QCOMPARE((*lineData)[0].getFirstNonWhiteChar(), 1);
        QCOMPARE((*lineData)[0].getLine(), "//");

        QVERIFY((*lineData)[1].getBuffer() != nullptr);
        QCOMPARE((*lineData)[1].size(), 3);
        QCOMPARE((*lineData)[1].getFirstNonWhiteChar(), 2);
        QCOMPARE((*lineData)[1].getLine(), " //");

        QVERIFY((*lineData)[2].getBuffer() != nullptr);
        QCOMPARE((*lineData)[2].size(), 6);
        QCOMPARE((*lineData)[2].getFirstNonWhiteChar(), 0);
        QCOMPARE((*lineData)[2].getLine(), "     \t");

        QVERIFY((*lineData)[3].getBuffer() != nullptr);
        QCOMPARE((*lineData)[3].size(), 9);
        QCOMPARE((*lineData)[3].getFirstNonWhiteChar(), 3);
        QCOMPARE((*lineData)[3].getLine(), "  D//   \t");
    }

    void testWhiteLineComment()
    {
        SourceData simData;
        simData.setOptions(defualtOptions);

        simData.setFilename(testFile.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getSizeBytes(), file.size());

        const QVector<LineData>* lineData = simData.getLineDataForDisplay();
        //Verify we actually have data.
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
