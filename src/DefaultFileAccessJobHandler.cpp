
/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "DefaultFileAccessJobHandler.h"

#include "common.h"
#include "defmac.h"
#include "fileaccess.h"
#include "IgnoreList.h"
#include "Logging.h"
#include "progress.h"
#include "ProgressProxyExtender.h"
#include "TypeUtils.h"

#include <algorithm>
#include <list>
#include <utility>

#include <QFileInfoList>
#include <QDir>
#include <QUrl>

#include <KJob>
#include <KIO/CopyJob>
#include <KIO/Job>
#include <KLocalizedString>

#include <KMessageBox>
#include <KIO/JobUiDelegate>

bool DefaultFileAccessJobHandler::stat(bool bWantToWrite)
{
    m_bSuccess = false;
    mFileAccess->setStatusText(QString());
#if KF_VERSION < KF_VERSION_CHECK(5, 69, 0)
    KIO::StatJob* pStatJob = KIO::stat(mFileAccess->url(),
                                       bWantToWrite ? KIO::StatJob::DestinationSide : KIO::StatJob::SourceSide,
                                       2/*all details*/, KIO::HideProgressInfo);
#else
    KIO::StatJob* pStatJob = KIO::statDetails(mFileAccess->url(),
                                       bWantToWrite ? KIO::StatJob::DestinationSide : KIO::StatJob::SourceSide,
                                       KIO::StatDefaultDetails, KIO::HideProgressInfo);

#endif

    chk_connect(pStatJob, &KIO::StatJob::result, this, &DefaultFileAccessJobHandler::slotStatResult);
    chk_connect(pStatJob, &KIO::StatJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

    ProgressProxy::enterEventLoop(pStatJob, i18n("Getting file status: %1", mFileAccess->prettyAbsPath()));

    return m_bSuccess;
}

void DefaultFileAccessJobHandler::slotStatResult(KJob* pJob)
{
    qint32 err = pJob->error();
    if(err != KJob::NoError)
    {
        qCDebug(kdiffFileAccess) << "slotStatResult: pJob->error() = " << pJob->error();
        if(err != KIO::ERR_DOES_NOT_EXIST)
        {
            pJob->uiDelegate()->showErrorMessage();
            m_bSuccess = false;
            mFileAccess->reset();
        }
        else
        {
            mFileAccess->doError();
            m_bSuccess = true;
        }
    }
    else
    {
        m_bSuccess = true;

        const KIO::UDSEntry e = static_cast<KIO::StatJob*>(pJob)->statResult();

        mFileAccess->setFromUdsEntry(e, mFileAccess->parent());
        m_bSuccess = mFileAccess->isValid();
    }
}

bool DefaultFileAccessJobHandler::get(void* pDestBuffer, long maxLength)
{
    ProgressProxyExtender pp; // Implicitly used in slotPercent()

    if(maxLength > 0 && !ProgressProxy::wasCancelled())
    {
        KIO::TransferJob* pJob = KIO::get(mFileAccess->url(), KIO::NoReload);
        m_transferredBytes = 0;
        m_pTransferBuffer = (char*)pDestBuffer;
        m_maxLength = maxLength;
        m_bSuccess = false;
        mFileAccess->setStatusText(QString());

        chk_connect(pJob, &KIO::TransferJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::TransferJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);
        chk_connect(pJob, &KIO::TransferJob::data, this, &DefaultFileAccessJobHandler::slotGetData);
        chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

        ProgressProxy::enterEventLoop(pJob, i18nc("Mesage for progress dialog %1 = path to file", "Reading file: %1", mFileAccess->prettyAbsPath()));
        return m_bSuccess;
    }
    else
        return true;
}

void DefaultFileAccessJobHandler::slotGetData(KJob* pJob, const QByteArray& newData)
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

bool DefaultFileAccessJobHandler::put(const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume, qint32 permissions)
{
    ProgressProxyExtender pp; // Implicitly used in slotPercent()
    if(maxLength > 0)
    {
        KIO::TransferJob* pJob = KIO::put(mFileAccess->url(), permissions,
                                          KIO::HideProgressInfo | (bOverwrite ? KIO::Overwrite : KIO::DefaultFlags) | (bResume ? KIO::Resume : KIO::DefaultFlags));
        m_transferredBytes = 0;
        m_pTransferBuffer = (char*)pSrcBuffer;
        m_maxLength = maxLength;
        m_bSuccess = false;
        mFileAccess->setStatusText(QString());

        chk_connect(pJob, &KIO::TransferJob::result, this, &DefaultFileAccessJobHandler::slotPutJobResult);
        chk_connect(pJob, &KIO::TransferJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);
        chk_connect(pJob, &KIO::TransferJob::dataReq, this, &DefaultFileAccessJobHandler::slotPutData);
        chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));

        ProgressProxy::enterEventLoop(pJob, i18nc("Mesage for progress dialog %1 = path to file", "Writing file: %1", mFileAccess->prettyAbsPath()));
        return m_bSuccess;
    }
    else
        return true;
}

