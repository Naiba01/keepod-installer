#include "cuimain.h"
#include "customitemdelegate.h"
#include "cnorchangedblocks.h"
#include "definitions.h"
#include "messages.h"

CUIMain::CUIMain(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    customizeUI();

    unetbootin::ubninitialize();

    getUsbDrivesList();
    refreshDiskTable();

    m_aInstallTasks = NULL;
}


// XXX
void CUIMain::customizeUI()
{
    // define the table model.
    QStandardItemModel *model = new QStandardItemModel(0,4,this);

    model->setHorizontalHeaderItem(TABLE_COLUMN_CHECK, new QStandardItem(QString("")));
    model->setHorizontalHeaderItem(TABLE_COLUMN_LABEL, new QStandardItem(QString("Drives")));
    model->setHorizontalHeaderItem(TABLE_COLUMN_PROGRESS, new QStandardItem(QString("Progress")));
    model->setHorizontalHeaderItem(TABLE_COLUMN_MOREINFO, new QStandardItem(QString("")));

    tblDevices->setModel(model);

    CustomItemDelegate *itemDelegate = new CustomItemDelegate(tblDevices);
    tblDevices->setItemDelegateForColumn(TABLE_COLUMN_PROGRESS, itemDelegate);

    // set attributes of headers
    QHeaderView *hzHeader = tblDevices->horizontalHeader();

#if QTCORE_VERSION>4
    hzHeader->setSectionResizeMode(QHeaderView::Fixed);
    hzHeader->setSectionsClickable(false);
    hzHeader->setSectionsMovable(false);
#else
    hzHeader->setResizeMode(QHeaderView::Fixed);
    hzHeader->setClickable(false);
    hzHeader->setMovable(false);
#endif

    hzHeader->resizeSection(TABLE_COLUMN_CHECK, 25);
    hzHeader->resizeSection(TABLE_COLUMN_LABEL, 120);
    hzHeader->resizeSection(TABLE_COLUMN_PROGRESS, 200);
    hzHeader->resizeSection(TABLE_COLUMN_MOREINFO, 24);

    QHeaderView *vtHeader = tblDevices->verticalHeader();
//    vtHeader->setShown(false);
    vtHeader->setHidden(true);

    tblDevices->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tblDevices->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // now allow user to edit cell
    tblDevices->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // make the label as a hyperlink
    lblDownloadUrl->setText("<a href=\"http://repos.keepod.org/installer-latest/\">(http://repos.keepod.org/installer-latest)</a>");
    lblDownloadUrl->setTextFormat(Qt::RichText);
    lblDownloadUrl->setTextInteractionFlags(Qt::TextBrowserInteraction);
    lblDownloadUrl->setOpenExternalLinks(true);

    setWindowTitle(KEEPOD_INSTALLER_TITLE);
}


void CUIMain::refreshDiskTable ()
{
    int diskCount = m_aUsbDiskInfos.size();
    if ( diskCount == 0 ) {
        return;
    }

    QStandardItemModel *model = (QStandardItemModel*)tblDevices->model();
    for ( int i=0; i<diskCount; i++ ) {
        CUsbDiskInfo *adisk = m_aUsbDiskInfos.at(i);

        model->insertRow(i, new QStandardItem(""));

        QStandardItem *checkItem = new QStandardItem(true);
        checkItem->setCheckable(true);
        model->setItem(i, TABLE_COLUMN_CHECK, checkItem);

        model->setItem(i, TABLE_COLUMN_LABEL, new QStandardItem(adisk->getShowableName()));

        model->setItem(i, TABLE_COLUMN_PROGRESS, new QStandardItem(""));

        QStandardItem *moreInfoItem = new QStandardItem(false);
        moreInfoItem->setIcon(QIcon(":/images/images/info_small.png"));
        model->setItem(i, TABLE_COLUMN_MOREINFO, moreInfoItem);
    }
}

