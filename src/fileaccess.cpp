// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#include "fileaccess.h"

#include "common.h"

#ifndef AUTOTEST
#include "DefaultFileAccessJobHandler.h"
#endif
#include "FileAccessJobHandler.h"
#include "IgnoreList.h"
#include "Logging.h"
#include "ProgressProxy.h"
#include "Utils.h"

#include <algorithm>                      // for min
#include <cstdlib>
#include <sys/stat.h>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif
#include <utility>                        // for move

#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QtMath>

#include <KLocalizedString>

//This triggers template instantiation even if set to default.
#ifndef AUTOTEST
FileAccess::FileAccess()
{
    mJobHandler.reset(new DefaultFileAccessJobHandler(this));
}
#else
FileAccess::FileAccess() = default;
#endif

FileAccess::~FileAccess() = default;

FileAccess::FileAccess(const FileAccess& b):
    m_pParent{b.m_pParent},
    m_url{b.m_url},
    m_bValidData{b.m_bValidData},
    m_baseDir{b.m_baseDir},
    m_fileInfo{b.m_fileInfo},
    m_linkTarget{b.m_linkTarget},
    m_name{b.m_name},
    mDisplayName{b.mDisplayName},
    m_localCopy{b.m_localCopy},
    mPhysicalPath{b.mPhysicalPath},
    tmpFile{b.tmpFile},
    realFile{b.realFile},
    m_size{b.m_size},
    m_modificationTime{b.m_modificationTime},
    m_bSymLink{b.m_bSymLink},
    m_bFile{b.m_bFile},
    m_bDir{b.m_bDir},
    m_bExists{b.m_bExists},
    m_bWritable{b.m_bWritable},
    m_bReadable{b.m_bReadable},
    m_bExecutable{b.m_bExecutable},
    m_bHidden{b.m_bHidden}
{
    mJobHandler.reset(b.mJobHandler ? b.mJobHandler->copy(this) : nullptr);
}

FileAccess::FileAccess(FileAccess&& b) noexcept:
    m_pParent{b.m_pParent},
    m_bValidData{b.m_bValidData},
    m_baseDir{b.m_baseDir},
    m_fileInfo{b.m_fileInfo},
    m_linkTarget{b.m_linkTarget},
    m_name{b.m_name},
    mDisplayName{b.mDisplayName},
    m_localCopy{b.m_localCopy},
    mPhysicalPath{b.mPhysicalPath},
    tmpFile{b.tmpFile},
    realFile{b.realFile},
    m_size{b.m_size},
    m_modificationTime{b.m_modificationTime},
    m_bSymLink{b.m_bSymLink},
    m_bFile{b.m_bFile},
    m_bDir{b.m_bDir},
    m_bExists{b.m_bExists},
    m_bWritable{b.m_bWritable},
    m_bReadable{b.m_bReadable},
    m_bExecutable{b.m_bExecutable},
    m_bHidden{b.m_bHidden}
{
    mJobHandler.reset(b.mJobHandler.take());
    if(mJobHandler) mJobHandler->setFileAccess(this);

    m_url = std::move(b.m_url);

    b.m_pParent = nullptr;
    b.m_bValidData = false;

    b.m_baseDir = QDir();
    b.m_fileInfo = QFileInfo();
    b.m_linkTarget = QString();
    b.m_name = QString();
    b.mDisplayName = QString();
    b.m_localCopy = QString();
    b.mPhysicalPath = QString();
    b.tmpFile = nullptr;
    b.realFile = nullptr;
    b.m_size = 0;
    b.m_modificationTime = QDateTime::fromMSecsSinceEpoch(0);
    b.m_bSymLink = false;
    b.m_bFile = false;
    b.m_bDir = false;
    b.m_bExists = false;
    b.m_bWritable = false;
    b.m_bReadable = false;
    b.m_bExecutable = false;
    b.m_bHidden = false;
}

