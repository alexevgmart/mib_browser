
#include "form.h"
#include "ui_form.h"
#include <QProcess>
#include <QPalette>
#include <QtConcurrent/QtConcurrent>

Form::Form(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form)
{
    ui->setupUi(this);

    char* MIBName = "all";
    char* directory = "/opt/morion/monitor/mibs";
    setenv("MIBS", MIBName, 1);
    setenv("MIBDIRS", directory, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SAVE_MIB_DESCRS, 1);
    init_snmp("snmp");

    snmp.version = 1;
    snmp.community = "public";
    snmp.port = "161";
    snmp.v3Name = "";
    snmp.v3Level = "";
    connect(&snmp, &Snmp::signalCancel, this, &Form::cancelSnmpWindow);
    connect(&snmp, &Snmp::signalOk, this, &Form::okSnmpWindow);

//    QIcon snmpIcon("/home/astra/Desktop/mibBrowser/mibBrowser/param.png");
//    ui->paramSnmp->setIcon(snmpIcon);

    ui->treeSearchInput->setPlaceholderText("Поиск");

    ui->ipInput->setPlaceholderText("IP-адрес: AAA.BBB.CCC.DDD");

    ui->mibTree->setColumnCount(1);
    QTreeWidgetItem *mib = new QTreeWidgetItem(ui->mibTree);
    mib->setText(0, "mib");
    infoMibTree(mib);

    connect(ui->mibTree, &QTreeWidget::itemClicked, this, &Form::itemInfo);

    connect(ui->treeSearchInput, &QLineEdit::textChanged, this, &Form::searchTextChanged);

    ui->indexesButton->setVisible(false);

    connect(this, &Form::sendTable, &indexes, &Indexes::getTable);

    connect(&indexes, &Indexes::sendOid, this, &Form::sendTableCopiedOid);

    ui->splitter->setChildrenCollapsible(false);
    ui->splitter_2->setChildrenCollapsible(false);
}

Form::~Form()
{
    snmp_shutdown("snmp");
    delete ui;
}

/**
 * @brief Создание QTreeWidget для Mib-дерева
 * @param treeHead Родительский элемент Mib-дерева
 * @param parentTree Родительский элемент QTreeWidgetItem
 * @param oid OID предыдущего элемента
 * @param fullName Полное имя предыдущего элемента
 * @param fullNameNoSubid Полное имя предыдущего элемента без числовых значений
 */
