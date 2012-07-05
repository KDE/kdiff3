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

#ifndef OPTIONS_H
#define OPTIONS_H


#include <QStringList>
#include <list>

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
    // Some settings are not available in the option dialog:
    QSize  m_geometry;
    QPoint m_position;
    bool   m_bMaximised;
    bool   m_bShowToolBar;
    bool   m_bShowStatusBar;
    int    m_toolBarPos;

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
    QColor m_oldestFileColor;
    QColor m_midAgeFileColor;
    QColor m_newestFileColor;
    QColor m_missingFileColor;
    QColor m_manualHelpRangeColor;

    bool m_bWordWrap;

    bool m_bReplaceTabs;
    bool m_bAutoIndentation;
    int  m_tabSize;
    bool m_bAutoCopySelection;
    bool m_bSameEncoding;
    QTextCodec*  m_pEncodingA;
    bool m_bAutoDetectUnicodeA;
    QTextCodec*  m_pEncodingB;
    bool m_bAutoDetectUnicodeB;
    QTextCodec*  m_pEncodingC;
    bool m_bAutoDetectUnicodeC;
    QTextCodec*  m_pEncodingOut;
    bool m_bAutoSelectOutEncoding;
    QTextCodec*  m_pEncodingPP;
    int  m_lineEndStyle;

    bool m_bPreserveCarriageReturn;
    bool m_bTryHard;
    bool m_bShowWhiteSpaceCharacters;
    bool m_bShowWhiteSpace;
    bool m_bShowLineNumbers;
    bool m_bHorizDiffWindowSplitting;
    bool m_bShowInfoDialogs;
    bool m_bDiff3AlignBC;

    int  m_whiteSpace2FileMergeDefault;
    int  m_whiteSpace3FileMergeDefault;
    bool m_bIgnoreCase;
    bool m_bIgnoreNumbers;
    bool m_bIgnoreComments;
    QString m_PreProcessorCmd;
    QString m_LineMatchingPreProcessorCmd;
    bool m_bRunRegExpAutoMergeOnMergeStart;
    QString m_autoMergeRegExp;
    bool m_bRunHistoryAutoMergeOnMergeStart;
    QString m_historyStartRegExp;
    QString m_historyEntryStartRegExp;
    bool m_bHistoryMergeSorting;
    QString m_historyEntryStartSortKeyOrder;
    int m_maxNofHistoryEntries;
    QString m_IrrelevantMergeCmd;
    bool m_bAutoSaveAndQuitOnMergeWithoutConflicts;

    bool m_bAutoAdvance;
    int  m_autoAdvanceDelay;

    QStringList m_recentAFiles;
    QStringList m_recentBFiles;
    QStringList m_recentCFiles;

    QStringList m_recentEncodings;

    QStringList m_recentOutputFiles;

    // Directory Merge options
    bool m_bDmSyncMode;
    bool m_bDmRecursiveDirs;
    bool m_bDmFollowFileLinks;
    bool m_bDmFollowDirLinks;
    bool m_bDmFindHidden;
    bool m_bDmCreateBakFiles;
    bool m_bDmBinaryComparison;
    bool m_bDmFullAnalysis;
    bool m_bDmTrustDate;
    bool m_bDmTrustDateFallbackToBinary;
    bool m_bDmTrustSize;
    bool m_bDmCopyNewer;
    //bool m_bDmShowOnlyDeltas;
    bool m_bDmShowIdenticalFiles;
    bool m_bDmUseCvsIgnore;
    bool m_bDmWhiteSpaceEqual;
    bool m_bDmCaseSensitiveFilenameComparison;
    bool m_bDmUnfoldSubdirs;
    bool m_bDmSkipDirStatus;
    QString m_DmFilePattern;
    QString m_DmFileAntiPattern;
    QString m_DmDirAntiPattern;

    QString m_language;
    bool m_bRightToLeftLanguage;

    QString m_ignorableCmdLineOptions;
    bool m_bIntegrateWithClearCase;
    bool m_bEscapeKeyQuits;
};


#endif
