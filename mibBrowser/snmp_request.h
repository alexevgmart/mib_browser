#ifndef SNMPREQUEST_H
#define SNMPREQUEST_H

#include "form.h"


class SnmpRequest : public QThread
{
        Q_OBJECT

private:
    void* session;
    bool printResult = true;

public:
    enum RequestType : uint8_t {
        Get = 0,
        GetNext = 1,
        Walk = 2,
        WalkNoTranslate = 3
    };

    QString currentOid;
    int requestType;

    SnmpRequest(void*, bool);
    int snmpSyncResponse(void*, netsnmp_pdu*, netsnmp_pdu**);
    static int snmpSynchInput(int, netsnmp_session*, int, netsnmp_pdu*, void*);
    void clearingResponse(netsnmp_pdu*&);
    QString checkStatusResponse(void*, int, netsnmp_pdu*, bool);
    QString sendSnmpGetRequest(void*, QString, const int);
    QString ParseCurrentOid(netsnmp_variable_list*);
    QVariant getParseValue(netsnmp_variable_list*);
    QString getStringFromBuf(u_char **, size_t *);
    QString snmpWalk(void*, QString, bool);
    void getRequest();
    void getNextRequest();
    void walkRequest(bool);

signals:
    void sendResult(QString, bool);

protected:
    void run() override;
};

#endif // SNMPREQUEST_H
