/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef OVERVIEW_H
#define OVERVIEW_H

#include "LineRef.h"       // for LineRef
#include "TypeUtils.h"     // for QtNumberType

#include <QString>         // for QString
#include <QPixmap>
#include <QWidget>

class Diff3LineList;
class Options;

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

    explicit Overview(const QSharedPointer<Options> &pOptions);

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
    QSharedPointer<Options> m_pOptions;
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
