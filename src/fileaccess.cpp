/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include "fileaccess.h"
#include "cvsignorelist.h"
#include "common.h"
#include "progress.h"

#include <QDir>
#include <QProcess>
#include <QRegExp>
#include <QTemporaryFile>

#include <cstdlib>
#include <vector>

#include <KIO/CopyJob>
#include <KIO/Job>
#include <KLocalizedString>
#include <KMessageBox>
#include <kio/global.h>
#include <kio/jobclasses.h>
#include <kio/jobuidelegate.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifndef Q_OS_WIN
#include <unistd.h> // Needed for creating symbolic links via symlink().
#include <utime.h>
#endif

FileAccess::FileAccess(const QString& name, bool bWantToWrite)
{
    reset();
    
    setFile(name, bWantToWrite);
}

FileAccess::FileAccess()
{
    reset();
}

void FileAccess::reset(void)
{
    m_fileInfo = QFileInfo();
    m_bExists = false;
    m_bFile = false;
    m_bDir = false;
    m_bSymLink = false;
    m_bWritable = false;
    m_bHidden = false;
    m_size = 0;
    m_modificationTime = QDateTime();

    m_url = QUrl();
    m_bValidData = false;
    m_name = QString();

    m_linkTarget = "";
    //m_fileType = -1;
    m_pParent = nullptr;
    tmpFile.clear();
    tmpFile = QSharedPointer<QTemporaryFile>(new QTemporaryFile());
}

FileAccess::~FileAccess()
{
    tmpFile.clear();
}

// Two kinds of optimization are applied here:
// 1. Speed: don't ask for data as long as it is not needed or cheap to get.
//    When opening a file it is easy enough to ask for details.

void FileAccess::setFilePrivate(FileAccess* pParent)
{
    //This is an internal function which requires a seprate reset call prior to use.
    m_fileInfo.setCaching(true);
    //convert to absolute path that doesn't depend on the current directory.
    if(pParent == nullptr)
        m_baseDir = m_fileInfo.absoluteFilePath();
    else
        m_baseDir = pParent->m_baseDir;
    
    m_fileInfo.makeAbsolute();
    m_bSymLink = m_fileInfo.isSymLink();
    m_pParent = pParent;

    m_bFile = m_fileInfo.isFile();
    m_bDir = m_fileInfo.isDir();
    m_bExists = m_fileInfo.exists();
    m_size = m_fileInfo.size();
    m_modificationTime = m_fileInfo.lastModified();
    m_bHidden = m_fileInfo.isHidden();
    
    m_bWritable = m_fileInfo.isWritable();
    m_bReadable = m_fileInfo.isReadable();
    m_bExecutable = m_fileInfo.isExecutable();
    
    Q_ASSERT(isLocal());
    //if(isLocal())
    //{
    m_name = m_fileInfo.fileName();
    if(m_bSymLink)
    {
        m_linkTarget = m_fileInfo.readLink();
#ifndef Q_OS_WIN
        // Unfortunately Qt5 symLinkTarget/readLink always returns an absolute path, even if the link is relative
        char *s = (char*)malloc(PATH_MAX + 1);
        ssize_t len = readlink(QFile::encodeName(absoluteFilePath()).constData(), s, PATH_MAX);
        if(len > 0)
        {
            s[len] = '\0';
            m_linkTarget = QFile::decodeName(s);
        }
        free(s);
#endif
    }

    m_bValidData = true;
    m_url = QUrl::fromLocalFile(m_fileInfo.filePath());
    if(m_url.isRelative())
    {
        m_url.setPath(absoluteFilePath());
    }
    //}
}

void FileAccess::setFile(const QString& name, bool bWantToWrite)
{
    reset();

    //Nothing to do here.
    if(name.isEmpty())
        return;
    
    QUrl url = QUrl::fromUserInput(name, QString(), QUrl::AssumeLocalFile);
    m_url = url;
    m_name = m_url.fileName();

    // FileAccess tries to detect if the given name is an URL or a local file.
    // This is a problem if the filename looks like an URL (i.e. contains a colon ':').
    // e.g. "file:f.txt" is a valid filename.
    // Most of the time it is sufficient to check if the file exists locally.
    // 2 Problems remain:
    //   1. When the local file exists and the remote location is wanted nevertheless. (unlikely)
    //   2. When the local file doesn't exist and should be written to.

    bool bExistsLocal = QDir().exists(name);
    if(url.isLocalFile() || url.isRelative() || !url.isValid() || bExistsLocal) // Treate invalid urls as relative.
    {
        m_fileInfo = QFileInfo(url.path());
        setFilePrivate(nullptr);
    }
    else
    {

        FileAccessJobHandler jh(this);            // A friend, which writes to the parameters of this class!
        jh.stat(2 /*all details*/, bWantToWrite); // returns bSuccess, ignored

        m_bValidData = true; // After running stat() the variables are initialised
                                  // and valid even if the file doesn't exist and the stat
                                  // query failed.
    }
}