FileAccess& FileAccess::operator=(const FileAccess& b)
{
    if(&b == this) return *this;

    //mJobHandler defaults to nullptr

    mJobHandler.reset(b.mJobHandler ? b.mJobHandler->copy(this) : nullptr);

    m_pParent = b.m_pParent;
    m_url = b.m_url;
    m_bValidData = b.m_bValidData;
    m_baseDir = b.m_baseDir;
    m_fileInfo = b.m_fileInfo;
    m_linkTarget = b.m_linkTarget;
    m_name = b.m_name;
    mDisplayName = b.mDisplayName;
    m_localCopy = b.m_localCopy;
    mPhysicalPath = b.mPhysicalPath;
    tmpFile = b.tmpFile;
    realFile = b.realFile;
    m_size = b.m_size;
    m_modificationTime = b.m_modificationTime;
    m_bSymLink = b.m_bSymLink;
    m_bFile = b.m_bFile;
    m_bDir = b.m_bDir;
    m_bExists = b.m_bExists;
    m_bWritable = b.m_bWritable;
    m_bReadable = b.m_bReadable;
    m_bExecutable = b.m_bExecutable;
    m_bHidden = b.m_bHidden;
    return *this;
}

FileAccess& FileAccess::operator=(FileAccess&& b) noexcept
{
    if(&b == this) return *this;

    mJobHandler.reset(b.mJobHandler.take());
    if (mJobHandler) mJobHandler->setFileAccess(this);

    m_pParent = b.m_pParent;
    m_url = b.m_url;
    m_bValidData = b.m_bValidData;
    m_baseDir = b.m_baseDir;
    m_fileInfo = b.m_fileInfo;
    m_linkTarget = b.m_linkTarget;
    m_name = b.m_name;
    mDisplayName = b.mDisplayName;
    m_localCopy = b.m_localCopy;
    mPhysicalPath = b.mPhysicalPath;
    tmpFile = b.tmpFile;
    realFile = b.realFile;
    m_size = b.m_size;
    m_modificationTime = b.m_modificationTime;
    m_bSymLink = b.m_bSymLink;
    m_bFile = b.m_bFile;
    m_bDir = b.m_bDir;
    m_bExists = b.m_bExists;
    m_bWritable = b.m_bWritable;
    m_bReadable = b.m_bReadable;
    m_bExecutable = b.m_bExecutable;
    m_bHidden = b.m_bHidden;

    b.m_pParent = nullptr;
    b.m_url = QUrl();
    b.m_bValidData = false;

    b.m_baseDir = QDir();
    b.m_fileInfo = QFileInfo();
    b.m_linkTarget = QString();
    b.m_name = QString();
    b.mDisplayName = QString();
    b.m_localCopy = QString();
    b.mPhysicalPath = QString();
    b.tmpFile = nullptr;
    b.realFile = nullptr;
    b.m_size = 0;
    b.m_modificationTime = QDateTime::fromMSecsSinceEpoch(0);
    b.m_bSymLink = false;
    b.m_bFile = false;
    b.m_bDir = false;
    b.m_bExists = false;
    b.m_bWritable = false;
    b.m_bReadable = false;
    b.m_bExecutable = false;
    b.m_bHidden = false;
    return *this;
}

FileAccess::FileAccess(const QString& name, bool bWantToWrite)
{
    setFile(name, bWantToWrite);
}

FileAccess::FileAccess(const QUrl& name, bool bWantToWrite)
{
    setFile(name, bWantToWrite);
}

/*
    Performs a re-init. This delibratly does not include mJobHandler.
*/
void FileAccess::reset()
{
    m_url.clear();
    m_name.clear();
    m_fileInfo = QFileInfo();
    m_bExists = false;
    m_bFile = false;
    m_bDir = false;
    m_bSymLink = false;
    m_bWritable = false;
    m_bHidden = false;
    m_size = 0;
    m_modificationTime = QDateTime::fromMSecsSinceEpoch(0);

    mDisplayName.clear();
    mPhysicalPath.clear();
    m_linkTarget.clear();
    //Cleanup temp file if any.
    tmpFile = QSharedPointer<QTemporaryFile>::create();
    realFile.clear();

    m_pParent = nullptr;
    m_bValidData = false;
}

/*
    Needed only during directory listing right now.
*/
void FileAccess::setFile(FileAccess* pParent, const QFileInfo& fi)
{
    assert(pParent != this);
#ifndef AUTOTEST
    if(mJobHandler == nullptr) mJobHandler.reset(new DefaultFileAccessJobHandler(this));
#endif
    reset();

    m_fileInfo = fi;
    m_url = QUrl::fromLocalFile(m_fileInfo.absoluteFilePath());

    m_pParent = pParent;
    loadData();
}

