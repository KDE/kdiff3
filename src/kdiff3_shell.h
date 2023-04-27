// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef KDIFF3SHELL_H
#define KDIFF3SHELL_H

#include <memory>

#include <QCommandLineParser>
#include <QPointer>
#include <QString>

#include <KParts/MainWindow>

class KToggleAction;

// clang-format off
namespace KParts {
    class ReadWritePart;
}
// clang-format on

class KDiff3App;

/**
 * This is the application "Shell".  It has a menubar, toolbar, and
 * statusbar but relies on the "Part" to do all the real work.
 *
 * @short Application Shell
 * @author Joachim Eibl <joachim.eibl at gmx.de>
 */
class KDiff3Shell: public KParts::MainWindow
{
    Q_OBJECT
  public:
    /**
     * Default Constructor
     */
    explicit KDiff3Shell(const QString& fn1 = "", const QString& fn2 = "", const QString& fn3 = "");

    /**
     * Default Destructor
     */
    ~KDiff3Shell() override;

    bool queryClose() override;
    bool queryExit();
    void closeEvent(QCloseEvent* e) override;

    static inline std::unique_ptr<QCommandLineParser>& getParser()
    {
        static std::unique_ptr<QCommandLineParser> parser = std::make_unique<QCommandLineParser>();
        return parser;
    };
  private Q_SLOTS:
    void optionsShowToolbar();
    void optionsShowStatusbar();
    void optionsConfigureKeys();
    void optionsConfigureToolbars();

    void applyNewToolbarConfig();
    void slotNewInstance(const QString& fn1, const QString& fn2, const QString& fn3);

  private:
    KParts::ReadWritePart* m_part;
    QPointer<KDiff3App> m_widget;

    KToggleAction* m_toolbarAction;
    KToggleAction* m_statusbarAction;
    bool m_bUnderConstruction;
};

#endif // _KDIFF3_H_
