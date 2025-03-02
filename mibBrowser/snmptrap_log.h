#ifndef SNMPTRAP_LOG_H
#define SNMPTRAP_LOG_H

/**
*     @defgroup     SnmpTrapTranslator
*
*     @file         snmptrap_log.h
*
*     @brief        Считывания и форматирования информации с SNMP типа TRAP и INFORM
*
*     @author       Konychev Dmitry
*
*     Copyright&copy; Morion Inc. 2024
*/

//#include "include.h"

#include "../net-snmp-5.7.3/include/net-snmp/net-snmp-config.h"
#include "../net-snmp-5.7.3/include/net-snmp//net-snmp-includes.h"
#include "../net-snmp-5.7.3/include/net-snmp//definitions.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/tools.h"
#include "../net-snmp-5.7.3/include/net-snmp/mib_api.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/mib.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/parse.h"
#include "../net-snmp-5.7.3/include/net-snmp/agent/net-snmp-agent-includes.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/fd_event_manager.h"
#include "../net-snmp-5.7.3/include/net-snmp/library/vacm.h"
#include "../net-snmp-5.7.3/apps/snmptrapd_handlers.h"
#include "../net-snmp-5.7.3/apps/snmptrapd_auth.h"
#include "../net-snmp-5.7.3/include/net-snmp/net-snmp-config.h"

#include <iostream>
using namespace std;

#ifndef POS
#define POS  "SnmpTrapLog::" << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << ")"
#endif
#ifndef POSF
#define POSF __FUNCTION__
#endif

#ifndef BSD4_3
#define BSD4_2
#endif

/*
 * These flags mark undefined values in the options structure
 */
#define UNDEF_CMD '*'
#define UNDEF_PRECISION -1

/*
 * macros
 */

#define is_cur_time_cmd(chr) ((((chr) == CHR_CUR_TIME)     \
    || ((chr) == CHR_CUR_YEAR)  \
    || ((chr) == CHR_CUR_MONTH) \
    || ((chr) == CHR_CUR_MDAY)  \
    || ((chr) == CHR_CUR_HOUR)  \
    || ((chr) == CHR_CUR_MIN)   \
    || ((chr) == CHR_CUR_SEC)) ? TRUE : FALSE)
/*
      * Function:
      *    Returns true if the character is a format command that outputs
      * some field that deals with the current time.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define is_up_time_cmd(chr) ((((chr) == CHR_UP_TIME)     \
    || ((chr) == CHR_UP_YEAR)  \
    || ((chr) == CHR_UP_MONTH) \
    || ((chr) == CHR_UP_MDAY)  \
    || ((chr) == CHR_UP_HOUR)  \
    || ((chr) == CHR_UP_MIN)   \
    || ((chr) == CHR_UP_SEC)) ? TRUE : FALSE)
/*
      * Function:
      *    Returns true if the character is a format command that outputs
      * some field that deals with up-time.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define is_agent_cmd(chr) ((((chr) == CHR_AGENT_IP) \
    || ((chr) == CHR_AGENT_NAME)) ? TRUE : FALSE)
/*
      * Function:
      *    Returns true if the character outputs information about the
      * agent.
      *
      * Input Parameters:
      *    chr - the character to check
      */

#define is_pdu_ip_cmd(chr) ((((chr) == CHR_PDU_IP)   \
    || ((chr) == CHR_PDU_NAME)) ? TRUE : FALSE)

/*
      * Function:
      *    Returns true if the character outputs information about the SNMP
      *      authentication information
      * Input Parameters:
      *    chr - the character to check
      */

#define is_auth_cmd(chr) ((((chr) == CHR_SNMP_VERSION       \
    || (chr) == CHR_SNMP_SECMOD     \
    || (chr) == CHR_SNMP_USER)) ? TRUE : FALSE)

/*
      * Function:
      *    Returns true if the character outputs information about the PDU's
      * host name or IP address.
      *
      * Input Parameters:
      *    chr - the character to check
      */

#define is_trap_cmd(chr) ((((chr) == CHR_TRAP_NUM)      \
    || ((chr) == CHR_TRAP_DESC)  \
    || ((chr) == CHR_TRAP_STYPE) \
    || ((chr) == CHR_TRAP_VARS)) ? TRUE : FALSE)

/*
      * Function:
      *    Returns true if the character outputs information about the trap.
      *
      * Input Parameters:
      *    chr - the character to check
      */

#define is_fmt_cmd(chr) ((is_cur_time_cmd (chr)     \
    || is_up_time_cmd (chr)   \
    || is_auth_cmd (chr)   \
    || is_agent_cmd (chr)     \
    || is_pdu_ip_cmd (chr)    \
    || ((chr) == CHR_PDU_ENT) \
    || ((chr) == CHR_TRAP_CONTEXTID) \
    || ((chr) == CHR_PDU_WRAP) \
    || is_trap_cmd (chr)) ? TRUE : FALSE)
