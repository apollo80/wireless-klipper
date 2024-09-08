/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "pty2udp-proxy.h"
#include <string.h>

typedef void (*option_func_t)(struct pty2udp_proxy* proxy, const char* key, const char* value);

static void proxy_option(struct pty2udp_proxy* proxy, const char* key, const char* value);
static void unknown_option(struct pty2udp_proxy* proxy, const char* key, const char* value);


void config_read(const char* cfg_filename) {
    if (!cfg_filename) {
        fprintf(stderr, "failed: invalid filename");
        return;
    }

    FILE* cfgfile = fopen(cfg_filename, "r");
    if(!cfgfile) {
        fprintf(stderr, "failed: can't open file %s: %i", cfg_filename, errno);
        return;
    }

    int line_size = 1024;
    char* line = calloc(line_size, sizeof(char));
    size_t line_num = 0;

    char* section = calloc(line_size, sizeof(char));
    option_func_t option_func = unknown_option;
    struct pty2udp_proxy* proxy = NULL;

    while(NULL != fgets(line, line_size, cfgfile)) {
        line_num++;

        size_t line_len = strlen(line);
        char* ptr = line;

        // ищем первый символ, отличный от пробельный (пробел, табуляци и т.п.)
        while (*ptr && (*ptr == ' ' || *ptr == '\f' || *ptr == '\t' || *ptr == '\v'))
            ptr++;

        // если строке не содержит не пробельных символов - пропускам ее
        if (! *ptr)
            continue;

        // если первый не пробельный символ является символом комментария - пропускам строку
        if (*ptr == '#' || *ptr == ';')
            continue;

        if ( *ptr == '[' )
        {
            ptr++;

            // после символа '[' ищем первый не пробельный символ
            while (*ptr && (*ptr == ' ' || *ptr == '\f' || *ptr == '\t' || *ptr == '\v'))
                ptr++;

            char* section_name = ptr;
            char* end = ptr;

            // следом ищем символ ']'
            while(*end && *end != ']')
                ++end;

            // если не символ ']' не найден - ошибка
            if(! *end) {
                fprintf(stderr, "failed read config: error in line %zu\n", line_num);
                break;

                // free(line);
                // free(section);
                // fclose(cfgfile);
                // return;
            }

            end--;

            // удаляем лишние пробельные символы в конце строки
            while(*end == ' ' || *end == '\f' || *end == '\t' || *end == '\v')
                end--;

            end++;
            *end = 0;

            // выделяем из строки значение секции
            if (0 == strcasecmp(section_name, "proxy")) {
                proxy = pty2udp_proxy_new();
                option_func = proxy_option;
            } else
                option_func = unknown_option;

            continue;
        }

        // определяем начало ключа
        char* key = ptr;

        // пропускаем пустую строку
        if (*ptr == '\r' || *ptr == '\n')
            continue;

        // ищем разделитель - знак разделителя ':'
        while(*ptr && *ptr != ':')
            ptr++;

        // если разделитель не найден - ошибка
        if (! *ptr) {
            fprintf(stderr, "failed read config: error in line %zu\n", line_num);
            break;

            // free(line);
            // free(section);
            // fclose(cfgfile);
            // return;
        }

        // выделяем из строки значение ключа
        {
            char *end = ptr - 1;
            while (*end == ' ' || *end == '\f' || *end == '\t' || *end == '\v')
                end--;

            end++;
            *end = 0;
        }

        // выделяем из строки значение параметра ключа
        char* value = ptr + 1;
        while(*value && (*value == ' ' || *value == '\f' || *value == '\t' || *value == '\v'))
            value++;

        // выделяем из строки значение параметра ключа
        {
            char *end = line + line_len - 1;
            while (*end == ' ' || *end == '\f' || *end == '\t' || *end == '\v'  || *end == '\r' || *end == '\n' )
                end--;

            end++;
            *end = 0;
        }

        option_func(proxy, key, value);
    }

    free(line);
    free(section);
    fclose(cfgfile);
}

void proxy_option(struct pty2udp_proxy* proxy, const char* key, const char* value) {

    if (! proxy)
        return;

    if (! key)
        return;

    if (! value)
        return;

    if (0 == strcasecmp(key, "serial_path")) {
        memcpy(proxy->serial_path, value, strlen(value));
        return;
    }

    if (0 == strcasecmp(key, "serial_baud")) {
        proxy->serial_baud = strtoul(value, NULL, 0);
        return;
    }

    if (0 == strcasecmp(key, "server_name")) {
        memcpy(proxy->server_name, value, strlen(value));
        return;
    }

    if (0 == strcasecmp(key, "server_port")) {
        proxy->server_port = strtoul(value, NULL, 0);
        return;
    }

    if (0 == strcasecmp(key, "log_file")) {
        memcpy(proxy->log_path, value, strlen(value));
        return;
    }

    if (0 == strcasecmp(key, "log_level")) {
        if (0 == strcasecmp(value, "trace")) {
            proxy->log_level = TRACE;
        } else if (0 == strcasecmp(value, "debug")) {
            proxy->log_level = DEBUG;
        } else if (0 == strcasecmp(value, "info")) {
            proxy->log_level = INFO;
        } else if (0 == strcasecmp(value, "warning")) {
            proxy->log_level = WARNING;
        } else if (0 == strcasecmp(value, "error")) {
            proxy->log_level = ERROR;
        } else if (0 == strcasecmp(value, "critical")) {
            proxy->log_level = CRITICAL;
        }

        return;
    }

}

void unknown_option(struct pty2udp_proxy* proxy, const char* key, const char* value) {

}
