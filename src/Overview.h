/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef OVERVIEW_H
#define OVERVIEW_H

#include "diff.h"
#include "options.h"

#include <QPixmap>
#include <QWidget>

class Overview : public QWidget
{
    Q_OBJECT
  public:
    enum e_OverviewMode
    {
        eOMNormal,
        eOMAvsB,
        eOMAvsC,
        eOMBvsC
    };

    explicit Overview(Options* pOptions);

    void init(Diff3LineList* pDiff3LineList, bool bTripleDiff);
    void reset();
    void setRange(QtNumberType firstLine, QtNumberType pageHeight);
    void setPaintingAllowed(bool bAllowPainting);

    void setOverviewMode(e_OverviewMode eOverviewMode);
    e_OverviewMode getOverviewMode();

  public Q_SLOTS:
    void setFirstLine(QtNumberType firstLine);
    void slotRedraw();
  Q_SIGNALS:
    void setLine(LineRef);

  private:
    const Diff3LineList* m_pDiff3LineList;
    Options* m_pOptions;
    bool m_bTripleDiff;
    LineRef m_firstLine;
    int m_pageHeight;
    QPixmap m_pixmap;
    e_OverviewMode m_eOverviewMode;
    int m_nofLines;

    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void drawColumn(QPainter& p, e_OverviewMode eOverviewMode, int x, int w, int h, int nofLines);
};

#endif // !OVERVIEW_H
