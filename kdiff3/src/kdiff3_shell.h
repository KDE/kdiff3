/*
 * Copyright (C) 2003 Joachim Eibl <joachim.eibl@gmx.de>
 */

#ifndef _KDIFF3SHELL_H_
#define _KDIFF3SHELL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kapplication.h>
#include <kparts/mainwindow.h>

class KToggleAction;

/**
 * This is the application "Shell".  It has a menubar, toolbar, and
 * statusbar but relies on the "Part" to do all the real work.
 *
 * @short Application Shell
 * @author Joachim Eibl <joachim.eibl@gmx.de>
 */
class KDiff3Shell : public KParts::MainWindow
{
    Q_OBJECT
public:
    /**
     * Default Constructor
     */
    KDiff3Shell();

    /**
     * Default Destructor
     */
    virtual ~KDiff3Shell();

    bool queryClose();
    bool queryExit();

private slots:
    void optionsShowToolbar();
    void optionsShowStatusbar();
    void optionsConfigureKeys();
    void optionsConfigureToolbars();

    void applyNewToolbarConfig();

private:
    KParts::ReadWritePart *m_part;

    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
    bool m_bUnderConstruction;
};

#endif // _KDIFF3_H_
