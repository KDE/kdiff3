/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OPTIONS_H
#define OPTIONS_H

#include <KSharedConfig>

#include <list>

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QSize>
#include <QStringList>

class OptionItemBase;

enum e_LineEndStyle
{
   eLineEndStyleUnix=0,
   eLineEndStyleDos,
   eLineEndStyleAutoDetect,
   eLineEndStyleUndefined, // only one line exists
   eLineEndStyleConflict   // User must resolve manually
};

class Options
{
public:
    void init();

    void apply();

    void resetToDefaults();

    void setToCurrent();

    void saveOptions(const KSharedConfigPtr config);
    void readOptions(const KSharedConfigPtr config);

    const QString parseOptions(const QStringList& optionList);
    QString calcOptionHelp();

    void addOptionItem(OptionItemBase* inItem);

    const QSize& getGeometry() const { return m_geometry; }
    void setGeometry(const QSize& size) { m_geometry = size; }

    const QPoint& getPosition() const { return m_position; }
    void setPosition(const QPoint& pos) { m_position = pos; }

    bool isMaximised() const { return m_bMaximised; };

    void setMaximised(const bool maximised) { m_bMaximised = maximised;};

    Qt::ToolBarArea getToolbarPos() const { return m_toolBarPos; }

    bool isToolBarVisable() const { return m_bShowToolBar; }
    void setToolbarState(bool inShown) { m_bShowToolBar = inShown; }

    bool isStatusBarVisable() const { return m_bShowStatusBar; }
    void setStatusBarState(bool inShown) { m_bShowStatusBar = inShown; }
  private:
    std::list<OptionItemBase*> mOptionItemList;

    // Some settings that are not available in the option dialog:
    QSize  m_geometry = QSize(600, 400);
    QPoint m_position = QPoint(0, 22);
    bool   m_bMaximised = false;
    bool   m_bShowToolBar = true;
    bool   m_bShowStatusBar = true;
    Qt::ToolBarArea    m_toolBarPos = Qt::TopToolBarArea;
  public:

    // These are the results of the option dialog.
    QFont m_font;
    //bool m_bItalicForDeltas;
    QFont m_appFont;

    QColor m_fgColor = Qt::black;
    QColor m_bgColor = Qt::white;
    QColor m_diffBgColor;
    QColor m_colorA;
    QColor m_colorB;
    QColor m_colorC;
    QColor m_colorForConflict = Qt::red;
    QColor m_currentRangeBgColor;
    QColor m_currentRangeDiffBgColor;
    QColor m_oldestFileColor = qRgb(0xf0, 0, 0);
    QColor m_midAgeFileColor = qRgb(0xc0, 0xc0, 0);
    QColor m_newestFileColor = qRgb(0, 0xd0, 0);
    QColor m_missingFileColor = qRgb(0, 0, 0);
    QColor m_manualHelpRangeColor = qRgb(0xff, 0xd0, 0x80);

    bool m_bWordWrap = false;

    bool m_bReplaceTabs = false;
    bool m_bAutoIndentation = true;
    int  m_tabSize = 8;
    bool m_bAutoCopySelection = false;
    bool m_bSameEncoding = true;
    QTextCodec*  m_pEncodingA = nullptr;
    bool m_bAutoDetectUnicodeA = true;
    QTextCodec*  m_pEncodingB = nullptr;
    bool m_bAutoDetectUnicodeB = true;
    QTextCodec*  m_pEncodingC = nullptr;
    bool m_bAutoDetectUnicodeC = true;
    QTextCodec*  m_pEncodingOut = nullptr;
    bool m_bAutoSelectOutEncoding = true;
;
    QTextCodec*  m_pEncodingPP = nullptr;
    e_LineEndStyle  m_lineEndStyle = eLineEndStyleAutoDetect;

    bool m_bPreserveCarriageReturn = false;
    bool m_bTryHard = true;
    bool m_bShowWhiteSpaceCharacters = true;
    bool m_bShowWhiteSpace = true;
    bool m_bShowLineNumbers = false;
    bool m_bHorizDiffWindowSplitting = true;
    bool m_bShowInfoDialogs = true;
    bool m_bDiff3AlignBC = false;

    int  m_whiteSpace2FileMergeDefault = 0;
    int  m_whiteSpace3FileMergeDefault = 0;
    bool m_bIgnoreCase = false;
    bool m_bIgnoreNumbers = false;
    bool m_bIgnoreComments = false;
    QString m_PreProcessorCmd;
    QString m_LineMatchingPreProcessorCmd;
    bool m_bRunRegExpAutoMergeOnMergeStart = false;
    QString m_autoMergeRegExp = ".*\\$(Version|Header|Date|Author).*\\$.*";
    bool m_bRunHistoryAutoMergeOnMergeStart = false;
    QString m_historyStartRegExp = ".*\\$Log.*\\$.*";
    QString m_historyEntryStartRegExp;
    bool m_bHistoryMergeSorting = false;
    QString m_historyEntryStartSortKeyOrder = "4,3,2,5,1,6";
    int m_maxNofHistoryEntries = -1;
    QString m_IrrelevantMergeCmd;
    bool m_bAutoSaveAndQuitOnMergeWithoutConflicts = false;

    bool m_bAutoAdvance = false;
    int  m_autoAdvanceDelay = 500;

    QStringList m_recentAFiles;
    QStringList m_recentBFiles;
    QStringList m_recentCFiles;

    QStringList m_recentEncodings;

    QStringList m_recentOutputFiles;

    // Directory Merge options
    bool m_bDmSyncMode = false;
    bool m_bDmRecursiveDirs = true;
    bool m_bDmFollowFileLinks = false;
    bool m_bDmFollowDirLinks = false;
    bool m_bDmFindHidden = true;
    bool m_bDmCreateBakFiles;
    bool m_bDmBinaryComparison = true;
    bool m_bDmFullAnalysis = false;
    bool m_bDmTrustDate = false;
    bool m_bDmTrustDateFallbackToBinary = false;
    bool m_bDmTrustSize = false;
    bool m_bDmCopyNewer = false;
    //bool m_bDmShowOnlyDeltas;
    bool m_bDmShowIdenticalFiles = true;
    bool m_bDmUseCvsIgnore = false;
    bool m_bDmWhiteSpaceEqual = true;
    bool m_bDmCaseSensitiveFilenameComparison;
    bool m_bDmUnfoldSubdirs = false;
    bool m_bDmSkipDirStatus = false;
    QString m_DmFilePattern = "*";
    QString m_DmFileAntiPattern = "*.orig;*.o;*.obj;*.rej;*.bak";
    QString m_DmDirAntiPattern = "CVS;.deps;.svn;.hg;.git";

    bool m_bRightToLeftLanguage = false;

    QString m_ignorableCmdLineOptions = QString("-u;-query;-html;-abort");
    bool m_bEscapeKeyQuits = false;
};


#endif
