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
        fprintf(stderr, "failed: invalid filename\n");
        return;
    }

    FILE* cfgfile = fopen(cfg_filename, "r");
    if(!cfgfile) {
        fprintf(stderr, "failed: can't open file %s: %i\n", cfg_filename, errno);
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

        // looking for the first character other than the space character (space, tab, etc.)
        while (*ptr && (*ptr == ' ' || *ptr == '\f' || *ptr == '\t' || *ptr == '\v'))
            ptr++;

        // if the string does not contain non-whitespace characters, skip it
        if (! *ptr)
            continue;

        // if the first non-whitespace character is a comment character, skip the line
        if (*ptr == '#' || *ptr == ';')
            continue;

        if ( *ptr == '[' )
        {
            ptr++;

            // after the '[' character, looking for the first non-whitespace character
            while (*ptr && (*ptr == ' ' || *ptr == '\f' || *ptr == '\t' || *ptr == '\v'))
                ptr++;

            char* section_name = ptr;
            char* end = ptr;

            // next, looking for the symbol ']'
            while(*end && *end != ']')
                ++end;

            // if not the character ']' is not found - an error
            if(! *end) {
                fprintf(stderr, "failed read config: error in line %zu\n", line_num);
                break;

                // free(line);
                // free(section);
                // fclose(cfgfile);
                // return;
            }

            end--;

            // removing extra whitespace characters at the end of the line
            while(*end == ' ' || *end == '\f' || *end == '\t' || *end == '\v')
                end--;

            end++;
            *end = 0;

            // cutting the value of the section from the string
            if (0 == strcasecmp(section_name, "proxy")) {
                proxy = pty2udp_proxy_new();
                option_func = proxy_option;
            } else
                option_func = unknown_option;

            continue;
        }

        // determining the beginning of the key
        char* key = ptr;

        // skip the empty line
        if (*ptr == '\r' || *ptr == '\n')
            continue;

        // looking for a separator - the separator sign ':'
        while(*ptr && *ptr != ':')
            ptr++;

        // if the separator is not found, an error occurs
        if (! *ptr) {
            fprintf(stderr, "failed read config: error in line %zu\n", line_num);
            break;

            // free(line);
            // free(section);
            // fclose(cfgfile);
            // return;
        }

        // cutting out the key value from the string
        {
            char *end = ptr - 1;
            while (*end == ' ' || *end == '\f' || *end == '\t' || *end == '\v')
                end--;

            end++;
            *end = 0;
        }

        // cutting out the value of the key parameter from the string
        char* value = ptr + 1;
        while(*value && (*value == ' ' || *value == '\f' || *value == '\t' || *value == '\v'))
            value++;

        // cutting out the value of the key parameter from the string
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
        memcpy(proxy->serial.path, value, strlen(value));
        return;
    }

    if (0 == strcasecmp(key, "serial_baud")) {
        proxy->serial.baud = strtoul(value, NULL, 0);
        return;
    }

    if (0 == strcasecmp(key, "server_name")) {
        memcpy(proxy->network.to_name, value, strlen(value));
        return;
    }

    if (0 == strcasecmp(key, "server_port")) {
        proxy->network.to_port = strtoul(value, NULL, 0);
        return;
    }

    if (0 == strcasecmp(key, "log_file")) {
        memcpy(proxy->log.path, value, strlen(value));
        return;
    }

    if (0 == strcasecmp(key, "log_level")) {
        if (0 == strcasecmp(value, "trace")) {
            proxy->log.level = TRACE;
        } else if (0 == strcasecmp(value, "debug")) {
            proxy->log.level = DEBUG;
        } else if (0 == strcasecmp(value, "info")) {
            proxy->log.level = INFO;
        } else if (0 == strcasecmp(value, "warning")) {
            proxy->log.level = WARNING;
        } else if (0 == strcasecmp(value, "error")) {
            proxy->log.level = ERROR;
        } else if (0 == strcasecmp(value, "critical")) {
            proxy->log.level = CRITICAL;
        }

        return;
    }
}

void unknown_option(struct pty2udp_proxy* proxy, const char* key, const char* value) {
    ;
}