void FileAccess::setFile(const QString& name, bool bWantToWrite)
{
    if(name.isEmpty())
        return;

    QUrl url = QUrl::fromUserInput(name, QString(), QUrl::AssumeLocalFile);
    setFile(url, bWantToWrite);
}

void FileAccess::setFile(const QUrl& url, bool bWantToWrite)
{
    if(url.isEmpty())
        return;

#ifndef AUTOTEST
    if(mJobHandler == nullptr) mJobHandler.reset(new DefaultFileAccessJobHandler(this));
#endif
    reset();
    assert(parent() == nullptr || url != parent()->url());

    m_url = url;

    if(isLocal()) // Invalid urls are treated as local files.
    {
        /*
            Utils::urlToString handles choosing the right API from QUrl.
        */
        m_fileInfo.setFile(Utils::urlToString(url));

        m_pParent = nullptr;

        loadData();
    }
    else
    {
        m_name = m_url.fileName();

        if(mJobHandler->stat(bWantToWrite))
            m_bValidData = true; // After running stat() the variables are initialised
                                 // and valid even if the file doesn't exist and the stat
                                 // query failed.
    }
}

void FileAccess::loadData()
{
    m_fileInfo.setCaching(true);

    if(parent() == nullptr)
        m_baseDir.setPath(m_fileInfo.absoluteFilePath());
    else
        m_baseDir = m_pParent->m_baseDir;

    //convert to absolute path that doesn't depend on the current directory.
    m_fileInfo.makeAbsolute();
    m_bSymLink = m_fileInfo.isSymLink();

    m_bFile = m_fileInfo.isFile();
    m_bDir = m_fileInfo.isDir();
    m_bExists = m_fileInfo.exists();
    m_size = m_fileInfo.size();
    m_modificationTime = m_fileInfo.lastModified();
    m_bHidden = m_fileInfo.isHidden();

    m_bWritable = m_fileInfo.isWritable();
    m_bReadable = m_fileInfo.isReadable();
    m_bExecutable = m_fileInfo.isExecutable();

    m_name = m_fileInfo.fileName();
    if(isLocal() && m_name.isEmpty())
    {
        m_name = m_fileInfo.absoluteDir().dirName();
    }

    if(isLocal() && m_bSymLink)
    {
        m_linkTarget = m_fileInfo.symLinkTarget();

#ifndef Q_OS_WIN
        // Unfortunately Qt5 symLinkTarget/readLink always return an absolute path, even if the link is relative
        char* s = (char*)malloc(PATH_MAX + 1);
        ssize_t len = readlink(QFile::encodeName(absoluteFilePath()).constData(), s, PATH_MAX);
        if(len > 0)
        {
            s[len] = '\0';
            m_linkTarget = QFile::decodeName(s);
        }
        free(s);
#endif

        m_bBrokenLink = !QFileInfo::exists(m_linkTarget);
        //We want to know if the link itself exists
        if(!m_bExists)
            m_bExists = true;

        if(!m_modificationTime.isValid())
            m_modificationTime = QDateTime::fromMSecsSinceEpoch(0);
    }

    realFile = QSharedPointer<QFile>::create(absoluteFilePath());
    m_bValidData = true;
}

void FileAccess::addPath(const QString& txt, bool reinit)
{
    if(!isLocal())
    {
        QUrl url = m_url.adjusted(QUrl::StripTrailingSlash);
        url.setPath(url.path() + '/' + txt);
        m_url = url;

        if(reinit)
            setFile(url); // reinitialize
    }
    else
    {
        QString slash = (txt.isEmpty() || txt[0] == '/') ? QString() : u8"/";
        setFile(absoluteFilePath() + slash + txt);
    }
}

