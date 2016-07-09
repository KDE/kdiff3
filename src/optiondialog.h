/*
 *   kdiff3 - Text Diff And Merge Tool
 *   Copyright (C) 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef OPTION_DIALOG_H
#define OPTION_DIALOG_H


#include <kpagedialog.h>
#include <QStringList>
#include <list>
#include <kcmdlineargs.h>

#include "options.h"

class OptionItem;
class OptionCheckBox;
class OptionEncodingComboBox;
class OptionLineEdit;
class KKeyDialog;


class OptionDialog : public KPageDialog
{
   Q_OBJECT

public:

    OptionDialog( bool bShowDirMergeSettings, QWidget *parent = 0, char *name = 0 );
    ~OptionDialog( void );
    QString parseOptions( const QStringList& optionList );
    QString calcOptionHelp();

    Options m_options;

    void saveOptions(KSharedConfigPtr config);
    void readOptions(KSharedConfigPtr config);

    void setState(); // Must be called before calling exec();

    void addOptionItem(OptionItem*);
    KKeyDialog* m_pKeyDialog;
protected slots:
    virtual void slotDefault( void );
    virtual void slotOk( void );
    virtual void slotApply( void );

    void slotEncodingChanged();
    void slotHistoryMergeRegExpTester();
    void slotIntegrateWithClearCase();
    void slotRemoveClearCaseIntegration();
signals:
    void applyDone();
private:
    void resetToDefaults();

    std::list<OptionItem*> m_optionItemList;

    OptionCheckBox* m_pSameEncoding;
    OptionEncodingComboBox* m_pEncodingAComboBox;
    OptionCheckBox* m_pAutoDetectUnicodeA;
    OptionEncodingComboBox* m_pEncodingBComboBox;
    OptionCheckBox* m_pAutoDetectUnicodeB;
    OptionEncodingComboBox* m_pEncodingCComboBox;
    OptionCheckBox* m_pAutoDetectUnicodeC;
    OptionEncodingComboBox* m_pEncodingOutComboBox;
    OptionCheckBox* m_pAutoSelectOutEncoding;
    OptionEncodingComboBox* m_pEncodingPPComboBox;
    OptionCheckBox* m_pHistoryAutoMerge;
    OptionLineEdit* m_pAutoMergeRegExpLineEdit;
    OptionLineEdit* m_pHistoryStartRegExpLineEdit;
    OptionLineEdit* m_pHistoryEntryStartRegExpLineEdit;
    OptionCheckBox* m_pHistoryMergeSorting;
    OptionLineEdit* m_pHistorySortKeyOrderLineEdit;

private:
    void setupFontPage();
    void setupColorPage();
    void setupEditPage();
    void setupDiffPage();
    void setupMergePage();
    void setupDirectoryMergePage();
    void setupKeysPage();
    void setupRegionalPage();
    void setupIntegrationPage();
    void setupOtherOptions();
};




#endif







