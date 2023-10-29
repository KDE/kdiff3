// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef FILEACCESS_H
#define FILEACCESS_H

#include "DirectoryList.h"

#include <type_traits>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QTemporaryFile>
#include <QUrl>

#ifndef AUTOTEST
#include <KIO/UDSEntry>
#endif

class FileAccessJobHandler;
class DefaultFileAccessJobHandler;
class IgnoreList;
/*
  Defining a function as virtual in FileAccess is intended to allow testing sub classes to be written
  more easily. This way the test can use a moc class that emulates the needed conditions with no
  actual file being present. This would otherwise be a technical and logistical nightmare.
*/
class FileAccess
{
  public:
    FileAccess();

    FileAccess(const FileAccess&);
    FileAccess(FileAccess&&) noexcept;
    FileAccess& operator=(const FileAccess&);
    FileAccess& operator=(FileAccess&&) noexcept;
    virtual ~FileAccess();
    explicit FileAccess(const QString& name, bool bWantToWrite = false); // name: local file or dirname or url (when supported)

    explicit FileAccess(const QUrl& name, bool bWantToWrite = false); // name: local file or dirname or url (when supported)
    void setFile(const QString& name, bool bWantToWrite = false);
    void setFile(const QUrl& url, bool bWantToWrite = false);
    void setFile(FileAccess* pParent, const QFileInfo& fi);

    virtual void loadData();

    Q_REQUIRED_RESULT bool isNormal() const;
    Q_REQUIRED_RESULT bool isValid() const;
    Q_REQUIRED_RESULT virtual bool isFile() const;
    Q_REQUIRED_RESULT bool isBrokenLink() const { return m_bBrokenLink; }
    Q_REQUIRED_RESULT virtual bool isDir() const;
    Q_REQUIRED_RESULT virtual bool isSymLink() const;
    Q_REQUIRED_RESULT virtual bool exists() const;
    Q_REQUIRED_RESULT virtual qint64 size() const;     // Size as returned by stat().
    Q_REQUIRED_RESULT virtual qint64 sizeForReading(); // If the size can't be determined by stat() then the file is copied to a local temp file.
    Q_REQUIRED_RESULT virtual bool isReadable() const;
    Q_REQUIRED_RESULT virtual bool isWritable() const;
    Q_REQUIRED_RESULT virtual bool isExecutable() const;
    Q_REQUIRED_RESULT virtual bool isHidden() const;
    Q_REQUIRED_RESULT QString readLink() const;

    Q_REQUIRED_RESULT QDateTime lastModified() const;

    Q_REQUIRED_RESULT QString displayName() const { return mDisplayName.isEmpty() ? fileName() : mDisplayName; }
    Q_REQUIRED_RESULT QString fileName(bool needTmp = false) const; // Just the name-part of the path, without parent directories
    Q_REQUIRED_RESULT QString fileRelPath() const;                  // The path relative to base comparison directory
    Q_REQUIRED_RESULT QString prettyAbsPath() const;
    Q_REQUIRED_RESULT QUrl url() const;
    void setUrl(const QUrl& inUrl) { m_url = inUrl; }

    //Workaround for QUrl::toDisplayString/QUrl::toString behavior that does not fit KDiff3's expectations
    Q_REQUIRED_RESULT QString absoluteFilePath() const;
    Q_REQUIRED_RESULT static QString prettyAbsPath(const QUrl& url)
    {
        if(!isLocal(url)) return url.toDisplayString();

        //work around for bad path in windows drop event urls. (Qt 5.15.2 affected)
        QString path = url.toLocalFile();
        if(!path.isEmpty() && !path.startsWith('/'))
          return path;

        return QFileInfo(url.path()).absoluteFilePath();
    }

    //Workaround for QUrl::isLocalFile behavior that does not fit KDiff3's expectations.
    Q_REQUIRED_RESULT bool isLocal() const;
    Q_REQUIRED_RESULT static bool isLocal(const QUrl& url)
    {
        return url.isLocalFile() || !url.isValid() || url.scheme().isEmpty();
    }

    virtual bool readFile(void* pDestBuffer, qint64 maxLength);
    virtual bool writeFile(const void* pSrcBuffer, qint64 length);
    bool listDir(DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, IgnoreList& ignoreList);
    virtual bool copyFile(const QString& destUrl);
    virtual bool createBackup(const QString& bakExtension);

    Q_REQUIRED_RESULT QString getTempName() const;
    virtual bool createLocalCopy();
    static void createTempFile(QTemporaryFile&);
    bool removeFile();
    static bool makeDir(const QString&);
    static bool removeDir(const QString&);
    static bool exists(const QString&);
    static QString cleanPath(const QString&);

    //bool chmod( const QString& );
    bool rename(const FileAccess&);
    static bool symLink(const QString& linkTarget, const QString& linkLocation);

    virtual void addPath(const QString& txt, bool reinit = true);
    Q_REQUIRED_RESULT const QString& getStatusText() const;

    Q_REQUIRED_RESULT FileAccess* parent() const; // !=0 for listDir-results, but only valid if the parent was not yet destroyed.

    void doError();
    void filterList(const QString& dir, DirectoryList* pDirList, const QString& filePattern,
                    const QString& fileAntiPattern, const QString& dirAntiPattern,
                    IgnoreList& ignoreList);

    Q_REQUIRED_RESULT QDir getBaseDirectory() const { return m_baseDir; }

    bool open(const QFile::OpenMode flags);

    qint64 read(char* data, const qint64 maxlen);
    void close();

    [[nodiscard]] const QString& errorString() const;

    //These should be exposed for auto tests
  protected:
#ifndef AUTOTEST
    friend DefaultFileAccessJobHandler;
    void setFromUdsEntry(const KIO::UDSEntry& e, FileAccess* parent);
#endif
    void setStatusText(const QString& s);

    void reset();

    bool interruptableReadFile(void* pDestBuffer, qint64 maxLength);

    QScopedPointer<FileAccessJobHandler> mJobHandler;
    FileAccess* m_pParent = nullptr;
    QUrl m_url;
    bool m_bValidData = false;

    QDir m_baseDir;
    QFileInfo m_fileInfo;
    QString m_linkTarget;
    QString m_name;

    QString mDisplayName;
    QString m_localCopy;
    QString mPhysicalPath;
    QSharedPointer<QTemporaryFile> tmpFile = QSharedPointer<QTemporaryFile>::create();
    QSharedPointer<QFile> realFile = nullptr;

    qint64 m_size = 0;
    QDateTime m_modificationTime = QDateTime::fromMSecsSinceEpoch(0);
    bool m_bBrokenLink = false;
    bool m_bSymLink = false;
    bool m_bFile = false;
    bool m_bDir = false;
    bool m_bExists = false;
    bool m_bWritable = false;
    bool m_bReadable = false;
    bool m_bExecutable = false;
    bool m_bHidden = false;

    QString m_statusText; // Might contain an error string, when the last operation didn't succeed.

  private:
    /*
    These two variables are used to prevent infinate/long running loops when a symlinks true target
    must be found. isNormal is right now the only place this is needed.

    Never expose these outside FileAccess as they are internal values.
    */
    mutable bool mVisited = false;
};
/*
 FileAccess objects should be copy and move assignable.
  Used a few places in KDiff3 itself.
  Also used in std::list<FileAccess>
*/
static_assert(std::is_copy_assignable<FileAccess>::value, "FileAccess must be copy assignable.");
static_assert(std::is_move_assignable<FileAccess>::value, "FileAccess must be move assignable.");

#endif
