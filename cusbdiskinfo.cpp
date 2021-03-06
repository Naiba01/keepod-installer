#include "cusbdiskinfo.h"
#include "unetbootin.h"

#include <QStringList>

#ifdef Q_OS_WIN
    #include <windows.h>
    typedef BOOL (WINAPI *P_GDFSE)(LPCTSTR, PULARGE_INTEGER,
                               PULARGE_INTEGER, PULARGE_INTEGER);
#endif

CUsbDiskInfo::CUsbDiskInfo(QObject *parent) :
    QObject(parent)
{
    m_llSizeKb = 0L;
}

bool CUsbDiskInfo::umount () {
    for ( int i=1; i<=256; i++ ) {
        QString szPartition = m_szDevName + QString::number(i);

        if ( QFile::exists(szPartition) == false ) {
            break;
        }

        QString szResult = QString(unetbootin::callexternapp("umount",szPartition));

        if ( szResult.contains("fail") ) {
            return false;
        }
    }

    m_szMountPoint = "";

    return true;
}

#ifdef OLD_VERSION
#define FDISK_CMD_REPARTITION       "d\n1\nd\n2\nd\n3\nd\n4\nn\np\n1\n\n+1024M\nn\np\n2\n\n\nt\n1\nc\na\n1\nw"
bool CUsbDiskInfo::keepodFormat ()
{
    QString pdevname = getParentDevname();
    QString cmdline;

    cmdline = "fdisk " + pdevname;

    FILE *fp = popen(cmdline.toAscii(), "w");
    fprintf (fp, FDISK_CMD_REPARTITION);
    pclose(fp);

    unetbootin::callexternapp("sync", "");

    unetbootin::callexternapp("mkfs.vfat", pdevname + "1 -n \"Keepod_Boot\"");
    unetbootin::callexternapp("mkfs.ext3", pdevname+"2 -L \"Keepod_Root\"");

    unetbootin::callexternapp("sync", "");

    m_szDevName = pdevname+"2";

    return true;
}

bool CUsbDiskInfo::writeGrub () {
    if ( m_szMountPoint.isEmpty() ) {
        m_szMountPoint = getNewMountPoint();
        unetbootin::callexternapp("mount", m_szDevName+QString(" ")+m_szMountPoint);
    }

    unetbootin::callexternapp("grub-install", "--force-lba --recheck --no-floppy --root-directory="
                              + m_szMountPoint + QString(" ") + getParentDevname());


    QFile file(m_szMountPoint+"/boot/grub/grub.conf");
    file.open(QIODevice::WriteOnly|QIODevice::Text);
    QTextStream out(&file);

    out << "default=0\n"
        << "timeout=3\n"
        << "splashimage=/grub/android-x86.xpm.gz\n"
        << "hiddenmenu\n"
        << "\n"
        << "title Keepod\n"
        << "\troot (hd0,1)\n"
        << "\tkernel /android/kernel quiet root=/dev/ram0 androidboot.hardware=android_x86 acpi_sleep=s3_bios,s3_mode video=-16 SRC=/android\n"
        << "\tinitrd /android/initrd.img\n"
        << "\n"
        << "title Keepod (Debug mode)\n"
        << "\troot (hd0,1)\n"
        << "\tkernel /android/kernel root=/dev/ram0 androidboot.hardware=android_x86 acpi_sleep=s3_bios,s3_mode video=-16 DEBUG=2 SRC=/android\n"
        << "\tinitrd /android/initrd.img\n"
        << "\n"
        << "title Keepod (Debug nomodeset)\n"
        << "\troot (hd0,1)\n"
        << "\tkernel /android/kernel nomodeset root=/dev/ram0 androidboot.hardware=android_x86 video=-16 DEBUG=2 SRC=/android\n"
        << "\tinitrd /android/initrd.img\n"
        << "\n"
        << "title Keepod (Debug video=LVDS-1:d)\n"
        << "\troot (hd0,1)\n"
        << "\tkernel /android/kernel video=LVDS-1:d root=/dev/ram0 androidboot.hardware=android_x86 video=-16 DEBUG=2 SRC=/android\n"
        << "\tinitrd /android/initrd.img\n";

    file.close();

    unetbootin::callexternapp("sync", "");
    unetbootin::callexternapp("umount", m_szMountPoint);
    unetbootin::callexternapp("rm", "-rf "+m_szMountPoint.toLocal8Bit());

    return true;
}
#endif

