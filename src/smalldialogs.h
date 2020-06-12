/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SMALLDIALOGS_H
#define SMALLDIALOGS_H

#include <QDialog>
#include <QPointer>

class Options;
class QComboBox;
class QCheckBox;
class QLineEdit;
class KDiff3App;

class OpenDialog: public QDialog
{
    Q_OBJECT
  public:
    OpenDialog( // krazy:exclude=explicit
        KDiff3App* pParent, const QString& n1, const QString& n2, const QString& n3,
        bool bMerge, const QString& outputName, const QSharedPointer<Options>& pOptions);

    QPointer<QComboBox> m_pLineA;
    QPointer<QComboBox> m_pLineB;
    QPointer<QComboBox> m_pLineC;
    QPointer<QComboBox> m_pLineOut;

    QPointer<QCheckBox> m_pMerge;
    void accept() override;
    bool eventFilter(QObject* o, QEvent* e) override;

  private:
    void selectURL(QComboBox* pLine, bool bDir, int i, bool bSave);

    void fixCurrentText(QComboBox* pCB);
    QSharedPointer<Options> m_pOptions;
    bool m_bInputFileNameChanged;
  private Q_SLOTS:
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
    void slotSwapCopyNames(QAction*) const;
  Q_SIGNALS:
    void internalSignal(bool);
};

class FindDialog: public QDialog
{
    Q_OBJECT
  public:
    explicit FindDialog(QWidget* pParent);
    void setVisible(bool) override;

  Q_SIGNALS:
    void findNext();

  public:
    QLineEdit* m_pSearchString;
    QCheckBox* m_pSearchInA;
    QCheckBox* m_pSearchInB;
    QCheckBox* m_pSearchInC;
    QCheckBox* m_pSearchInOutput;
    QCheckBox* m_pCaseSensitive;

    int currentLine = 0;
    int currentPos = 0;
    int currentWindow = 0;
};

class RegExpTester: public QDialog
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
    RegExpTester(QWidget* pParent, const QString& autoMergeRegExpToolTip, const QString& historyStartRegExpToolTip,
                                   const QString& historyEntryStartRegExpToolTip, const QString& historySortKeyOrderToolTip);
    void init(const QString& autoMergeRegExp, const QString& historyStartRegExp, const QString& historyEntryStartRegExp, const QString& sortKeyOrder);
    QString autoMergeRegExp();
    QString historyStartRegExp();
    QString historyEntryStartRegExp();
    QString historySortKeyOrder();
  public Q_SLOTS:
    void slotRecalc();
};

#endif
