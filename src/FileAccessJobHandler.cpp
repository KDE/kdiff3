/**
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 * 
 */

#include "FileAccessJobHandler.h"

#include "defmac.h"
#include "Logging.h"
#include "progress.h"
#include "ProgressProxyExtender.h"

#include <KJob>
#include <KIO/CopyJob>
#include <KIO/Job>
#include <KLocalizedString>

#include <KMessageBox>
#include <KIO/JobUiDelegate>

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

    chk_connect(pStatJob, &KIO::StatJob::result, this, &FileAccessJobHandler::slotStatResult);
    chk_connect(pStatJob, &KIO::StatJob::finished, this, &FileAccessJobHandler::slotJobEnded);

    ProgressProxy::enterEventLoop(pStatJob, i18n("Getting file status: %1", m_pFileAccess->prettyAbsPath()));

    return m_bSuccess;
}

void FileAccessJobHandler::slotStatResult(KJob* pJob)
{
    int err = pJob->error();
    if(err != KJob::NoError)
    {
        qCDebug(kdiffFileAccess) << "slotStatResult: pJob->error() = " << pJob->error();
        if(err != KIO::ERR_DOES_NOT_EXIST)
        {
            pJob->uiDelegate()->showErrorMessage();
            m_bSuccess = false;
            m_pFileAccess->reset();
        }
        else
        {
            m_pFileAccess->doError();
            m_bSuccess = true;
        }
    }
    else
    {
        m_bSuccess = true;

        const KIO::UDSEntry e = static_cast<KIO::StatJob*>(pJob)->statResult();

        m_pFileAccess->setFromUdsEntry(e, m_pFileAccess->parent());
        m_bSuccess = m_pFileAccess->isValid();
    }
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

        chk_connect(pJob, &KIO::TransferJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::TransferJob::finished, this, &FileAccessJobHandler::slotJobEnded);
        chk_connect(pJob, &KIO::TransferJob::data, this, &FileAccessJobHandler::slotGetData);
        chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

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
        qCDebug(kdiffFileAccess) << "slotGetData: pJob->error() = " << pJob->error();
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

        chk_connect(pJob, &KIO::TransferJob::result, this, &FileAccessJobHandler::slotPutJobResult);
        chk_connect(pJob, &KIO::TransferJob::finished, this, &FileAccessJobHandler::slotJobEnded);
        chk_connect(pJob, &KIO::TransferJob::dataReq, this, &FileAccessJobHandler::slotPutData);
        chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

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
        qCDebug(kdiffFileAccess) << "slotPutData: pJob->error() = " << pJob->error();
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
        qCDebug(kdiffFileAccess) << "slotPutJobResult: pJob->error() = " << pJob->error();
        pJob->uiDelegate()->showErrorMessage();
    }
    else
    {
        m_bSuccess = (m_transferredBytes == m_maxLength); // Special success condition
    }
}

bool FileAccessJobHandler::mkDirImp(const QString& dirName)
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
        chk_connect(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::SimpleJob::finished, this, &FileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob, i18n("Making folder: %1", dirName));
        return m_bSuccess;
    }
}

bool FileAccessJobHandler::rmDirImp(const QString& dirName)
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
        chk_connect(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::SimpleJob::finished, this, &FileAccessJobHandler::slotJobEnded);

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
        chk_connect(pJob, &KIO::SimpleJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::SimpleJob::finished, this, &FileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob, i18n("Removing file: %1", FileAccess::prettyAbsPath(fileName)));
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
        chk_connect(pJob, &KIO::CopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::CopyJob::finished, this, &FileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Creating symbolic link: %1 -> %2", FileAccess::prettyAbsPath(linkLocation), FileAccess::prettyAbsPath(linkTarget)));
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
        chk_connect(pJob, &KIO::FileCopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));
        chk_connect(pJob, &KIO::FileCopyJob::finished, this, &FileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Renaming file: %1 -> %2", m_pFileAccess->prettyAbsPath(), destFile.prettyAbsPath()));
        return m_bSuccess;
    }
}

void FileAccessJobHandler::slotJobEnded(KJob* pJob)
{
    Q_UNUSED(pJob);

    ProgressProxy::exitEventLoop(); // Close the dialog, return from exec()
}

void FileAccessJobHandler::slotSimpleJobResult(KJob* pJob)
{
    if(pJob->error() != KJob::NoError)
    {
        qCDebug(kdiffFileAccess) << "slotSimpleJobResult: pJob->error() = " << pJob->error();
        pJob->uiDelegate()->showErrorMessage();
    }
    else
    {
        m_bSuccess = true;
    }
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
    chk_connect(pJob, &KIO::FileCopyJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
    chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));
    chk_connect(pJob, &KIO::FileCopyJob::finished, this, &FileAccessJobHandler::slotJobEnded);

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
    qCInfo(kdiffFileAccess) << "Reading folder: " << m_pFileAccess->absoluteFilePath();

    if(m_pFileAccess->isLocal())
    {
        m_bSuccess = true;
        QDir dir(m_pFileAccess->absoluteFilePath());

        dir.setSorting(QDir::Name | QDir::DirsFirst);
        if(bFindHidden)
            dir.setFilter(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        else
            dir.setFilter(QDir::Files | QDir::Dirs | QDir::System | QDir::NoDotAndDotDot);

        const QFileInfoList fiList = dir.entryInfoList();
        if(fiList.isEmpty())
        {
            /*
                Sadly Qt provides no error information making this case ambiguous.
                A readability check is the best we can do.
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
            chk_connect(pListJob, &KIO::ListJob::entries, this, &FileAccessJobHandler::slotListDirProcessNewEntries);
            chk_connect(pListJob, &KIO::ListJob::result, this, &FileAccessJobHandler::slotSimpleJobResult);
            chk_connect(pListJob, &KIO::ListJob::finished, this, &FileAccessJobHandler::slotJobEnded);
            chk_connect(pListJob, &KIO::ListJob::infoMessage, &pp, &ProgressProxyExtender::slotListDirInfoMessage);

            // This line makes the transfer via fish unreliable.:-(
            /*if(m_pFileAccess->url().scheme() != QLatin1String("fish")){
                chk_connect( pListJob, static_cast<void (KIO::ListJob::*)(KJob*,qint64)>(&KIO::ListJob::percent), &pp, &ProgressProxyExtender::slotPercent);
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
            Q_ASSERT(i->isValid());
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
    for(const KIO::UDSEntry& e: l)
    {
        FileAccess fa;

        fa.setFromUdsEntry(e, m_pFileAccess);

        //must be manually filtered KDE does not supply API for ignoring these.
        if(fa.fileName() != "." && fa.fileName() != ".." && fa.isValid())
        {
            m_pDirList->push_back(std::move(fa));
        }
    }
}
