/*
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fileaccess.h"
#include "cvsignorelist.h"
#include "common.h"
#include "defmac.h"
#include "Logging.h"
#include "progress.h"
#include "ProgressProxyExtender.h"
#include "Utils.h"

#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include <QDir>
#include <QFile>
#include <QtMath>
#include <QRegExp>
#include <QTemporaryFile>

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KLocalizedString>
#include <KMessageBox>
#include <kio/global.h>
#include <kio/jobclasses.h>
#include <kio/jobuidelegate.h>

FileAccess::FileAccess(const QString& name, bool bWantToWrite)
{
    setFile(name, bWantToWrite);
}

void FileAccess::reset()
{
    *this = FileAccess();
}

/*
    Needed only during directory listing right now.
*/
void FileAccess::setFile(FileAccess* pParent, const QFileInfo& fi)
{
    reset();

    m_fileInfo = fi;
    m_url = QUrl::fromLocalFile(m_fileInfo.filePath());
    if(!m_url.scheme().isEmpty())
        m_url.setScheme(QLatin1Literal("file"));

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
    reset();
    Q_ASSERT(parent() == nullptr || url != parent()->url());

    m_url = url;
    //QUrl::isLocalFile assumes the scheme is set.
    if(m_url.scheme().isEmpty())
        m_url.setScheme(QLatin1Literal("file"));

    if(m_url.isLocalFile() || !m_url.isValid()) // Treat invalid urls as local files.
    {
        m_fileInfo.setFile(m_url.toLocalFile());
        m_pParent = nullptr;

        loadData();
    }
    else
    {
        m_name = m_url.fileName();
        FileAccessJobHandler jh(this);            // A friend, which writes to the parameters of this class!
        jh.stat(2 /*all details*/, bWantToWrite); // returns bSuccess, ignored

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
        // Unfortunately Qt5 symLinkTarget/readLink always returns an absolute path, even if the link is relative
        char* s = (char*)malloc(PATH_MAX + 1);
        ssize_t len = readlink(QFile::encodeName(absoluteFilePath()).constData(), s, PATH_MAX);
        if(len > 0)
        {
            s[len] = '\0';
            m_linkTarget = QFile::decodeName(s);
        }
        free(s);
#endif
    }

    realFile = QSharedPointer<QFile>::create(absoluteFilePath());
    m_bValidData = true;
}

void FileAccess::addPath(const QString& txt)
{
    if(!isLocal() && m_url.isValid())
    {
        QUrl url = m_url.adjusted(QUrl::StripTrailingSlash);
        url.setPath(url.path() + '/' + txt);
        setFile(url); // reinitialise
    }
    else
    {
        QString slash = (txt.isEmpty() || txt[0] == '/') ? QLatin1String("") : QLatin1String("/");
        setFile(absoluteFilePath() + slash + txt);
    }
}

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
void FileAccess::setFromUdsEntry(const KIO::UDSEntry& e, FileAccess *parent)
{
    long acc = 0;
    long fileType = 0;
    QVector<uint> fields = e.fields();
    QString filePath;

    m_pParent = parent;
    for(QVector<uint>::ConstIterator ei = fields.constBegin(); ei != fields.constEnd(); ++ei)
    {
        uint f = *ei;
        switch(f)
        {
            case KIO::UDSEntry::UDS_SIZE:
                m_size = e.numberValue(f);
                break;
            case KIO::UDSEntry::UDS_NAME:
                filePath = e.stringValue(f);
                break; // During listDir the relative path is given here.
            case KIO::UDSEntry::UDS_MODIFICATION_TIME:
                m_modificationTime = QDateTime::fromMSecsSinceEpoch(e.numberValue(f));
                break;
            case KIO::UDSEntry::UDS_LINK_DEST:
                m_linkTarget = e.stringValue(f);
                break;
            case KIO::UDSEntry::UDS_ACCESS:
            {
#ifndef Q_OS_WIN
                acc = e.numberValue(f);
                m_bReadable = (acc & S_IRUSR) != 0;
                m_bWritable = (acc & S_IWUSR) != 0;
                m_bExecutable = (acc & S_IXUSR) != 0;
#endif
                break;
            }
            case KIO::UDSEntry::UDS_FILE_TYPE:
            {
                fileType = e.numberValue(f);
                m_bDir = (fileType & QT_STAT_MASK) == QT_STAT_DIR;
                m_bFile = (fileType & QT_STAT_MASK) == QT_STAT_REG;
                m_bSymLink = (fileType & QT_STAT_MASK) == QT_STAT_LNK;
                m_bExists = fileType != 0;
                //m_fileType = fileType;
                break;
            }

            case KIO::UDSEntry::UDS_URL:
                m_url = QUrl(e.stringValue(f));
                break;
            case KIO::UDSEntry::UDS_MIME_TYPE:
            case KIO::UDSEntry::UDS_GUESSED_MIME_TYPE:
            case KIO::UDSEntry::UDS_XML_PROPERTIES:
            default:
                break;
        }
    }

    m_fileInfo = QFileInfo(filePath);
    m_fileInfo.setCaching(true);
    if(m_url.isEmpty())
    {
        if(parent != nullptr)
        {
            m_url = parent->url().resolved(QUrl(filePath));
            //Verify that the scheme doesn't change.
            Q_ASSERT(m_url.scheme() == parent->url().scheme());
        }
        else
        {
            /*
             Invalid entry we don't know the full url because KIO didn't tell us and there is no parent
             node supplied.
             This is a bug if it happens and should be logged . However it is a recoverable error.
            */
            qCWarning(kdiffFileAccess) << i18n("Unable to determine full url. No parent specified.");
            return;
        }
    }

    m_name = m_fileInfo.fileName();
    if(isLocal() && m_name.isEmpty())
    {
        m_name = m_fileInfo.absoluteDir().dirName();
    }
    m_bExists = m_fileInfo.exists();
    //insure modification time is initialized if it wasn't already.
    if(m_modificationTime == QDateTime::fromMSecsSinceEpoch(0))
        m_modificationTime = m_fileInfo.lastModified();

    m_bValidData = true;
    m_bSymLink = !m_linkTarget.isEmpty();

#ifndef Q_OS_WIN
    m_bHidden = m_name[0] == '.';
#endif
}

