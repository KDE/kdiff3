/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "../fileaccess.h"
#include "SourceDataMoc.h"

#include <memory>

#include <QTemporaryFile>
#include <QTest>

class DataReadTest: public QObject
{
    Q_OBJECT;
  private Q_SLOTS:
    void initTestCase()
    {
        SourceDataMoc simData;
        QTemporaryFile testFile;

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
        FileAccess file = FileAccess(testFile.fileName());
        QVERIFY(file.exists());
        QVERIFY(file.isFile());
        QVERIFY(file.isReadable());
        QVERIFY(file.isLocal());
        QVERIFY(file.size() > 0);
        // Sanity check essential functions. Failure of these makes further testing pointless.
        simData.setFilename(testFile.fileName());
        QCOMPARE(simData.getFilename(), testFile.fileName());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(!simData.hasData());
        QVERIFY(simData.isText());
        QVERIFY(!simData.isDir());

        simData.setFilename("");
        QVERIFY(simData.getFilename().isEmpty());
        QVERIFY(simData.isEmpty());
        QVERIFY(!simData.hasData());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(simData.isText());
    }

    /*
        Check basic ablity to read data in.
    */
    void testRead()
    {
        QTemporaryFile testFile;
        SourceDataMoc simData;
        FileAccess file;
        //write out test file
        testFile.open();
        testFile.write(u8"//\n //\n     \t\n  D//   \t\n");
        testFile.close();
        file = FileAccess(testFile.fileName());
        QVERIFY(file.size() > 0);

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

    void testEOLStyle()
    {
        QTemporaryFile testFile;
        SourceDataMoc simData;

        testFile.open();
        testFile.write(u8"//\n //\n     \t\n  D//   \t\n");
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());

        QVERIFY(simData.getLineEndStyle() == eLineEndStyleUnix);

        testFile.resize(0);
        testFile.open();
        testFile.write(u8"//\r\n //\r\n     \t\r\n  D//   \t\r\n");
        testFile.close();

        simData.reset();
        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(QTextCodec::codecForName("UTF-8"), true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());

        QVERIFY(simData.getLineEndStyle() == eLineEndStyleDos);

        QTextCodec *codec = QTextCodec::codecForName("UTF-16");
        testFile.resize(0);

        testFile.open();
        testFile.write(codec->fromUnicode(u8"\r\n\r\n7\r\n"));
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(codec, true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getLineEndStyle(), eLineEndStyleDos);

        testFile.resize(0);
        testFile.open();
        testFile.write(codec->fromUnicode(u8"\n\n7\n"));
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(codec, true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getLineEndStyle(), eLineEndStyleUnix);

        codec = QTextCodec::codecForName("UTF-16LE");
        testFile.resize(0);

        testFile.open();
        testFile.write(codec->fromUnicode(u8"\r\n\r\n7\r\n"));
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(codec, true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getLineEndStyle(), eLineEndStyleDos);

        testFile.resize(0);

        testFile.open();
        testFile.write(codec->fromUnicode(u8"\n\n7\n"));
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(codec, true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getLineEndStyle(), eLineEndStyleUnix);

        codec = QTextCodec::codecForName("UTF-16BE");
        testFile.resize(0);

        testFile.open();
        testFile.write(codec->fromUnicode(u8"\r\n\r\n7\r\n"));
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(codec, true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getLineEndStyle(), eLineEndStyleDos);

        testFile.resize(0);

        testFile.open();
        testFile.write(codec->fromUnicode(u8"\n\n7\n"));
        testFile.close();

        simData.setFilename(testFile.fileName());
        simData.readAndPreprocess(codec, true);
        QVERIFY(simData.getErrors().isEmpty());
        QVERIFY(!simData.isFromBuffer());
        QVERIFY(!simData.isEmpty());
        QVERIFY(simData.hasData());
        QCOMPARE(simData.getLineEndStyle(), eLineEndStyleUnix);
    }

    void trailingEOLTest()
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
        QCOMPARE(lineData->size() - 1, 3); //Phantom line generated for last EOL mark if present

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
};

QTEST_MAIN(DataReadTest);

#include "datareadtest.moc"
