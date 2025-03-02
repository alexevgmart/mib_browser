#ifndef SNMP_H
#define SNMP_H

#include <QWidget>

namespace Ui {
class Snmp;
}

class Snmp : public QWidget
{
    Q_OBJECT

public:
    explicit Snmp(QWidget *parent = nullptr);
    ~Snmp();
    int version;
    QString community;
    QString port;
    QString v3Name;
    QString v3Level;
    QString v3AuthKey;
    QString v3PrivKey;
    QString v3AuthProto;
    QString v3PrivProto;

private:
    Ui::Snmp *ui;

signals:
    void signalCancel();
    void signalOk(int, QString, QString,  QString, QString, QString, QString, QString, QString);

private slots:
    void on_cancelButton_clicked();
    void on_okButton_clicked();
};

#endif // SNMP_H
