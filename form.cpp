#include "form.h"
#include "ui_form.h"
#include <QProcess>
#include <QPalette>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

/**
 * @brief Основная форма создания приложения
 */
Form::Form(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form)
{
    ui->setupUi(this);

    snmp.version = "2c";
    snmp.community = "public";
    snmp.port = "161";
    snmp.v3Name = "";
    snmp.v3Level = "";
    connect(&snmp, &Snmp::signalCancel, this, &Form::cancelSnmpWindow);
    connect(&snmp, &Snmp::signalOk, this, &Form::okSnmpWindow);

    QIcon snmpIcon("/home/astra/Desktop/mibBrowser/param.png");
    ui->paramSnmp->setIcon(snmpIcon);

    ui->treeSearchInput->setPlaceholderText("Поиск");

    ui->ipInput->setPlaceholderText("IP-адрес: AAA.BBB.CCC.DDD");

    ui->mibTree->setColumnCount(1);
    QTreeWidgetItem *mib = new QTreeWidgetItem(ui->mibTree);
    mib->setText(0, "mib");
    infoMibTree(mib);

    connect(ui->mibTree, &QTreeWidget::itemClicked, this, &Form::itemInfo);

    connect(ui->treeSearchInput, &QLineEdit::textChanged, this, &Form::searchTextChanged);

    showOrHideIndexes(false, spacer);

    connect(ui->indexesList, &QTreeWidget::itemClicked, this, &Form::getRequestIndex);
}

BackgroundProcess::BackgroundProcess(QObject *parent)
{
}

BackgroundProcess::~BackgroundProcess()
{
}

/**
 * @brief Фоновый поиск информации об элементе
 */
void BackgroundProcess::run(){
    QString snmptranslate = "", command = "snmptranslate -M /opt/morion/monitor/mibs -m ALL -On -TBd 2>/dev/null " + name;
    int timeoutMsec = 6000;
    try {
        QProcess process;

        qDebug() << "CommandScraper::::start command" << command;
        process.start("bash", QStringList() << "-c" << command, QIODevice::ReadOnly);

        // Ждем X миллисекунд
        if( !process.waitForStarted(timeoutMsec)) {
            qDebug() << "CommandScraper::command" << command << "Failed to start";
        }
        if( !process.waitForFinished(timeoutMsec)) {
            qDebug() << "CommandScraper::command" << command << "Failed to finish";
        }
        // Если успешно - код выхода 0
        if( process.exitCode() != 0)
        {
            qDebug() << "CommandScraper::command" << command << "Exit code!=0:" << process.exitCode();
        }

        snmptranslate = process.readAllStandardOutput();
    }   catch (std::exception& ex) {
        qDebug() << "CommandScraper:: Error:" << ex.what();
        return;
    }

    QString type = "", description = "", syntax = "", module = "";
    bool flagOid = false, flagDescription = false, flagType = false;
    for(QString tmp : snmptranslate.split("\n")){
        if(flagOid){
            if (tmp.indexOf(name) != -1 && !flagType){
                type = tmp.remove(name + " ");
                flagType = true;
            }
            if (tmp.indexOf("DESCRIPTION") != -1){
                tmp = tmp.remove("  DESCRIPTION\t");
                description += tmp;
                flagDescription = true;
            }
            if (tmp.indexOf("SYNTAX") != -1){
                syntax = tmp.remove("  SYNTAX\t");
            }
            if(tmp.indexOf("  -- FROM") != -1){
                module = tmp.remove("  -- FROM\t");
            }
            else if(flagDescription){
                if(tmp.indexOf("::=") != -1){
                    break;
                }
                if (tmp.indexOf("            ") != -1) description += tmp.replace("            ", "\n\t");
                else description += tmp.replace("        ", "\n\t");
            }
        }
        else{
            tmp = tmp.remove("\n");
            if (tmp == ("." + oid)){
                flagOid = true;
            }
        }
    }

    emit processFinished(type + "#$#" + description + "#$#" + syntax + "#$#" + module);
}

/**
 * @brief Вывод полной информации об элементе
 * @param result Текст полученный из фонового процесса
 */