void Form::appendChildrenMibTree(struct tree *treeHead, QTreeWidgetItem* parentTree, QString previousOid, QString fullName, QString fullNameNoSubid){
    QVector<struct tree*> elements;
    struct tree* tmp = treeHead;
    while (tmp) {
        elements.push_back(tmp);
        tmp = tmp->next_peer;
    }
    for(int i = 0; i < elements.size(); i++){
        for (int j = i + 1; j < elements.size(); j++){
            if ((elements[j]->subid) > (elements[i])->subid){
                tmp = elements[i];
                elements[i] = elements[j];
                elements[j] = tmp;
            }
        }
    }
    for(int i = 0; i < elements.size() / 2; i++){
        tmp = elements[i];
        elements[i] = elements[elements.size() - i - 1];
        elements[elements.size() - i - 1] = tmp;
    }

    for(struct tree *tr : elements){
        oid new_oid[MAX_OID_LEN];
        std::size_t len = MAX_OID_LEN;

        QString translate;
        if (snmp_parse_oid((previousOid + "." + QString::number(tr->subid)).toLocal8Bit().data(), new_oid, &len) == nullptr)
            qDebug() << "error";
        else {
            translate = description(new_oid, len, 1000000);
        }

        QString type = "", access = "", status = "", module = "", syntax = "", tmp = "", currentString = "", description = "";
        for (QString item : translate.split("\n")) {
            bool checkWantedString = true;
            for (QString string : wantedString) {
                if (item.indexOf(string) != -1) {
                    checkWantedString = false;
                    break;
                }
            }

            if (currentString == "DESCRIPTION" && item.indexOf("::= {") == -1)
                description += item;

            if (checkWantedString) {
                if (!(item.indexOf("UNITS") != -1 || item.indexOf("TEXTUAL CONVENTION") != -1 || item.indexOf("DISPLAY-HINT") != -1))
                    tmp += item + "\n";
            }
            else {
                if (item.indexOf("MAX-ACCESS") != -1) {
                    if (currentString == "STATUS")
                        status += tmp;
                    else if (currentString == "SYNTAX")
                        syntax += tmp;
                    else if (currentString == "FROM")
                        module += tmp.remove(" --\t\t").remove("\n");
                    tmp.clear();
                    currentString = "MAX-ACCESS";
                    access += item.remove("  MAX-ACCESS\t");
                }
                else if (item.indexOf("STATUS") != -1) {
                    if (currentString == "MAX-ACCESS")
                        access += tmp.replace("\t  ", "  ");
                    else if (currentString == "SYNTAX")
                        syntax += tmp;
                    else if (currentString == "FROM")
                        module += tmp.remove(" --\t\t").remove("\n");
                    tmp.clear();
                    currentString = "STATUS";
                    status += item.remove("  STATUS\t");
                }
                else if (item.indexOf("FROM") != -1) {
                    if (currentString == "STATUS")
                        status += tmp;
                    else if (currentString == "SYNTAX")
                        syntax += tmp;
                    else if (currentString == "MAX-ACCESS")
                        access += tmp.replace("\t  ", "  ");
                    tmp.clear();
                    currentString = "FROM";
                    module += item.remove("  -- FROM\t");
                }
                else if (item.indexOf("SYNTAX") != -1) {
                    if (currentString == "STATUS")
                        status += tmp;
                    else if (currentString == "MAX-ACCESS")
                        access += tmp.replace("\t  ", "  ");
                    else if (currentString == "FROM")
                        module += tmp.remove(" --\t\t").remove("\n");
                    tmp.clear();
                    currentString = "SYNTAX";
                    syntax += item.remove("  SYNTAX\t");
                }
                else if (item.indexOf("DESCRIPTION") != -1) {
                    if (currentString == "STATUS")
                        status += tmp;
                    else if (currentString == "MAX-ACCESS")
                        access += tmp.replace("\t  ", "  ");
                    else if (currentString == "FROM")
                        module += tmp.remove(" --\t\t").remove("\n");
                    else if (currentString == "SYNTAX")
                        syntax += tmp;
                    tmp.clear();
                    currentString = "DESCRIPTION";
                    description += item.remove("DESCRIPTION\t");
                }
            }
        }
        if (module == "")
            module = "unknown";
        if (syntax == "")
            syntax = "unknown";
        if (access == "")
            access = "unknown";
        if (status == "")
            status = "unknown";
        type = (translate.split("\n")[0]).remove(QString(tr->label) + " ");
        // qDebug() << QString(tr->label) << type << module << syntax << access << status << "\n";

        QTreeWidgetItem* childTree = new QTreeWidgetItem(parentTree);
        childTree->setText(0, tr->label);
        childTree->setData(0, Qt::ToolTipRole, QString(tr->label));
        childTree->setData(1, Qt::ToolTipRole, type);
        childTree->setData(4, Qt::ToolTipRole, module);
        childTree->setData(5, Qt::ToolTipRole, parentTree->data(0, Qt::ToolTipRole).toString());
        childTree->setData(6, Qt::ToolTipRole, syntax);
        childTree->setData(7, Qt::ToolTipRole, status);
        childTree->setData(8, Qt::ToolTipRole, access);
        childTree->setData(9, Qt::ToolTipRole, description);

        if (previousOid == "") {
            childTree->setData(2, Qt::ToolTipRole, QString::number(tr->subid));
            childTree->setData(3, Qt::ToolTipRole, QString(tr->label) + "(" + QString::number(tr->subid) + ")");
            childTree->setData(10, Qt::ToolTipRole, QString(tr->label));
            appendChildrenMibTree(tr->child_list, childTree, QString::number(tr->subid), QString(tr->label) + "(" + QString::number(tr->subid) + ")", QString(tr->label));
        }
        else {
            childTree->setData(2, Qt::ToolTipRole, previousOid + "." + QString::number(tr->subid));
            childTree->setData(3, Qt::ToolTipRole, fullName + "." + QString(tr->label) + "(" + QString::number(tr->subid) + ")");
            childTree->setData(10, Qt::ToolTipRole, fullNameNoSubid + "." + QString(tr->label));
            appendChildrenMibTree(tr->child_list, childTree, previousOid + "." + QString::number(tr->subid), fullName + "." + QString(tr->label) + "(" + QString::number(tr->subid) + ")", fullNameNoSubid + "." + QString(tr->label));
        }
    }

    elements.clear();
}


/**
 * @brief Парсинг Mib файлов
 * @param mibTreeHead Корневой элемент QTreeWidget
 */
