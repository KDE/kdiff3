// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef OPTIONS_H
#define OPTIONS_H

#include "combiners.h"
#include "RecentItems.h"
#include "TypeUtils.h"

#include <boost/signals2.hpp>
#include <list>
#include <memory>

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QSize>
#include <QStringList>

#include <KSharedConfig>

class ValueMap;

class OptionItemBase;

constexpr char KDIFF3_CONFIG_GROUP[] = "KDiff3 Options";

enum e_LineEndStyle
{
    eLineEndStyleUnix = 0,
    eLineEndStyleDos,
    eLineEndStyleOldMac,
    eLineEndStyleAutoDetect,
    eLineEndStyleUndefined, // only one line exists
    eLineEndStyleConflict   // User must resolve manually
};

class Options
{
  public:
    inline static boost::signals2::signal<void()> apply;
    inline static boost::signals2::signal<void()> resetToDefaults;
    inline static boost::signals2::signal<void()> setToCurrent;
    inline static boost::signals2::signal<void(ValueMap*)> read;
    inline static boost::signals2::signal<void(ValueMap*)> write;

    inline static boost::signals2::signal<void()> preserve;
    inline static boost::signals2::signal<void()> unpreserve;
    inline static boost::signals2::signal<bool(const QString&, const QString&), find> accept;

    void init();

    void readOptions(const KSharedConfigPtr config);
    void saveOptions(const KSharedConfigPtr config);

    const QString parseOptions(const QStringList& optionList);
    [[nodiscard]] QString calcOptionHelp();

    [[nodiscard]] const QSize& getGeometry() const { return m_geometry; }
    [[nodiscard]] const QPoint& getPosition() const { return m_position; }

    [[nodiscard]] bool isFullScreen() const { return m_bFullScreen; };
    void setFullScreen(const bool fullScreen) { m_bFullScreen = fullScreen; };

    [[nodiscard]] bool isMaximized() const { return m_bMaximized; };
    void setMaximized(const bool maximized) { m_bMaximized = maximized; };

    [[nodiscard]] bool isStatusBarVisible() const { return m_bShowStatusBar; }
    void setStatusBarState(bool inShown) { m_bShowStatusBar = inShown; }

    [[nodiscard]] const QFont& defaultFont() { return mFont; };
    [[nodiscard]] const QFont& appFont() { return mAppFont; };

    [[nodiscard]] bool wordWrapOn() const { return m_bWordWrap; }
    void setWordWrap(const bool enabled) { m_bWordWrap = enabled; }

    [[nodiscard]] bool replaceTabs() const { return m_bReplaceTabs; }
    [[nodiscard]] bool autoIndent() const { return m_bAutoIndentation; }
    [[nodiscard]] qint32 tabSize() const { return m_tabSize; }

    [[nodiscard]] bool ignoreComments() const { return m_bIgnoreComments; }

    [[nodiscard]] bool whiteSpaceIsEqual() const { return m_bDmWhiteSpaceEqual; }

    [[nodiscard]] const QColor& foregroundColor() const { return m_fgColor; };
    [[nodiscard]] const QColor& backgroundColor() const
    {
        if(mPrintMode) return mPrintBackground;
        return m_bgColor;
    };
    [[nodiscard]] const QColor& diffBackgroundColor() const { return m_diffBgColor; };
    [[nodiscard]] const QColor& aColor() const { return m_colorA; };
    [[nodiscard]] const QColor& bColor() const { return m_colorB; };
    [[nodiscard]] const QColor& cColor() const { return m_colorC; };

    [[nodiscard]] const QColor& conflictColor() const { return m_colorForConflict; }

    [[nodiscard]] RecentItems<maxNofRecentFiles>& getRecentFilesA() { return m_recentAFiles; }
    [[nodiscard]] RecentItems<maxNofRecentFiles>& getRecentFilesB() { return m_recentBFiles; }
    [[nodiscard]] RecentItems<maxNofRecentFiles>& getRecentFilesC() { return m_recentCFiles; }