QString CUsbDiskInfo::mount () {
    QString mntpoint = getNewMountPoint();

    unetbootin::callexternapp("mkdir", "-p "+mntpoint);
    unetbootin::callexternapp("mount", m_szDevName+QString(" ")+mntpoint);

    m_szMountPoint = mntpoint;

    return mntpoint;
}

QString CUsbDiskInfo::getShowableName () {
#if 0

    QString newLabel;

    if ( m_szLabel.isEmpty() ) {
        newLabel = "No Label";
    } else if ( m_szLabel.length() > 12 ) {
        newLabel = m_szLabel.left(8) + "..." + m_szLabel.right(4);
    } else {
        newLabel = m_szLabel;
    }

    return newLabel + QString(" (") + m_szDevName + QString(")");

#else

    return m_szDevName;

#endif
}

QString CUsbDiskInfo::getDevNodeName () {
    return m_szDevName.section('/', 2, 2);
}

QString CUsbDiskInfo::getSizeString () {
    QString  result;
    QStringList sizes;

    sizes << "KB" << "MB" << "GB";

    double len = (double)m_llSizeKb;
    int order = 0;
    while (len >= 1024 && order + 1 < sizes.size()) {
        order++;
        len = len/1024;
    }

    // Adjust the format string to your preferences. For example "{0:0.#}{1}" would
    // show a single decimal place, and no space.
    result.sprintf("%.1f %s", len, sizes.at(order).toLocal8Bit().data());

    return result;
}

QString CUsbDiskInfo::getMoreInfoString () {
#ifdef OLD_VERSION
    return QString("UUID: ") + m_szUuid + QString(", FileSystem: ") + m_szFileSystem + QString(", Size: ") + getSizeString();
#else
    return QString("Size: ") + getSizeString();
#endif
}

void CUsbDiskInfo::setInfoByDevname ( QString devname )
{
    this->m_szDevName = devname;
    //this->label = getLabelFromDevname();
    //this->m_szFileSystem = getFstypeFromDevname();
#ifdef OLD_VERSION
    this->m_llSizeKb = getSizeKbFromDevname();
#else
    this->m_llSizeKb = getBlockSizeKb(devname/*getDevNodeName()*/);
#endif
    //this->m_szModel = getModelFromDevname();
    //this->uuid = getUuidFromDevname();
}

void CUsbDiskInfo::setLabel ( QString label )
{
    this->m_szLabel = label;
}

void CUsbDiskInfo::setUuid ( QString uuid )
{
    this->m_szUuid = uuid;
}

#define KEY_LABEL       "ID_FS_LABEL_ENC="
QString CUsbDiskInfo::getLabelFromDevname ()
{
    return CUsbDiskInfo::getBlkidValue(KEY_LABEL);
}

#define KEY_SIZE_SECTOR       "ID_PART_ENTRY_SIZE="
quint64 CUsbDiskInfo::getSizeKbFromDevname ()
{
    return CUsbDiskInfo::getBlkidValue(KEY_SIZE_SECTOR).toLongLong()/2;
}

#define KEY_MODEL           ""
QString CUsbDiskInfo::getModelFromDevname ()
{
    return QString("");
}

#define KEY_FSTYPE          "ID_FS_TYPE="
QString CUsbDiskInfo::getFstypeFromDevname ()
{
    return CUsbDiskInfo::getBlkidValue(KEY_FSTYPE);
}

#define KEY_UUID            "ID_FS_UUID_ENC="
QString CUsbDiskInfo::getUuidFromDevname ()
{
    return CUsbDiskInfo::getBlkidValue(KEY_UUID);
}

QString CUsbDiskInfo::getParentDevname ()
{
    int i;
    int dnlen = m_szDevName.length();

    for ( i=dnlen-1; i>=7; i-- ) { // 5 = strlen("/dev/sd*")
        if (m_szDevName.at(i).isDigit() == false) {
            break;
        }
    }

    return m_szDevName.left(i+1);
}

