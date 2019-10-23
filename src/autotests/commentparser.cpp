/**
 * Copyright (C) 2019 Michael Reeves <reeves.87@gmail.com>
 *
 * This file is part of KDiff3.
 *
 * KDiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * KDiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KDiff3.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QTest>

#include "../CommentParser.h"

class CommentParserTest : public QObject
{
    Q_OBJECT
  private slots:
    void init()
    {
        DefaultCommentParser parser;

        //Sanity check defaults.
        QVERIFY(!parser.isPureComment());
        QVERIFY(!parser.isEscaped());
        QVERIFY(!parser.inString());
        QVERIFY(!parser.inComment());
    }

    void singleLineComment1()
    {
        DefaultCommentParser test1;

        test1.processLine("//ddj ?*8");
        QVERIFY(!test1.inComment());
        QVERIFY(test1.isPureComment());
    }

    void singleLineComment2()
    {
        DefaultCommentParser test1;

        test1.processLine("//comment with quotes embedded \"\"");
        QVERIFY(!test1.inComment());
        QVERIFY(test1.isPureComment());
    }

    void singleLineComment3()
    {
        DefaultCommentParser test1;

        test1.processLine("anythis//endof line comment");
        QVERIFY(!test1.inComment());
        QVERIFY(!test1.isPureComment());
    }

    void singleLineComment4()
    {
        DefaultCommentParser test1;

        test1.processLine("anythis//ignore embedded multiline sequence  /*");
        QVERIFY(!test1.inComment());
        QVERIFY(!test1.isPureComment());
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
        QVERIFY(test.isPureComment());
        QVERIFY(!test.inComment());

        //mid line comment start.
        test = DefaultCommentParser();
        test.processLine("fskk /* kjd */");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isPureComment());

        //mid line comment start. multiple lines
        test = DefaultCommentParser();
        test.processLine("fskk /* kjd ");
        QVERIFY(test.inComment());
        QVERIFY(!test.isPureComment());

        test.processLine("  comment line ");
        QVERIFY(test.inComment());
        QVERIFY(test.isPureComment());

        test.processLine("  comment */ not comment ");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isPureComment());

        //mid line comment start. multiple lines
        test = DefaultCommentParser();
        test.processLine("fskk /* kjd ");
        QVERIFY(test.inComment());
        QVERIFY(!test.isPureComment());

        test.processLine("  comment line ");
        QVERIFY(test.inComment());
        QVERIFY(test.isPureComment());
        //embedded single line character sequence should not end comment
        test.processLine("  comment line //");
        QVERIFY(test.inComment());
        QVERIFY(test.isPureComment());

        test.processLine("  comment */");
        QVERIFY(!test.inComment());
        QVERIFY(test.isPureComment());

        //Escape squeances not relavate to comments
        test.processLine("/*  comment \\*/");
        QVERIFY(!test.inComment());
        QVERIFY(test.isPureComment());

        //invalid in C++ should not be flagged as pure comment
        test.processLine("/*  comment */ */");
        QVERIFY(!test.inComment());
        QVERIFY(!test.isPureComment());
    }

    void stringTest()
    {
        DefaultCommentParser test;

        test.processLine("\"quoted string // \"");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isPureComment());

        test = DefaultCommentParser();
        test.processLine("\"quoted string /* \"");
        QVERIFY(!test.inString());
        QVERIFY(!test.inComment());
        QVERIFY(!test.isPureComment());

        //test only escape sequence we care about
        test = DefaultCommentParser();
        test.processChar("\"", '"');
        QVERIFY(!test.isEscaped());
        QVERIFY(test.inString());

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
};

QTEST_MAIN(CommentParserTest);

#include "commentparser.moc"