void DefaultFileAccessJobHandler::slotPutData(KIO::Job* pJob, QByteArray& data)
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
            The maxChunkSize must be able to fit a 32-bit qint32. Given that the fallowing is safe for qt5.
            Qt6 resolves this issue as it uses 64 bit sizes.
        */
        qint64 maxChunkSize = 100000;
        qint64 length = std::min(maxChunkSize, m_maxLength - m_transferredBytes);
        if(length > 0)
        {
            data.resize((QtSizeType)length);
            if(data.size() == (QtSizeType)length)
            {
                ::memcpy(data.data(), m_pTransferBuffer + m_transferredBytes, data.size());
                m_transferredBytes += length;
            }
        }
        else
        {
            KMessageBox::error(g_pProgressDialog, i18n("Out of memory"));
            data.resize(0);
            m_bSuccess = false;
        }
    }
}

void DefaultFileAccessJobHandler::slotPutJobResult(KJob* pJob)
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

bool DefaultFileAccessJobHandler::mkDirImp(const QString& dirName)
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
        chk_connect(pJob, &KIO::SimpleJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::SimpleJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob, i18nc("Mesage for progress dialog %1 = path to file", "Making folder: %1", dirName));
        return m_bSuccess;
    }
}

bool DefaultFileAccessJobHandler::rmDirImp(const QString& dirName)
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
        chk_connect(pJob, &KIO::SimpleJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::SimpleJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob, i18nc("Mesage for progress dialog %1 = path to file", "Removing folder: %1", dirName));
        return m_bSuccess;
    }
}

bool DefaultFileAccessJobHandler::removeFile(const QUrl& fileName)
{
    if(fileName.isEmpty())
        return false;
    else
    {
        m_bSuccess = false;
        KIO::SimpleJob* pJob = KIO::file_delete(fileName, KIO::HideProgressInfo);
        chk_connect(pJob, &KIO::SimpleJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::SimpleJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob, i18nc("Mesage for progress dialog %1 = path to file", "Removing file: %1", FileAccess::prettyAbsPath(fileName)));
        return m_bSuccess;
    }
}