/*
      * Function:
      *    Returns true if the character is a format command.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define is_numeric_cmd(chr) ((is_cur_time_cmd(chr)   \
    || is_up_time_cmd(chr) \
    || (chr) == CHR_TRAP_NUM) ? TRUE : FALSE)
/*
      * Function:
      *    Returns true if this is a numeric format command.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define reference(var) ((var) == (var))

/**
 * @brief The SnmpTrapLog class
 * Класс, предоставляющий функциональность для форматирования и записи информации о SNMP типа TRAP и INFORM
 */
class SnmpTrapLog
{
public:
    /**
     * @brief options_type
     * Структура содержит параметры для одной команды форматирования
     */
    typedef struct {
        char            cmd;        /* the format command itself */
        size_t          width;      /* the field's minimum width */
        int             precision;  /* the field's precision */
        int             left_justify;       /* if true, left justify this field */
        int             alt_format; /* if true, display in alternate format */
        int             leading_zeroes;     /* if true, display with leading zeroes */
    } options_type;
    /**
     * @brief separator
     * Разделение переменных при форматировании вывода
     */
    static char            separator[32];
    /**
     * @brief The parse_chr_type enum
     * Структура содержит символы, который распознает синтаксический анализатор
     */
    typedef enum {
        CHR_FMT_DELIM = '%',        /* starts a format command */
        CHR_LEFT_JUST = '-',        /* left justify */
        CHR_LEAD_ZERO = '0',        /* use leading zeroes */
        CHR_ALT_FORM = '#',         /* use alternate format */
        CHR_FIELD_SEP = '.',        /* separates width and precision fields */

        /* Date / Time Information */
        CHR_CUR_TIME = 't',         /* current time, Unix format */
        CHR_CUR_YEAR = 'y',         /* current year */
        CHR_CUR_MONTH = 'm',        /* current month */
        CHR_CUR_MDAY = 'l',         /* current day of month */
        CHR_CUR_HOUR = 'h',         /* current hour */
        CHR_CUR_MIN = 'j',          /* current minute */
        CHR_CUR_SEC = 'k',          /* current second */
        CHR_UP_TIME = 'T',          /* uptime, Unix format */
        CHR_UP_YEAR = 'Y',          /* uptime year */
        CHR_UP_MONTH = 'M',         /* uptime month */
        CHR_UP_MDAY = 'L',          /* uptime day of month */
        CHR_UP_HOUR = 'H',          /* uptime hour */
        CHR_UP_MIN = 'J',           /* uptime minute */
        CHR_UP_SEC = 'K',           /* uptime second */

        /* transport information */
        CHR_AGENT_IP = 'a',         /* agent's IP address */
        CHR_AGENT_NAME = 'A',       /* agent's host name if available */

        /* authentication information */
        CHR_SNMP_VERSION = 's',     /* SNMP Version Number */
        CHR_SNMP_SECMOD  = 'S',     /* SNMPv3 Security Model Version Number */
        CHR_SNMP_USER = 'u',        /* SNMPv3 secName or v1/v2c community */
        CHR_TRAP_CONTEXTID = 'E',   /* SNMPv3 context engineID if available */

        /* PDU information */
        CHR_PDU_IP = 'b',           /* PDU's IP address */
        CHR_PDU_NAME = 'B',         /* PDU's host name if available */
        CHR_PDU_ENT = 'N',          /* PDU's enterprise string */
        CHR_PDU_WRAP = 'P',         /* PDU's wrapper info (community, security) */
        CHR_TRAP_NUM = 'w',         /* trap number */
        CHR_TRAP_DESC = 'W',        /* trap's description (textual) */
        CHR_TRAP_STYPE = 'q',       /* trap's subtype */
        CHR_TRAP_VARSEP = 'V',      /* character (or string) to separate variables */
        CHR_TRAP_VARS = 'v'        /* tab-separated list of trap's variables */

    } parse_chr_type;
    /**
     * @brief The parse_state_type enum
     * Структура содержит символы предназначенные для состояния конечного автомата анализатора
     */
    typedef enum {
        PARSE_NORMAL,               /* looking for next character */
        PARSE_BACKSLASH,            /* saw a backslash */
        PARSE_IN_FORMAT,            /* saw a % sign, in a format command */
        PARSE_GET_WIDTH,            /* getting field width */
        PARSE_GET_PRECISION,        /* getting field precision */
        PARSE_GET_SEPARATOR         /* getting field separator */
    } parse_state_type;
    /**
     * @brief init_options
     * Инициализация структуры, содержащую параметры для команды форматирования
     * @param options - структура команды форматирования
     */
    static void init_options(options_type * options);
    /**
     * @brief realloc_output_temp_bfr
     * Добавляет содержимое временного буфера в указанный буфер
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param temp_buf - указатель для добавления в выходной буфер
     * @param options - структура команды форматирования
     * @return - статус выполнения
     */
    static int realloc_output_temp_bfr(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, u_char ** temp_buf, options_type * options);
    /**
     * @brief realloc_handle_time_fmt
     * Обработка команды форматирования, которая имеет дело с текущим временем или временем безотказной работы
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param options - указатель для добавления в выходной буфер
     * @param pdu - структура блока данных протокола snmp
     * @return - статус выполнения
     */
    static int realloc_handle_time_fmt(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, options_type * options, netsnmp_pdu *pdu);
    /**
     * @brief convert_agent_addr
     * Получение строкового представления IP-адреса агента Snmp
     * @param agent_addr - структура для представления IPv4-адреса
     * @param name - имя хоста
     * @param size - размер хоста
     */
    static void convert_agent_addr(struct in_addr agent_addr, char *name, size_t size);
    /**
     * @brief realloc_handle_ip_fmt
     * Обрабатывает команду форматирования, которая имеет дело с IP-адресом или именем хоста
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param options - структура команды форматирования
     * @param pdu - структура блока данных протокола snmp
     * @param transport - идентифкитор объекта транспортного домена
     * @return - статус выполнения
     */
    static int realloc_handle_ip_fmt(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, options_type * options, netsnmp_pdu *pdu, netsnmp_transport *transport);
    /**
     * @brief realloc_handle_ent_fmt
     * Обработка команды форматирования, которая имеет дело со строками OID
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param options - структура команды форматирования
     * @param pdu - структура блока данных протокола snmp
     * @return - статус выполнения
     */
    static int realloc_handle_ent_fmt(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, options_type * options, netsnmp_pdu *pdu);
    /**
     * @brief realloc_handle_trap_fmt
     * Обработка команды форматирования, которая имеет дело с Snmp трапом
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param options - структура команды форматирования
     * @param pdu - структура блока данных протокола snmp
     * @return - статус выполнения
     */
    static int realloc_handle_trap_fmt(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, options_type * options, netsnmp_pdu *pdu);

