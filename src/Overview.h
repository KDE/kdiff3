/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef OVERVIEW_H
#define OVERVIEW_H

#include "LineRef.h"       // for LineRef
#include "TypeUtils.h"     // for QtNumberType

#include <QSharedPointer>
#include <QString>         // for QString
#include <QPixmap>
#include <QWidget>

class Diff3LineList;
class Options;

enum class e_OverviewMode
{
    eOMNormal,
    eOMAvsB,
    eOMAvsC,
    eOMBvsC
};

class Overview : public QWidget
{
    Q_OBJECT
  public:
    explicit Overview(const QSharedPointer<Options> &pOptions);

    void init(Diff3LineList* pDiff3LineList);
    void reset();
    void setRange(QtNumberType firstLine, QtNumberType pageHeight);
    void setPaintingAllowed(bool bAllowPainting);

    e_OverviewMode getOverviewMode();

  public Q_SLOTS:
    void setOverviewMode(e_OverviewMode eOverviewMode);
    void setFirstLine(QtNumberType firstLine);
    void slotRedraw();
  Q_SIGNALS:
    void setLine(LineRef);

  private:
    const Diff3LineList* m_pDiff3LineList;
    QSharedPointer<Options> m_pOptions;
    LineRef m_firstLine;
    int m_pageHeight;
    QPixmap m_pixmap;
    e_OverviewMode mOverviewMode;
    int m_nofLines;

    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void drawColumn(QPainter& p, e_OverviewMode eOverviewMode, int x, int w, int h, int nofLines);
};

#endif // !OVERVIEW_H
