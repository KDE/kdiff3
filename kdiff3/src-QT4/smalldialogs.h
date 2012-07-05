/***************************************************************************
 *   Copyright (C) 2005 by Joachim Eibl                                    *
 *   joachim.eibl at gmx.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.           *
 ***************************************************************************/

#ifndef SMALLDIALOGS_H
#define SMALLDIALOGS_H

#include <QDialog>

class Options;
class QComboBox;
class QCheckBox;
class QLineEdit;

class OpenDialog : public QDialog
{
   Q_OBJECT
public:
   OpenDialog(
      QWidget* pParent, const QString& n1, const QString& n2, const QString& n3,
      bool bMerge, const QString& outputName, const char* slotConfigure, Options* pOptions  );

   QComboBox* m_pLineA;
   QComboBox* m_pLineB;
   QComboBox* m_pLineC;
   QComboBox* m_pLineOut;

   QCheckBox* m_pMerge;
   virtual void accept();
   virtual bool eventFilter(QObject* o, QEvent* e);
private:
   Options* m_pOptions;
   void selectURL( QComboBox* pLine, bool bDir, int i, bool bSave );
   bool m_bInputFileNameChanged;
private slots:
   void selectFileA();
   void selectFileB();
   void selectFileC();
   void selectDirA();
   void selectDirB();
   void selectDirC();
   void selectOutputName();
   void selectOutputDir();
   void internalSlot(int);
   void inputFilenameChanged();
   void slotSwapCopyNames(QAction*);
signals:
   void internalSignal(bool);
};

class FindDialog : public QDialog
{
   Q_OBJECT
public:
   FindDialog(QWidget* pParent);
   void setVisible(bool); //override QDialog::setVisible()

signals:
   void findNext();

public:
   QLineEdit* m_pSearchString;
   QCheckBox* m_pSearchInA;
   QCheckBox* m_pSearchInB;
   QCheckBox* m_pSearchInC;
   QCheckBox* m_pSearchInOutput;
   QCheckBox* m_pCaseSensitive;

   int currentLine;
   int currentPos;
   int currentWindow;
};


class RegExpTester : public QDialog
{
   Q_OBJECT
private:
   QLineEdit* m_pAutoMergeRegExpEdit;
   QLineEdit* m_pAutoMergeMatchResult;
   QLineEdit* m_pAutoMergeExampleEdit;
   QLineEdit* m_pHistoryStartRegExpEdit;
   QLineEdit* m_pHistoryStartMatchResult;
   QLineEdit* m_pHistoryStartExampleEdit;
   QLineEdit* m_pHistoryEntryStartRegExpEdit;
   QLineEdit* m_pHistorySortKeyOrderEdit;
   QLineEdit* m_pHistoryEntryStartExampleEdit;
   QLineEdit* m_pHistoryEntryStartMatchResult;
   QLineEdit* m_pHistorySortKeyResult;
public:
   RegExpTester( QWidget* pParent, const QString& autoMergeRegExpToolTip, const QString& historyStartRegExpToolTip, 
                                   const QString& historyEntryStartRegExpToolTip, const QString& historySortKeyOrderToolTip  );
   void init( const QString& autoMergeRegExp, const QString& historyStartRegExp, const QString& historyEntryStartRegExp, const QString sortKeyOrder );
   QString autoMergeRegExp();
   QString historyStartRegExp();
   QString historyEntryStartRegExp();
   QString historySortKeyOrder();
public slots:
   void slotRecalc();
};

#endif