    [[nodiscard]] RecentItems<maxNofRecentFiles>& getRecentOutputFiles() { return m_recentOutputFiles; }

    void beginPrint() { mPrintMode = true; }
    void endPrint() { mPrintMode = false; }

    [[nodiscard]] const QColor& getCurrentRangeBgColor() const { return m_currentRangeBgColor; };
    [[nodiscard]] const QColor& getCurrentRangeDiffBgColor() const { return m_currentRangeDiffBgColor; };
    [[nodiscard]] const QColor& oldestFileColor() const { return m_oldestFileColor; }
    [[nodiscard]] const QColor& midAgeFileColor() const { return m_midAgeFileColor; }
    [[nodiscard]] const QColor& newestFileColor() const { return m_newestFileColor; }
    [[nodiscard]] const QColor& missingFileColor() const { return m_missingFileColor; }
    [[nodiscard]] const QColor& manualHelpRangeColor() const { return m_manualHelpRangeColor; }

  private:
    void addOptionItem(std::shared_ptr<OptionItemBase> inItem);

    friend class OptionDialog;
    std::list<std::shared_ptr<OptionItemBase>> mOptionItemList;

    bool mPrintMode = false;

    const QColor mPrintBackground = Qt::white;

    // clang-format off
    // Some settings that are not available in the option dialog:
    QSize  m_geometry = QSize(600, 400);
    QPoint m_position = QPoint(0, 22);
    bool   m_bFullScreen = false;
    bool   m_bMaximized = false;
    bool   m_bShowStatusBar = true;
    // clang-format on

    // These are the results of the option dialog.
    QFont mFont;
    QFont mAppFont;

    //bool m_bItalicForDeltas;

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
    qint32 m_tabSize = 8;

  public:
    bool m_bAutoCopySelection = false;
    bool m_bSameEncoding = true;
    QByteArray mEncodingA = nullptr;
    bool mAutoDetectA = true;
    QByteArray mEncodingB = nullptr;
    bool mAutoDetectB = true;
    QByteArray mEncodingC = nullptr;
    bool mAutoDetectC = true;
    QByteArray mEncodingOut = nullptr;
    bool m_bAutoSelectOutEncoding = true;
    QByteArray mEncodingPP = nullptr;
    e_LineEndStyle m_lineEndStyle = eLineEndStyleAutoDetect;

    bool m_bTryHard = true;
    bool m_bShowWhiteSpaceCharacters = true;
    bool m_bShowWhiteSpace = true;
    bool m_bShowLineNumbers = false;
    bool m_bHorizDiffWindowSplitting = true;
    bool m_bShowInfoDialogs = true;
    bool m_bDiff3AlignBC = false;

    qint32  m_whiteSpace2FileMergeDefault = 0;
    qint32  m_whiteSpace3FileMergeDefault = 0;
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
    qint32 m_maxNofHistoryEntries = -1;
    QString m_IrrelevantMergeCmd;
    bool m_bAutoSaveAndQuitOnMergeWithoutConflicts = false;

    bool m_bAutoAdvance = false;
    qint32  m_autoAdvanceDelay = 500;

    RecentItems<maxNofRecentFiles> m_recentAFiles;
    RecentItems<maxNofRecentFiles> m_recentBFiles;
    RecentItems<maxNofRecentFiles> m_recentCFiles;

    RecentItems<maxNofRecentCodecs> m_recentEncodings;

    RecentItems<maxNofRecentFiles> m_recentOutputFiles;

    // Directory Merge options
    bool m_bDmSyncMode = false;
    bool m_bDmRecursiveDirs = true;
    bool m_bDmFollowFileLinks = true;
    bool m_bDmFollowDirLinks = true;
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

inline std::unique_ptr<Options> gOptions = std::make_unique<Options>();

#endif
