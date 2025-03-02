#include "snmp_request.h"

SnmpRequest::SnmpRequest (void* ss, bool out)
{
    session = ss;
    printResult = out;
}

/**
 * @brief Запрос в фоновом режиме
 */
void SnmpRequest::run()
{
    switch (requestType) {
    case SnmpRequest::RequestType::Get:
        getRequest();
        break;
    case SnmpRequest::RequestType::GetNext:
        getNextRequest();
        break;
    case SnmpRequest::RequestType::Walk:
        walkRequest(true);
        break;
    case SnmpRequest::RequestType::WalkNoTranslate:
        walkRequest(false);
    default:
        break;
    }
}

/**
 * @brief Реализация синхронного Snmp-запроса с использованием блокирующего ввода/вывода в библиотеке Net-Snmp
 * @param sessp - непрозрачный указатель полученный при открытии сессии для чтения Snmp запросов
 * @param pdu - блок данных протокола snmp. Изначальная настройка для отправки Snmp запроса
 * @param response - блок данных протокола snmp. Результат Snmp запроса
 * @return - статус кода
 */
int SnmpRequest::snmpSyncResponse(void *sessp, netsnmp_pdu *pdu, netsnmp_pdu **response)
{
    netsnmp_session      *ss;
    struct synch_state    lstate, *state;
    int                   numfds = 0, count = 0;
    netsnmp_large_fd_set  fdset;
    struct timeval        timeout, *tvp;
    int                   block = 1;
    snmp_callback         cbsav;
    void                 *cbmagsav;

    ss = snmp_sess_session(sessp);
    if (ss == NULL)
    {
        return STAT_ERROR;
    }

    memset((void *) &lstate, 0, sizeof(lstate));
    state = &lstate;
    cbsav = ss->callback;
    cbmagsav = ss->callback_magic;
    ss->callback = SnmpRequest::snmpSynchInput;
    ss->callback_magic = (void *)state;
    netsnmp_large_fd_set_init(&fdset, FD_SETSIZE);

    if (snmp_sess_send(sessp, pdu) == 0)
    {
        snmp_free_pdu(pdu);
        state->status = STAT_ERROR;
        return state->status;
    } else {
        state->waiting = 1;
        state->reqid = pdu->reqid;
    }

    while (state->waiting)
    {
        numfds = 0;
        NETSNMP_LARGE_FD_ZERO(&fdset);
        block = NETSNMP_SNMPBLOCK;
        tvp = &timeout;
        timerclear(tvp);
        snmp_sess_select_info2_flags(sessp, &numfds, &fdset, tvp, &block,
                                     NETSNMP_SELECT_NOALARMS);
        if (block == 1)//Опрос закончен
        {
            state->status = STAT_TIMEOUT;
            netsnmp_large_fd_set_cleanup(&fdset);
            return state->status;
        }
        count = netsnmp_large_fd_set_select(numfds, &fdset, NULL, NULL, tvp);
        if (count > 0)
        {
            snmp_sess_read2(sessp, &fdset);
        }
        else
        {
            switch (count)
            {
            case 0:
                snmp_sess_timeout(sessp);
                break;
            case -1:
                if (errno == EINTR) {
                    continue;
                } else {
                    snmp_errno = SNMPERR_GENERR;    /*MTCRITICAL_RESOURCE */
                    snmp_set_detail(strerror(errno));
                    state->status = STAT_ERROR;
                    state->waiting = 0;
                }
                /* FALLTHRU */
            default:
                state->status = STAT_ERROR;
                state->waiting = 0;
            }
        }
    }
    *response = state->pdu;
    ss->callback = cbsav;
    ss->callback_magic = cbmagsav;
    netsnmp_large_fd_set_cleanup(&fdset);
    return state->status;
}

/**
 * @brief Функция обратного вызова callback, которая используется в контексте синхронного SNMP-запроса в библиотеке Net-SNMP
 * @param op - статус запроса
 * @param session - текущий настроенный сеанс для Snmp запроса
 * @param reqid - идентификатор запроса
 * @param pdu - блок данных протокола snmp. Результат Snmp запроса
 * @param magic - содержит адрес на callback функцию
 * @return
 */