void Form::backgroundFinished(QString result){
    QList<QString> tmp = result.split("#$#");
    QString text, type = tmp[0], description = tmp[1], syntax = tmp[2], module = tmp[3];
    QTreeWidgetItem* item = clickedItem;
    if (type == "OBJECT-TYPE"){
        text += "Имя:\t    " + item->data(0, Qt::ToolTipRole).toString() + "\n";
        text += "Тип:\t    " + type + "\n";
        text += "OID:\t    " + item->data(2, Qt::ToolTipRole).toString() + "\n";
        text += "Полное имя:  " + item->data(3, Qt::ToolTipRole).toString() + "\n";
        text += "Модуль:\t    " + module + "\n";
        text += "Родитель:\t    " + item->data(5, Qt::ToolTipRole).toString() + "\n";
        if (syntax == "") text += "Тип данных:   unknown\n";
        else text += "Тип данных:   " + syntax + "\n";
        text += "Статус:\t    " + item->data(7, Qt::ToolTipRole).toString() + "\n";
        text += "Доступ:\t    " + item->data(8, Qt::ToolTipRole).toString() + "\n";
        if (description != "") text += "Описание:\t" + description;
    }
    else {
        text += "Имя:\t    " + item->data(0, Qt::ToolTipRole).toString() + "\n";
        text += "Тип:\t    " + type + "\n";
        text += "OID:\t    " + item->data(2, Qt::ToolTipRole).toString() + "\n";
        text += "Полное имя:  " + item->data(3, Qt::ToolTipRole).toString() + "\n";
        text += "Модуль:\t    " + module + "\n";
    }

    if (ui->oid->text() != item->data(2, Qt::ToolTipRole).toString()) return;
    ui->descriptionMibElement->setText(text);
}

/**
 * @brief Скрыть/Показать виджет для индексов
 * @param show Показать - true, скрыть - false
 * @param spacer Разделитель для замещения QListWidget
 */
void Form::showOrHideIndexes(bool show, QSpacerItem* spacer){
    if (indexesStatus != show){
        ui->indexesLabel->setVisible(show);
        ui->indexesList->setVisible(show);

        if (show) ui->verticalLayout_5->removeItem(spacer);
        else if (!show) ui->verticalLayout_5->addItem(spacer);
        indexesStatus = show;
    }
}

/**
 * @brief Перевод числового значения доступа в строку
 * @param access Числовое значение доступа
 * @return Строчное представление доступа
 */
QString getAccess(int access){
    if (access == MIB_ACCESS_READONLY) return "read-only";
    else if (access == MIB_ACCESS_READWRITE) return "read-write";
    else if (access == MIB_ACCESS_WRITEONLY) return "write-only";
    else if (access == MIB_ACCESS_NOACCESS) return "no access";
    else if (access == MIB_ACCESS_CREATE) return "read-create";
    else return "unknown";
}

/**
 * @brief Перевод числового значения статуса в строку
 * @param status Числовое значение статуса
 * @return Строчное представление статуса
 */
QString getStatus(int status){
    if (status == MIB_STATUS_CURRENT) return "current";
    else if (status == MIB_STATUS_MANDATORY) return "mandatory";
    else if (status == MIB_STATUS_DEPRECATED) return "deprecated";
    else if (status == MIB_STATUS_OBSOLETE) return "obsolete";
    else if (status == MIB_STATUS_OPTIONAL) return "optional";
    else return "unknown";
}

/**
 * @brief Создание QTreeWidget для Mib-дерева
 * @param treeHead Родительский элемент Mib-дерева
 * @param parentTree Родительский элемент QTreeWidgetItem
 * @param oid OID предыдущего элемента
 * @param fullName Полное имя предыдущего элемента
 */