void Form::infoMibTree(QTreeWidgetItem* mibTreeHead){
    add_mibdir("/opt/morion/monitor/mibs");
    netsnmp_init_mib();
    NETSNMP_IMPORT struct tree* tree_head;
//    struct tree* treeHead = tree_head;
    appendChildrenMibTree(tree_head, mibTreeHead, "", "", "");
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
void Form::okSnmpWindow(int version,\
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
    snmp.v3AuthKey = v3AuthPass;
    snmp.v3PrivKey = v3PrivPass;
    snmp.v3AuthProto = v3AuthAlg;
    snmp.v3PrivProto = v3PrivAlg;

    snmp.close();
}

/**
 * @brief Получение описания элемента
 * @param objid OID элемента
 * @param objidlen Длина OID
 * @param width
 * @return
 */
QString Form::description(oid * objid, size_t objidlen, int width)
{
    QString returnBuf = "";
    u_char         *buf = NULL;
    size_t          buf_len = 256, out_len = 0;

    if ((buf = (u_char *) calloc(buf_len, 1)) == NULL) {
        qDebug() << "[TRUNCATED]\n";
    } else {
        if (!sprint_realloc_description(&buf, &buf_len, &out_len, 1,
                                   objid, objidlen, width)) {
            qDebug() << buf << "[TRUNCATED]\n";
        } else {
            returnBuf = QString((char*)buf);
        }
    }

    SNMP_FREE(buf);
    return returnBuf;
}

/**
 * @brief Вывод информации о выбранном элементе
 * @param item QTreeWidgetItem для которого нужно вывести информацию
 */
void Form::itemInfo(QTreeWidgetItem* item){
    QString text = "";
    text += "<font color='grey'>Имя:\t </font>" + item->data(0, Qt::ToolTipRole).toString() + "<br>";
    text += "<font color='grey'>Тип:\t </font>" + item->data(1, Qt::ToolTipRole).toString() + "<br>";
    text += "<font color='grey'>OID:\t </font>" + item->data(2, Qt::ToolTipRole).toString() + "<br>";
    text += "<font color='grey'>Полное имя: </font>" + item->data(3, Qt::ToolTipRole).toString() + "<br>";
    text += "<font color='grey'>Модуль:\t </font>" + item->data(4, Qt::ToolTipRole).toString() + "<br>";

    if (item->data(1, Qt::ToolTipRole).toString() == "OBJECT-TYPE") {
        text += "<font color='grey'>Родитель:\t </font>" + item->data(5, Qt::ToolTipRole).toString() + "<br>";
        text += "<font color='grey'>Статус:\t </font>" + item->data(7, Qt::ToolTipRole).toString() + "<br>";
        text += "<font color='grey'>Доступ:\t </font>" + item->data(8, Qt::ToolTipRole).toString() + "<br>";
        text += "<font color='grey'>Тип данных:\t </font>" + (item->data(6, Qt::ToolTipRole).toString()).replace("}", " }") + "<br>";
        if (item->data(9, Qt::ToolTipRole).toString() != "")
            text += "<font color='grey'>Описание:\t </font>" + item->data(9, Qt::ToolTipRole).toString() + "<br>";
    }

    ui->oid->setText(item->data(2, Qt::ToolTipRole).toString());
    ui->descriptionMibElement->setText(text);

    if (item->data(8, Qt::ToolTipRole).toString() == "not-accessible" && item->data(0, Qt::ToolTipRole).toString().indexOf("Table") != -1) {
        ui->indexesButton->setVisible(true);
    }
    else ui->indexesButton->setVisible(false);
 }

/**
 * @brief Отправка OID в главное окно
 */
void Form::on_selectOid_clicked()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ui->oid->text());
    emit sendSelectedOid("Выбранный OID: " + ui->oid->text());
}

/**
 * @brief Отправить в вызывающее окно выбранный OID
 * @param oid
 */
void Form::sendTableCopiedOid(QString oid) {
    emit sendSelectedOid("Выбранный OID: " + oid);
}

/**
 * @brief Удаление OID с главного окна
 */
void Form::on_cancelOid_clicked()
{
    emit sendSelectedOid("Выбранный OID: ");
}

/**
 * @brief Получение результата запроса
 * @param result - Результат запроса
 * @param out - Вывод на экран
 */
void Form::getRequestResult(QString result, bool out)
{
    if (out)
        ui->requestResult->setText(result);
    else {
        if (result == "")
            return ui->requestResult->setText("No Such Object available on this agent at this OID");
//            QStringList tmp = table.split(" = ");
//            if (tmp[1] == "No Such Object available on this agent at this OID\n") return ui->requestResult->setText("No Such Object available on this agent at this OID");

        emit sendTable(result, tableOid, translatedTableOid, variablesIndexes);
        indexes.show();
    }

    ui->requestGet->setEnabled(true);
    ui->requestGetNext->setEnabled(true);
}