#ifndef AUTOTEST
/*     Filetype:
       S_IFMT     0170000   bitmask for the file type bitfields
       S_IFSOCK   0140000   socket
       S_IFLNK    0120000   symbolic link
       S_IFREG    0100000   regular file
       S_IFBLK    0060000   block device
       S_IFDIR    0040000   directory
       S_IFCHR    0020000   character device
       S_IFIFO    0010000   fifo
       S_ISUID    0004000   set UID bit
       S_ISGID    0002000   set GID bit (see below)
       S_ISVTX    0001000   sticky bit (see below)

       Access:
       S_IRWXU    00700     mask for file owner permissions
       S_IRUSR    00400     owner has read permission
       S_IWUSR    00200     owner has write permission
       S_IXUSR    00100     owner has execute permission
       S_IRWXG    00070     mask for group permissions
       S_IRGRP    00040     group has read permission
       S_IWGRP    00020     group has write permission
       S_IXGRP    00010     group has execute permission
       S_IRWXO    00007     mask for permissions for others (not in group)
       S_IROTH    00004     others have read permission
       S_IWOTH    00002     others have write permission
       S_IXOTH    00001     others have execute permission
*/
//This is what KIO uses on windows so we might as well check it.
#ifdef Q_OS_WIN
#define S_IRUSR 0400  // Read by owner.
#define S_IWUSR 0200  // Write by owner.
#define S_IXUSR 0100  // Execute by owner.
#define S_IROTH 00004 // others have read permission
#define S_IWOTH 00002 // others have write permission
#define S_IXOTH 00001 // others have execute permission
#endif
void FileAccess::setFromUdsEntry(const KIO::UDSEntry& e, FileAccess* parent)
{
    long acc = 0;
    long fileType = 0;
    const QVector<uint> fields = e.fields();
    QString filePath;

    assert(this != parent);
    m_pParent = parent;

    for(const uint fieldId: fields)
    {
        switch(fieldId)
        {
            case KIO::UDSEntry::UDS_SIZE:
                m_size = e.numberValue(fieldId);
                break;
            case KIO::UDSEntry::UDS_NAME:
                filePath = e.stringValue(fieldId);
                qCDebug(kdiffFileAccess) << "filePath = " << filePath;
                break; // During listDir the relative path is given here.
            case KIO::UDSEntry::UDS_MODIFICATION_TIME:
                m_modificationTime = QDateTime::fromMSecsSinceEpoch(e.numberValue(fieldId));
                break;
            case KIO::UDSEntry::UDS_LINK_DEST:
                m_linkTarget = e.stringValue(fieldId);
                break;
            case KIO::UDSEntry::UDS_ACCESS:
                acc = e.numberValue(fieldId);
                m_bReadable = (acc & S_IRUSR) != 0;
                m_bWritable = (acc & S_IWUSR) != 0;
                m_bExecutable = (acc & S_IXUSR) != 0;
                break;
            case KIO::UDSEntry::UDS_FILE_TYPE:
                /*
                    According to KIO docs UDS_LINK_DEST not S_ISLNK should be used to determine if the url is a symlink.
                    UDS_FILE_TYPE is explicitly stated to be the type of the linked file not the link itself.
                */

                m_bSymLink = e.isLink();
                if(!m_bSymLink)
                {
                    fileType = e.numberValue(fieldId);
                    m_bDir = (fileType & QT_STAT_MASK) == QT_STAT_DIR;
                    m_bFile = (fileType & QT_STAT_MASK) == QT_STAT_REG;
                    m_bExists = fileType != 0;
                }
                else
                {
                    m_bDir = false;
                    m_bFile = false;
                    m_bExists = true;
                }
                break;
            case KIO::UDSEntry::UDS_URL:
                m_url = QUrl(e.stringValue(fieldId));
                qCDebug(kdiffFileAccess) << "Url = " << m_url;
                break;
            case KIO::UDSEntry::UDS_DISPLAY_NAME:
                mDisplayName = e.stringValue(fieldId);
                break;
            case KIO::UDSEntry::UDS_LOCAL_PATH:
                mPhysicalPath = e.stringValue(fieldId);
                break;
            case KIO::UDSEntry::UDS_MIME_TYPE:
            case KIO::UDSEntry::UDS_GUESSED_MIME_TYPE:
            case KIO::UDSEntry::UDS_XML_PROPERTIES:
            default:
                break;
        }
    }

    //Seems to be the norm for fish and possibly other prototcol handlers.
    if(m_url.isEmpty())
    {
        qCInfo(kdiffFileAccess) << "Url not received from KIO.";
        if(Q_UNLIKELY(parent == nullptr))
        {
            /*
             Invalid entry we don't know the full url because KIO didn't tell us and there is no parent
             node supplied.
             This is a bug if it happens and should be logged. However it is a recoverable error.
            */
            qCCritical(kdiffFileAccess) << i18n("Unable to determine full url. No parent specified.");
            return;
        }
        /*
            Don't trust QUrl::resolved it doesn't always do what kdiff3 wants.
        */
        m_url = parent->url();
        addPath(filePath, false);
        //Not something I expect to happen but can't rule it out either
        if(Q_UNLIKELY(m_url == parent->url()))
        {
            m_url.clear();
            qCritical() << "Parent and child could not be distinguished.";
            return;
        }

        qCDebug(kdiffFileAccess) << "Computed url is: " << m_url;
        //Verify that the scheme doesn't change.
        assert(m_url.scheme() == parent->url().scheme());
    }

    //KIO does this when stating a remote folder.
    if(filePath.isEmpty())
    {
        filePath = m_url.path();
    }

    m_fileInfo = QFileInfo(filePath);
    m_fileInfo.setCaching(true);
    //These functions work on the path string without accessing the file system
    m_name = m_fileInfo.fileName();
    if(m_name.isEmpty())
        m_name = m_fileInfo.absoluteDir().dirName();

    if(isLocal())
    {
        m_bBrokenLink = !m_fileInfo.exists() && m_fileInfo.isSymLink();
        m_bExists = m_fileInfo.exists() || m_bBrokenLink;

        //insure modification time is initialized if it wasn't already.
        if(!m_bBrokenLink && m_modificationTime == QDateTime::fromMSecsSinceEpoch(0))
            m_modificationTime = m_fileInfo.lastModified();
    }

    m_bValidData = true;
    m_bSymLink = !m_linkTarget.isEmpty();

#ifndef Q_OS_WIN
    m_bHidden = m_name[0] == '.';
#endif
}
#endif

