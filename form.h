#ifndef FORM_H
#define FORM_H

#include <QWidget>
#include "snmp.h"
#include  <QtWidgets>

namespace Ui {
class Form;
}

class Form : public QWidget
{
    Q_OBJECT

public:
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

public slots:
    void cancelSnmpWindow();
    void okSnmpWindow(QString, QString, QString, QString, QString, QString, QString, QString, QString);
    void itemInfoBegining();
    // void itemInfoFull();
    void itemInfo(QTreeWidgetItem* item);
    void searchTextChanged();
    void searchInfoMibTree(QTreeWidgetItem*, QString, QColor);
    QString commandBash(QString);
    void showOrHideIndexes(bool, QSpacerItem*);
    void appendChildrenMibTree(struct tree*, QTreeWidgetItem*, QString, QString);
    void infoMibTree(QTreeWidgetItem*);
    void deleteIndexes();
    void getRequestIndex(QTreeWidgetItem*);
    void backgroundFinished(QString);

signals:
    void sendSelectedOid(QString);

private:
    Ui::Form *ui;
    Snmp snmp;
    QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    bool indexesStatus = true;
    int searchCount = 0;
    QString searchText;
    QList<QTreeWidgetItem*> searchList;
    QTreeWidgetItem* clickedItem;
};

class BackgroundProcess : public QThread
{
    Q_OBJECT

public:
    explicit BackgroundProcess(QObject *parent = nullptr);
    ~BackgroundProcess();
    QString oid;
    QString name;

signals:
    void processFinished(QString);

protected:
    void run() override;
};

#endif // FORM_H
