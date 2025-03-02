#include "snmp.h"
#include "ui_snmp.h"

/**
 * @brief Основная форма создания окна параметров
 */
Snmp::Snmp(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Snmp)
{
    ui->setupUi(this);

    ui->v2->setChecked(true);

    QStringList levelItems = QStringList() << "authPriv" << "authNoPriv" << "noAuthNoPriv";
    for (QString item : levelItems) ui->level->addItem(item);

    QStringList authAlgItems = QStringList() << "MD5" << "SHA1";
    for (QString item : authAlgItems) ui->authAlg->addItem(item);

    QStringList privAlgItems = QStringList() << "DES" << "AES";
    for (QString item : privAlgItems) ui->privAlg->addItem(item);
}

/**
 * @brief Закрытие окна параметров
 */
Snmp::~Snmp()
{
    delete ui;
}

/**
 * @brief Нажатие кнопки отмена
 */
void Snmp::on_cancelButton_clicked()
{
    emit signalCancel();
}

/**
 * @brief Нажатие кнопки ok
 */
void Snmp::on_okButton_clicked()
{
    int version;
    if(ui->v1->isChecked()) version = 0;
    else if(ui->v2->isChecked()) version = 1;
    else if (ui->v3->isChecked()) version = 3;

    if (version  != 3){
        ui->inputName->setText("");
        ui->inputAuthPass->setText("");
        ui->inputPrivPass->setText("");
    }
    emit signalOk(version, ui->inputCommunity->text(),\
                           ui->inputPort->text(),\
                           ui->inputName->text(),\
                           ui->level->currentText(),\
                           ui->inputAuthPass->text(),\
                           ui->inputPrivPass->text(),\
                           ui->authAlg->currentText(),\
                           ui->privAlg->currentText());
}
