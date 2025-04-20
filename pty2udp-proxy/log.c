/*
 * @file
 * @brief pty2udp proxy for klipper
 * @details
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */

#include "log.h"
#include "pty2udp-proxy.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>
#include <zlib.h>


static const char* log_level_to_string[] = {
        "none"
        , "trace"
        , "debug"
        , "info"
        , "warning"
        , "error"
        , "critical"
};


static void* thread_rotate_log(void* arg);
static void rotate_log(struct pty2udp_proxy* proxy);


int log_init(struct pty2udp_proxy* proxy) {
    if (!proxy)
        return EXIT_FAILURE;

    if (0 == strlen(proxy->log.path))
        return EXIT_FAILURE;

    memset(proxy->log.tmp_path, 0, sizeof(proxy->log.tmp_path));
    memcpy(proxy->log.tmp_path, proxy->log.path, strlen(proxy->log.path));

    static char* tmp_log_file_suffix = ".tmp";
    strcat(proxy->log.tmp_path, tmp_log_file_suffix);

    proxy->log.file = fopen(proxy->log.path, "a");
    if (NULL == proxy->log.file) {
        fprintf(stderr, "failed open or create log-file '%s'", proxy->log.path);
        return EXIT_FAILURE;
    }

    proxy->log.file_size = ftell(proxy->log.file);
    proxy->log.rotate_file_count = 9;
    proxy->log.rotate_file_size = 10 * 1024 * 1024;

    return EXIT_SUCCESS;
}

void log_func(log_level_t level, struct pty2udp_proxy* proxy, const char* format, ...) {
    static char log_buffer[1024];
    // char time_buff[128];

    if (level > CRITICAL)
        level = None;

    if (proxy && proxy->log.level > level)
        return;

    FILE* log_out = NULL;
    if (proxy) {
        if (proxy->log.file)
            log_out = proxy->log.file;
    } else
        log_out = stderr;

    if (!log_out)
        return;

    struct timespec ts;
    timespec_get(&ts, TIME_UTC);

    size_t prefix_offset = 0;

    // if (proxy_cfg->log_impl == "stdout") {
    prefix_offset += strftime(log_buffer, sizeof(log_buffer), "%Y.%m.%d %T", gmtime(&ts.tv_sec));
    prefix_offset += snprintf(log_buffer + prefix_offset, sizeof(log_buffer) - prefix_offset
            ,".%06lu - %8s - ", ts.tv_nsec / 1000, log_level_to_string[level]);
    // }

    va_list args;
    va_start(args, format);
    prefix_offset += vsnprintf(log_buffer + prefix_offset, sizeof(log_buffer) - prefix_offset, format, args);
    va_end(args);

    // if (proxy_cfg->log_impl == "stdout") {
    log_buffer[prefix_offset] = '\n';
    log_buffer[prefix_offset + 1] = 0;
    fputs(log_buffer, log_out);
    fflush(log_out);
    // }

    if (proxy) {
        proxy->log.file_size += (prefix_offset + 1);
        if (proxy->log.file_size > proxy->log.rotate_file_size) {
            rotate_log(proxy);
            proxy->log.file_size = 0;
        }
    }
}

char* array2hex(uint8_t* array, size_t array_size) {
    static char hex_buff[4096];

    if (array_size == 0) {
        hex_buff[0] = 0;
        return hex_buff;
    }

    for(size_t idx = 0; idx < array_size; ++idx) {
        snprintf(hex_buff + idx*3, sizeof(hex_buff) - idx*3, "%02x ", array[idx]);
    }
    hex_buff[array_size*3 - 1] = 0;

    return hex_buff;
}

void rotate_log(struct pty2udp_proxy* proxy) {
    int ret = fclose(proxy->log.file);
    if (ret != 0) {
        fprintf(stderr, "failed close log file '%s'", proxy->log.path);
    }

    if (0 == access(proxy->log.tmp_path, F_OK)) {
        (void) remove(proxy->log.tmp_path);
    }
    ret = rename( proxy->log.path , proxy->log.tmp_path);
    // if ret != 0 -> ????

    proxy->log.file = fopen(proxy->log.path, "a");
    if (NULL == proxy->log.file) {
        fprintf(stderr, "failed open or create log-file '%s'", proxy->log.path);
    }

    pthread_t thread;
    ret = pthread_create(&thread, NULL, thread_rotate_log, proxy);
    if (ret != 0) {
        // ???
    }

    ret = pthread_detach(thread);
    if (ret != 0) {
        // ???
    }
}

void* thread_rotate_log(void* arg) {
    assert(arg !=NULL);
    struct pty2udp_proxy* proxy = (struct pty2udp_proxy*) arg;
    int ret = -1;
#define COMPRESS_SUFFIX     ".gz"

    // обнаружение последнего сжатого лог-файла
    size_t idx = 1;
    char* indexed_filename = calloc(PATH_MAX, sizeof(char));
    for (; idx <= proxy->log.rotate_file_count; ++idx) {
        ret = snprintf(indexed_filename, PATH_MAX, "%s.%zu" COMPRESS_SUFFIX, proxy->log.path, idx);
        // TODO: if return error -> ???

        if (0 == access(indexed_filename, F_OK)) {
            continue;
        }

        break;
    }
    idx--;

    // если количесво файлов максимально -> удаляем последний
    if (idx == proxy->log.rotate_file_count) {
        ret = remove(indexed_filename);
        // TODO: if return error -> ???

        idx--;
    }
    free(indexed_filename);


    // сдвигам индексы файлов
    for(; idx != 0; idx--) {
        char *old_filename = calloc(4096, sizeof(char));
        ret = snprintf(old_filename, 4096, "%s.%zu" COMPRESS_SUFFIX, proxy->log.path, idx);
        // TODO: if return error -> ???

        char *new_filename = calloc(PATH_MAX, sizeof(char));
        ret = snprintf(new_filename, PATH_MAX, "%s.%zu" COMPRESS_SUFFIX, proxy->log.path, idx + 1);
        // TODO: if return error -> ???

        ret = rename(old_filename, new_filename);
        // TODO: if return error -> ???

        free(new_filename);
        free(old_filename);
    }


    FILE *tmp_file = fopen(proxy->log.tmp_path, "r");
    if (tmp_file == NULL) {
        ; // TODO: ????
    }

    char* compress_filename = calloc(PATH_MAX, sizeof(char));
    ret = snprintf(compress_filename, PATH_MAX, "%s.1" COMPRESS_SUFFIX, proxy->log.path);
    // TODO: if return error -> ???

    gzFile gz_file = gzopen(compress_filename, "wb9");
    if (gz_file == NULL) {
        ; // TODO: ???
    }
    free(compress_filename);

    const size_t tmp_buffer_size = 4096;
    char *tmp_buffer = calloc(tmp_buffer_size, sizeof(char));
    while(!feof(tmp_file)) {
        size_t read_bytes = fread(tmp_buffer, sizeof(char), tmp_buffer_size, tmp_file);
        // TODO: if return error -> ???

        ret = gzwrite(gz_file, tmp_buffer, read_bytes);
        // TODO: if return error -> ???
    }
    free(tmp_buffer);


    ret = gzclose(gz_file);
    // TODO: if ret != Z_OK -> ???

    ret= fclose(tmp_file);
    // TODO: if return error -> ???

    ret = remove(proxy->log.tmp_path);
    // TODO: if return error -> ???

    return NULL;
}