void Form::appendChildrenMibTree(struct tree *treeHead, QTreeWidgetItem* parentTree, QString oid, QString fullName){
    for(struct tree *tr = treeHead; tr; tr = tr->next_peer){
        QTreeWidgetItem* childTree = new QTreeWidgetItem(parentTree);
        childTree->setText(0, tr->label);
        childTree->setData(0, Qt::ToolTipRole, QString(tr->label));
        childTree->setData(5, Qt::ToolTipRole, parentTree->data(0, Qt::ToolTipRole).toString());
        childTree->setData(7, Qt::ToolTipRole, getStatus(tr->status));
        childTree->setData(8, Qt::ToolTipRole, getAccess(tr->access));
        if (oid == "") {
            childTree->setData(2, Qt::ToolTipRole, QString::number(tr->subid));
            childTree->setData(3, Qt::ToolTipRole, QString(tr->label) + "(" + QString::number(tr->subid) + ")");
            appendChildrenMibTree(tr->child_list, childTree, QString::number(tr->subid), QString(tr->label) + "(" + QString::number(tr->subid) + ")");
        }
        else {
            childTree->setData(2, Qt::ToolTipRole, oid + "." + QString::number(tr->subid));
            childTree->setData(3, Qt::ToolTipRole, fullName + "." + QString(tr->label) + "(" + QString::number(tr->subid) + ")");
            appendChildrenMibTree(tr->child_list, childTree, oid + "." + QString::number(tr->subid), fullName + "." + QString(tr->label) + "(" + QString::number(tr->subid) + ")");
        }
    }
}

/**
 * @brief Парсинг Mib файлов
 * @param mibTreeHead Корневой элемент QTreeWidget
 */
void Form::infoMibTree(QTreeWidgetItem* mibTreeHead){
    init_snmp("");
    add_mibdir("/opt/morion/monitor/mibs");
    netsnmp_init_mib();
    struct tree *treeHead = read_all_mibs();
    appendChildrenMibTree(treeHead, mibTreeHead, "", "");
    snmp_shutdown("");
}

/**
 * @brief Закрытие окна
 */
Form::~Form()
{
    delete ui;
}

/**
 * @brief Открытие окна настроек SNMP
 */
void Form::on_paramSnmp_clicked()
{
    snmp.show();
}

/**
 * @brief Закрытие окна настроек SNMP
 */
void Form::cancelSnmpWindow()
{
    snmp.close();
}

// три возможных параметра для -l: noAuthNoPriv, authNoPriv, authPriv  (security level/уровень безопасности)
/**
 * @brief Передача параметров из окна настроек SNMP
 * @param version Версия
 * @param community
 * @param port Порт
 * @param v3Name Имя
 * @param v3Level Уровень безопасности
 * @param v3AuthPass Пароль аутентификации
 * @param v3PrivPass Пароль шифрования
 * @param v3AuthAlg Алгоритм аутентификации
 * @param v3PrivAlg Алгоритм шифроваиня
 */
void Form::okSnmpWindow(QString version,\
                        QString community,\
                        QString port,\
                        QString v3Name,\
                        QString v3Level,\
                        QString v3AuthPass,\
                        QString v3PrivPass,\
                        QString v3AuthAlg,\
                        QString v3PrivAlg)
{
    snmp.version = version;
    snmp.community = community;
    snmp.port = port;
    snmp.v3Name = v3Name;
    snmp.v3Level = v3Level;
    snmp.v3AuthPass = v3AuthPass;
    snmp.v3PrivPass = v3PrivPass;
    snmp.v3AuthAlg = v3AuthAlg;
    snmp.v3PrivAlg = v3PrivAlg;

    snmp.close();
}

/**
 * @brief Скрытие QListWidget для индексов
 */
void Form::deleteIndexes(){
    while (ui->indexesList->takeTopLevelItem(0) != NULL) {
        delete ui->indexesList->takeTopLevelItem(0);
    }
    ui->indexesList->clear();
}

/**
 * @brief Создание двух потоков для поиска информации
 * @param item QTreeWidgetItem для которого нужно вывести информацию
 */
void Form::itemInfo(QTreeWidgetItem* item){
    clickedItem = item;

    BackgroundProcess* process = new BackgroundProcess(this);
    process->oid = item->data(2, Qt::ToolTipRole).toString();
    process->name = item->data(0, Qt::ToolTipRole).toString();
    connect(process, &BackgroundProcess::processFinished, this, &Form::backgroundFinished);
    process->start();
    itemInfoBegining();
 }

/**
 * @brief Предварительный вывод информации об элементе и поиск индексов
 */
