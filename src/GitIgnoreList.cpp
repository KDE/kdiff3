/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 David Hallas <david@davidhallas.dk>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "GitIgnoreList.h"

#include "Logging.h"
#include "fileaccess.h"

#include <algorithm>

namespace {

bool isComment(const QString& line)
{
    return line.startsWith(QChar('#'));
}

} // namespace

GitIgnoreList::GitIgnoreList() = default;

GitIgnoreList::~GitIgnoreList() = default;

void GitIgnoreList::enterDir(const QString& dir, const DirectoryList& directoryList)
{
    const auto directoryListIt = std::find_if(directoryList.begin(), directoryList.end(), [](const FileAccess& file) {
        return file.fileName() == ".gitignore";
    });
    if(directoryListIt != directoryList.end())
    {
        addEntries(dir, readFile(directoryListIt->absoluteFilePath()));
    }
}

bool GitIgnoreList::matches(const QString& dir, const QString& text, bool bCaseSensitive) const
{
    for(auto& dirPattern : m_patterns)
    {
        if(dir.startsWith(dirPattern.first) == false)
        {
            continue;
        }
        for(auto& pattern : dirPattern.second)
        {
            if(bCaseSensitive == false)
            {
                pattern.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
            }
            const auto match = pattern.match(text);
            if(match.hasMatch())
            {
                qCDebug(kdiffGitIgnoreList) << "Matched entry" << text;
                return true;
            }
        }
    }
    return false;
}

QString GitIgnoreList::readFile(const QString& fileName) const
{
    QFile file(fileName);
    if(file.open(QIODevice::ReadOnly) == false)
    {
        return QString();
    }
    QTextStream stream(&file);
    return stream.readAll();
}

void GitIgnoreList::addEntries(const QString& dir, const QString& lines)
{
    const QStringList lineList = lines.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    for(const auto& line : lineList)
    {
        if(isComment(line))
        {
            continue;
        }
        QRegularExpression expression(QRegularExpression::wildcardToRegularExpression(line));
        if(expression.isValid() == false)
        {
            qCDebug(kdiffGitIgnoreList) << "Expression" << line << "is not valid - skipping ...";
            continue;
        }
        qCDebug(kdiffGitIgnoreList) << "Adding entry [" << dir << "]" << line;
        m_patterns[dir].push_back(expression);
    }
}