bool FileAccess::isValid() const
{
    return m_bValidData;
}

bool FileAccess::isNormal() const
{
    static quint32 depth = 0;
    /*
        Speed is important here isNormal is called for every file during directory
        comparison. It can therefor have great impact on overall performance.

        We also need to insure that we don't keep looking indefinitely when following
        links that point to links. Therefore we hard cap at 15 such links in a chain
        and make sure we don't cycle back to something we already saw.
    */
    if(!mVisited && depth < 15 && isLocal() && isSymLink())
    {
        /*
            wierd psudo-name created from commandline input redirection from output of another command.
            KIO/Qt does not handle it as a normal file but presents it as such.
        */
        if(m_linkTarget.startsWith("pipe:"))
        {
            return false;
        }

        FileAccess target(m_linkTarget);

        mVisited = true;
        ++depth;
        /*
            Catch local links to special files. '/dev' has many of these.
        */
        bool result = target.isNormal();
        // mVisited has done its job and should be reset here.
        mVisited = false;
        --depth;

        return result;
    }

    return !exists() || isFile() || isDir() || isSymLink();
}

bool FileAccess::isFile() const
{
    if(!isLocal())
        return m_bFile;
    else
        return m_fileInfo.isFile();
}

bool FileAccess::isDir() const
{
    if(!isLocal())
        return m_bDir;
    else
        return m_fileInfo.isDir();
}

bool FileAccess::isSymLink() const
{
    if(!isLocal())
        return m_bSymLink;
    else
        return m_fileInfo.isSymLink();
}

bool FileAccess::exists() const
{
    if(!isLocal())
        return m_bExists;
    else
        return (m_fileInfo.exists() || isSymLink()) && // QFileInfo.exists returns false for broken links,
               absoluteFilePath() != "/dev/null";      // git uses /dev/null as a placeholder meaning does not exist
}

qint64 FileAccess::size() const
{
    if(!isLocal())
        return m_size;
    else
        return m_fileInfo.size();
}

QUrl FileAccess::url() const
{
    return m_url;
}

/*
    FileAccess::isLocal() should return whether or not the m_url contains what KDiff3 considers
    a local i.e. non-KIO path. This is not the necessarily same as what QUrl::isLocalFile thinks.
*/
bool FileAccess::isLocal() const
{
    return m_url.isLocalFile() || !m_url.isValid() || m_url.scheme().isEmpty();
}