void Form::itemInfoBegining()
{
    QTreeWidgetItem* item = clickedItem;
    if (item->parent() == NULL) {
        ui->descriptionMibElement->setText("Описание выбранного элемента");
        ui->oid->setText("");
    }
    else {
        QString text;

        text += "Имя:\t    " + item->data(0, Qt::ToolTipRole).toString() + "\n";
        text += "OID:\t    " + item->data(2, Qt::ToolTipRole).toString() + "\n";
        text += "Полное имя:  " + item->data(3, Qt::ToolTipRole).toString() + "\n";
        text += "Родитель:\t    " + item->data(5, Qt::ToolTipRole).toString() + "\n";
        text += "Статус:\t    " + item->data(7, Qt::ToolTipRole).toString() + "\n";
        text += "Доступ:\t    " + item->data(8, Qt::ToolTipRole).toString() + "\n";

        ui->descriptionMibElement->setText(text);
        ui->oid->setText(item->data(2, Qt::ToolTipRole).toString());

        showOrHideIndexes(false, spacer);
        deleteIndexes();
        if (item->childCount() == 0){
            QString ip = ui->ipInput->text();
            if (ip == ""){
                // если не введен айпи, то сообщать об этом и не искать индексы
                ui->requestResult->setText("Вы не ввели IP-адрес !");
                return;
            }
            else {
                ui->requestResult->setText("Результат запроса");

                QString stdOut;
                QString oid = item->data(2, Qt::ToolTipRole).toString();
                QString Request = "snmpgetnext -v " + snmp.version + " -c " + snmp.community + " " + ip + ":" + snmp.port + " ";
                QString v3Request = "snmpgetnext -v " + snmp.version + " -A " + snmp.v3AuthPass + " -a " + snmp.v3AuthAlg + " -X " + snmp.v3PrivPass + " -x " + snmp.v3PrivAlg +\
                                    " -l " + snmp.v3Level + " -u " + snmp.v3Name + " " + ip + ":" + snmp.port + " ";
                QString translatedOid = commandBash("snmptranslate " + oid).remove("\n");

                bool flag = true;
                int count = 0;
                while (flag){
                    if (snmp.version != "3"){
                        stdOut = commandBash(Request + oid);
                    }
                    else{
                        stdOut = commandBash(v3Request + oid);
                    }

                    if (stdOut.indexOf(translatedOid) != -1 && stdOut.split(" ")[0] != (translatedOid + ".0") && stdOut.indexOf(translatedOid + ".") != -1){
                        if (count == 0) showOrHideIndexes(true, spacer);
                        oid = stdOut.split(" ")[0];
                        QTreeWidgetItem* indexItem = new QTreeWidgetItem(ui->indexesList);
                        count++;
                        indexItem->setText(0, QString::number(count));
                        indexItem->setData(0, Qt::ToolTipRole, oid);
                    }
                    else{
                        flag = false;
                    }
                }
                ui->indexesList->setHeaderLabel(QString::number(count) + " indexes found");
            }
        }
    }
}

///**
// * @brief Вывод полной информации об элементе и поиск индексов
// */
//void Form::itemInfoFull()
//{
//    QTreeWidgetItem* item = clickedItem;
//    if (item->parent() == NULL) {
//        ui->descriptionMibElement->setText("Описание выбранного элемента");
//        ui->oid->setText("");
//    }
//    else {
//        QString text, type = "", description = "", syntax = "", module = "";

//        QString snmptranslate = commandBash("snmptranslate -M /opt/morion/monitor/mibs -m ALL -On -TBd 2>/dev/null " + item->data(0, Qt::ToolTipRole).toString());
//        bool flagOid = false, flagDescription = false, flagType = false;
//        for(QString tmp : snmptranslate.split("\n")){
//            if(flagOid){
//                if (tmp.indexOf(item->data(0, Qt::ToolTipRole).toString()) != -1 && !flagType){
//                    type = tmp.remove(item->data(0, Qt::ToolTipRole).toString() + " ");
//                    flagType = true;
//                }
//                if (tmp.indexOf("DESCRIPTION") != -1){
//                    tmp = tmp.remove("  DESCRIPTION\t");
//                    description += tmp;
//                    flagDescription = true;
//                }
//                if (tmp.indexOf("SYNTAX") != -1){
//                    syntax = tmp.remove("  SYNTAX\t");
//                }
//                if(tmp.indexOf("  -- FROM") != -1){
//                    module = tmp.remove("  -- FROM\t");
//                }
//                else if(flagDescription){
//                    if(tmp.indexOf("::=") != -1){
//                        break;
//                    }
//                    if (tmp.indexOf("            ") != -1) description += tmp.replace("            ", "\n\t");
//                    else description += tmp.replace("        ", "\n\t");
//                }
//            }
//            else{
//                tmp = tmp.remove("\n");
//                if (tmp == ("." + item->data(2, Qt::ToolTipRole).toString())){
//                    flagOid = true;
//                }
//            }
//        }

