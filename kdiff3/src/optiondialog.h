/*
 *   kdiff3 - Text Diff And Merge Tool
 *   This file only: Copyright (C) 2002  Joachim Eibl, joachim.eibl@gmx.de
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef OPTION_DIALOG_H
#define OPTION_DIALOG_H

class QCheckBox;
class QLabel;
class QLineEdit;
class KColorButton;
class KFontChooser;
class KConfig;

#include <kdialogbase.h>
#include <qstringlist.h>
#include <list>

class OptionItem;
class KKeyDialog;


class OptionDialog : public KDialogBase
{
   Q_OBJECT

public:

    OptionDialog( bool bShowDirMergeSettings, QWidget *parent = 0, char *name = 0 );
    ~OptionDialog( void );

    // These are the results of the option dialog.
    QFont m_font;
    bool m_bItalicForDeltas;

    QColor m_fgColor;
    QColor m_bgColor;
    QColor m_diffBgColor;
    QColor m_colorA;
    QColor m_colorB;
    QColor m_colorC;
    QColor m_colorForConflict;
    QColor m_currentRangeBgColor;
    QColor m_currentRangeDiffBgColor;

    bool m_bReplaceTabs;
    bool m_bAutoIndentation;
    int  m_tabSize;
    bool m_bAutoCopySelection;
    bool m_bStringEncoding;

    bool m_bPreserveCarriageReturn;
    bool m_bTryHard;
    bool m_bShowWhiteSpaceCharacters;
    bool m_bShowWhiteSpace;
    bool m_bShowLineNumbers;
    bool m_bHorizDiffWindowSplitting;

    int  m_whiteSpace2FileMergeDefault;
    int  m_whiteSpace3FileMergeDefault;
    bool m_bUpCase;
    bool m_bIgnoreNumbers;
    bool m_bIgnoreComments;
    QString m_PreProcessorCmd;
    QString m_LineMatchingPreProcessorCmd;

    bool m_bAutoAdvance;
    int  m_autoAdvanceDelay;

    QStringList m_recentAFiles;
    QStringList m_recentBFiles;
    QStringList m_recentCFiles;

    QStringList m_recentOutputFiles;

    // Directory Merge options
    bool m_bDmSyncMode;
    bool m_bDmRecursiveDirs;
    bool m_bDmFollowFileLinks;
    bool m_bDmFollowDirLinks;
    bool m_bDmFindHidden;
    bool m_bDmCreateBakFiles;
    bool m_bDmTrustDate;
    bool m_bDmTrustSize;
    bool m_bDmCopyNewer;
    bool m_bDmShowOnlyDeltas;
    bool m_bDmUseCvsIgnore;
    QString m_DmFilePattern;
    QString m_DmFileAntiPattern;
    QString m_DmDirAntiPattern;

    void saveOptions(KConfig* config);
    void readOptions(KConfig* config);

    void setState(); // Must be called before calling exec();

    void addOptionItem(OptionItem*);
    KKeyDialog* m_pKeyDialog;
protected slots:
    virtual void slotDefault( void );
    virtual void slotOk( void );
    virtual void slotApply( void );
    virtual void slotHelp( void );

private:
    void resetToDefaults();

    std::list<OptionItem*> m_optionItemList;

    // FontConfigDlg
    KFontChooser *m_fontChooser;

private:
    void setupFontPage();
    void setupColorPage();
    void setupEditPage();
    void setupDiffPage();
    void setupDirectoryMergePage();
    void setupKeysPage();
    void setupOtherOptions();
};



#endif