bool FileAccess::isValid() const
{
    return m_bValidData;
}

bool FileAccess::isNormal() const
{
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
    else//Thank you git for being different.
        return m_fileInfo.exists() && absoluteFilePath() != "/dev/null";
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
    QUrl url = m_url;

    if(url.isLocalFile())
    {
        url = QUrl::fromLocalFile(absoluteFilePath());
    }
    return url;
}

bool FileAccess::isLocal() const
{
    return m_url.isLocalFile() || !m_url.isValid();
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
    QString path = m_baseDir.relativeFilePath(m_fileInfo.absoluteFilePath());

    return path;
}

FileAccess* FileAccess::parent() const
{
    return m_pParent;
}

QString FileAccess::prettyAbsPath() const
{
    return isLocal() ? absoluteFilePath() : m_url.toDisplayString();
}

QDateTime FileAccess::lastModified() const
{
    Q_ASSERT(!m_modificationTime.isNull());
    return m_modificationTime;
}

bool FileAccess::interruptableReadFile(void* pDestBuffer, qint64 maxLength)
{
    ProgressProxy pp;
    const qint64 maxChunkSize = 100000;
    qint64 i = 0;
    pp.setMaxNofSteps(maxLength / maxChunkSize + 1);
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

        pp.setCurrent(qFloor(double(i) / maxLength * 100));
        if(pp.wasCancelled())
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
        FileAccessJobHandler jh(this);
        success = jh.get(pDestBuffer, maxLength);
    }

    close();
    Q_ASSERT(!realFile->isOpen() && !tmpFile->isOpen());
    return success;
}

