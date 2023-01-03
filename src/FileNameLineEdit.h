/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef FILENAMELINEEDIT_H

#include <QLineEdit>

class FileNameLineEdit : public QLineEdit
{
  public:
    using QLineEdit::QLineEdit;
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
};

#endif // !FILENAMELINEEDIT_H
