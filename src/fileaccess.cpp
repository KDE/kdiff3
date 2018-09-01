/***************************************************************************
 *   Copyright (C) 2003-2011 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
 *   Copyright (C) Michael Reeves reeves.87@gmail.com                      *
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

#ifdef Q_OS_WIN
#include <io.h>
#include <process.h>
#include <sys/utime.h>
#include <windows.h>
#else
#include <unistd.h> // Needed for creating symbolic links via symlink().
#include <utime.h>
#endif

class FileAccess::FileAccessPrivateData
{
  public:
    FileAccessPrivateData()
    {
        reset();
    }
    
    void reset()
    {
        m_url = QUrl();
        m_bValidData = false;
        m_name = QString();
       
        m_linkTarget = "";
        //m_fileType = -1;
        m_pParent = nullptr;
        //Insure that old tempFile is removed we will recreate it as needed.
        if(!m_localCopy.isEmpty())
        {
            removeTempFile(m_localCopy);
            m_localCopy = "";
        }
    }

    inline bool isLocal() const
    {
        return m_url.isLocalFile() || !m_url.isValid();
    }

    ~FileAccessPrivateData()
    {
        //Cleanup tempfile when FileAccessPrivateData object referancing it is destroyed.
        if(!m_localCopy.isEmpty())
        {
            removeTempFile(m_localCopy);
        }
    }

    QUrl m_url;
    bool m_bValidData;

    //long m_fileType; // for testing only
    FileAccess* m_pParent;
    
    QString m_linkTarget;
    QString m_name = QString("");
    QString m_localCopy = QString("");
    QString m_statusText; // Might contain an error string, when the last operation didn't succeed.
};

FileAccess::FileAccess(const QString& name, bool bWantToWrite)
{
    m_pData = nullptr;
    m_modificationTime = QDateTime();
    createData();
    setFile(name, bWantToWrite);
}

FileAccess::FileAccess()
{
    m_bExists = false;
    m_bFile = false;
    m_bDir = false;
    m_bSymLink = false;
    m_bWritable = false;
    m_bHidden = false;
    m_pData = nullptr;
    m_size = 0;
    m_modificationTime = QDateTime();

    createData();
}

FileAccess::FileAccess(const FileAccess& other)
{
    createData();

    *m_pData = *other.m_pData;
    *this = other;
}

void FileAccess::createData()
{
    if(d() == nullptr)
        m_pData = new FileAccessPrivateData();
}

FileAccess& FileAccess::operator=(const FileAccess& other)
{
    m_size = other.m_size;
    m_filePath = other.m_filePath;
    m_modificationTime = other.m_modificationTime;
    m_bSymLink = other.m_bSymLink;
    m_bFile = other.m_bFile;
    m_bDir = other.m_bDir;
    m_bReadable = other.m_bReadable;
    m_bExecutable = other.m_bExecutable;
    m_bExists = other.m_bExists;
    m_bWritable = other.m_bWritable;
    m_bHidden = other.m_bHidden;
    *m_pData = *other.m_pData;

    return *this;
}

FileAccess::~FileAccess()
{
    if(m_pData != nullptr)
        delete m_pData;
}

// Two kinds of optimization are applied here:
// 1. Speed: don't ask for data as long as it is not needed or cheap to get.
//    When opening a file it is easy enough to ask for details.
// 2. Memory usage: Don't store data that is not needed, and avoid redundancy.
//    For recursive directory trees don't store the full path if a parent is available.
//    Store urls only if files are not local.

void FileAccess::setFile(const QFileInfo& fi, FileAccess* pParent)
{
    m_fileInfo = fi;
    m_filePath = pParent == nullptr ? fi.absoluteFilePath() : fi.fileName();

    m_bSymLink = fi.isSymLink();
    createData();

    d()->m_pParent = pParent;

    m_bFile = fi.isFile();
    m_bDir = fi.isDir();
    m_bExists = fi.exists();
    m_size = fi.size();
    m_modificationTime = fi.lastModified();
    m_bHidden = fi.isHidden();
    
    m_bWritable = fi.isWritable();
    m_bReadable = fi.isReadable();
    m_bExecutable = fi.isExecutable();

    if(d()->isLocal())
    {
        d()->m_name = fi.fileName();
        if(m_bSymLink)
        {
            d()->m_linkTarget = fi.readLink();
#ifndef Q_OS_WIN 
            // Unfortunately Qt5 symLinkTarget/readLink always returns an absolute path, even if the link is relative
            char s[PATH_MAX + 1];
            ssize_t len = readlink(QFile::encodeName(fi.absoluteFilePath()).constData(), s, PATH_MAX);
            if(len > 0)
            {
                s[len] = '\0';
                d()->m_linkTarget = QFile::decodeName(s);
            }
#endif
        }

        d()->m_bValidData = true;
        d()->m_url = QUrl::fromLocalFile(fi.filePath());
        if(d()->m_url.isRelative())
        {
            d()->m_url.setPath(absoluteFilePath());
        }
    }
}

void FileAccess::setFile(const QString& name, bool bWantToWrite)
{
    m_bExists = false;
    m_bFile = false;
    m_bDir = false;
    m_bSymLink = false;
    m_size = 0;
    m_modificationTime = QDateTime();

    createData();
    d()->reset();
    //Nothing to do here.
    if(name.isEmpty())
        return;
    
    QUrl url = QUrl::fromUserInput(name, QString(), QUrl::AssumeLocalFile);

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
        QString localName = url.path();

        d()->m_url = url;
#if defined(Q_OS_WIN)
        if(localName.startsWith(QLatin1String("/tmp/")))
        {
            // git on Cygwin will put files in /tmp
            // A workaround for the a native kdiff3 binary to find them...

            QString cygwinBin = QLatin1String(qgetenv("CYGWIN_BIN"));
            if(!cygwinBin.isEmpty())
            {
                localName = QString("%1\\..%2").arg(cygwinBin).arg(name);
            }
        }
#endif
        QFileInfo fi(localName);
        setFile(fi, nullptr);
    }
    else
    {
        d()->m_url = url;
        d()->m_name = d()->m_url.fileName();

        FileAccessJobHandler jh(this);            // A friend, which writes to the parameters of this class!
        jh.stat(2 /*all details*/, bWantToWrite); // returns bSuccess, ignored

        m_filePath = name;
        d()->m_bValidData = true; // After running stat() the variables are initialised
                                  // and valid even if the file doesn't exist and the stat
                                  // query failed.
    }
}