bool FileAccess::writeFile(const void* pSrcBuffer, qint64 length)
{
    ProgressProxy pp;
    if(isLocal())
    {
        if(realFile->open(QIODevice::WriteOnly))
        {
            const qint64 maxChunkSize = 100000;
            pp.setMaxNofSteps(length / maxChunkSize + 1);
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

                pp.step();
                if(pp.wasCancelled())
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
        FileAccessJobHandler jh(this);
        bool success = jh.put(pSrcBuffer, length, true /*overwrite*/);
        close();

        Q_ASSERT(!realFile->isOpen() && !tmpFile->isOpen());

        return success;
    }
    close();
    Q_ASSERT(!realFile->isOpen() && !tmpFile->isOpen());
    return false;
}

bool FileAccess::copyFile(const QString& dest)
{
    FileAccessJobHandler jh(this);
    return jh.copyFile(dest); // Handles local and remote copying.
}

bool FileAccess::rename(const FileAccess& dest)
{
    FileAccessJobHandler jh(this);
    return jh.rename(dest);
}

bool FileAccess::removeFile()
{
    if(isLocal())
    {
        return QDir().remove(absoluteFilePath());
    }
    else
    {
        FileAccessJobHandler jh(this);
        return jh.removeFile(url());
    }
}

bool FileAccess::listDir(t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                         const QString& filePattern, const QString& fileAntiPattern, const QString& dirAntiPattern,
                         bool bFollowDirLinks, bool bUseCvsIgnore)
{
    FileAccessJobHandler jh(this);
    return jh.listDir(pDirList, bRecursive, bFindHidden, filePattern, fileAntiPattern,
                      dirAntiPattern, bFollowDirLinks, bUseCvsIgnore);
}

QString FileAccess::getTempName() const
{
    return m_localCopy;
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
    if(isLocal() || !m_localCopy.isEmpty())
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

bool FileAccess::makeDir(const QString& dirName)
{
    FileAccessJobHandler fh(nullptr);
    return fh.mkDir(dirName);
}

bool FileAccess::removeDir(const QString& dirName)
{
    FileAccessJobHandler fh(nullptr);
    return fh.rmDir(dirName);
}

bool FileAccess::symLink(const QString& linkTarget, const QString& linkLocation)
{
    if(linkTarget.isEmpty() || linkLocation.isEmpty())
        return false;
    return QFile::link(linkTarget, linkLocation);
    //FileAccessJobHandler fh(0);
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
    if(!isLocal() && m_size == 0)
    {
        // Size couldn't be determined. Copy the file to a local temp place.
        createLocalCopy();
        QString localCopy = tmpFile->fileName();
        bool bSuccess = copyFile(localCopy);
        if(bSuccess)
        {
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
    FileAccess fa(path);
    if(fa.isLocal())
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
    m_bExists = false;
}

void FileAccess::filterList(t_DirectoryList* pDirList, const QString& filePattern,
                            const QString& fileAntiPattern, const QString& dirAntiPattern,
                            const bool bUseCvsIgnore)
{
    CvsIgnoreList cvsIgnoreList;
    if(bUseCvsIgnore)
    {
        cvsIgnoreList.init(*this, pDirList);
    }
    //TODO: Ask os for this information don't hard code it.
#if defined(Q_OS_WIN)
    bool bCaseSensitive = false;
#else
    bool bCaseSensitive = true;
#endif

    // Now remove all entries that should be ignored:
    t_DirectoryList::iterator i;
    for(i = pDirList->begin(); i != pDirList->end();)
    {
        t_DirectoryList::iterator i2 = i;
        ++i2;
        QString fileName = i->fileName();

        if((i->isFile() &&
            (!Utils::wildcardMultiMatch(filePattern, fileName, bCaseSensitive) ||
             Utils::wildcardMultiMatch(fileAntiPattern, fileName, bCaseSensitive))) ||
           (i->isDir() && Utils::wildcardMultiMatch(dirAntiPattern, fileName, bCaseSensitive)) ||
           (bUseCvsIgnore && cvsIgnoreList.matches(fileName, bCaseSensitive)))
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

FileAccessJobHandler::FileAccessJobHandler(FileAccess* pFileAccess)
{
    m_pFileAccess = pFileAccess;
}

bool FileAccessJobHandler::stat(short detail, bool bWantToWrite)
{
    m_bSuccess = false;
    m_pFileAccess->setStatusText(QString());
    KIO::StatJob* pStatJob = KIO::stat(m_pFileAccess->url(),
                                       bWantToWrite ? KIO::StatJob::DestinationSide : KIO::StatJob::SourceSide,
                                       detail, KIO::HideProgressInfo);

    chk_connect_a(pStatJob, &KIO::StatJob::result, this, &FileAccessJobHandler::slotStatResult);

    ProgressProxy::enterEventLoop(pStatJob, i18n("Getting file status: %1", m_pFileAccess->prettyAbsPath()));

    return m_bSuccess;
}

void FileAccessJobHandler::slotStatResult(KJob* pJob)
{
    if(pJob->error() != KJob::NoError)
    {
        //pJob->uiDelegate()->showErrorMessage();
        m_pFileAccess->doError();
        m_bSuccess = true;
    }
    else
    {
        m_bSuccess = true;

        const KIO::UDSEntry e = static_cast<KIO::StatJob*>(pJob)->statResult();

        m_pFileAccess->setFromUdsEntry(e, m_pFileAccess);
    }

    ProgressProxy::exitEventLoop();
}

bool FileAccessJobHandler::get(void* pDestBuffer, long maxLength)
{
    ProgressProxyExtender pp; // Implicitly used in slotPercent()
    if(maxLength > 0 && !pp.wasCancelled())
    {
        KIO::TransferJob* pJob = KIO::get(m_pFileAccess->url(), KIO::NoReload);
        m_transferredBytes = 0;
        m_pTransferBuffer = (char*)pDestBuffer;
        m_maxLength = maxLength;
        m_bSuccess = false;
        m_pFileAccess->setStatusText(QString());

        chk_connect_a(pJob, &KIO::TransferJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect_a(pJob, &KIO::TransferJob::data, this, &FileAccessJobHandler::slotGetData);
        chk_connect_a(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

        ProgressProxy::enterEventLoop(pJob, i18n("Reading file: %1", m_pFileAccess->prettyAbsPath()));
        return m_bSuccess;
    }
    else
        return true;
}

void FileAccessJobHandler::slotGetData(KJob* pJob, const QByteArray& newData)
{
    if(pJob->error() != KJob::NoError)
    {
        pJob->uiDelegate()->showErrorMessage();
    }
    else
    {
        qint64 length = std::min(qint64(newData.size()), m_maxLength - m_transferredBytes);
        ::memcpy(m_pTransferBuffer + m_transferredBytes, newData.data(), newData.size());
        m_transferredBytes += length;
    }
}

bool FileAccessJobHandler::put(const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume, int permissions)
{
    ProgressProxyExtender pp; // Implicitly used in slotPercent()
    if(maxLength > 0)
    {
        KIO::TransferJob* pJob = KIO::put(m_pFileAccess->url(), permissions,
                                          KIO::HideProgressInfo | (bOverwrite ? KIO::Overwrite : KIO::DefaultFlags) | (bResume ? KIO::Resume : KIO::DefaultFlags));
        m_transferredBytes = 0;
        m_pTransferBuffer = (char*)pSrcBuffer;
        m_maxLength = maxLength;
        m_bSuccess = false;
        m_pFileAccess->setStatusText(QString());

        chk_connect_a(pJob, &KIO::TransferJob::result, this, &FileAccessJobHandler::slotPutJobResult);
        chk_connect_a(pJob, &KIO::TransferJob::dataReq, this, &FileAccessJobHandler::slotPutData);
        chk_connect_a(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

        ProgressProxy::enterEventLoop(pJob, i18n("Writing file: %1", m_pFileAccess->prettyAbsPath()));
        return m_bSuccess;
    }
    else
        return true;
}

void FileAccessJobHandler::slotPutData(KIO::Job* pJob, QByteArray& data)
{
    if(pJob->error() != KJob::NoError)
    {
        pJob->uiDelegate()->showErrorMessage();
    }
    else
    {
        /*
            Think twice before doing this in new code.
            The maxChunkSize must be able to fit a 32-bit int. Given that the fallowing is safe.

        */
        qint64 maxChunkSize = 100000;
        qint64 length = std::min(maxChunkSize, m_maxLength - m_transferredBytes);
        data.resize((int)length);
        if(data.size() == (int)length)
        {
            if(length > 0)
            {
                ::memcpy(data.data(), m_pTransferBuffer + m_transferredBytes, data.size());
                m_transferredBytes += length;
            }
        }
        else
        {
            KMessageBox::error(ProgressProxy::getDialog(), i18n("Out of memory"));
            data.resize(0);
            m_bSuccess = false;
        }
    }
}

void FileAccessJobHandler::slotPutJobResult(KJob* pJob)
{
    if(pJob->error() != KJob::NoError)
    {
        pJob->uiDelegate()->showErrorMessage();
    }
    else
    {
        m_bSuccess = (m_transferredBytes == m_maxLength); // Special success condition
    }
    ProgressProxy::exitEventLoop(); // Close the dialog, return from exec()
}

bool FileAccessJobHandler::mkDir(const QString& dirName)
{
    if(dirName.isEmpty())
        return false;

    FileAccess dir(dirName);
    if(dir.isLocal())
    {
        return QDir().mkdir(dir.absoluteFilePath());
    }
    else
    {
        m_bSuccess = false;
        KIO::SimpleJob* pJob = KIO::mkdir(dir.url());
        chk_connect_a(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob, i18n("Making folder: %1", dirName));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::rmDir(const QString& dirName)
{
    if(dirName.isEmpty())
        return false;

    FileAccess fa(dirName);
    if(fa.isLocal())
    {
        return QDir().rmdir(fa.absoluteFilePath());
    }
    else
    {
        m_bSuccess = false;
        KIO::SimpleJob* pJob = KIO::rmdir(fa.url());
        chk_connect_a(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob, i18n("Removing folder: %1", dirName));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::removeFile(const QUrl& fileName)
{
    if(fileName.isEmpty())
        return false;
    else
    {
        m_bSuccess = false;
        KIO::SimpleJob* pJob = KIO::file_delete(fileName, KIO::HideProgressInfo);
        chk_connect_a(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob, i18n("Removing file: %1", fileName.toDisplayString()));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::symLink(const QUrl& linkTarget, const QUrl& linkLocation)
{
    if(linkTarget.isEmpty() || linkLocation.isEmpty())
        return false;
    else
    {
        m_bSuccess = false;
        KIO::CopyJob* pJob = KIO::link(linkTarget, linkLocation, KIO::HideProgressInfo);
        chk_connect_a(pJob, &KIO::CopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Creating symbolic link: %1 -> %2", linkLocation.toDisplayString(), linkTarget.toDisplayString()));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::rename(const FileAccess& destFile)
{
    if(destFile.fileName().isEmpty())
        return false;

    if(m_pFileAccess->isLocal() && destFile.isLocal())
    {
        return QDir().rename(m_pFileAccess->absoluteFilePath(), destFile.absoluteFilePath());
    }
    else
    {
        ProgressProxyExtender pp;
        int permissions = -1;
        m_bSuccess = false;
        KIO::FileCopyJob* pJob = KIO::file_move(m_pFileAccess->url(), destFile.url(), permissions, KIO::HideProgressInfo);
        chk_connect_a(pJob, &KIO::FileCopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect_a(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Renaming file: %1 -> %2", m_pFileAccess->prettyAbsPath(), destFile.prettyAbsPath()));
        return m_bSuccess;
    }
}

void FileAccessJobHandler::slotSimpleJobResult(KJob* pJob)
{
    if(pJob->error() != KJob::NoError)
    {
        pJob->uiDelegate()->showErrorMessage();
    }
    else
    {
        m_bSuccess = true;
    }
    ProgressProxy::exitEventLoop(); // Close the dialog, return from exec()
}

// Copy local or remote files.
bool FileAccessJobHandler::copyFile(const QString& inDest)
{
    ProgressProxyExtender pp;
    FileAccess dest;
    dest.setFile(inDest);

    m_pFileAccess->setStatusText(QString());
    if(!m_pFileAccess->isNormal() || !dest.isNormal()) return false;

    int permissions = (m_pFileAccess->isExecutable() ? 0111 : 0) + (m_pFileAccess->isWritable() ? 0222 : 0) + (m_pFileAccess->isReadable() ? 0444 : 0);
    m_bSuccess = false;
    KIO::FileCopyJob* pJob = KIO::file_copy(m_pFileAccess->url(), dest.url(), permissions, KIO::HideProgressInfo|KIO::Overwrite);
    chk_connect_a(pJob, &KIO::FileCopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
    chk_connect_a(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));
    ProgressProxy::enterEventLoop(pJob,
                                  i18n("Copying file: %1 -> %2", m_pFileAccess->prettyAbsPath(), dest.prettyAbsPath()));

    return m_bSuccess;
    // Note that the KIO-slave preserves the original date, if this is supported.
}

bool FileAccessJobHandler::listDir(t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden, const QString& filePattern,
                                   const QString& fileAntiPattern, const QString& dirAntiPattern, bool bFollowDirLinks, const bool bUseCvsIgnore)
{
    ProgressProxyExtender pp;
    m_pDirList = pDirList;
    m_pDirList->clear();
    m_bFindHidden = bFindHidden;
    m_bRecursive = bRecursive;
    m_bFollowDirLinks = bFollowDirLinks; // Only relevant if bRecursive==true.
    m_fileAntiPattern = fileAntiPattern;
    m_filePattern = filePattern;
    m_dirAntiPattern = dirAntiPattern;

    if(pp.wasCancelled())
        return true; // Cancelled is not an error.

    pp.setInformation(i18n("Reading folder: %1", m_pFileAccess->absoluteFilePath()), 0, false);

    if(m_pFileAccess->isLocal())
    {
        m_bSuccess = true;
        QDir dir(m_pFileAccess->absoluteFilePath());

        dir.setSorting(QDir::Name | QDir::DirsFirst);
        if(bFindHidden)
            dir.setFilter(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        else
            dir.setFilter(QDir::Files | QDir::Dirs | QDir::System | QDir::NoDotAndDotDot);

        QFileInfoList fiList = dir.entryInfoList();
        if(fiList.isEmpty())
        {
            /*
                Sadly Qt provides no error information making this case ambigious.
                A readablity check is the best we can do.
            */
            m_bSuccess = dir.isReadable();
        }
        else
        {
            for(const QFileInfo& fi: fiList) // for each file...
            {
                if(pp.wasCancelled())
                    break;

                Q_ASSERT(fi.fileName() != "." && fi.fileName() != "..");

                FileAccess fa;

                fa.setFile(m_pFileAccess, fi);
                pDirList->push_back(fa);
            }
        }
    }
    else
    {
        KIO::ListJob* pListJob = nullptr;
        pListJob = KIO::listDir(m_pFileAccess->url(), KIO::HideProgressInfo, true /*bFindHidden*/);

        m_bSuccess = false;
        if(pListJob != nullptr)
        {
            chk_connect_a(pListJob, &KIO::ListJob::entries, this, &FileAccessJobHandler::slotListDirProcessNewEntries);
            chk_connect_a(pListJob, &KIO::ListJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

            chk_connect_a(pListJob, &KIO::ListJob::infoMessage, &pp, &ProgressProxyExtender::slotListDirInfoMessage);

            // This line makes the transfer via fish unreliable.:-(
            /*if(m_pFileAccess->url().scheme() != QLatin1Literal("fish")){
                chk_connect_a( pListJob, static_cast<void (KIO::ListJob::*)(KJob*,qint64)>(&KIO::ListJob::percent), &pp, &ProgressProxyExtender::slotPercent);
            }*/

            ProgressProxy::enterEventLoop(pListJob,
                                          i18n("Listing directory: %1", m_pFileAccess->prettyAbsPath()));
        }
    }

    m_pFileAccess->filterList(pDirList, filePattern, fileAntiPattern, dirAntiPattern, bUseCvsIgnore);

    if(bRecursive)
    {
        t_DirectoryList::iterator i;
        t_DirectoryList subDirsList;

        for(i = m_pDirList->begin(); i != m_pDirList->end(); ++i)
        {
            if(i->isDir() && (!i->isSymLink() || m_bFollowDirLinks))
            {
                t_DirectoryList dirList;
                i->listDir(&dirList, bRecursive, bFindHidden,
                           filePattern, fileAntiPattern, dirAntiPattern, bFollowDirLinks, bUseCvsIgnore);

                // append data onto the main list
                subDirsList.splice(subDirsList.end(), dirList);
            }
        }

        m_pDirList->splice(m_pDirList->end(), subDirsList);
    }

    return m_bSuccess;
}

void FileAccessJobHandler::slotListDirProcessNewEntries(KIO::Job*, const KIO::UDSEntryList& l)
{
    //This function is called for non-local urls. Don't use QUrl::fromLocalFile here as it does not handle these.
    KIO::UDSEntryList::ConstIterator i;
    for(i = l.begin(); i != l.end(); ++i)
    {
        const KIO::UDSEntry& e = *i;
        FileAccess fa;

        fa.setFromUdsEntry(e, m_pFileAccess);

        //must be manually filtered KDE does not supply API for ignoring these.
        if(fa.fileName() != "." && fa.fileName() != "..")
        {
            //quick fix to preserve behavoir without creating invalid urls. TODO: look for altertive machanism for use with next major release.
            fa.setFile(fa.url());

            m_pDirList->push_back(fa);
        }
    }
}

//#include "fileaccess.moc"