bool FileAccess::isReadable() const
{
    //This can be very slow in some network setups so use cached value
    if(!isLocal())
        return m_bReadable;
    else
        return m_fileInfo.isReadable();
}

bool FileAccess::isWritable() const
{
    //This can be very slow in some network setups so use cached value
    if(!isLocal())
        return m_bWritable;
    else
        return m_fileInfo.isWritable();
}

bool FileAccess::isExecutable() const
{
    //This can be very slow in some network setups so use cached value
    if(!isLocal())
        return m_bExecutable;
    else
        return m_fileInfo.isExecutable();
}

bool FileAccess::isHidden() const
{
    if(!(isLocal()))
        return m_bHidden;
    else
        return m_fileInfo.isHidden();
}

QString FileAccess::readLink() const
{
    return m_linkTarget;
}

QString FileAccess::absoluteFilePath() const
{
    if(!isLocal())
        return m_url.url(); // return complete url

    return m_fileInfo.absoluteFilePath();
} // Full abs path

// Just the name-part of the path, without parent directories
QString FileAccess::fileName(bool needTmp) const
{
    if(!isLocal())
        return (needTmp) ? m_localCopy : m_name;
    else
        return m_name;
}

QString FileAccess::fileRelPath() const
{
#ifndef AUTOTEST
    assert(m_pParent == nullptr || m_baseDir == m_pParent->m_baseDir);
#endif
    QString path;

    if(isLocal())
    {
        path = m_baseDir.relativeFilePath(m_fileInfo.absoluteFilePath());

        return path;
    }
    else
    {
        //Stop right before the root directory
        if(parent() == nullptr) return QString();

        const FileAccess* curEntry = this;
        path = fileName();
        //Avoid recursing to FileAccess::fileRelPath or we can get very large stacks.
        curEntry = curEntry->parent();
        while(curEntry != nullptr)
        {
            if(curEntry->parent())
                path.prepend(curEntry->fileName() + '/');
            curEntry = curEntry->parent();
        }
        return path;
    }
}

FileAccess* FileAccess::parent() const
{
    assert(m_pParent != this);
    return m_pParent;
}

//Workaround for QUrl::toDisplayString/QUrl::toString behavior that does not fit KDiff3's expectations
QString FileAccess::prettyAbsPath() const
{
    return isLocal() ? absoluteFilePath() : m_url.toDisplayString();
}

QDateTime FileAccess::lastModified() const
{
    return m_modificationTime;
}

bool FileAccess::interruptableReadFile(void* pDestBuffer, qint64 maxLength)
{
    ProgressScope pp;
    const qint64 maxChunkSize = 100000;
    qint64 i = 0;
    ProgressProxy::setMaxNofSteps(maxLength / maxChunkSize + 1);
    while(i < maxLength)
    {
        qint64 nextLength = std::min(maxLength - i, maxChunkSize);
        qint64 reallyRead = read((char*)pDestBuffer + i, nextLength);
        if(reallyRead != nextLength)
        {
            setStatusText(i18n("Failed to read file: %1", absoluteFilePath()));
            return false;
        }
        i += reallyRead;

        ProgressProxy::setCurrent(qFloor(double(i) / maxLength * 100));
        if(ProgressProxy::wasCancelled())
            return false;
    }
    return true;
}

bool FileAccess::readFile(void* pDestBuffer, qint64 maxLength)
{
    bool success = false;
    //Avoid hang on linux for special files.
    if(!isNormal())
        return true;

    if(isLocal() || !m_localCopy.isEmpty())
    {
        if(open(QIODevice::ReadOnly))//krazy:exclude=syscalls
        {
            success = interruptableReadFile(pDestBuffer, maxLength); // maxLength == f.read( (char*)pDestBuffer, maxLength )
            close();
        }
    }
    else
    {
        success = mJobHandler->get(pDestBuffer, maxLength);
    }

    close();
    assert(!realFile->isOpen() && !tmpFile->isOpen());
    return success;
}