int SnmpRequest::snmpSynchInput(int op, netsnmp_session * session, int reqid, netsnmp_pdu *pdu, void *magic)
{
    struct synch_state *state = (struct synch_state *) magic;
    int             rpt_type;

    if (reqid != state->reqid && pdu && pdu->command != 168) { // SNMP_MSG_REPORT = 168
        DEBUGMSGTL(("snmp_synch", "Unexpected response (ReqID: %d,%d - Cmd %d)\n",
                                   reqid, state->reqid, pdu->command ));
        return 0;
    }

    state->waiting = 0;
    DEBUGMSGTL(("snmp_synch", "Response (ReqID: %d - Cmd %d)\n",
                               reqid, (pdu ? pdu->command : -1)));

    if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && pdu) {
        if (pdu->command == 168) { // SNMP_MSG_REPORT = 168
            rpt_type = snmpv3_get_report_type(pdu);
            if (SNMPV3_IGNORE_UNAUTH_REPORTS ||
                rpt_type == SNMPERR_NOT_IN_TIME_WINDOW) {
                state->waiting = 1;
            }
            state->pdu = NULL;
            state->status = STAT_ERROR;
            session->s_snmp_errno = rpt_type;
            SET_SNMP_ERROR(rpt_type);
        } else if (pdu->command == 162) { // SNMP_MSG_RESPONSE = 162
            /*
             * clone the pdu to return to snmp_synch_response
             */
            state->pdu = snmp_clone_pdu(pdu);
            state->status = STAT_SUCCESS;
            session->s_snmp_errno = SNMPERR_SUCCESS;
        }
        else {
            char msg_buf[50];
            state->status = STAT_ERROR;
            session->s_snmp_errno = SNMPERR_PROTOCOL;
            SET_SNMP_ERROR(SNMPERR_PROTOCOL);
            snprintf(msg_buf, sizeof(msg_buf), "Expected RESPONSE-PDU but got %s-PDU",
                     snmp_pdu_type(pdu->command));
            snmp_set_detail(msg_buf);
            return 0;
        }
    } else if (op == NETSNMP_CALLBACK_OP_TIMED_OUT) {
        state->pdu = NULL;
        state->status = STAT_TIMEOUT;
        session->s_snmp_errno = SNMPERR_TIMEOUT;
        SET_SNMP_ERROR(SNMPERR_TIMEOUT);
    }
    else if (op == NETSNMP_CALLBACK_OP_DISCONNECT) {
        state->pdu = NULL;
        state->status = STAT_ERROR;
        session->s_snmp_errno = SNMPERR_ABORT;
        SET_SNMP_ERROR(SNMPERR_ABORT);
    }
    DEBUGMSGTL(("snmp_synch", "status = %d errno = %d\n",
                               state->status, session->s_snmp_errno));

    return 1;
}

/**
 * @brief Очистка текущей структуры PDU в результате Snmp запроса
 * @param response - блок данных протокола snmp. Результат Snmp запроса
 */
void SnmpRequest::clearingResponse(netsnmp_pdu *&value)
{
    if(value != nullptr)  {
        snmp_free_pdu(value);
        value = nullptr;
    }
}

/**
 * @brief Преобразование u_char** в QString
 * @param buf - переменная для преобразования
 * @param out_len - длина переменной
 * @return
 */
QString SnmpRequest::getStringFromBuf(u_char **buf, size_t *out_len){
    QString additionalInfo;

    for (uint i =0; i < *out_len; i++)
    {
        additionalInfo.append(static_cast<QChar>((*buf)[i]));
    }
    additionalInfo.remove(QRegExp("[\\n\\t\\r\\\"]"));
    additionalInfo = additionalInfo.trimmed();

    return additionalInfo;
}

/**
 * @brief Проверка текущего статуса при приеме Snmp запроса
 * @param sessp - непрозрачный указатель полученный при открытии сессии для чтения Snmp запросов
 * @param status - статус кода
 * @param setTextRequestResult - переменная указывающая нужно ли менять текст в ui->requestResult
 * @param response - блок данных протокола snmp. Результат Snmp запроса
 */
