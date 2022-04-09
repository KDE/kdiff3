/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "../diff.h"
#include "../fileaccess.h"

#include "SourceDataMoc.h"

#include <memory>

#include <QTextCodec>
#include <QString>
#include <QTest>

class Diff3LineMoc: public Diff3Line
{
  public:
    Diff3LineMoc(const LineRef inLineA, const LineRef inLineB, const LineRef inLineC,
                  bool inAEqC, bool inBEqC, bool inAEqB,
                  bool inWhiteLineA, bool inWhiteLineB, bool inWhiteLineC)
    {
        setLineA(inLineA);
        setLineB(inLineB);
        setLineC(inLineC);

        bBEqC = inBEqC;
        bAEqC = inAEqC;
        bAEqB = inAEqB;

        bWhiteLineA = inWhiteLineA;
        bWhiteLineB = inWhiteLineB;
        bWhiteLineC = inWhiteLineC;
    }
};

class DiffTest: public QObject
{
    Q_OBJECT;
  private:
    QTemporaryFile  testFile;
    FileAccess      file;

  private Q_SLOTS:
    void initTestCase()
    {
        SourceDataMoc simData;

        //Check assumed default values.
        QVERIFY(simData.isEmpty());
        QVERIFY(simData.isValid());
        QVERIFY(!simData.hasData());
        QVERIFY(simData.isText());
        QVERIFY(simData.getBuf() == nullptr);
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isIncompleteConversion());
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.hasEOLTermiantion());
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
    }

    /*
        This validates that LineData records are being setup with the correct data.
        Only size and raw content are checked here.
    */
    void testLineData()
    {
        SourceDataMoc simData;
        simData.setFilename(testFile.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);

        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getSizeLines(), 5);
        QCOMPARE(simData.getSizeBytes(), file.size());

        const std::shared_ptr<LineDataVector> &lineData = simData.getLineDataForDisplay();
        //Verify LineData is being setup correctly.
        QVERIFY(lineData != nullptr);
        QCOMPARE(lineData->size() - 1, 5);

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

    void testTwoWayDiffLineChanged()
    {
        SourceDataMoc simData, simData2;
        QTemporaryFile testFile2, testFile3;
        FileAccess file2;

        std::shared_ptr<const LineDataVector> lineData;
        ManualDiffHelpList manualDiffList;
        DiffList diffList, expectedDiffList;
        Diff3LineList diff3List, expectedDiff3;

        testFile2.open();
        testFile2.write(u8"1\n2\n3\n");
        testFile2.close();
        // Verify that we actcually wrote the test file.
        file2 = FileAccess(testFile2.fileName());
        QVERIFY(file2.exists());
        QVERIFY(file2.isFile());
        QVERIFY(file2.isReadable());
        QVERIFY(file2.size() > 0);

        testFile3.open();
        testFile3.write(u8"1\n4\n3\n");
        testFile3.close();

        simData2.setFilename(testFile2.fileName());
        simData.setFilename(testFile3.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(simData.hasData());

        lineData = simData.getLineDataForDisplay();
        QVERIFY(simData.hasData());
        // Verify basic line data structure.
        QCOMPARE(lineData->size() - 1, 4);

        simData2.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData2.getErrors().isEmpty());
        QVERIFY(simData2.hasData());

        manualDiffList.runDiff(simData.getLineDataForDiff(), simData.getSizeLines(), simData2.getLineDataForDiff(), simData2.getSizeLines(), diffList, e_SrcSelector::A, e_SrcSelector::B,
                               simData.options());
        /*
            Verify DiffList generated by runDiff.
        */
        expectedDiffList = {{1, 1, 1}, {2, 0, 0}};
        QVERIFY(expectedDiffList == diffList);

        /*
            Verify Diff3List generated by calcDiff3LineListUsingAB.
        */
        diff3List.calcDiff3LineListUsingAB(&diffList);
        QCOMPARE(diff3List.size(), 4);
        // This hard codes the known good result of the above for comparison.
        // In general this should not be changed unless you know what your doing.
        expectedDiff3 = {Diff3LineMoc(0, 0, LineRef::invalid, false, false, true, false, false, false),
                         Diff3LineMoc(1, 1, LineRef::invalid, false, false, false, false, false, false),
                         Diff3LineMoc(2, 2, LineRef::invalid, false, false, true, false, false, false),
                         Diff3LineMoc(3, 3, LineRef::invalid, false, false, true, false, false, false)};

        QVERIFY(diff3List == expectedDiff3);
    }

    void testWhiteLineComment()
    {
        SourceDataMoc simData;

        simData.setFilename(testFile.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getSizeBytes(), file.size());

        const std::shared_ptr<LineDataVector> &lineData = simData.getLineDataForDisplay();
        //Verify we actually have data.
        QVERIFY(lineData != nullptr);
        QCOMPARE(lineData->size() - 1, 5);

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

        QVERIFY((*lineData)[4].whiteLine());
        QVERIFY(!(*lineData)[4].isPureComment());
        QVERIFY(!(*lineData)[4].isSkipable());
   }

};

QTEST_MAIN(DiffTest);

#include "DiffTest.moc"
