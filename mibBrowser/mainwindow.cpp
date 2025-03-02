#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&form, &Form::sendSelectedOid, this, &MainWindow::showSelectedOid);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    form.show();
}

void MainWindow::showSelectedOid(QString oid){
    ui->selectedOid->setText(oid);
}