bool FileAccess::writeFile(const void* pSrcBuffer, qint64 length)
{
    ProgressScope pp;
    if(isLocal())
    {
        if(realFile->open(QIODevice::WriteOnly))
        {
            const qint64 maxChunkSize = 100000;
            ProgressProxy::setMaxNofSteps(length / maxChunkSize + 1);
            qint64 i = 0;
            while(i < length)
            {
                qint64 nextLength = std::min(length - i, maxChunkSize);
                qint64 reallyWritten = realFile->write((char*)pSrcBuffer + i, nextLength);
                if(reallyWritten != nextLength)
                {
                    realFile->close();
                    return false;
                }
                i += reallyWritten;

                ProgressProxy::step();
                if(ProgressProxy::wasCancelled())
                {
                    realFile->close();
                    return false;
                }
            }

            if(isExecutable()) // value is true if the old file was executable
            {
                // Preserve attributes
                realFile->setPermissions(realFile->permissions() | QFile::ExeUser);
            }

            realFile->close();
            return true;
        }
    }
    else
    {
        bool success = mJobHandler->put(pSrcBuffer, length, true /*overwrite*/);
        close();

        assert(!realFile->isOpen() && !tmpFile->isOpen());

        return success;
    }
    close();
    assert(!realFile->isOpen() && !tmpFile->isOpen());
    return false;
}

bool FileAccess::copyFile(const QString& dest)
{
    return mJobHandler->copyFile(dest); // Handles local and remote copying.
}

bool FileAccess::rename(const FileAccess& dest)
{
    return mJobHandler->rename(dest);
}

bool FileAccess::removeFile()
{
    if(isLocal())
    {
        return QDir().remove(absoluteFilePath());
    }
    else
    {
        return mJobHandler->removeFile(url());
    }
}

bool FileAccess::listDir(DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                         const QString& filePattern, const QString& fileAntiPattern, const QString& dirAntiPattern,
                         bool bFollowDirLinks, IgnoreList& ignoreList)
{
    assert(mJobHandler != nullptr);
    return mJobHandler->listDir(pDirList, bRecursive, bFindHidden, filePattern, fileAntiPattern,
                      dirAntiPattern, bFollowDirLinks, ignoreList);
}

QString FileAccess::getTempName() const
{
    if(mPhysicalPath.isEmpty())
        return m_localCopy;
    else
        return mPhysicalPath;
}

const QString& FileAccess::errorString() const
{
    return getStatusText();
}

bool FileAccess::open(const QFile::OpenMode flags)
{
    bool result;
    result = createLocalCopy();
    if(!result)
    {
        setStatusText(i18n("Creating temp copy of %1 failed.", absoluteFilePath()));
        return result;
    }

    if(m_localCopy.isEmpty() && realFile != nullptr)
    {
        bool r = realFile->open(flags);

        setStatusText(i18n("Opening %1 failed. %2", absoluteFilePath(), realFile->errorString()));
        return r;
    }

    bool r = tmpFile->open();
    setStatusText(i18n("Opening %1 failed. %2", tmpFile->fileName(), tmpFile->errorString()));
    return r;
}

qint64 FileAccess::read(char* data, const qint64 maxlen)
{
    if(!isNormal())
    {
        //This is not an error special files should be skipped
        setStatusText(QString());
        return 0;
    }

    qint64 len = 0;
    if(m_localCopy.isEmpty() && realFile != nullptr)
    {
        len = realFile->read(data, maxlen);
        if(len != maxlen)
        {
            setStatusText(i18n("Error reading from %1. %2", absoluteFilePath(), realFile->errorString()));
        }
    }
    else
    {
        len = tmpFile->read(data, maxlen);
        if(len != maxlen)
        {
            setStatusText(i18n("Error reading from %1. %2", absoluteFilePath(), tmpFile->errorString()));
        }
    }

    return len;
}

void FileAccess::close()
{
    if(m_localCopy.isEmpty() && realFile != nullptr)
    {
        realFile->close();
    }

    tmpFile->close();
}

bool FileAccess::createLocalCopy()
{
    if(isLocal() || !m_localCopy.isEmpty() || !mPhysicalPath.isEmpty())
        return true;

    tmpFile->setAutoRemove(true);
    tmpFile->open();
    tmpFile->close();
    m_localCopy = tmpFile->fileName();

    return copyFile(tmpFile->fileName());
}

//static tempfile Generator
void FileAccess::createTempFile(QTemporaryFile& tmpFile)
{
    tmpFile.setAutoRemove(true);
    tmpFile.open();
    tmpFile.close();
}