bool DefaultFileAccessJobHandler::symLink(const QUrl& linkTarget, const QUrl& linkLocation)
{
    if(linkTarget.isEmpty() || linkLocation.isEmpty())
        return false;
    else
    {
        m_bSuccess = false;
        KIO::CopyJob* pJob = KIO::link(linkTarget, linkLocation, KIO::HideProgressInfo);
        chk_connect(pJob, &KIO::CopyJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, &KIO::CopyJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Creating symbolic link: %1 -> %2", FileAccess::prettyAbsPath(linkLocation), FileAccess::prettyAbsPath(linkTarget)));
        return m_bSuccess;
    }
}

bool DefaultFileAccessJobHandler::rename(const FileAccess& destFile)
{
    if(destFile.fileName().isEmpty())
        return false;

    if(mFileAccess->isLocal() && destFile.isLocal())
    {
        return QDir().rename(mFileAccess->absoluteFilePath(), destFile.absoluteFilePath());
    }
    else
    {
        ProgressProxyExtender pp;
        qint32 permissions = -1;
        m_bSuccess = false;
        KIO::FileCopyJob* pJob = KIO::file_move(mFileAccess->url(), destFile.url(), permissions, KIO::HideProgressInfo);
        chk_connect(pJob, &KIO::FileCopyJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
        chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));
        chk_connect(pJob, &KIO::FileCopyJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

        ProgressProxy::enterEventLoop(pJob,
                                      i18n("Renaming file: %1 -> %2", mFileAccess->prettyAbsPath(), destFile.prettyAbsPath()));
        return m_bSuccess;
    }
}

void DefaultFileAccessJobHandler::slotJobEnded(KJob* pJob)
{
    Q_UNUSED(pJob);

    ProgressProxy::exitEventLoop(); // Close the dialog, return from exec()
}

void DefaultFileAccessJobHandler::slotSimpleJobResult(KJob* pJob)
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
bool DefaultFileAccessJobHandler::copyFile(const QString& inDest)
{
    ProgressProxyExtender pp;
    FileAccess dest;
    dest.setFile(inDest);

    mFileAccess->setStatusText(QString());
    if(!mFileAccess->isNormal() || !dest.isNormal()) return false;

    qint32 permissions = (mFileAccess->isExecutable() ? 0111 : 0) + (mFileAccess->isWritable() ? 0222 : 0) + (mFileAccess->isReadable() ? 0444 : 0);
    m_bSuccess = false;
    KIO::FileCopyJob* pJob = KIO::file_copy(mFileAccess->url(), dest.url(), permissions, KIO::HideProgressInfo|KIO::Overwrite);
    chk_connect(pJob, &KIO::FileCopyJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
    chk_connect(pJob, SIGNAL(percent(KJob*,ulong)), &pp, SLOT(slotPercent(KJob*,ulong)));
    chk_connect(pJob, &KIO::FileCopyJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);

    ProgressProxy::enterEventLoop(pJob,
                                  i18n("Copying file: %1 -> %2", mFileAccess->prettyAbsPath(), dest.prettyAbsPath()));

    return m_bSuccess;
    // Note that the KIO-slave preserves the original date, if this is supported.
}

bool DefaultFileAccessJobHandler::listDir(DirectoryList* pDirList, bool bRecursive, bool bFindHidden, const QString& filePattern,
                                   const QString& fileAntiPattern, const QString& dirAntiPattern, bool bFollowDirLinks, IgnoreList& ignoreList)
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

    if(ProgressProxy::wasCancelled())
        return true; // Cancelled is not an error.

    ProgressProxy::setInformation(i18n("Reading folder: %1", mFileAccess->absoluteFilePath()), 0, false);
    qCInfo(kdiffFileAccess) << "Reading folder: " << mFileAccess->absoluteFilePath();

    if(mFileAccess->isLocal())
    {
        m_bSuccess = true;
        QDir dir(mFileAccess->absoluteFilePath());

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
                if(ProgressProxy::wasCancelled())
                    break;

                assert(fi.fileName() != "." && fi.fileName() != "..");

                FileAccess fa;

                fa.setFile(mFileAccess, fi);
                pDirList->push_back(fa);
            }
        }
    }
    else
    {
        KIO::ListJob* pListJob = nullptr;
        pListJob = KIO::listDir(mFileAccess->url(), KIO::HideProgressInfo, true /*bFindHidden*/);

        m_bSuccess = false;
        if(pListJob != nullptr)
        {
            chk_connect(pListJob, &KIO::ListJob::entries, this, &DefaultFileAccessJobHandler::slotListDirProcessNewEntries);
            chk_connect(pListJob, &KIO::ListJob::result, this, &DefaultFileAccessJobHandler::slotSimpleJobResult);
            chk_connect(pListJob, &KIO::ListJob::finished, this, &DefaultFileAccessJobHandler::slotJobEnded);
            chk_connect(pListJob, &KIO::ListJob::infoMessage, &pp, &ProgressProxyExtender::slotListDirInfoMessage);

            // This line makes the transfer via fish unreliable.:-(
            /*if(mFileAccess->url().scheme() != u8"fish"){
                chk_connect( pListJob, static_cast<void (KIO::ListJob::*)(KJob*,qint64)>(&KIO::ListJob::percent), &pp, &ProgressProxyExtender::slotPercent);
            }*/

            ProgressProxy::enterEventLoop(pListJob,
                                          i18n("Listing directory: %1", mFileAccess->prettyAbsPath()));
        }
    }

    ignoreList.enterDir(mFileAccess->absoluteFilePath(), *pDirList);
    mFileAccess->filterList(mFileAccess->absoluteFilePath(), pDirList, filePattern, fileAntiPattern, dirAntiPattern, ignoreList);

    if(bRecursive)
    {
        DirectoryList::iterator i;
        DirectoryList subDirsList;

        for(i = m_pDirList->begin(); i != m_pDirList->end(); ++i)
        {
            assert(i->isValid());
            if(i->isDir() && (!i->isSymLink() || m_bFollowDirLinks))
            {
                DirectoryList dirList;
                i->listDir(&dirList, bRecursive, bFindHidden,
                           filePattern, fileAntiPattern, dirAntiPattern, bFollowDirLinks, ignoreList);

                // append data onto the main list
                subDirsList.splice(subDirsList.end(), dirList);
            }
        }

        m_pDirList->splice(m_pDirList->end(), subDirsList);
    }

    return m_bSuccess;
}

void DefaultFileAccessJobHandler::slotListDirProcessNewEntries(KIO::Job*, const KIO::UDSEntryList& l)
{
    //This function is called for non-local urls. Don't use QUrl::fromLocalFile here as it does not handle these.
    for(const KIO::UDSEntry& e: l)
    {
        FileAccess fa;

        fa.setFromUdsEntry(e, mFileAccess);

        //must be manually filtered KDE does not supply API for ignoring these.
        if(fa.fileName() != "." && fa.fileName() != ".." && fa.isValid())
        {
            m_pDirList->push_back(std::move(fa));
        }
    }
}
