// SPDX-License-Identifier: GPL-2.0-or-later
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
#include <qglobal.h>

#include "../cvsignorelist.h"

class CvsIgnoreListTest : public QObject
{
    const QString defaultPatterns = QString::fromLatin1(". .. core RCSLOG tags TAGS RCS SCCS .make.state "
                                                          ".nse_depinfo #* .#* cvslog.* ,* CVS CVS.adm .del-* *.a *.olb *.o *.obj "
                                                          "*.so *.Z *~ *.old *.elc *.ln *.bak *.BAK *.orig *.rej *.exe _$* *$");
    Q_OBJECT
  private Q_SLOTS:
    void init()
    {
        CvsIgnoreList test;
        //sanity check defaults
        QVERIFY(test.m_exactPatterns.isEmpty());
        QVERIFY(test.m_endPatterns.isEmpty());
        QVERIFY(test.m_generalPatterns.isEmpty());
        QVERIFY(test.m_startPatterns.isEmpty());
    }


    void addEntriesFromString()
    {
        CvsIgnoreList test;
        CvsIgnoreList expected;

        QString testString = ". .. core RCSLOG tags TAGS RCS SCCS .make.state";
        test.addEntriesFromString(testString);
        QVERIFY(!test.m_exactPatterns.isEmpty());
        QVERIFY(test.m_exactPatterns == testString.split(' '));
        
        expected = test = CvsIgnoreList();
    }

    void testDefaults()
    {
        CvsIgnoreList test;
        CvsIgnoreList expected;
        MocIgnoreFile file;
        t_DirectoryList dirList;

        /*
            Verify default init. For this to work we must:
                1. Unset CVSIGNORE
                2. Insure no patterns are read from a .cvsignore file.
            MocCvsIgnore emulates a blank cvs file by default insuring the second condition.
        */
        test = CvsIgnoreList();
        //
        qunsetenv("CVSIGNORE");

        expected.addEntriesFromString(defaultPatterns);
        
        test.init(file, &dirList);
        QVERIFY(test.m_endPatterns == expected.m_endPatterns);
        QVERIFY(test.m_exactPatterns == expected.m_exactPatterns);
        QVERIFY(test.m_startPatterns == expected.m_startPatterns);
        QVERIFY(test.m_generalPatterns == expected.m_generalPatterns);
    }

};

QTEST_MAIN(CvsIgnoreListTest);

#include "CvsIgnorelist.moc"