//        if (ui->selectOid->text() != item->data(2, Qt::ToolTipRole).toString()) return;

//        if (type == "OBJECT-TYPE"){
//            text += "Имя:\t    " + item->data(0, Qt::ToolTipRole).toString() + "\n";
//            text += "Тип:\t    " + type + "\n";
//            text += "OID:\t    " + item->data(2, Qt::ToolTipRole).toString() + "\n";
//            text += "Полное имя:  " + item->data(3, Qt::ToolTipRole).toString() + "\n";
//            text += "Модуль:\t    " + module + "\n";
//            text += "Родитель:\t    " + item->data(5, Qt::ToolTipRole).toString() + "\n";
//            if (syntax == "") text += "Тип данных:   unknown\n";
//            else text += "Тип данных:   " + syntax + "\n";
//            text += "Статус:\t    " + item->data(7, Qt::ToolTipRole).toString() + "\n";
//            text += "Доступ:\t    " + item->data(8, Qt::ToolTipRole).toString() + "\n";
//            if (description != "") text += "Описание:\t" + description;
//        }
//        else {
//            text += "Имя:\t    " + item->data(0, Qt::ToolTipRole).toString() + "\n";
//            text += "Тип:\t    " + type + "\n";
//            text += "OID:\t    " + item->data(2, Qt::ToolTipRole).toString() + "\n";
//            text += "Полное имя:  " + item->data(3, Qt::ToolTipRole).toString() + "\n";
//            text += "Модуль:\t    " + module + "\n";
//        }

//        ui->descriptionMibElement->setText(text);
//        ui->oid->setText(item->data(2, Qt::ToolTipRole).toString());

//        showOrHideIndexes(false, spacer);
//        deleteIndexes();
//        if (item->childCount() == 0){
//            QString ip = ui->ipInput->text();
//            if (ip == ""){
//                // если не введен айпи, то сообщать об этом и не искать индексы
//                ui->requestResult->setText("Вы не ввели IP-адрес !");
//                return;
//            }
//            else {
//                ui->requestResult->setText("Результат запроса");

//                QString stdOut;
//                QString oid = item->data(2, Qt::ToolTipRole).toString();
//                QString Request = "snmpgetnext -v " + snmp.version + " -c " + snmp.community + " " + ip + ":" + snmp.port + " ";
//                QString v3Request = "snmpgetnext -v " + snmp.version + " -A " + snmp.v3AuthPass + " -a " + snmp.v3AuthAlg + " -X " + snmp.v3PrivPass + " -x " + snmp.v3PrivAlg +\
//                                    " -l " + snmp.v3Level + " -u " + snmp.v3Name + " " + ip + ":" + snmp.port + " ";
//                QString translatedOid = commandBash("snmptranslate " + oid).remove("\n");

//                bool flag = true;
//                int count = 0;
//                while (flag){
//                    if (snmp.version != "3"){
//                        stdOut = commandBash(Request + oid);
//                    }
//                    else{
//                        stdOut = commandBash(v3Request + oid);
//                    }

//                    if (stdOut.indexOf(translatedOid) != -1 && stdOut.split(" ")[0] != (translatedOid + ".0") && stdOut.indexOf(translatedOid + ".") != -1){
//                        if (count == 0) showOrHideIndexes(true, spacer);
//                        oid = stdOut.split(" ")[0];
//                        QTreeWidgetItem* indexItem = new QTreeWidgetItem(ui->indexesList);
//                        count++;
//                        indexItem->setText(0, QString::number(count));
//                        indexItem->setData(0, Qt::ToolTipRole, oid);
//                    }
//                    else{
//                        flag = false;
//                    }
//                }
//                ui->indexesList->setHeaderLabel(QString::number(count) + " indexes found");
//            }
//        }
//    }
//}

