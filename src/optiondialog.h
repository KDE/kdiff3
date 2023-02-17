// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include "options.h"

#include <QFont>
#include <QSharedPointer>
#include <QStringList>
#include <QGroupBox>

#include <KLocalizedString>

#include <KPageDialog>
#include <KSharedConfig>

class QLabel;
class QPlainTextEdit;

class OptionCheckBox;
class OptionEncodingComboBox;
class OptionLineEdit;

class OptionDialog : public KPageDialog
{
   Q_OBJECT

public:

    explicit OptionDialog( bool bShowDirMergeSettings, QWidget *parent = nullptr );
    ~OptionDialog() override;
    const QString parseOptions( const QStringList& optionList );
    QString calcOptionHelp();

    void saveOptions(KSharedConfigPtr config);
    void readOptions(KSharedConfigPtr config);

    void setState(); // Must be called before calling exec();

    QSharedPointer<Options> getOptions() { return m_options; }

protected Q_SLOTS:
    virtual void slotDefault();
    virtual void slotOk();
    virtual void slotApply();
    //virtual void buttonClicked( QAbstractButton* );
    virtual void helpRequested();

    void slotEncodingChanged();
    void slotHistoryMergeRegExpTester();
Q_SIGNALS:
    void applyDone();
private:
    void setupFontPage();
    void setupColorPage();
    void setupEditPage();
    void setupDiffPage();
    void setupMergePage();
    void setupDirectoryMergePage();
    void setupRegionalPage();
    void setupIntegrationPage();
    void resetToDefaults();

    QSharedPointer<Options> m_options=QSharedPointer<Options>::create();
    //QDialogButtonBox *mButtonBox;
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

    const QString s_historyEntryStartRegExpToolTip = i18n("A version control history entry consists of several lines.\n"
                                                          "Specify the regular expression to detect the first line (without the leading comment).\n"
                                                          "Use parentheses to group the keys you want to use for sorting.\n"
                                                          "If left empty, then KDiff3 assumes that empty lines separate history entries.\n"
                                                          "See the documentation for details.");
    const QString s_historyEntryStartSortKeyOrderToolTip = i18n("Each pair of parentheses used in the regular expression for the history start entry\n"
                                                                "groups a key that can be used for sorting.\n"
                                                                "Specify the list of keys (that are numbered in order of occurrence\n"
                                                                "starting with 1) using ',' as separator (e.g. \"4,5,6,1,2,3,7\").\n"
                                                                "If left empty, then no sorting will be done.\n"
                                                                "See the documentation for details.");
    const QString s_autoMergeRegExpToolTip = i18n("Regular expression for lines where KDiff3 should automatically choose one source.\n"
                                                  "When a line with a conflict matches the regular expression then\n"
                                                  "- if available - C, otherwise B will be chosen.");
    const QString s_historyStartRegExpToolTip = i18n("Regular expression for the start of the version control history entry.\n"
                                                     "Usually this line contains the \"$Log$\" keyword.\n"
                                                     "Default value: \".*\\$Log.*\\$.*\"");
};


class FontChooser : public QGroupBox
{
   Q_OBJECT
   QFont m_font;
   QPushButton* m_pSelectFont;
   QPlainTextEdit* m_pExampleTextEdit;
   QLabel* m_pLabel;
public:
   explicit FontChooser( QWidget* pParent );
   QFont font();
   void setFont( const QFont&, bool );
private Q_SLOTS:
   void slotSelectFont();
};

#endif