void FileAccess::addPath(const QString& txt)
{
    if(!isLocal() && d()->m_url.isValid())
    {
        QUrl url = d()->m_url.adjusted(QUrl::StripTrailingSlash);
        d()->m_url.setPath(url.path() + '/' + txt);
        setFile(d()->m_url.url()); // reinitialise
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
    for(QVector<uint>::ConstIterator ei = fields.constBegin(); ei != fields.constEnd(); ++ei)
    {
        uint f = *ei;
        switch(f)
        {
        case KIO::UDSEntry::UDS_SIZE:
            m_size = e.numberValue(f);
            break;
        case KIO::UDSEntry::UDS_NAME:
            m_filePath = e.stringValue(f);
            break; // During listDir the relative path is given here.
        case KIO::UDSEntry::UDS_MODIFICATION_TIME:
            m_modificationTime = QDateTime::fromMSecsSinceEpoch(e.numberValue(f));
            break;
        case KIO::UDSEntry::UDS_LINK_DEST:
            d()->m_linkTarget = e.stringValue(f);
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
            #ifndef Q_OS_WIN
            fileType = e.numberValue(f);
            m_bDir = (fileType & S_IFMT) == S_IFDIR;
            m_bFile = (fileType & S_IFMT) == S_IFREG;
            m_bSymLink = (fileType & S_IFMT) == S_IFLNK;
            m_bExists = fileType != 0;
            //d()->m_fileType = fileType;
            #endif
            break;
        }

        case KIO::UDSEntry::UDS_URL: // m_url = QUrlFix( e.stringValue(f) );
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

    m_bExists = acc != 0 || fileType != 0;

    d()->m_bValidData = true;
    m_bSymLink = !d()->m_linkTarget.isEmpty();
    if(d()->m_name.isEmpty())
    {
        int pos = m_filePath.lastIndexOf('/') + 1;
        d()->m_name = m_filePath.mid(pos);
    }
#ifndef Q_OS_WIN
    m_bHidden = d()->m_name[0] == '.';
#endif
}

bool FileAccess::isValid() const
{
    return !m_filePath.isEmpty() || d()->m_bValidData;
}

bool FileAccess::isFile() const
{
    if(parent() || !isLocal())
        return m_bFile;
    else
        return QFileInfo(absoluteFilePath()).isFile();
}

bool FileAccess::isDir() const
{
    if(isLocal())
        return m_bDir;
    else
        return QFileInfo(absoluteFilePath()).isDir();
}

bool FileAccess::isSymLink() const
{
    return m_bSymLink;
}

bool FileAccess::exists() const
{
    if(isLocal())
        return m_bExists;
    else
        return QFileInfo::exists(absoluteFilePath());
}

qint64 FileAccess::size() const
{
    if(isLocal())
        return m_size;
    else
        return QFileInfo(absoluteFilePath()).size();
}

QUrl FileAccess::url() const
{
    if(!m_filePath.isEmpty())
        return d()->m_url;
    else
    {
        QUrl url = QUrl::fromLocalFile(m_filePath);
        if(url.isRelative())
        {
            url.setPath(absoluteFilePath());
        }
        return url;
    }
}

bool FileAccess::isLocal() const
{
    return d()->isLocal();
}

bool FileAccess::isReadable() const
{
    //This can be very slow in some network setups so use cached value
    if(!d()->isLocal())
        return m_bReadable;
    else
        return m_fileInfo.isReadable();
}

bool FileAccess::isWritable() const
{
    //This can be very slow in some network setups so use cached value
    if(parent() || !d()->isLocal())
        return m_bWritable;
    else
        return m_fileInfo.isWritable();
}

bool FileAccess::isExecutable() const
{
    //This can be very slow in some network setups so use cached value
    if(!d()->isLocal())
        return m_bExecutable;
    else
        return m_fileInfo.isExecutable();
}

bool FileAccess::isHidden() const
{
    if(!(d()->isLocal()))
        return m_bHidden;
    else
        return m_fileInfo.isHidden();
}

QString FileAccess::readLink() const
{
    return d()->m_linkTarget;
}

QString FileAccess::absoluteFilePath() const
{
    if(parent() != nullptr)
        return parent()->absoluteFilePath() + "/" + m_filePath;
    else
    {
        if(m_filePath.isEmpty())
            return QString();

        if(!isLocal())
            return m_filePath; // return complete url

        QFileInfo fi(m_filePath);
        if(fi.isAbsolute())
            return m_filePath;
        else
            return fi.absoluteFilePath(); // Probably never reached
    }
} // Full abs path

// Just the name-part of the path, without parent directories
QString FileAccess::fileName() const
{
    if(!d()->isLocal())
        return d()->m_name;
    else if(parent())
        return m_filePath;
    else
        return m_fileInfo.fileName();
}

QString FileAccess::filePath() const
{
    if(parent() && parent()->parent())
        return parent()->filePath() + "/" + m_filePath;
    else
        return m_filePath; // The path-string that was used during construction
}

FileAccess* FileAccess::parent() const
{
    return d()->m_pParent;
}

FileAccess::FileAccessPrivateData* FileAccess::d()
{
    return m_pData;
}

const FileAccess::FileAccessPrivateData* FileAccess::d() const
{
    return m_pData;
}

QString FileAccess::prettyAbsPath() const
{
    return isLocal() ? absoluteFilePath() : d()->m_url.toDisplayString();
}

/*
QDateTime FileAccess::created() const
{
   if ( d()!=0 )
   {
      if ( isLocal() && d()->m_creationTime.isNull() )
         const_cast<FileAccess*>(this)->d()->m_creationTime = QFileInfo( absoluteFilePath() ).created();
      return ( d()->m_creationTime.isValid() ?  d()->m_creationTime : lastModified() );
   }
   else
   {
      QDateTime created = QFileInfo( absoluteFilePath() ).created();
      return created.isValid() ? created : lastModified();
   }
}
*/

QDateTime FileAccess::lastModified() const
{
    if(isLocal() && m_modificationTime.isNull())
        const_cast<FileAccess*>(this)->m_modificationTime = QFileInfo(absoluteFilePath()).lastModified();
    return m_modificationTime;
}

/*
QDateTime FileAccess::lastRead() const
{
   QDateTime accessTime = d()!=0 ? d()->m_accessTime : QFileInfo( absoluteFilePath() ).lastRead();
   return ( accessTime.isValid() ?  accessTime : lastModified() );
}
*/

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
    if(!d()->m_localCopy.isEmpty())
    {
        QFile f(d()->m_localCopy);
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

QString FileAccess::tempFileName()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    //tmpFile.setAutoDelete( true );  // We only want the name. Delete the precreated file immediately.
    QString name = tmpFile.fileName() + ".2";
    tmpFile.close();
    return name;
}

bool FileAccess::removeTempFile(const QString& name) // static
{
    if(name.endsWith(QLatin1String(".2")))
        FileAccess(name.left(name.length() - 2)).removeFile();
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

#if defined(Q_OS_WIN)
bool FileAccess::symLink(const QString& /*linkTarget*/, const QString& /*linkLocation*/)
{
    return false;
}
#else
bool FileAccess::symLink(const QString& linkTarget, const QString& linkLocation)
{
    return 0 == ::symlink(linkTarget.toLocal8Bit().constData(), linkLocation.toLocal8Bit().constData());
    //FileAccessJobHandler fh(0);
    //return fh.symLink( linkTarget, linkLocation );
}
#endif

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
        QString localCopy = tempFileName();
        bool bSuccess = copyFile(localCopy);
        if(bSuccess)
        {
            QFileInfo fi(localCopy);
            m_size = fi.size();
            d()->m_localCopy = localCopy;
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
    return d()->m_statusText;
}

void FileAccess::setStatusText(const QString& s)
{
    createData();
    d()->m_statusText = s;
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
        createData();
        setFile(absoluteFilePath()); // make sure Data is initialized
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

        m_pFileAccess->d()->m_bValidData = true;
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
    if(!m_pFileAccess->isLocal() || !destUrl.isLocalFile()) // if either url is nonlocal
    {
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

    // Both files are local:
    QString srcName = m_pFileAccess->absoluteFilePath();
    QString destName = dest;
    QFile srcFile(srcName);
    QFile destFile(destName);
    bool bReadSuccess = srcFile.open(QIODevice::ReadOnly);
    if(bReadSuccess == false)
    {
        m_pFileAccess->setStatusText(i18n("Error during file copy operation: Opening file for reading failed. Filename: %1", srcName));
        return false;
    }
    bool bWriteSuccess = destFile.open(QIODevice::WriteOnly);
    if(bWriteSuccess == false)
    {
        m_pFileAccess->setStatusText(i18n("Error during file copy operation: Opening file for writing failed. Filename: %1", destName));
        return false;
    }

    std::vector<char> buffer(100000);
    qint64 bufSize = buffer.size();
    qint64 srcSize = srcFile.size();
    while(srcSize > 0 && !pp.wasCancelled())
    {
        qint64 readSize = srcFile.read(&buffer[0], std::min(srcSize, bufSize));
        if(readSize == -1 || readSize == 0)
        {
            m_pFileAccess->setStatusText(i18n("Error during file copy operation: Reading failed. Filename: %1", srcName));
            return false;
        }
        srcSize -= readSize;
        while(readSize > 0)
        {
            qint64 writeSize = destFile.write(&buffer[0], readSize);
            if(writeSize == -1 || writeSize == 0)
            {
                m_pFileAccess->setStatusText(i18n("Error during file copy operation: Writing failed. Filename: %1", destName));
                return false;
            }
            readSize -= writeSize;
        }
        destFile.flush();
        pp.setCurrent((double)(srcFile.size() - srcSize) / srcFile.size(), false);
    }
    srcFile.close();
    destFile.close();

// Update the times of the destFile
#ifdef Q_OS_WIN
    struct _stat srcFileStatus;
    int statResult = ::_stat(srcName.toLocal8Bit().constData(), &srcFileStatus);
    if(statResult == 0)
    {
        _utimbuf destTimes;
        destTimes.actime = srcFileStatus.st_atime;  /* time of last access */
        destTimes.modtime = srcFileStatus.st_mtime; /* time of last modification */

        _utime(destName.toLocal8Bit().constData(), &destTimes);
        _chmod(destName.toLocal8Bit().constData(), srcFileStatus.st_mode);
    }
#else
    struct stat srcFileStatus;
    int statResult = ::stat(srcName.toLocal8Bit().constData(), &srcFileStatus);
    if(statResult == 0)
    {
        utimbuf destTimes;
        destTimes.actime = srcFileStatus.st_atime;  /* time of last access */
        destTimes.modtime = srcFileStatus.st_mtime; /* time of last modification */

        utime(destName.toLocal8Bit().constData(), &destTimes);
        chmod(destName.toLocal8Bit().constData(), srcFileStatus.st_mode);
    }
#endif
    return true;
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
            dir.setFilter(QDir::Files | QDir::Dirs | /* from KDE3 QDir::TypeMaskDirs | */ QDir::Hidden | QDir::System);

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
                    
                    if(fi.fileName() == "." || fi.fileName() == "..")
                        continue;

                    FileAccess fa;
                    fa.setFile(fi, m_pFileAccess);
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
            //connect( pListJob, SIGNAL(percent(KJob*,qint64)), &pp, SLOT(slotPercent(KJob*, qint64)));

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
        if((!bFindHidden && i->isHidden()) ||
           (i->isFile() &&
            (!wildcardMultiMatch(filePattern, fn, bCaseSensitive) ||
             wildcardMultiMatch(fileAntiPattern, fn, bCaseSensitive))) ||
           (i->isDir() && wildcardMultiMatch(dirAntiPattern, fn, bCaseSensitive)) ||
           cvsIgnoreList.matches(fn, bCaseSensitive))
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

                t_DirectoryList::iterator j;
                for(j = dirList.begin(); j != dirList.end(); ++j)
                {
                    if(j->parent() == nullptr)
                        j->m_filePath = i->fileName() + "/" + j->m_filePath;
                }

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
        fa.createData();
        fa.m_pData->m_pParent = m_pFileAccess;
        fa.setUdsEntry(e);

        if(fa.fileName() != "." && fa.fileName() != "..")
        {
            fa.d()->m_url = parentUrl;
            QUrl url = fa.d()->m_url.adjusted(QUrl::StripTrailingSlash);
            fa.d()->m_url.setPath(url.path() + "/" + fa.fileName());
            //fa.d()->m_absoluteFilePath = fa.url().url();
            m_pDirList->push_back(fa);
        }
    }
}

void ProgressProxyExtender::slotListDirInfoMessage(KJob*, const QString& msg)
{
    setInformation(msg, 0);
}

void ProgressProxyExtender::slotPercent(KJob*, qint64 percent)
{
    setCurrent(percent);
}

//#include "fileaccess.moc"
