/**
 * @file os_console.c
 * @brief UART console driver implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Console I/O
 * 
 * Platform-independent console driver.
 * On host, uses stdin/stdout.
 * On ESP32, uses UART.
 */

#include "os_console.h"
#include "os_config.h"
#include <stdio.h>
#include <string.h>

#ifdef OS_PLATFORM_HOST
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>

static struct termios orig_termios;
static bool termios_saved = false;
#endif

static struct {
    bool initialized;
    char line_buf[OS_SHELL_LINE_MAX];
    size_t line_pos;
} console = {0};

os_err_t os_console_init(void) {
    if (console.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
#ifdef OS_PLATFORM_HOST
    /* Set stdin to non-blocking, raw mode */
    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        termios_saved = true;
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    
    /* Make stdin non-blocking */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif

    memset(&console, 0, sizeof(console));
    console.initialized = true;
    
    return OS_OK;
}

void os_console_putc(char c) {
    putchar(c);
    fflush(stdout);
}

void os_console_puts(const char *str) {
    if (str) {
        fputs(str, stdout);
        fflush(stdout);
    }
}

int os_console_getc(void) {
#ifdef OS_PLATFORM_HOST
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return (unsigned char)c;
    }
    return -1;
#else
    /* ESP32 implementation would go here */
    return -1;
#endif
}

bool os_console_available(void) {
#ifdef OS_PLATFORM_HOST
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
#else
    return false;
#endif
}

int os_console_readline(char *buf, size_t max_len, bool echo) {
    int c = os_console_getc();
    
    if (c < 0) {
        return -1;  /* No data */
    }
    
    if (c == '\n' || c == '\r') {
        /* End of line */
        console.line_buf[console.line_pos] = '\0';
        strncpy(buf, console.line_buf, max_len);
        buf[max_len - 1] = '\0';
        int len = (int)console.line_pos;
        console.line_pos = 0;
        if (echo) {
            os_console_putc('\n');
        }
        return len;
    } else if (c == 127 || c == 8) {
        /* Backspace */
        if (console.line_pos > 0) {
            console.line_pos--;
            if (echo) {
                os_console_puts("\b \b");
            }
        }
    } else if (c >= 32 && c < 127) {
        /* Printable character */
        if (console.line_pos < OS_SHELL_LINE_MAX - 1) {
            console.line_buf[console.line_pos++] = (char)c;
            if (echo) {
                os_console_putc((char)c);
            }
        }
    }
    
    return -1;  /* Line not complete */
}