QString SnmpRequest::checkStatusResponse(void *sessp, int status, netsnmp_pdu *response, bool setTextRequestResult)
{
    switch(status)
    {
    case STAT_SUCCESS:
    {
        if(response && response->errstat == (0)) // SNMP_ERR_NOERROR = (0)
        {
            for(auto vars = response->variables; vars; vars = vars->next_variable)
            {
                if (setTextRequestResult) {
                    u_char* buf = (u_char*)calloc(64, 1);
                    std::size_t buf_len = 64, out_len = 0;
                    SnmpTrapLog* tmp = new SnmpTrapLog();
                    int res = 0;

                    if (((netsnmp_session*)sessp)->version == 0)
                        res = tmp->translateAdditionalInfoTrap(&buf, &buf_len, &out_len, 1, SYSLOG_V1_STANDARD_FORMAT, response, reinterpret_cast<session_list*>(sessp)->transport);
                    else
                        res = tmp->translateAdditionalInfoTrap(&buf, &buf_len, &out_len, 1, SYSLOG_V23_NOTIFICATION_FORMAT, response, reinterpret_cast<session_list*>(sessp)->transport);
                    delete tmp;

                    if (getParseValue(vars).toString() == "")
                        return "Error in packet\nReason: (noSuchName) There is no such variable name in this MIB.";
                    else {
                        if (res)
                            return getStringFromBuf(&buf, &out_len).mid(2) + "\n";
                        else
                            return ParseCurrentOid(vars) + " = " + getParseValue(vars).toString() + "\n";
                    }
                    free(buf);
                }
            }
        }
        else if (response && response->errstat != (0))//При ошибке структуры PDU исключение не выбрасываем
        {
            QString messageError = QString(snmp_errstring(response->errstat));
            for(auto vars = response->variables; vars; vars = vars->next_variable)
            {
                QString OidName = ParseCurrentOid(vars);
                if(!OidName.isEmpty())//Добавляем в сообщение OID
                {
                    messageError += " Oid: " + OidName;
                }
                qDebug() << "SnmpSender" << messageError;
            }
            return messageError;
            qDebug() << "SnmpSender::checkStatusResponse " << messageError << "Index: " << response->errindex;
        }
    }break;
    case STAT_TIMEOUT:
    {
        return "Timeout: No response";
        qDebug() << "SnmpSender::checkStatusResponse::Timeout: No response from";
        // throw std::runtime_error("SnmpSender::checkStatusResponse::Timeout: No response");
    }break;
    case STAT_ERROR:
    {
        int liberr, syserr;
        char *errstr = nullptr;
        snmp_sess_error(sessp, &liberr, &syserr, &errstr);
        QString errorMessage = QString(errstr);
        free(errstr);
        if(errorMessage.isEmpty())
        {
            return "Unknown Error";
            qDebug() << QString("SnmpSender::checkStatusResponse:: Unknown Error");
            // throw std::runtime_error("SnmpSender::checkStatusResponse:: Unknown Error");
        }
        else
        {
            return errorMessage;
            qDebug() << QString("SnmpSender::checkStatusResponse:: %1").arg(errorMessage);
            // throw std::runtime_error("SnmpSender::checkStatusResponse:: " + errorMessage.toStdString());
        }

    }break;

    }
}

/**
 * @brief Преобразования переменной netsnmp_variable_list, представляющей собой OID в строку типа QString
 * @param currentVars - структура переменных Snmp
 * @return
 */
QString SnmpRequest::ParseCurrentOid(netsnmp_variable_list *currentVars)
{
    QString currentOid;
    for(uint i = 0; i < (currentVars->name_length); i++)
    {
        currentOid += QString::number(currentVars->name[i]);
        if ((i + 1) < (currentVars->name_length))
            currentOid += ".";
    }
    return currentOid;
}

/**
 * @brief Функция для преобразования переменных в читаемое значение
 * @param vars - структура переменных Snmp
 * @return
 */
