#ifndef INDEXES_H
#define INDEXES_H

#include <QWidget>

namespace Ui {
class Indexes;
}

class Indexes : public QWidget
{
    Q_OBJECT

public:
    explicit Indexes(QWidget *parent = nullptr);
    ~Indexes();

public slots:
    void getTable(QString, QString, QString, QStringList);
    QList<QString> getIndex(QString);
    void deleteInfoFromTable();

signals:
    void sendOid(QString);

private slots:
    void on_trans_clicked();
    void on_copyOid_clicked();

private:
    Ui::Indexes *ui;

    QStringList verticalHeaders;
    QStringList horizontalHeaders;
    int rows, columns;
    QString tableOid;
};

#endif // INDEXES_H