/**
 * @brief SNMP запрос для индекса элемента
 * @param item Элемент для которого нужно сделать запрос
 */
void Form::getRequestIndex(QTreeWidgetItem* item){
    QString ip = ui->ipInput->text();
    if (ip == "") {
        ui->requestResult->setText("Вы не ввели IP-адрес !");
        return;
    }
    QString command;
    QString oid = item->data(0, Qt::ToolTipRole).toString();
    if (snmp.version != "3"){
        command = "snmpget -v " + snmp.version + " -c " + snmp.community + " " + ip + ":" + snmp.port + " " + oid;
    }
    else{
        command = "snmpget -v " + snmp.version + " -A " + snmp.v3AuthPass + " -a " + snmp.v3AuthAlg + " -X " + snmp.v3PrivPass + " -x " + snmp.v3PrivAlg +\
                  " -l " + snmp.v3Level + " -u " + snmp.v3Name + " " + ip + ":" + snmp.port + " " + oid;
    }

    ui->requestResult->setText(commandBash(command));
}

/**
 * @brief Отправка OID в главное окно
 */
void Form::on_selectOid_clicked()
{
    emit sendSelectedOid("Выбранный OID: " + ui->oid->text());
}

/**
 * @brief Удаление OID с главного окна
 */
void Form::on_cancelOid_clicked()
{
    emit sendSelectedOid("Выбранный OID: ");
}

/**
 * @brief Отправление snmpget запроса
 */
void Form::on_requestGet_clicked()
{
    QString ip = ui->ipInput->text();
    if (ip == "") {
        ui->requestResult->setText("Вы не ввели IP-адрес !");
        return;
    }
    QString command;
    QString oid = ui->oid->text();
    if (snmp.version != "3"){
        command = "snmpget -v " + snmp.version + " -c " + snmp.community + " " + ip + ":" + snmp.port + " " + oid;
    }
    else{
        command = "snmpget -v " + snmp.version + " -A " + snmp.v3AuthPass + " -a " + snmp.v3AuthAlg + " -X " + snmp.v3PrivPass + " -x " + snmp.v3PrivAlg +\
                  " -l " + snmp.v3Level + " -u " + snmp.v3Name + " " + ip + ":" + snmp.port + " " + oid;
    }

    QString out = commandBash(command);
    if (out.indexOf("No Such Instance currently exists at this OID") != -1 || out.indexOf("No Such Object available on this agent at this OID") != -1){
        QString newOut = commandBash(command + ".0");
        if (newOut.indexOf("No Such Instance currently exists at this OID") != -1 || newOut.indexOf("No Such Object available on this agent at this OID") != -1){
            ui->requestResult->setText(out);
        }
        else{
            ui->requestResult->setText(newOut);
        }
    }
    else{
        ui->requestResult->setText(out);
    }
}

/**
 * @brief Отправление snmpgetnext запроса
 */
void Form::on_requestGetNext_clicked()
{
    QString ip = ui->ipInput->text();
    if (ip == "") {
        ui->requestResult->setText("Вы не ввели IP-адрес !");
        return;
    }
    QString command, commandNext;
    QString oid = ui->oid->text();
    if (snmp.version != "3"){
        command = "snmpget -v " + snmp.version + " -c " + snmp.community + " " + ip + ":" + snmp.port + " " + oid;
        commandNext = "snmpgetnext -v " + snmp.version + " -c " + snmp.community + " " +ip + ":" + snmp.port + " " + oid;
    }
    else{
        command = "snmpget -v " + snmp.version + " -A " + snmp.v3AuthPass + " -a " + snmp.v3AuthAlg + " -X " + snmp.v3PrivPass + " -x " + snmp.v3PrivAlg +\
                  " -l " + snmp.v3Level + " -u " + snmp.v3Name + " " + ip + ":" + snmp.port + " " + oid;
        commandNext = "snmpgetnext -v " + snmp.version + " -A " + snmp.v3AuthPass + " -a " + snmp.v3AuthAlg + " -X " + snmp.v3PrivPass + " -x " + snmp.v3PrivAlg +\
                  " -l " + snmp.v3Level + " -u " + snmp.v3Name + " " + ip + ":" + snmp.port + " " + oid;
    }

    QString out = commandBash(command);
    QString nextOut = commandBash(commandNext);
    if (out.indexOf("No Such Instance currently exists at this OID") != -1 || out.indexOf("No Such Object available on this agent at this OID") != -1){
        QString newOut = commandBash(command + ".0");
        if (newOut.indexOf("No Such Instance currently exists at this OID") != -1 || newOut.indexOf("No Such Object available on this agent at this OID") != -1){
            ui->requestResult->setText(nextOut);
        }
        else{
            ui->requestResult->setText(commandBash(commandNext + ".0"));
        }
    }
    else{
        ui->requestResult->setText(nextOut);
    }
}