QVariant SnmpRequest::getParseValue(netsnmp_variable_list *vars)
{
    QString answerMessage = "";
    QVariant data;//Основной ответ
    switch(vars->type)
    {
    case Form::Asn1DataType::Integer:
        answerMessage += QString::number(*vars->val.integer);
        data = QVariant((int)*vars->val.integer);
        break;
    case Form::Asn1DataType::OctetString:
    {
        bool checkHex = false;
        for (int i = 0; i < (int)vars->val_len; i++){
            if ((int)vars->val.string[i] < 0x20 || (int)vars->val.string[i] > 0x7f){
                checkHex = true;
                break;
            }
        }

        if (checkHex){
            uint base = 16;
            for (int i = 0; i < (int)vars->val_len; i++){
                if ((int)vars->val.string[i] < 10) answerMessage += "0" + QString::number((int)vars->val.string[i], base) + " ";
                else answerMessage += QString::number((int)vars->val.string[i], base) + " ";
            }
            data = QVariant(answerMessage);
        }
        else {
            answerMessage += QString::fromLocal8Bit((char*)vars->val.string, (int)vars->val_len);
            data = QVariant(QString::fromLocal8Bit((char*)vars->val.string, (int)vars->val_len));
        }
    }break;
    case Form::Asn1DataType::Oid:
    {
        QString blockOid;
        for(uint i = 0; i < (vars->val_len/(sizeof(std::size_t))); i++)
        {
            blockOid += QString::number(vars->val.objid[i]);
            if ((i + 1) < (vars->val_len/(sizeof(std::size_t))))
                blockOid += ".";
        }
        data = QVariant(blockOid);
        break;
    }
    case Form::Asn1DataType::IpAddress:
    {
        QString tempMessage = "";
        answerMessage = QString::fromLatin1((char*)vars->val.string, (int)vars->val_len);
        for(quint8 i =0; i < answerMessage.size(); i++)
        {
            uint tempIP = answerMessage.at(i).unicode();
            tempMessage += QString::number(tempIP);
            if((i+1) < answerMessage.size())
                tempMessage += ".";
        }
        data = QVariant(tempMessage);
    }break;
    case Form::Asn1DataType::TimeTicks:
    {
        quint64 high = 0;
        if(vars->val.counter64)
        {
            high = vars->val.counter64->high;
        }
        QTime time(0,0,0);
        long seconds = high/100;//Количество целых секунд
        long santiSecond = high % 100;//Количество сотых долей секунд
        long days = seconds / 86400;//Количество дней
        time = time.addSecs(seconds);//Преобразуем в формат hh::mm::ss
        data = QString("Timeticks: (%1) %2 days, %3.%4 ").arg(QString::number(high)).arg(QString::number(days)).arg(time.toString("hh:mm:ss")).arg(QString::number(santiSecond));
    }break;
    case Form::Asn1DataType::Gauge:
    case Form::Asn1DataType::Counter:
    {
        quint64 high = 0;
        if(vars->val.counter64)
        {
            high = vars->val.counter64->high;
        }
        data = QVariant(high);
    }break;
    case Form::Asn1DataType::Counter64:
    {
        quint64 low = 0;
        quint64 high = 0;
        if(vars->val.counter64)
        {
            low = vars->val.counter64->low;
            high = vars->val.counter64->high;
        }
        data = QVariant(low + high);
    }break;
    default:
        answerMessage += "not supported format";
        break;
    }
    return data;
}

/**
 * @brief Отправка Snmp запросов типа GET для получения данных с оборудования (устройства)
 * @param sessp - непрозрачный указатель полученный при открытии сессии для чтения Snmp запросов
 * @param variablesSnmp - переменные Snmp
 * @param snmpMessageType - тип PDU
 * @param emitSignal - флаг для отправки сигнала
 */