QString CUsbDiskInfo::getBlkidValue ( QString key )
{
    QString devinfostr = QString(unetbootin::callexternapp("blkid","-o udev -p "+m_szDevName));
    int a = devinfostr.indexOf(key);
    int b = devinfostr.indexOf("\n", a);

    return devinfostr.mid(a+key.length(),b-a-key.length());
}

QString CUsbDiskInfo::getNewMountPoint()
{
    return QString("/mnt/kpd") + getDevNodeName() + QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
}

// i_szDev: "sdc"
quint64 CUsbDiskInfo::getBlockSizeKb ( QString i_szDev )
{
    quint64 llSizeKb = 0;

#ifdef Q_OS_WIN
    // note: under windows i_szDev format "C:\";
//    quint64 nFreeBytesAvailable = 0;
//    quint64 nTotalNumberOfBytes = 0;
//    quint64 nTotalNumberOfFreeBytes = 0;
//    if(GetDiskFreeSpaceEx(LPWSTR(i_szDev.utf16()),
//                          (PULARGE_INTEGER) &nFreeBytesAvailable,
//                          (PULARGE_INTEGER) &nTotalNumberOfBytes,
//                          (PULARGE_INTEGER) &nTotalNumberOfFreeBytes))
//    {
//        llSizeKb = nTotalNumberOfBytes;
//    }

    BOOL  fResult;

    DWORD dwSectPerClust = 0,
            dwBytesPerSect = 0,
            dwFreeClusters = 0,
            dwTotalClusters = 0;

    P_GDFSE pGetDiskFreeSpaceEx = NULL;

    unsigned __int64 i64FreeBytesToCaller = 0,
            i64TotalBytes = 0,
            i64FreeBytes = 0;

    pGetDiskFreeSpaceEx = (P_GDFSE)GetProcAddress (
                GetModuleHandle (LPWSTR("kernel32.dll")),
                "GetDiskFreeSpaceExA");

    if (pGetDiskFreeSpaceEx)
    {
        fResult = pGetDiskFreeSpaceEx (LPWSTR(i_szDev.utf16()),
                                       (PULARGE_INTEGER)&i64FreeBytesToCaller,
                                       (PULARGE_INTEGER)&i64TotalBytes,
                                       (PULARGE_INTEGER)&i64FreeBytes);
        if (fResult)
        {
            printf ("\n\nGetDiskFreeSpaceEx reports\n\n");
            printf ("Available space to caller = %I64u MB\n",
                    i64FreeBytesToCaller / (1024*1024));
            printf ("Total space               = %I64u MB\n",
                    i64TotalBytes / (1024*1024));
            printf ("Free space on drive       = %I64u MB\n",
                    i64FreeBytes / (1024*1024));
        }
    }
    else
    {
        fResult = GetDiskFreeSpace (LPWSTR(i_szDev.utf16()),
                                    &dwSectPerClust,
                                    &dwBytesPerSect,
                                    &dwFreeClusters,
                                    &dwTotalClusters);
        if (fResult)
        {
            /* force 64-bit math */
            i64TotalBytes = (__int64)dwTotalClusters * dwSectPerClust *
                    dwBytesPerSect;
            i64FreeBytes = (__int64)dwFreeClusters * dwSectPerClust *
                    dwBytesPerSect;

            printf ("GetDiskFreeSpace reports\n\n");
            printf ("Free space  = %I64u MB\n",
                    i64FreeBytes / (1024*1024));
            printf ("Total space = %I64u MB\n",
                    i64TotalBytes / (1024*1024));
        }
    }

    llSizeKb = i64TotalBytes;

    if (!fResult)
    {
        qDebug()<<GetLastError()<<"error: could not get free space for " << i_szDev;
    }

#endif

#ifdef Q_OS_LINUX
    QString szCmdOut = QString(unetbootin::callexternapp("cat","/proc/partitions"));
    QStringList szLines = szCmdOut.split("\n");

    for ( int i=0; i<szLines.size(); i++ ) {
        QString szALine = szLines.at(i);
        szALine = szALine.simplified();

        QStringList szItems = szALine.split(" ");
        if ( szItems.size() != 4 ) {
            continue;
        }

        QString szDevName = szItems.at(3);
        if ( szDevName == i_szDev ) {
            llSizeKb = szItems.at(2).toLongLong();
            break;
        }
    }
#endif

    return llSizeKb;
}