void CUIMain::getUsbDrivesList()
{
    m_aUsbDiskInfos.clear();

    QStringList driveslist = unetbootin::listcurdrives();

    if ( driveslist.size() == 0 ) {
        QMessageBox::warning(this, KEEPOD_INSTALLER_TITLE, MSG_NO_USB_DISKS);
        exit (0);
    }

    // Remove drives which have same disk name, i.e. '/dev/sdd1' & '/dev/sdd2'
/*    for ( int i=0; i<driveslist.size(); i++ ) {
        QString aname = driveslist.at(i);

        for ( int j=0; j<driveslist.size(); j++) {

        }
    }*/

//    QString prevPDevName("");
    for (int i = 0; i < driveslist.size(); ++i)
    {
        CUsbDiskInfo *adisk = new CUsbDiskInfo();
        QString newDevName = driveslist.at(i);

        adisk->setInfoByDevname ( newDevName );
//        adisk->setLabel(unetbootin::getlabel( newDevName ));
//        adisk->setUuid(unetbootin::getuuid( newDevName ));

//        if ( prevPDevName == adisk->getParentDevname() ) {
//            continue;
//        }

//        prevPDevName = adisk->getParentDevname();

        m_aUsbDiskInfos.append(adisk);
    }
}

void CUIMain::on_btnBrowse_clicked()
{
    QString nameFloppy = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Open Keepod ISO File"), QDir::homePath(), tr("Update patch") + " (*.zip);;"));

    edtISOPath->clear();
    edtISOPath->insert(nameFloppy);
}

// XXX
QList<int> CUIMain::getSelectedIds ()
{
    QList<int> result;

    QStandardItemModel *model = (QStandardItemModel*) tblDevices->model();
    int itemcount = model->rowCount();

    for ( int i=0; i<itemcount; i++ ) {
        bool checked = (model->item(i, TABLE_COLUMN_CHECK)->checkState() == Qt::Checked);

        if ( checked == true ) {
            result.append(i);
        }
    }

    return result;
}

bool CUIMain::runInstallTasks()
{
    QList<int> selIds = getSelectedIds();
    int selcount = selIds.size();
    int i;

    m_bDownloadLatest = (chkDownloadLatest->checkState() == Qt::Checked);
    QString szIsoUrl = edtISOPath->text();
    bool ret = false;

    m_szIsoPath = "";

    QString isotmpf = unetbootin::ubntmpf + LATEST_VERSION_URL_NAME;
    QString md5tmpf = unetbootin::ubntmpf + LATEST_VERSION_MD5_NAME;
//    QString isotmpf = randtmpfile::getrandfilename(unetbootin::ubntmpf, "iso");

    m_statusManager.setStatus(STAT_BASE, lblStatus);
    // Set initial progress
    for ( i=0; i<selcount; i++ ) {
        onProgressUpdate ( selIds.at(i), PRG_INIT );
    }

    QDir::setCurrent(unetbootin::ubntmpf);
    QString md5cmdparam = QString("-c ") + LATEST_VERSION_MD5_NAME;
    QString cmdres;
    // Try downloading
    if ( m_bDownloadLatest == true ) {
        m_statusManager.setStatus(STAT_DOWNLOADING, lblStatus);

        // download the md5 file first.
        ret = unetbootin::downloadfile ( LATEST_VERSION_MD5, md5tmpf, 0, this );
        if ( ret == false ) {
            m_statusManager.setStatus(ERR_DOWNLOAD_FAILED, lblStatus);
            return false;
        }

        cmdres = unetbootin::callexternapp("md5sum", md5cmdparam);

        if ( cmdres.contains("OK") == FALSE ) {
            ret = unetbootin::downloadfile ( LATEST_VERSION_URL, isotmpf, 0, this );
            if ( ret == false ) {
                if ( unetbootin::s_dlewait != NULL ) {
                    m_statusManager.setStatus(ERR_DOWNLOAD_FAILED, lblStatus);
                } else {
                    // if the operation was cancelled by the user,
                    m_statusManager.setStatus(ERR_CANCELED, lblStatus);
                    SHOW_MESSAGE ( KEEPOD_INSTALLER_TITLE, MSG_CANCELED );
                }
                return false;
            }
        }

        m_szIsoPath = isotmpf;
    } else {
        if ( szIsoUrl.startsWith("http://") || szIsoUrl.startsWith("ftp://") ) {
            ret = unetbootin::downloadfile ( szIsoUrl, isotmpf );
            if ( ret == false ) {
                QString szNewMsg = MSG_DOWNLOADING_USER;

                szNewMsg.arg(szIsoUrl);
                m_statusManager.setStatus(ERR_DOWNLOAD_FAILED, lblStatus, szNewMsg);
                return false;
            }

            m_szIsoPath = isotmpf;
        } else {
            m_szIsoPath = szIsoUrl;
        }
    }

    for ( i=0; i<selcount; i++ ) {
        int diskId = selIds.at(i);
        onProgressUpdate ( diskId, PRG_ISO_PREPARED );
    }



    if ( m_szIsoPath.compare(szIsoUrl) != 0 ) {
        // if the iso file is a downloaded one, check md5sum
        // check for md5 checksum before download

        m_statusManager.setStatus(STAT_CHECKING_MD5SUM, lblStatus);

        // check for md5 checksum again after download.
        cmdres = unetbootin::callexternapp("md5sum", md5cmdparam);
        if ( cmdres.contains("OK") == FALSE ) {
            m_statusManager.setStatus(ERR_MD5_MISMATCH, lblStatus);
            return false;
        }
    }

    for ( i=0; i<selcount; i++ ) {
        int diskId = selIds.at(i);
        onProgressUpdate ( diskId, PRG_DRIVE_PREPARED );
    }



    m_statusManager.setStatus(STAT_EXTRACTING, lblStatus);

    CNorChangedBlocks::prepareClone(m_szIsoPath.toLocal8Bit().data());

    for ( i=0; i<selcount; i++ ) {
        int diskId = selIds.at(i);
        onProgressUpdate ( diskId, PRG_ISO_EXTRACTED );
    }



    m_statusManager.setStatus(STAT_WRITING, lblStatus);

    // Once the keepod iso file prepared, begin to install.
    m_aInstallTasks = (unetbootin**) malloc(sizeof(unetbootin)*selcount);
    m_nRunningTaskCount = selcount;
    for ( i=0; i<selcount; i++ ) {
        int diskId = selIds.at(i);

        // prepare the task info
        InstallTaskInfo *taskInfo = new InstallTaskInfo();

        taskInfo->diskIdOnUI = diskId;
        taskInfo->isoPath = m_szIsoPath;
        taskInfo->disk = m_aUsbDiskInfos.at(diskId);

        // run a installation task
        m_aInstallTasks[i] = new unetbootin(taskInfo);

        connect (m_aInstallTasks[i], SIGNAL(progress(int,int)), this, SLOT(onProgressUpdate(int,int)));
        connect (m_aInstallTasks[i], SIGNAL(failed(int,int)), this, SLOT(onTaskFailed(int,int)));

        connect (m_aInstallTasks[i], SIGNAL(finished()), this, SLOT(onThreadFinished()));
        connect (m_aInstallTasks[i], SIGNAL(terminated()), this, SLOT(onThreadTerminated()));
        m_aInstallTasks[i]->start();
    }

    return true;
}