QString SnmpRequest::sendSnmpGetRequest(void *sessp, QString oidString, const int snmpMessageType)
{
    netsnmp_pdu     *pdu{};
    oid             name[MAX_OID_LEN];
    std::size_t     name_length;
    int             failures = 0;//Количество неудачных
    netsnmp_pdu     *response{};

    pdu = snmp_pdu_create(snmpMessageType);

    name_length = MAX_OID_LEN;
    if (!snmp_parse_oid(oidString.toLocal8Bit().data(), name, &name_length)) {
        snmp_perror(oidString.toLocal8Bit().data());
        failures++;
    }
    else
        snmp_add_null_var(pdu, name, name_length);

    if(pdu->variables == nullptr && failures > 0)
    {
        qDebug() << "SnmpSender::sendSnmpGetRequest::countFailuresVariables:" << failures;
        snmp_free_pdu(pdu);
        return checkStatusResponse(sessp, STAT_ERROR, NULL, false);
    } else {
        int status = snmpSyncResponse(sessp, pdu, &response);//Основной запрос
        QString result = checkStatusResponse(sessp, status, response, true);//Проверка ответа
        clearingResponse(response);
        return result;
    }
}

/**
 * @brief Отправка серий Snmp запросов типа GET-NEXT для получения данных с оборудования (устройства)
 * @param sessp непрозрачный указатель полученный при открытии сессии для чтения Snmp запросов
 * @param currentSnmpOid идентификатор объекта
 * @param translate Транслирование OID индексов
 * @return Результат snmpwalk
 */