#ifndef AUTOTEST
bool FileAccess::makeDir(const QString& dirName)
{
    return DefaultFileAccessJobHandler::mkDir(dirName);
}

bool FileAccess::removeDir(const QString& dirName)
{
    return DefaultFileAccessJobHandler::rmDir(dirName);
}
#endif // !AUTOTEST

bool FileAccess::symLink(const QString& linkTarget, const QString& linkLocation)
{
    if(linkTarget.isEmpty() || linkLocation.isEmpty())
        return false;
    return QFile::link(linkTarget, linkLocation);
    //DefaultFileAccessJobHandler fh(0);
    //return fh.symLink( linkTarget, linkLocation );
}

bool FileAccess::exists(const QString& name)
{
    FileAccess fa(name);
    return fa.exists();
}

// If the size couldn't be determined by stat() then the file is copied to a local temp file.
qint64 FileAccess::sizeForReading()
{
    if(!isLocal() && m_size == 0 && mPhysicalPath.isEmpty())
    {
        // Size couldn't be determined. Copy the file to a local temp place.
        if(createLocalCopy())
        {
            QString localCopy = tmpFile->fileName();
            QFileInfo fi(localCopy);

            m_size = fi.size();
            m_localCopy = localCopy;
            return m_size;
        }
        else
        {
            return 0;
        }
    }
    else
        return size();
}

const QString& FileAccess::getStatusText() const
{
    return m_statusText;
}

void FileAccess::setStatusText(const QString& s)
{
    m_statusText = s;
}

QString FileAccess::cleanPath(const QString& path) // static
{
    /*
        Tell Qt to treat the supplied path as user input otherwise it will not make useful decisions
        about how to convert from the possibly local or remote "path" string to QUrl.
    */
    QUrl url = QUrl::fromUserInput(path, QString(), QUrl::AssumeLocalFile);

    if(FileAccess::isLocal(url))
    {
        return QDir::cleanPath(path);
    }
    else
    {
        return path;
    }
}

bool FileAccess::createBackup(const QString& bakExtension)
{
    if(exists())
    {
        // First rename the existing file to the bak-file. If a bak-file file exists, delete that.
        QString bakName = absoluteFilePath() + bakExtension;
        FileAccess bakFile(bakName, true /*bWantToWrite*/);
        if(bakFile.exists())
        {
            bool bSuccess = bakFile.removeFile();
            if(!bSuccess)
            {
                setStatusText(i18n("While trying to make a backup, deleting an older backup failed.\nFilename: %1", bakName));
                return false;
            }
        }
        bool bSuccess = rename(bakFile); // krazy:exclude=syscalls
        if(!bSuccess)
        {
            setStatusText(i18n("While trying to make a backup, renaming failed.\nFilenames: %1 -> %2",
                               absoluteFilePath(), bakName));
            return false;
        }
    }
    return true;
}

void FileAccess::doError()
{
    m_bValidData = true;
    m_bExists = false;
}

void FileAccess::filterList(const QString& dir, DirectoryList* pDirList, const QString& filePattern,
                            const QString& fileAntiPattern, const QString& dirAntiPattern,
                            IgnoreList& ignoreList)
{
    //TODO: Ask os for this information don't hard code it.
#if defined(Q_OS_WIN)
    bool bCaseSensitive = false;
#else
    bool bCaseSensitive = true;
#endif

    // Now remove all entries that should be ignored:
    DirectoryList::iterator i;
    for(i = pDirList->begin(); i != pDirList->end();)
    {
        DirectoryList::iterator i2 = i;
        ++i2;
        QString fileName = i->fileName();

        if((i->isFile() &&
            (!Utils::wildcardMultiMatch(filePattern, fileName, bCaseSensitive) ||
             Utils::wildcardMultiMatch(fileAntiPattern, fileName, bCaseSensitive))) ||
           (i->isDir() && Utils::wildcardMultiMatch(dirAntiPattern, fileName, bCaseSensitive)) ||
           (ignoreList.matches(dir, fileName, bCaseSensitive)))
        {
            // Remove it
            pDirList->erase(i);
            i = i2;
        }
        else
        {
            ++i;
        }
    }
}

//#include "fileaccess.moc"