void FileAccess::addPath(const QString& txt)
{
    if(!isLocal() && m_url.isValid())
    {
        QUrl url = m_url.adjusted(QUrl::StripTrailingSlash);
        m_url.setPath(url.path() + '/' + txt);
        setFile(m_url.url()); // reinitialise
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
       S_IWOTH    00002     others have write permisson
       S_IXOTH    00001     others have execute permission
*/
void FileAccess::setUdsEntry(const KIO::UDSEntry& e)
{
    long acc = 0;
    long fileType = 0;
    QVector<uint> fields = e.fields();
    QString filePath;

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

        case KIO::UDSEntry::UDS_URL: // m_url = QUrl( e.stringValue(f) );
            break;
        case KIO::UDSEntry::UDS_MIME_TYPE:
            break;
        case KIO::UDSEntry::UDS_GUESSED_MIME_TYPE:
            break;
        case KIO::UDSEntry::UDS_XML_PROPERTIES:
            break;
        default:
            break;
        }
    }
    
    m_fileInfo = QFileInfo(filePath);
    m_fileInfo.setCaching(true);
    if(m_url.isEmpty())
        m_url = QUrl::fromUserInput(m_fileInfo.absoluteFilePath());
    
    m_name = m_url.fileName();
    m_bExists = m_fileInfo.exists();
    //insure modifcation time is initialized if it wasn't already.
    if(m_modificationTime.isNull())
        m_modificationTime = m_fileInfo.lastModified();

    m_bValidData = true;
    m_bSymLink = !m_linkTarget.isEmpty();
    if(m_name.isEmpty())
    {
        m_name = m_fileInfo.fileName();
    }


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
    return isFile() || isDir() || isSymLink();
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
        return m_fileInfo.exists();
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
    
    if(url.isLocalFile() && url.isRelative())
    {
        url.setPath(absoluteFilePath());
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
        return m_fileInfo.fileName();
}

QString FileAccess::fileRelPath() const
{
    QString basePath = m_baseDir.canonicalPath();
    QString filePath = m_fileInfo.canonicalFilePath();
    QString path = filePath.replace(basePath + "/", QLatin1String(""));

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

static bool interruptableReadFile(QFile& f, void* pDestBuffer, qint64 maxLength)
{
    ProgressProxy pp;
    const qint64 maxChunkSize = 100000;
    qint64 i = 0;
    pp.setMaxNofSteps(maxLength / maxChunkSize + 1);
    while(i < maxLength)
    {
        qint64 nextLength = std::min(maxLength - i, maxChunkSize);
        qint64 reallyRead = f.read((char*)pDestBuffer + i, nextLength);
        if(reallyRead != nextLength)
        {
            return false;
        }
        i += reallyRead;

        pp.setCurrent(double(i) / maxLength);
        if(pp.wasCancelled())
            return false;
    }
    return true;
}

bool FileAccess::readFile(void* pDestBuffer, qint64 maxLength)
{
    //Avoid hang on linux for special files.
    if(!isNormal())
        return true;
    
    if(!m_localCopy.isEmpty())
    {
        QFile f(m_localCopy);
        if(f.open(QIODevice::ReadOnly))
            return interruptableReadFile(f, pDestBuffer, maxLength); // maxLength == f.read( (char*)pDestBuffer, maxLength );
    }
    else if(isLocal())
    {
        QFile f(absoluteFilePath());

        if(f.open(QIODevice::ReadOnly))
            return interruptableReadFile(f, pDestBuffer, maxLength); //maxLength == f.read( (char*)pDestBuffer, maxLength );
    }
    else
    {
        FileAccessJobHandler jh(this);
        return jh.get(pDestBuffer, maxLength);
    }
    return false;
}

bool FileAccess::writeFile(const void* pSrcBuffer, qint64 length)
{
    ProgressProxy pp;
    if(isLocal())
    {
        QFile f(absoluteFilePath());
        if(f.open(QIODevice::WriteOnly))
        {
            const qint64 maxChunkSize = 100000;
            pp.setMaxNofSteps(length / maxChunkSize + 1);
            qint64 i = 0;
            while(i < length)
            {
                qint64 nextLength = std::min(length - i, maxChunkSize);
                qint64 reallyWritten = f.write((char*)pSrcBuffer + i, nextLength);
                if(reallyWritten != nextLength)
                {
                    return false;
                }
                i += reallyWritten;

                pp.step();
                if(pp.wasCancelled())
                    return false;
            }
            f.close();

            if(isExecutable()) // value is true if the old file was executable
            {
                // Preserve attributes
                f.setPermissions(f.permissions() | QFile::ExeUser);
            }

            return true;
        }
    }
    else
    {
        FileAccessJobHandler jh(this);
        return jh.put(pSrcBuffer, length, true /*overwrite*/);
    }
    return false;
}

bool FileAccess::copyFile(const QString& dest)
{
    FileAccessJobHandler jh(this);
    return jh.copyFile(dest); // Handles local and remote copying.
}

bool FileAccess::rename(const QString& dest)
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

bool FileAccess::removeFile(const QString& name) // static
{
    return FileAccess(name).removeFile();
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

bool FileAccess::createLocalCopy()
{
    if(isLocal() || !m_localCopy.isEmpty())
       return true;
    
    tmpFile->setAutoRemove(true);
    tmpFile->setFileTemplate(QStringLiteral("XXXXXX-kdiff3tmp"));
    tmpFile->open();
    tmpFile->close();
    m_localCopy = tmpFile->fileName();

    return copyFile(tmpFile->fileName());
}
//static tempfile Generator
void FileAccess::createTempFile(QTemporaryFile& tmpFile)
{
    tmpFile.setAutoRemove(true);
    tmpFile.setFileTemplate(QStringLiteral("XXXXXX-kdiff3tmp"));
    tmpFile.open();
    tmpFile.close();
}
//TODO:Depricated to be removed
/*QString FileAccess::tempFileName()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    //tmpFile.setAutoDelete( true );
    QString name = tmpFile.fileName();
    tmpFile.close();
    return name;
}*/

bool FileAccess::removeTempFile(const QString& name) // static
{   
    return FileAccess(name).removeFile();
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

QString FileAccess::getStatusText()
{
    return m_statusText;
}

void FileAccess::setStatusText(const QString& s)
{
    m_statusText = s;
}

QString FileAccess::cleanPath(const QString& path) // static
{
    QUrl url = QUrl::fromUserInput(path, QString(""), QUrl::AssumeLocalFile);
    if(url.isLocalFile() || !url.isValid())
    {
        return QDir().cleanPath(path);
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
        bool bSuccess = rename(bakName);
        if(!bSuccess)
        {
            setStatusText(i18n("While trying to make a backup, renaming failed.\nFilenames: %1 -> %2",
                               absoluteFilePath(), bakName));
            return false;
        }
    }
    return true;
}

FileAccessJobHandler::FileAccessJobHandler(FileAccess* pFileAccess)
{
    m_pFileAccess = pFileAccess;
    m_bSuccess = false;
}

bool FileAccessJobHandler::stat(int detail, bool bWantToWrite)
{
    m_bSuccess = false;
    m_pFileAccess->setStatusText(QString());
    KIO::StatJob* pStatJob = KIO::stat(m_pFileAccess->url(),
                                       bWantToWrite ? KIO::StatJob::DestinationSide : KIO::StatJob::SourceSide,
                                       detail, KIO::HideProgressInfo);

    connect(pStatJob, &KIO::StatJob::result, this, &FileAccessJobHandler::slotStatResult);

    ProgressProxy::enterEventLoop(pStatJob, i18n("Getting file status: %1", m_pFileAccess->prettyAbsPath()));

    return m_bSuccess;
}

void FileAccessJobHandler::slotStatResult(KJob* pJob)
{
    if(pJob->error())
    {
        //pJob->uiDelegate()->showErrorMessage();
        m_pFileAccess->m_bExists = false;
        m_bSuccess = true;
    }
    else
    {
        m_bSuccess = true;

        m_pFileAccess->m_bValidData = true;
        const KIO::UDSEntry e = static_cast<KIO::StatJob*>(pJob)->statResult();

        m_pFileAccess->setUdsEntry(e);
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

        connect(pJob, &KIO::TransferJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        connect(pJob, &KIO::TransferJob::data, this, &FileAccessJobHandler::slotGetData);
        connect(pJob, SIGNAL(percent(KJob*, qint64)), &pp, SLOT(slotPercent(KJob*, qint64)));

        ProgressProxy::enterEventLoop(pJob, i18n("Reading file: %1", m_pFileAccess->prettyAbsPath()));
        return m_bSuccess;
    }
    else
        return true;
}

void FileAccessJobHandler::slotGetData(KJob* pJob, const QByteArray& newData)
{
    if(pJob->error())
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

        connect(pJob, &KIO::TransferJob::result, this, &FileAccessJobHandler::slotPutJobResult);
        connect(pJob, &KIO::TransferJob::dataReq, this, &FileAccessJobHandler::slotPutData);
        connect(pJob, SIGNAL(percent(KJob*, qint64)), &pp, SLOT(slotPercent(KJob*, qint64)));

        ProgressProxy::enterEventLoop(pJob, i18n("Writing file: %1", m_pFileAccess->prettyAbsPath()));
        return m_bSuccess;
    }
    else
        return true;
}

void FileAccessJobHandler::slotPutData(KIO::Job* pJob, QByteArray& data)
{
    if(pJob->error())
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
    if(pJob->error())
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
    QUrl dirURL = QUrl::fromUserInput(dirName, QString(""), QUrl::AssumeLocalFile);
    if(dirName.isEmpty())
        return false;
    else if(dirURL.isLocalFile() || dirURL.isRelative())
    {
        return QDir().mkdir(dirURL.path());
    }
    else
    {
        m_bSuccess = false;
        KIO::SimpleJob* pJob = KIO::mkdir(dirURL);
        connect(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob, i18n("Making directory: %1", dirName));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::rmDir(const QString& dirName)
{
    QUrl dirURL = QUrl::fromUserInput(dirName, QString(""), QUrl::AssumeLocalFile);
    if(dirName.isEmpty())
        return false;
    else if(dirURL.isLocalFile())
    {
        return QDir().rmdir(dirURL.path());
    }
    else
    {
        m_bSuccess = false;
        KIO::SimpleJob* pJob = KIO::rmdir(dirURL);
        connect(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob, i18n("Removing directory: %1", dirName));
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
        connect(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

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
        connect(pJob, &KIO::CopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Creating symbolic link: %1 -> %2", linkLocation.toDisplayString(), linkTarget.toDisplayString()));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::rename(const QString& dest)
{
    if(dest.isEmpty())
        return false;

    QUrl kurl = QUrl::fromUserInput(dest, QString(""), QUrl::AssumeLocalFile);
    if(kurl.isRelative())
        kurl = QUrl::fromUserInput(QDir().absoluteFilePath(dest), QString(""), QUrl::AssumeLocalFile); // assuming that invalid means relative

    if(m_pFileAccess->isLocal() && kurl.isLocalFile())
    {
        return QDir().rename(m_pFileAccess->absoluteFilePath(), kurl.path());
    }
    else
    {
        ProgressProxyExtender pp;
        int permissions = -1;
        m_bSuccess = false;
        KIO::FileCopyJob* pJob = KIO::file_move(m_pFileAccess->url(), kurl, permissions, KIO::HideProgressInfo);
        connect(pJob, &KIO::FileCopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        connect(pJob, SIGNAL(percent(KJob*, qint64)), &pp, SLOT(slotPercent(KJob*, qint64)));

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Renaming file: %1 -> %2", m_pFileAccess->prettyAbsPath(), dest));
        return m_bSuccess;
    }
}

void FileAccessJobHandler::slotSimpleJobResult(KJob* pJob)
{
    if(pJob->error())
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
bool FileAccessJobHandler::copyFile(const QString& dest)
{
    ProgressProxyExtender pp;
    QUrl destUrl = QUrl::fromUserInput(dest, QString(""), QUrl::AssumeLocalFile);
    m_pFileAccess->setStatusText(QString());

    int permissions = (m_pFileAccess->isExecutable() ? 0111 : 0) + (m_pFileAccess->isWritable() ? 0222 : 0) + (m_pFileAccess->isReadable() ? 0444 : 0);
    m_bSuccess = false;
    KIO::FileCopyJob* pJob = KIO::file_copy(m_pFileAccess->url(), destUrl, permissions, KIO::HideProgressInfo);
    connect(pJob, &KIO::FileCopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
    connect(pJob, SIGNAL(percent(KJob*, qint64)), &pp, SLOT(slotPercent(KJob*, qint64)));
    ProgressProxy::enterEventLoop(pJob,
                                  i18n("Copying file: %1 -> %2", m_pFileAccess->prettyAbsPath(), dest));

    return m_bSuccess;
    // Note that the KIO-slave preserves the original date, if this is supported.
}

bool wildcardMultiMatch(const QString& wildcard, const QString& testString, bool bCaseSensitive)
{
    static QHash<QString, QRegExp> s_patternMap;

    QStringList sl = wildcard.split(";");

    for(QStringList::Iterator it = sl.begin(); it != sl.end(); ++it)
    {
        QHash<QString, QRegExp>::iterator patIt = s_patternMap.find(*it);
        if(patIt == s_patternMap.end())
        {
            QRegExp pattern(*it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive, QRegExp::Wildcard);
            patIt = s_patternMap.insert(*it, pattern);
        }

        if(patIt.value().exactMatch(testString))
            return true;
    }

    return false;
}

bool FileAccessJobHandler::listDir(t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden, const QString& filePattern,
                                   const QString& fileAntiPattern, const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore)
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

    pp.setInformation(i18n("Reading directory: %1", m_pFileAccess->absoluteFilePath()), 0, false);

    if(m_pFileAccess->isLocal())
    {
        QString currentPath = QDir::currentPath();
        m_bSuccess = QDir::setCurrent(m_pFileAccess->absoluteFilePath());
        if(m_bSuccess)
        {
            m_bSuccess = true;
            QDir dir(".");

            dir.setSorting(QDir::Name | QDir::DirsFirst);
            if(bFindHidden)
                dir.setFilter(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
            else
                dir.setFilter(QDir::Files | QDir::Dirs | QDir::System | QDir::NoDotAndDotDot);

            QFileInfoList fiList = dir.entryInfoList();
            if(fiList.isEmpty())
            {
                // No Permission to read directory or other error.
                m_bSuccess = false;
            }
            else
            {
                foreach(const QFileInfo& fi, fiList) // for each file...
                {
                    if(pp.wasCancelled())
                        break;

                    Q_ASSERT(fi.fileName() != "." && fi.fileName() != "..");

                    FileAccess fa;
                    fa.m_fileInfo = fi;
                    fa.setFilePrivate(m_pFileAccess);
                    pDirList->push_back(fa);
                }
            }
        }
        QDir::setCurrent(currentPath); // restore current path
    }
    else
    {
        KIO::ListJob* pListJob = nullptr;
        pListJob = KIO::listDir(m_pFileAccess->url(), KIO::HideProgressInfo, true /*bFindHidden*/);

        m_bSuccess = false;
        if(pListJob != nullptr)
        {
            connect(pListJob, &KIO::ListJob::entries, this, &FileAccessJobHandler::slotListDirProcessNewEntries);
            connect(pListJob, &KIO::ListJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);

            connect(pListJob, &KIO::ListJob::infoMessage, &pp, &ProgressProxyExtender::slotListDirInfoMessage);

            // This line makes the transfer via fish unreliable.:-(
            if(m_pFileAccess->url().scheme() != QLatin1Literal("fish")){
                connect( pListJob, SIGNAL(percent(KJob*,qint64)), &pp, SLOT(slotPercent(KJob*, qint64)));
            }

            ProgressProxy::enterEventLoop(pListJob,
                                          i18n("Listing directory: %1", m_pFileAccess->prettyAbsPath()));
        }
    }

    CvsIgnoreList cvsIgnoreList;
    if(bUseCvsIgnore)
    {
        cvsIgnoreList.init(*m_pFileAccess, pDirList);
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
        QString fn = i->fileName();
        Q_ASSERT(bFindHidden || !i->isHidden());
        if((!bFindHidden && i->isHidden()) ||
           (i->isFile() &&
            (!wildcardMultiMatch(filePattern, fn, bCaseSensitive) ||
             wildcardMultiMatch(fileAntiPattern, fn, bCaseSensitive))) ||
           (i->isDir() && wildcardMultiMatch(dirAntiPattern, fn, bCaseSensitive)) ||
           (bUseCvsIgnore && cvsIgnoreList.matches(fn, bCaseSensitive)))
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

    if(bRecursive)
    {
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
    QUrl parentUrl = QUrl::fromUserInput(m_pFileAccess->absoluteFilePath(), QString(""), QUrl::AssumeLocalFile);

    KIO::UDSEntryList::ConstIterator i;
    for(i = l.begin(); i != l.end(); ++i)
    {
        const KIO::UDSEntry& e = *i;
        FileAccess fa;
        
        fa.m_pParent = m_pFileAccess;
        fa.setUdsEntry(e);

        if(fa.fileName() != "." && fa.fileName() != "..")
        {
            fa.m_url = parentUrl;
            QUrl url = fa.m_url.adjusted(QUrl::StripTrailingSlash);
            fa.m_url.setPath(url.path() + "/" + fa.fileName());
            //fa.m_absoluteFilePath = fa.url().url();
            m_pDirList->push_back(fa);
        }
    }
}

//#include "fileaccess.moc"
