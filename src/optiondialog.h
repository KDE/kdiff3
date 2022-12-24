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
#include "ui_FontChooser.h"

#include <QFont>
#include <QGroupBox>
#include <QSharedPointer>
#include <QStringList>

#include <KPageDialog>
#include <KSharedConfig>

class OptionCheckBox;
class OptionEncodingComboBox;
class OptionLineEdit;

class OptionDialog: public KPageDialog
{
    Q_OBJECT

  public:
    explicit OptionDialog(bool bShowDirMergeSettings, QWidget* parent = nullptr);
    ~OptionDialog() override;
    const QString parseOptions(const QStringList& optionList);
    QString calcOptionHelp();

    void saveOptions(KSharedConfigPtr config);
    void readOptions(KSharedConfigPtr config);

    void setState(); // Must be called before calling exec();

    QSharedPointer<Options> getOptions() { return m_options; }

    static const QString s_historyEntryStartRegExpToolTip;
    static const QString s_historyEntryStartSortKeyOrderToolTip;
    static const QString s_autoMergeRegExpToolTip;
    static const QString s_historyStartRegExpToolTip;

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

    QSharedPointer<Options> m_options = QSharedPointer<Options>::create();
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
};

class FontChooser: public QGroupBox
{
    Q_OBJECT
    QFont m_font;
    Ui::FontGroupBox fontChooserUi;

  public:
    explicit FontChooser(QWidget* pParent);
    QFont font();
    void setFont(const QFont&, bool);

  private Q_SLOTS:
    void slotSelectFont();
};

#endif