    /**
     * @brief realloc_handle_auth_fmt
     * Обработка команды форматирования, которая имеет дело с аутентификационной информацией
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param options - структура команды форматирования
     * @param pdu - структура блока данных протокола snmp
     * @return - статус выполнения
     */
    static int realloc_handle_auth_fmt(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, options_type * options, netsnmp_pdu *pdu);
    /**
     * @brief realloc_handle_wrap_fmt
     * Обработка команды форматирования, которая имеет дело с типом команды, строка сообщества и т.д
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param pdu - структура блока данных протокола snmp
     * @return - статус выполнения
     */
    static int realloc_handle_wrap_fmt(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, netsnmp_pdu *pdu);
    /**
     * @brief realloc_dispatch_format_cmd
     * Обработка различных команд форматирования
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param options - структура команды форматирования
     * @param pdu - структура блока данных протокола snmp
     * @param transport - идентифкитор объекта транспортного домена
     * @return - статус выполнения
     */
    static int realloc_dispatch_format_cmd(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, options_type * options, netsnmp_pdu *pdu, netsnmp_transport *transport);
    /**
     * @brief realloc_handle_backslash
     * Обработка символа '\'
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param fmt_cmd - символ после '\'
     * @return - статус выполнения
     */
    static int realloc_handle_backslash(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, char fmt_cmd);
//    /**
//     * @brief realloc_format_trap
//     * Обработка Snmp-трапа для отображения в журнале событий
//     * @param buf - указатель на буфер
//     * @param buf_len - размер буфера
//     * @param out_len - выходной размер
//     * @param allow_realloc - статус перераспределения
//     * @param format_str - указывает, как отформатировать информацию о Snmp трапе
//     * @param pdu - структура блока данных протокола snmp
//     * @param transport - идентифкитор объекта транспортного домена
//     * @return - статус выполнения
//     */
//    int realloc_format_trap(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, const char *format_str, netsnmp_pdu *pdu, netsnmp_transport *transport);
    /**
     * @brief translateAdditionalInfoTrap
     * Обработка чтения информации из распарсенного Mib файла, когда фильтр не был найден в csv файле
     * @param buf - указатель на буфер
     * @param buf_len - размер буфера
     * @param out_len - выходной размер
     * @param allow_realloc - статус перераспределения
     * @param format_str - указывает, как отформатировать информацию о Snmp трапе
     * @param pdu - структура блока данных протокола snmp
     * @param transport - идентифкитор объекта транспортного домена
     * @return - статус выполнения
     */
    int translateAdditionalInfoTrap(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, const char *format_str, netsnmp_pdu *pdu, netsnmp_transport *transport);
};

#endif // SNMPTRAP_LOG_H
