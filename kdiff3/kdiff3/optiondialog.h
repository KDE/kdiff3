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

/***************************************************************************
 * $Log$
 * Revision 1.1  2002/08/18 16:24:17  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

#ifndef OPTION_DIALOG_H
#define OPTION_DIALOG_H

class QCheckBox;
class QLabel;
class QLineEdit;
class KColorButton;
class KFontChooser;
class KConfig;

#include <kdialogbase.h>
#include <qstrlist.h>


class OptionDialog : public KDialogBase
{
  Q_OBJECT

public:

    OptionDialog( QWidget *parent = 0, char *name = 0 );
    ~OptionDialog( void );

    // These are the results of the option dialog.
    QFont m_font;

    QColor m_fgColor;
    QColor m_bgColor;
    QColor m_diffBgColor;
    QColor m_colorA;
    QColor m_colorB;
    QColor m_colorC;
    QColor m_colorForConflict;

    bool m_bReplaceTabs;
    bool m_bAutoIndentation;
    int  m_tabSize;
    bool m_bAutoCopySelection;

    bool m_bIgnoreWhiteSpace;
    bool m_bIgnoreTrivialMatches;

    QStrList m_recentAFiles;
    QStrList m_recentBFiles;
    QStrList m_recentCFiles;
    QStrList m_recentOutputFiles;

    void saveOptions(KConfig* config);
    void readOptions(KConfig* config);

    void setState(); // Must be called before calling exec();

protected slots:
    virtual void slotDefault( void );
    virtual void slotOk( void );
    virtual void slotApply( void );
    virtual void slotHelp( void );

private:
    // FontConfigDlg
    KFontChooser *m_fontChooser;

    // ColorConfigDlg
    KColorButton* m_pFgColor;
    KColorButton* m_pBgColor;
    KColorButton* m_pDiffBgColor;
    KColorButton* m_pColorA;
    KColorButton* m_pColorB;
    KColorButton* m_pColorC;
    KColorButton* m_pColorForConflict;


    // EditConfigDlg
    QCheckBox* m_pReplaceTabs;
    QLineEdit* m_pTabSize;
    QCheckBox* m_pAutoIndentation;
    QCheckBox* m_pAutoCopySelection;

    // DiffConfigDlg
    QCheckBox* m_pIgnoreWhiteSpace;
    QCheckBox* m_pIgnoreTrivialMatches;

private:
    void setupFontPage( void );
    void setupColorPage( void );
    void setupEditPage( void );
    void setupDiffPage( void );
};


#endif







