// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on
#ifndef OVERVIEW_H
#define OVERVIEW_H

#include "LineRef.h"       // for LineRef
#include "TypeUtils.h"     // for qint32

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
    void setRange(qint32 firstLine, qint32 pageHeight);
    void setPaintingAllowed(bool bAllowPainting);

    e_OverviewMode getOverviewMode();

  public Q_SLOTS:
    void setOverviewMode(e_OverviewMode eOverviewMode);
    void setFirstLine(qint32 firstLine);
    void slotRedraw();
  Q_SIGNALS:
    void setLine(LineRef);

  private:
    const Diff3LineList* m_pDiff3LineList;
    QSharedPointer<Options> m_pOptions;
    LineRef m_firstLine;
    qint32 m_pageHeight;
    QPixmap m_pixmap;
    e_OverviewMode mOverviewMode;
    qint32 m_nofLines;

    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void drawColumn(QPainter& p, e_OverviewMode eOverviewMode, qint32 x, qint32 w, qint32 h, qint32 nofLines);
};

#endif // !OVERVIEW_H
