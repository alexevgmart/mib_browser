#ifndef FORM_H
#define FORM_H

#include <QWidget>
#include <QtWidgets>

#include "snmp.h"
#include "indexes.h"
#include "snmptrap_log.h"
#include "snmp_request.h"

#include "../net-snmp-5.7.3/include/net-snmp/net-snmp-config.h"
#include "../net-snmp-5.7.3/include/net-snmp/net-snmp-includes.h"
#include "../net-snmp-5.7.3/include/net-snmp/utilities.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/large_fd_set.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/mib.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/snmp.h"


#ifndef SNMP_SEC_LEVEL_NOAUTH
#define SNMP_SEC_LEVEL_NOAUTH (0x00)
#endif

#ifndef SNMP_SEC_LEVEL_AUTHNOPRIV
#define SNMP_SEC_LEVEL_AUTHNOPRIV (0x02)
#endif

#ifndef SNMP_SEC_LEVEL_AUTHPRIV
#define SNMP_SEC_LEVEL_AUTHPRIV (0x03)
#endif

//#define SYSLOG_V1_STANDARD_FORMAT      "%a: %W Trap (%q) Uptime: %#T%#v\n"
#define SYSLOG_V1_STANDARD_FORMAT      "%#v\n"
//#define SYSLOG_V1_ENTERPRISE_FORMAT    "%a: %W Trap (%q) Uptime: %#T%#v\n" /* XXX - (%q) become (.N) ??? */
//#define SYSLOG_V23_NOTIFICATION_FORMAT "%[%b]: Trap %#v\n"
#define SYSLOG_V23_NOTIFICATION_FORMAT "%#v\n"
//#define PRINT_V23_NOTIFICATION_FORMAT "%.4y-%.2m-%.2l %.2h:%.2j:%.2k %B [%b]:\n%v\n"

#include <iostream>

namespace Ui {
class Form;
}

class Form : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief The Asn1DataType enum
     * Определение типа данных ASN.1
     */
    enum Asn1DataType : quint8 {
        Unknown         = 0,
        Integer         = 0x02,
        OctetString     = 0x04,
        Null            = 0x05,
        Oid             = 0x06,
        Sequence        = 0x30,
        IpAddress       = 0x40,
        Counter         = 0x41,
        Gauge           = 0x42,
        TimeTicks       = 0x43,
        Opaque          = 0x44,
        Counter64       = 0x46,
        GetRequest      = 0xA0, // 0xa2 == 10 1 00010  Это означает что переменная имеет класс: Context-specific, тип Constructed и тег GetResponse
        GetNextRequest  = 0xA1,
        GetResponse     = 0xA2,
        SetRequest      = 0xA3,
        V1Trap          = 0xA4,
        GetBulkRequest  = 0xA5,
        Inform          = 0xA6,
        V2Trap          = 0xA7,
        Report          = 0xA8
    };

    explicit Form(QWidget *parent = nullptr);
    ~Form();
    void itemInfoFull();

private slots:
    void on_paramSnmp_clicked();
    void on_selectOid_clicked();
    void on_cancelOid_clicked();
    void on_requestGet_clicked();
    void on_requestGetNext_clicked();
    void on_treeSearchInput_returnPressed();
    void on_indexesButton_clicked();

public slots:
    void cancelSnmpWindow();
    void okSnmpWindow(int, QString, QString, QString, QString, QString, QString, QString, QString);
    void itemInfo(QTreeWidgetItem* item);
    void searchTextChanged();
    void searchInfoMibTree(QTreeWidgetItem*, QString, QColor);
    void appendChildrenMibTree(struct tree*, QTreeWidgetItem*, QString, QString, QString);
    void infoMibTree(QTreeWidgetItem*);
    void* netsnmp_sessp_conf(char*, char*, int);
    QString description(oid *, size_t, int);
    void sendTableCopiedOid(QString);
    void getRequestResult(QString, bool);

signals:
    void sendSelectedOid(QString);
    void sendTable(QString, QString, QString, QStringList);

private:
    Ui::Form *ui;
    Snmp snmp;
    Indexes indexes;
    QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    bool indexesStatus = true;
    int searchCount = 0;
    QString searchText;
    QList<QTreeWidgetItem*> searchList;
    QStringList wantedString = QStringList() << "FROM" << "MAX-ACCESS" << "SYNTAX" << "STATUS" << "DESCRIPTION";
    QString tableOid, translatedTableOid;
    QStringList variablesIndexes;
};

#endif // FORM_H