/**
 * @brief Отправление snmpget запроса
 */
void Form::on_requestGet_clicked()
{
    if (ui->ipInput->text() == "")
        return ui->requestResult->setText("Введите IP-адрес!");

    if (!ui->requestGet->isEnabled())
        return;

    ui->requestGet->setEnabled(false);
    ui->requestGetNext->setEnabled(false);

    QString peerName = ui->ipInput->text() + ":" + snmp.port;
    SnmpRequest* request = new SnmpRequest(netsnmp_sessp_conf(peerName.toLatin1().data(), (snmp.community).toLatin1().data(), snmp.version), true);
    connect(request, &SnmpRequest::sendResult, this, &Form::getRequestResult);
    connect(request, &SnmpRequest::finished, request, &SnmpRequest::deleteLater);

    if (ui->mibTree->currentItem()->parent() && (ui->mibTree->currentItem()->parent())->parent()) {
        if (((ui->mibTree->currentItem()->parent())->parent())->data(8, Qt::ToolTipRole).toString() == "not-accessible" &&
            ((ui->mibTree->currentItem()->parent())->parent())->data(0, Qt::ToolTipRole).toString().indexOf("Table") != -1 &&
             (ui->mibTree->currentItem()->data(2, Qt::ToolTipRole)).toString() == (ui->oid->text()).remove(" ")) {
            QString oid = ui->oid->text();
            request->currentOid = oid;
            request->requestType = SnmpRequest::RequestType::Walk;
            request->start();
        }
        else if ((ui->mibTree->currentItem()->data(2, Qt::ToolTipRole)).toString() == (ui->oid->text()).remove(" ")){
            request->currentOid = ui->oid->text() + ".0";
            request->requestType = SnmpRequest::RequestType::Get;
        }
        else {
            request->currentOid = ui->oid->text();
            request->requestType = SnmpRequest::RequestType::Get;
        }
    }
    else {
        if ((ui->mibTree->currentItem()->data(2, Qt::ToolTipRole)).toString() == (ui->oid->text()).remove(" ")){
            request->currentOid = ui->oid->text() + ".0";
            request->requestType = SnmpRequest::RequestType::Get;
        }
        else {
            request->currentOid = ui->oid->text();
            request->requestType = SnmpRequest::RequestType::Get;
        }
    }

    request->start();
}

/**
 * @brief Отправление snmpgetnext запроса
 */
void Form::on_requestGetNext_clicked()
{
    if (ui->ipInput->text() == "")
        return ui->requestResult->setText("Введите IP-адрес!");

    if (!ui->requestGetNext->isEnabled())
        return;

    ui->requestGet->setEnabled(false);
    ui->requestGetNext->setEnabled(false);

    QString peerName = ui->ipInput->text() + ":" + snmp.port;

    SnmpRequest* request = new SnmpRequest(netsnmp_sessp_conf(peerName.toLatin1().data(), (snmp.community).toLatin1().data(), snmp.version), true);
    connect(request, &SnmpRequest::sendResult, this, &Form::getRequestResult);
    connect(request, &SnmpRequest::finished, request, &SnmpRequest::deleteLater);
    request->currentOid = ui->oid->text() + ".0";
    request->requestType = SnmpRequest::RequestType::GetNext;
    request->start();
}

/**
 * @brief Выполнение конфигурации и открытие сессии Snmp с использованием библиотеки Net-Snmp
 * @param peerName - имя или адрес однораногового узла по умолчанию
 * @param community - строка сообщества для исходящих запросов
 * @param version - версия SNMP
 * @return - возвращается непрозрачный указатель на сессию
 */
