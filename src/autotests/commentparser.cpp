/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include <QTest>

#include "../CommentParser.h"

class CommentParserTest : public QObject
{
    Q_OBJECT
  private Q_SLOTS:
    void init()
    {
        DefaultCommentParser parser;

        //Sanity check defaults.
        QVERIFY(!parser.isSkipable());
        QVERIFY(!parser.isEscaped());
        QVERIFY(!parser.inString());
        QVERIFY(!parser.inComment());
        QVERIFY(!parser.isPureComment());
    }

    void singleLineComment()
    {
        DefaultCommentParser test;

        test.processLine("//ddj ?*8");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        //ignore trailing+leading whitespace
        test = DefaultCommentParser();
        test.processLine("\t  \t  // comment     ");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("//comment with quotes embedded \"\"");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("anythis//endof line comment");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("anythis//ignore embedded multiline sequence  /*");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());
    }

    void inComment()
    {
        DefaultCommentParser test;

        test.mCommentType = DefaultCommentParser::multiLine;
        QVERIFY(test.inComment());

        test.mCommentType = DefaultCommentParser::singleLine;
        QVERIFY(test.inComment());

        test.mCommentType = DefaultCommentParser::none;
        QVERIFY(!test.inComment());
    }

    void multiLineComment()
    {
        DefaultCommentParser test;

        //mutiline syntax on one line
        test.processLine("/* kjd*/");
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());
        QVERIFY(!test.inComment());

        //mid line comment start.
        test = DefaultCommentParser();
        test.processLine("fskk /* kjd */");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());


        //mid line comment start. multiple lines
        test = DefaultCommentParser();
        test.processLine("fskk /* kjd ");
        QVERIFY(test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test.processLine("  comment line ");
        QVERIFY(test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        test.processLine("  comment */ not comment ");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        //mid line comment start. multiple lines
        test = DefaultCommentParser();
        test.processLine("fskk /* kjd ");
        QVERIFY(test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test.processLine("  comment line ");
        QVERIFY(test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        test.processLine("  comment * line /  ");
        QVERIFY(test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        //embedded single line character sequence should not end comment
        test.processLine("  comment line //");
        QVERIFY(test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        test.processLine("  comment */");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        //Escape squeances not relavate to comments
        test = DefaultCommentParser();
        test.processLine("/*  comment \\*/");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("  qint32 i = 8 / 8 * 3;/* comment*/");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        //invalid in C++ should not be flagged as pure comment
        test.processLine("/*  comment */ */");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        //leading whitespace
        test = DefaultCommentParser();
        test.processLine("\t  \t  /*  comment */");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(!test.isPureComment());

        //trailing whitespace
        test = DefaultCommentParser();
        test.processLine("\t  \t  /*  comment */    ");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("/*  comment */    ");
        QVERIFY(!test.inComment());
        QVERIFY(test.isSkipable());
        QVERIFY(!test.isPureComment());
    }

    void stringTest()
    {
        DefaultCommentParser test;

        test.processLine("\"quoted string // \"");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("\"quoted string /* \"");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("\"\"");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());

        test = DefaultCommentParser();
        test.processChar("\"", '"');
        QVERIFY(!test.isEscaped());
        QVERIFY(test.inString());

        test.processChar("\"'", '\'');
        QVERIFY(test.inString());
        test.processChar("\"'\"", '"');
        QVERIFY(!test.inString());

        //test only escape sequence we care about
        test = DefaultCommentParser();
        test.processChar("\"", '"');

        test.processChar("\"", '\\');
        QVERIFY(test.isEscaped());
        QVERIFY(test.inString());

        test.processChar("\"\\\"", '"');
        QVERIFY(!test.isEscaped());
        QVERIFY(test.inString());

        test.processChar("\"\\\"\"", '"');
        QVERIFY(!test.isEscaped());
        QVERIFY(!test.inString());
    }

    void quotedCharacter()
    {
        DefaultCommentParser test;

        test.processLine("'\"'");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test.processLine("'a'");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test.processLine("'\\\''");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test.processLine("'*'");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test.processLine("'/'");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());
    }

    void nonComment()
    {
        DefaultCommentParser test;

        test.processLine("  qint32 i = 8 / 8 * 3;");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("  ");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("  / ");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("  * ");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());

        //invalid in C++ should not be flagged as non-comment
        test.processLine(" */");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isSkipable());
        QVERIFY(!test.isPureComment());
    }

    void removeComment()
    {
        DefaultCommentParser test;
        QString line=u8"  qint32 i = 8 / 8 * 3;", correct=u8"  qint32 i = 8 / 8 * 3;";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        test = DefaultCommentParser();
        correct = line = u8"  //qint32 i = 8 / 8 * 3;";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);

        test = DefaultCommentParser();
        correct = line = u8"//  qint32 i = 8 / 8 * 3;";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        test = DefaultCommentParser();
        line = u8"  qint32 i = 8 / 8 * 3;// comment";
        correct = u8"  qint32 i = 8 / 8 * 3;          ";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        test = DefaultCommentParser();
        line = u8"  qint32 i = 8 / 8 * 3;/* comment";
        correct = u8"  qint32 i = 8 / 8 * 3;          ";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        correct = line = u8"  qint32 i = 8 / 8 * 3;/* mot a comment";
        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        //end comment mid-line
        line = u8"d  */ why";
        correct = u8"      why";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        test = DefaultCommentParser();
        line = u8"  qint32 i = 8 / 8 * 3;/* comment*/";
        correct = u8"  qint32 i = 8 / 8 * 3;            ";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        test = DefaultCommentParser();
        correct = line = u8"  /*qint32 i = 8 / 8 * 3;/* comment*/";

        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        //line with multiple comments weird but legal c/c++
        test = DefaultCommentParser();
        line = u8"  qint32 /*why?*/ i = 8 / 8 * 3;/* comment*/";
        correct = u8"  qint32          i = 8 / 8 * 3;            ";
        test.processLine(line);
        test.removeComment(line);
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());

        //invalid in C++ should be flagged as non-comment
        test = DefaultCommentParser();
        line = correct = " */";
        test.processLine(" */");
        QCOMPARE(line, correct);
        QCOMPARE(line.length(), correct.length());
    }
};

QTEST_MAIN(CommentParserTest);

#include "commentparser.moc"