void CUIMain::setMode ( bool bActive )
{
    chkDownloadLatest->setEnabled ( bActive );
    edtISOPath->setEnabled ( bActive );
    btnBrowse->setEnabled ( bActive );
    tblDevices->setEnabled ( bActive );

    btnStart->setEnabled(true);
    if ( bActive == true ) {
        btnStart->setText(tr("Make it!"));
    } else {
        btnStart->setText(tr("Cancel"));
    }
}

void CUIMain::closeEvent (QCloseEvent *event)
{
    if ( btnStart->text() == tr("Cancel") ) {
        QMessageBox msgb;

        msgb.setIcon(QMessageBox::Question);
        msgb.setWindowTitle(KEEPOD_INSTALLER_TITLE);
        if ( m_nRunningTaskCount == 0 ) {
            msgb.setText(MSG_CONFIRM_EXIT);
        } else {
            msgb.setText(MSG_CONFIRM_EXIT2);
        }
        msgb.setStandardButtons(QMessageBox::No|QMessageBox::Yes);

        if ( msgb.exec() == QMessageBox::Yes ) {
            cancelTasks();
        }

        event->ignore();
    }
}

void CUIMain::on_btnStart_clicked()
{
#ifdef OLD_VERSION
    // Check if grub-0.97 installed
    if ( unetbootin::checkGrub1Installed() == false ) {
        QMessageBox msgbGrub;

        msgbGrub.setIcon(QMessageBox::Question);
        msgbGrub.setWindowTitle(KEEPOD_INSTALLER_TITLE);
        msgbGrub.setText(MSG_INSTALL_GRUB);
        msgbGrub.setStandardButtons(QMessageBox::No|QMessageBox::Yes);

        if ( msgbGrub.exec() == QMessageBox::Yes ) {
            m_statusManager.setStatus(STAT_INSTALLING_GRUB, lblStatus);

            setMode(false);
            unetbootin::callexternapp("apt-get", "install grub");
            setMode(true);
        } else {
            return;
        }
    }
#endif

    // Confirm format
    if ( btnStart->text() == tr("Cancel") ) {
        QMessageBox msgb;

        msgb.setIcon(QMessageBox::Question);
        msgb.setWindowTitle(KEEPOD_INSTALLER_TITLE);
        msgb.setText(MSG_CONFIRM_CANCEL);
        msgb.setStandardButtons(QMessageBox::No|QMessageBox::Yes);

        if ( msgb.exec() == QMessageBox::Yes ) {
            cancelTasks();
        }
    } else {
        // Check if the given path is correct.
        m_bDownloadLatest = (chkDownloadLatest->checkState() == Qt::Checked);
        m_szIsoPath = edtISOPath->text();
        if ( m_bDownloadLatest == false ) {
            if ( m_szIsoPath.isEmpty() ) {
                SHOW_MESSAGE (KEEPOD_INSTALLER_TITLE, MSG_NO_SOURCE_ISO);
                return;
            }

            if ( !QFile::exists(m_szIsoPath) && !m_szIsoPath.startsWith("http://") && !m_szIsoPath.startsWith("ftp://") ) {
                SHOW_MESSAGE (KEEPOD_INSTALLER_TITLE, MSG_FILE_NOT_FOUND+QString(": ")+m_szIsoPath);
                return;
            }
        }

        // Check if there are selected devices.
        int selcount = getSelectedIds().size();
        if ( selcount == 0 ) {
            SHOW_MESSAGE (KEEPOD_INSTALLER_TITLE, MSG_NO_SELECTED_DEVICE);
            return;
        }

        // Confirm format disk
        QMessageBox msgb;

        msgb.setIcon(QMessageBox::Question);
        msgb.setWindowTitle(KEEPOD_INSTALLER_TITLE);
        msgb.setText(MSG_CONFIRM_BUILD);
        msgb.setStandardButtons(QMessageBox::No|QMessageBox::Yes);

        if ( msgb.exec() == QMessageBox::No ) {
            return;
        }

        setMode ( false );

        if ( runInstallTasks() == false ) {
            // if an error occurs before writing into the disk.
            setMode( true );
        }
    }
}