void* Form::netsnmp_sessp_conf(char *peerName, char* community, int version)
{
    qDebug() << "SessionHandler::netsnmp_sessp_conf" << __LINE__;
    int liberr, syserr;
    char *errstr;
    void *sessp;//Непрозрачный указатель

    netsnmp_session session;
    snmp_sess_init(&session);//Инициализация сессии

    session.version = version;
    session.peername = peerName;
    session.retries = 3;//Количество попыток при неудачном ответе
    session.timeout = 2000000;
    if (version != 3){
        session.community = (unsigned char *)community;
        session.community_len = strlen(community);
    }
    else {
        session.securityName = strdup((snmp.v3Name).toLocal8Bit().data());
        session.securityNameLen = strlen(session.securityName);

        if (snmp.v3Level == "noAuthNoPriv")
            session.securityLevel = SNMP_SEC_LEVEL_NOAUTH;
        else {
            if (snmp.v3AuthProto == "MD5"){
                session.securityAuthProto = usmHMACMD5AuthProtocol;
                session.securityAuthProtoLen = sizeof(usmHMACMD5AuthProtocol)/sizeof(oid);
            }
            else {
                session.securityAuthProto = usmHMACSHA1AuthProtocol;
                session.securityAuthProtoLen = sizeof(usmHMACSHA1AuthProtocol)/sizeof(oid);
            }
            session.securityAuthKeyLen = USM_AUTH_KU_LEN;

            if (generate_Ku(session.securityAuthProto,
                            session.securityAuthProtoLen,
                            (u_char *)(snmp.v3AuthKey.toLocal8Bit().data()), snmp.v3AuthKey.length(),
                            session.securityAuthKey,
                            &session.securityAuthKeyLen) != SNMPERR_SUCCESS) {
                qDebug() << "Can't generate auth key";
            }

            if (snmp.v3Level == "authNoPriv")
                session.securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
            else {
                session.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;

                if (snmp.v3PrivProto == "DES"){
                    session.securityPrivProto = usmDESPrivProtocol;
                    session.securityPrivProtoLen = sizeof(usmDESPrivProtocol)/sizeof(oid);
                }
                else {
                    session.securityPrivProto = usmAESPrivProtocol;
                    session.securityPrivProtoLen = sizeof(usmAESPrivProtocol)/sizeof(oid);
                }
                session.securityPrivKeyLen = USM_PRIV_KU_LEN;

                if (generate_Ku(session.securityAuthProto,
                                session.securityAuthProtoLen,
                                (u_char*)((snmp.v3PrivKey).toLocal8Bit().data()), (std::size_t)(snmp.v3PrivKey).length(),
                                session.securityPrivKey,
                                &session.securityPrivKeyLen) != SNMPERR_SUCCESS) {
                    qDebug() << "Can't generate priv key";
                }
            }
        }
    }

    /*
     * Open an SNMP session.
     */
    sessp = snmp_sess_open(&session);

    if (sessp == nullptr)
    {
        /* Error codes found in open calling argument */
        snmp_error(&session, &liberr, &syserr, &errstr);
        free(errstr);
        return NULL;
    }
    return sessp;
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
    if (item->data(0, Qt::ToolTipRole).toString() == text ||
        item->data(2, Qt::ToolTipRole).toString() == text ||
        item->data(10, Qt::ToolTipRole).toString() == text) {
        ui->mibTree->setCurrentItem(item);
        itemInfo(item);
    }

    if (item->data(0, Qt::ToolTipRole).toString().indexOf(text) != -1 ||
        item->data(2, Qt::ToolTipRole).toString().indexOf(text) != -1 ||
        item->data(10, Qt::ToolTipRole).toString().indexOf(text) != -1) {
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
    if (query == "")
        query = " ";
    QTreeWidgetItem* head = ui->mibTree->topLevelItem(0);
    searchInfoMibTree(head, query, color);
}

/**
 * @brief Нажатие клавиши enter при поиске
 */
void Form::on_treeSearchInput_returnPressed()
{
    QString query = ui->treeSearchInput->text();
    if (query == "")
        query = " ";
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

/**
 * @brief Поиск индексов в таблице
 */
void Form::on_indexesButton_clicked()
{
    QTreeWidgetItem *item = ui->mibTree->currentItem();
    QString oid = item->data(2, Qt::ToolTipRole).toString();

    if (ui->ipInput->text() == "")
        return ui->requestResult->setText("Введите IP-адрес!");

    QString peerName = ui->ipInput->text() + ":" + snmp.port;

    item = item->child(0);
    variablesIndexes.clear();
    for (int i = 0; i < item->childCount(); i++){
        variablesIndexes.append(item->child(i)->text(0));
    }

    tableOid = oid;
    translatedTableOid = ui->mibTree->currentItem()->text(0);

    indexes.deleteInfoFromTable();

    SnmpRequest* request = new SnmpRequest(netsnmp_sessp_conf(peerName.toLatin1().data(), (snmp.community).toLatin1().data(), snmp.version), false);
    connect(request, &SnmpRequest::sendResult, this, &Form::getRequestResult);
    connect(request, &SnmpRequest::finished, request, &SnmpRequest::deleteLater);
    request->currentOid = oid;
    request->requestType = SnmpRequest::RequestType::WalkNoTranslate;
    request->start();
}
