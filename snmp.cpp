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
    QString version;
    if(ui->v1->isChecked()) version = "1";
    else if(ui->v2->isChecked()) version = "2c";
    else version = "3";

    if (version  != "3"){
        ui->inputName->setText("");
        ui->inputLevel->setText("");
        ui->inputAuthPass->setText("");
        ui->inputPrivPass->setText("");
        ui->inputAuthAlg->setText("");
        ui->inputPrivAlg->setText("");
    }
    emit signalOk(version, ui->inputCommunity->text(),\
                           ui->inputPort->text(),\
                           ui->inputName->text(),\
                           ui->inputLevel->text(),\
                           ui->inputAuthPass->text(),\
                           ui->inputPrivPass->text(),\
                           ui->inputAuthAlg->text(),\
                           ui->inputPrivAlg->text());
}