QString SnmpRequest::snmpWalk(void *sessp, QString currentSnmpOid, bool translate)
{
    qDebug() << "SnmpSender" << 161 << "address:" << sessp;

    netsnmp_pdu    *pdu{};
    oid             name[MAX_OID_LEN];
    std::size_t     name_length;
    int             failures = 0;
    netsnmp_pdu     *response{};

    QString defaultOid = currentSnmpOid;//Исходный oid для сравнения
    QString result;

    while(!(currentSnmpOid.isEmpty()))
    {
        qDebug() << "SnmpSender" << "While condition" << "address:" << sessp;

        pdu = snmp_pdu_create(161);

        name_length = MAX_OID_LEN;
        if (!snmp_parse_oid(currentSnmpOid.toLocal8Bit().data(), name, &name_length)) {
            snmp_perror(currentSnmpOid.toLocal8Bit().data());
            failures++;
        } else
            snmp_add_null_var(pdu, name, name_length);

        if(pdu->variables == nullptr && failures > 0)
        {
            qDebug() << "SnmpSender::sendSnmpGetRequest::countFailuresVariables:" << failures;
            snmp_free_pdu(pdu);
            checkStatusResponse(sessp, STAT_ERROR, NULL, false);
            break;
        }

        clearingResponse(response);//Очистка структуры response

        int status = snmpSyncResponse(sessp, pdu, &response);//Отправляем запрос

        if(status == STAT_SUCCESS)
        {
            if(response && response->errstat == (0))
            {
                currentSnmpOid = ParseCurrentOid(response->variables);//Для дальнейшего опроса по snmpWalk
                QVariant currentValueVariable = getParseValue(response->variables);//Значение данного oid

                qDebug() << "SnmpSender" << "Oid:" << currentSnmpOid << "Value: " << currentValueVariable.toString();

                if(defaultOid == currentSnmpOid)//Если oid полностью совпадают при опросе, то это означает, что это конец mib или строки
                {
                    u_char* buf = {};
                    buf = (u_char*)calloc(64, 1);
                    std::size_t buf_len = 64, out_len = 0;
                    SnmpTrapLog* tmp = new SnmpTrapLog();
                    int res = 0;

                    if (((netsnmp_session*)sessp)->version == 0)
                        res = tmp->translateAdditionalInfoTrap(&buf, &buf_len, &out_len, 1, SYSLOG_V1_STANDARD_FORMAT, response, reinterpret_cast<session_list*>(sessp)->transport);
                    else
                        res = tmp->translateAdditionalInfoTrap(&buf, &buf_len, &out_len, 1, SYSLOG_V23_NOTIFICATION_FORMAT, response, reinterpret_cast<session_list*>(sessp)->transport);
                    delete tmp;

                    if (getParseValue(response->variables).toString() == "")
                        qDebug() << "Error in packet\nReason: (noSuchName) There is no such variable name in this MIB.";
                    else {
                        if (res)
                            result += getStringFromBuf(&buf, &out_len).mid(2) + "\n";
                        else
                            result += currentSnmpOid + " = " + currentValueVariable.toString() + "\n";
                    }
                    free(buf);
                    qDebug() << currentSnmpOid << currentValueVariable;
                    break;//Завершаем цикл
                }
                if(currentSnmpOid.startsWith(defaultOid))//Продолжаем опрос по snmpwalk
                {
                    u_char* buf = {};
                    buf = (u_char*)calloc(64, 1);
                    std::size_t buf_len = 64, out_len = 0;
                    SnmpTrapLog* tmp = new SnmpTrapLog();
                    int res = 0;

                    if (((netsnmp_session*)sessp)->version == 0)
                        res = tmp->translateAdditionalInfoTrap(&buf, &buf_len, &out_len, 1, SYSLOG_V1_STANDARD_FORMAT, response, reinterpret_cast<session_list*>(sessp)->transport);
                    else
                        res = tmp->translateAdditionalInfoTrap(&buf, &buf_len, &out_len, 1, SYSLOG_V23_NOTIFICATION_FORMAT, response, reinterpret_cast<session_list*>(sessp)->transport);
                    delete tmp;

                    if (getParseValue(response->variables).toString() == "")
                        qDebug() << "Error in packet\nReason: (noSuchName) There is no such variable name in this MIB.";
                    else {
                        if (res) {
                            if (translate)
                                result += getStringFromBuf(&buf, &out_len).mid(2) + "\n";
                            else
                                result += currentSnmpOid + " = " + (getStringFromBuf(&buf, &out_len).mid(2)).split(" = ")[1] + "\n";
                        }
                        else
                            result += currentSnmpOid + " = " + currentValueVariable.toString() + "\n";
                    }
                    free(buf);
                    qDebug() << currentSnmpOid << currentValueVariable;
                }
                else //Если oid не совпадают вначале, то отправляем сигнал
                {
                    break;
                }
            }
            else if (response && response->errstat != (0))//При ошибке PDU исключение не выбрасываем
            {
                QString messageError = QString(snmp_errstring(response->errstat));
                for(auto vars = response->variables; vars; vars = vars->next_variable)
                {
                    QString OidName = ParseCurrentOid(vars);
                    if(!OidName.isEmpty())
                    {
                        messageError += " Oid: " + OidName;
                    }
                    qDebug() << "SnmpSender" << messageError;
                }
                qDebug() << "SnmpSender::checkStatusResponse " << messageError << "Index: " << response->errindex;
                break;//Завершаем цикл
            }
        }
        else if(status == STAT_TIMEOUT)
        {
            qDebug() << "SnmpSender::checkStatusResponse::Timeout: No response from";
            throw std::runtime_error("SnmpSender::checkStatusResponse::Timeout: No response");
        }
        else
        {
            int liberr, syserr;
            char *errstr;
            snmp_sess_error(sessp, &liberr, &syserr, &errstr);
            QString errorMessage = QString(errstr);
            free(errstr);
            if(errorMessage.isEmpty())
            {
                qDebug() << "SnmpSender::checkStatusResponse:: Unknown Error";
                throw std::runtime_error("SnmpSender::checkStatusResponse:: Unknown Error");
            }
            else
            {
                qDebug() << QString("SnmpSender::checkStatusResponse:: %1").arg(errorMessage);
                throw std::runtime_error("SnmpSender::checkStatusResponse:: " + errorMessage.toStdString());
            }
        }
    }
    clearingResponse(response);//Очистка структуры response
    qDebug() << "SnmpMessageType" << 161 << "address:" << sessp << "done";
    return result;
}

/**
 * @brief Обертка GET запроса
 */
void SnmpRequest::getRequest()
{
    emit sendResult(sendSnmpGetRequest(session, currentOid, 160), true);
}

/**
 * @brief Обертка GET-NEXT запроса
 */
void SnmpRequest::getNextRequest()
{
    emit sendResult(sendSnmpGetRequest(session, currentOid, 161), true);
}

/**
 * @brief Обертка Walk запроса
 * @param translate - Транслирование OID индексов
 */
void SnmpRequest::walkRequest(bool translate)
{
    emit sendResult(snmpWalk(session, currentOid, translate), printResult);
}
