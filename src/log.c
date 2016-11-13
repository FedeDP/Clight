#include "../inc/log.h"

void _log(FILE *type, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
#if defined(DEBUG) || !defined(SYSTEMD_PRESENT)
    vfprintf(type, format, args);
#else
    if (type == stdout) {
        sd_journal_printv(LOG_NOTICE, format, args);
    } else {
        sd_journal_printv(LOG_ERR, format, args);
    }
#endif
    va_end(args);
}

void _perror(const char *str) {
#ifdef DEBUG
    perror(str);
#else
    sd_journal_perror(str);
#endif
}