QString CUIMain::displayfisize(quint64 fisize)
{
    if (fisize < 10000)
    {
        return QString("%1 B").arg(fisize);
    }
    else if (fisize < 10240000)
    {
        return QString("%1 KB").arg(fisize / 1024);
    }
    else
    {
        return QString("%1 MB").arg(fisize / 1048576);
    }
}

void CUIMain::on_tblDevices_clicked(const QModelIndex &index)
{
    int row = index.row();
    int column = index.column();

    if ( column == TABLE_COLUMN_MOREINFO ) {
        QMessageBox::information(this, KEEPOD_INSTALLER_TITLE, m_aUsbDiskInfos.at(row)->getMoreInfoString());
    }
}

void CUIMain::on_chkDownloadLatest_stateChanged(int arg1)
{
    /*if ( arg1 == Qt::Checked ) {
        isoPath = edtISOPath->text();
        edtISOPath->setText(LATEST_VERSION_URL);
    } else {
        edtISOPath->setText(isoPath);
    }*/
    edtISOPath->setEnabled(arg1==Qt::Unchecked);
    btnBrowse->setEnabled(arg1==Qt::Unchecked);
}

void CUIMain::onThreadFinished()
{
    if ( m_nRunningTaskCount > 0 ) {
        m_nRunningTaskCount --;
    }

    if ( m_nRunningTaskCount <= 0 ) {
        CNorChangedBlocks::finalize();

        if ( btnStart->isEnabled() ) {
            m_statusManager.setStatus(STAT_DONE, lblStatus);
            SHOW_MESSAGE ( KEEPOD_INSTALLER_TITLE, MSG_COMPLETED );

            exit ( 0 );
        } else {
            m_statusManager.setStatus(ERR_CANCELED, lblStatus);
            SHOW_MESSAGE ( KEEPOD_INSTALLER_TITLE, MSG_CANCELED );
        }

        setMode ( true );
    }
}

void CUIMain::onThreadTerminated()
{
}