/**
 * @brief Получение результата вывода команды в терминале
 * @param command Команда для терминала
 * @return Вывод команды в терминале
 */
QString Form::commandBash(QString command)
{
    int timeoutMsec = 6000;
    try {
        QProcess process;

        qDebug() << "CommandScraper::::start command" << command;
        process.start("bash", QStringList() << "-c" << command, QIODevice::ReadOnly);

        // Ждем X миллисекунд
        if( !process.waitForStarted(timeoutMsec)) {
            qDebug() << "CommandScraper::command" << command << "Failed to start";
        }
        if( !process.waitForFinished(timeoutMsec)) {
            qDebug() << "CommandScraper::command" << command << "Failed to finish";
        }
        // Если успешно - код выхода 0
        if( process.exitCode() != 0)
        {
            qDebug() << "CommandScraper::command" << command << "Exit code!=0:" << process.exitCode();
        }

        QString stdOut = process.readAllStandardOutput();
        return stdOut;
    }   catch (std::exception& ex) {
        qDebug() << "CommandScraper:: Error:" << ex.what();
        return NULL;
    }
}

/**
 * @brief Рекурсивная процедура для поиска в Mib-дереве
 * @param item Элемент в названии которого надо найти текст
 * @param text Текст для поиска
 * @param mibTree Корень QTreeWidget элемента
 * @param color Цвет для заднего фона
 */
void Form::searchInfoMibTree(QTreeWidgetItem* item, QString text, QColor color)
{
    if (item->data(0, Qt::ToolTipRole).toString() == text || item->data(2, Qt::ToolTipRole).toString() == text) {
        ui->mibTree->setCurrentItem(item);
        itemInfo(item);
    }

    if (item->data(0, Qt::ToolTipRole).toString().indexOf(text) != -1 || item->data(2, Qt::ToolTipRole).toString().indexOf(text) != -1) {
        item->setBackground(0, QColor(255, 179, 161));
    }
    else {
        item->setBackground(0, color);
    }
    
    for (int i = 0; i < item->childCount(); i++){
        searchInfoMibTree(item->child(i), text, color);
    }
}

/**
 * @brief Процедура для поиска при изменении текста в строке поиска
 */
void Form::searchTextChanged()
{
    QPalette palette = ui->mibTree->palette();
    QColor color = palette.color(QPalette::Window);
    QString query = ui->treeSearchInput->text();
    if (query == "") query = " ";
    QTreeWidgetItem* head = ui->mibTree->topLevelItem(0);
    searchInfoMibTree(head, query, color);
}

/**
 * @brief Нажатие клавиши enter при поиске
 */
void Form::on_treeSearchInput_returnPressed()
{
    QString query = ui->treeSearchInput->text();
    if (query == "") query = " ";
    if (searchText != query) {
        searchText = query;
        searchCount = 0;
        searchList.clear();
        searchList = ui->mibTree->findItems(query, Qt::MatchContains | Qt::MatchRecursive, 0);
    }
    else if (searchText == query) {
        searchCount++;
    }
    if (searchCount < searchList.size()){
        ui->mibTree->setCurrentItem(searchList[searchCount]);
        itemInfo(searchList[searchCount]);
    }
}

