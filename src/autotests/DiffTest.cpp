/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "../diff.h"
#include "../SourceData.h"

#include <QTextCodec>
#include <QString>
#include <QTest>
#include <qtestcase.h>

class SourceDataMoc: public SourceData
{
    private:
        QSharedPointer<Options> defualtOptions = QSharedPointer<Options>::create();
    public:
        SourceDataMoc()
        {
            setOptions(defualtOptions);
        }

        [[nodiscard]] const QSharedPointer<Options>& options() { return defualtOptions; }
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
        SourceDataMoc simData;

        simData.setFilename(testFile.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QVERIFY(simData.getEncoding() != nullptr);
        QCOMPARE(simData.getSizeLines(), 5);
        QCOMPARE(simData.getSizeBytes(), file.size());
    }

    void eolTest()
    {
        QTemporaryFile eolTest;
        SourceDataMoc simData;

        eolTest.open();
        eolTest.write("\n}\n");
        eolTest.close();

        simData.setFilename(eolTest.fileName());
        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);

        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QVERIFY(simData.getEncoding() != nullptr);

        QVERIFY(simData.hasEOLTermiantion());
        QCOMPARE(simData.getSizeLines(), 3);
        QCOMPARE(simData.getSizeBytes(), FileAccess(eolTest.fileName()).size());

        const std::shared_ptr<LineDataVector> &lineData = simData.getLineDataForDisplay();
        QVERIFY(lineData != nullptr);
        QCOMPARE(lineData->size() - 1, 3);//Phantom line generated for last EOL mark if present

        QVERIFY(eolTest.remove());

        eolTest.open();
        eolTest.write("\n}");
        eolTest.close();

        simData.setFilename(eolTest.fileName());
        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);

        QVERIFY(simData.getErrors().isEmpty());

        QVERIFY(!simData.hasEOLTermiantion());
        QCOMPARE(simData.getSizeLines(), 2);
        QCOMPARE(simData.getSizeBytes(), FileAccess(eolTest.fileName()).size());
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
        QTemporaryFile testFile2;
        DiffList diffList;

        simData.setFilename(testFile.fileName());
        testFile2.open();
        testFile2.write(u8"//\nas //\n     \t\n  D//   \t\n");
        testFile2.close();
        //Verify that we actcually wrote the test file.
        FileAccess file2 = FileAccess(testFile2.fileName());
        QVERIFY(file2.exists());
        QVERIFY(file2.isFile());
        QVERIFY(file2.isReadable());
        QVERIFY(file2.size() > 0);

        simData2.setFilename(testFile2.fileName());

        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        std::shared_ptr<const LineDataVector> lineData = simData.getLineDataForDisplay();
        QVERIFY(simData.hasData());
        //Verify basic line data structure.
        QCOMPARE(lineData->size() - 1, 5);

        simData2.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData2.getErrors().isEmpty());
        QVERIFY(simData2.hasData());
        lineData = simData.getLineDataForDisplay();
        //Verify basic line data structure.
        QCOMPARE(lineData->size() - 1, 5);

        ManualDiffHelpList manualDiffList;
        manualDiffList.runDiff(simData.getLineDataForDiff(), simData.getSizeLines(), simData2.getLineDataForDiff(), simData2.getSizeLines(), diffList, e_SrcSelector::A, e_SrcSelector::B,
                               simData.options());
        /*
            Verify DiffList generated by runDiff. Not tested elsewhere.
        */
        DiffList::const_iterator diff = diffList.begin();
        QCOMPARE(diffList.size(), 2);
        QCOMPARE(diff->numberOfEquals(), 1);
        QCOMPARE(diff->diff1(), 1);
        QCOMPARE(diff->diff2(), 1);
        diff++;

        QCOMPARE(diff->numberOfEquals(), 3);
        QCOMPARE(diff->diff1(), 0);
        QCOMPARE(diff->diff2(), 0);

        Diff3LineList   diff3List;
        diff3List.calcDiff3LineListUsingAB(&diffList);
        QCOMPARE(diff3List.size(), 5);
                /*
    {[0] = {lineA = {mLineNumber = 0}, lineB = {mLineNumber = 0}, lineC = {mLineNumber = -1}, bAEqC = false, bBEqC = false, bAEqB = true, bWhiteLineA = false, bWhiteLineB = false, bWhiteLineC = false, pFineAB = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineBC = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineCA = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, mLinesNeededForDisplay = 1, mSumLinesNeededForDisplay = 0},
    [1] = {lineA = {mLineNumber = 1}, lineB = {mLineNumber = 1}, lineC = {mLineNumber = -1}, bAEqC = false, bBEqC = false, bAEqB = false, bWhiteLineA = false, bWhiteLineB = false, bWhiteLineC = false, pFineAB = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineBC = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineCA = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, mLinesNeededForDisplay = 1, mSumLinesNeededForDisplay = 0},
    [2] = {lineA = {mLineNumber = 2}, lineB = {mLineNumber = 2}, lineC = {mLineNumber = -1}, bAEqC = false, bBEqC = false, bAEqB = true, bWhiteLineA = false, bWhiteLineB = false, bWhiteLineC = false, pFineAB = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineBC = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineCA = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, mLinesNeededForDisplay = 1, mSumLinesNeededForDisplay = 0},
    [3] = {lineA = {mLineNumber = 3}, lineB = {mLineNumber = 3}, lineC = {mLineNumber = -1}, bAEqC = false, bBEqC = false, bAEqB = true, bWhiteLineA = false, bWhiteLineB = false, bWhiteLineC = false, pFineAB = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineBC = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, pFineCA = std::shared_ptr<DiffList> (empty) = {get() = 0x0}, mLinesNeededForDisplay = 1, mSumLinesNeededForDisplay = 0}}
    [4] = {lineA = (mLineNumber = 4), lineB = (mLineNumber = 4). lineC = (mLineNumber = -1), bAEqC = false, bBEqC = false, bAEqB = true, bWhiteLineA = false, bWhiteLineB = false, bWhiteLineC = false, pFineAB = nullptr, pFineBC = nullptr, pFineCA = nullptr, mLinesNeededForDisplay = 1. mSumLinesNeededForDisplay = 0}
        */
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