void CUIMain::onProgressUpdate(int itemId, int prg)
{
    ////qDebug() << "Task " << itemId << " : " << prg;
    QStandardItemModel *model = (QStandardItemModel*) tblDevices->model();

    ////QStandardItem *prgItem = model->item(itemId, TABLE_COLUMN_PROGRESS);
    ////model->setData(prgItem->index(), progress);
    model->setData(model->index(itemId,TABLE_COLUMN_PROGRESS), QVariant(prg));


    ////prgItem->setText(QString::number(progress));
}

void CUIMain::onTaskFailed ( int itemId, int err )
{
    m_statusManager.setStatus((ENStatus)err, lblStatus);
}

void CUIMain::dlprogressupdate(int dlbytes, int maxbytes)
{
    QList<int> selIds = getSelectedIds();
    int selcount = selIds.size();

    QTime time = QTime::currentTime();
    static int oldsec = 0;
    // refresh the progress bar every second
    if(oldsec != time.second())
    {
      oldsec = time.second();
      int newprg = PRG_INIT+(float)(PRG_ISO_PREPARED-PRG_INIT)*dlbytes/maxbytes;

      // Set initial progress
      for ( int i=0; i<selcount; i++ ) {
          onProgressUpdate ( selIds.at(i), newprg );
      }

      // Show the download progress as percentage.
      QString szMsg;
      szMsg.sprintf("%s %.2f%%", MSG_DOWNLOADING, (float)dlbytes/maxbytes*100.0);
      m_statusManager.setStatus(STAT_DOWNLOADING, lblStatus, szMsg);

      //  tprogress->setValue(dlbytes);
      //  tprogress->setMaximum(maxbytes);
      // display the downloaded size with suffix
      //  pdesc1->setText(tr("<b>Downloaded:</b> %1 of %2").arg(displayfisize(dlbytes)).arg(displayfisize(maxbytes)));
    }
}

void CUIMain::dlprogressupdate64(qint64 dlbytes, qint64 maxbytes)
{
    QList<int> selIds = getSelectedIds();
    int selcount = selIds.size();

    QTime time = QTime::currentTime();
    static int oldsec = 0;
    // refresh the progress bar every second
    if(oldsec != time.second())
    {
      oldsec = time.second();
      int newprg = PRG_INIT+(float)(PRG_ISO_PREPARED-PRG_INIT)*dlbytes/maxbytes;

      // Set initial progress
      for ( int i=0; i<selcount; i++ ) {
          onProgressUpdate ( selIds.at(i), newprg );
      }

      // Show the download progress as percentage.
      QString szMsg;
      szMsg.sprintf("%s %.2f%%", MSG_DOWNLOADING, (float)dlbytes/maxbytes*100.0);
      m_statusManager.setStatus(STAT_DOWNLOADING, lblStatus, szMsg);

        //tprogress->setValue(dlbytes);
        //tprogress->setMaximum(maxbytes);
      // display the downloaded size with suffix
        //pdesc1->setText(tr("<b>Downloaded:</b> %1 of %2").arg(displayfisize(dlbytes)).arg(displayfisize(maxbytes)));
    }
}

void CUIMain::cancelTasks()
{
    btnStart->setEnabled(false);

    if ( m_aInstallTasks != NULL ) {
        QList<int> selIds = getSelectedIds();
        int selcount = selIds.size();

        for ( int i=0; i<selcount; i++ ) {
            // m_aInstallTasks[i]->quit();
            m_aInstallTasks[i]->m_bStopFlag = true;
            m_aInstallTasks[i]->wait(2000);
        }
    } else {
        if ( unetbootin::s_isftp == true ) {
            if ( unetbootin::s_dlftp != NULL ) {
                unetbootin::s_dlftp->disconnect();
                unetbootin::s_dlftp->close();
                unetbootin::s_dlftp->abort();
                unetbootin::s_dlftp = NULL;
            }
        } else {
            if ( unetbootin::s_dlhttp != NULL ) {
                unetbootin::s_dlhttp->disconnect();
                unetbootin::s_dlhttp->close();
                unetbootin::s_dlhttp->abort();
                unetbootin::s_dlhttp = NULL;
            }
        }

        if ( unetbootin::s_dlewait != NULL ) {
            unetbootin::s_dlewait->quit();
            unetbootin::s_dlewait = NULL;
        }
    }
}

