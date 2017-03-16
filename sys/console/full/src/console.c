/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "syscfg/syscfg.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "console/console.h"
#include "console/ticks.h"
#include "console_priv.h"

/* Control characters */
#define ESC                0x1b
#define DEL                0x7f

/* ANSI escape sequences */
#define ANSI_ESC           '['
#define ANSI_UP            'A'
#define ANSI_DOWN          'B'
#define ANSI_FORWARD       'C'
#define ANSI_BACKWARD      'D'
#define ANSI_END           'F'
#define ANSI_HOME          'H'
#define ANSI_DEL           '~'

#define ESC_ESC         (1 << 0)
#define ESC_ANSI        (1 << 1)
#define ESC_ANSI_FIRST  (1 << 2)
#define ESC_ANSI_VAL    (1 << 3)
#define ESC_ANSI_VAL_2  (1 << 4)

/* Indicates whether the previous line of output was completed. */
int console_is_midline;

static int esc_state;
static int echo = 1;
static unsigned int ansi_val, ansi_val_2;

static uint8_t cur, end;
static struct os_eventq *avail_queue;
static struct os_eventq *lines_queue;
static uint8_t (*completion_cb)(char *line, uint8_t len);

/* Definitions for libc */
static size_t stdin_read(FILE *fp, char *bp, size_t n);
static size_t stdout_write(FILE *fp, const char *str, size_t cnt);

static struct File_methods _stdio_methods = {
    .write = stdout_write,
    .read = stdin_read
};

static struct File _stdio = {
    .vmt = &_stdio_methods
};

struct File *const stdin = &_stdio;
struct File *const stdout = &_stdio;
struct File *const stderr = &_stdio;
/***/

static size_t
stdin_read(FILE *fp, char *bp, size_t n)
{
    return 0;
}

static size_t
stdout_write(FILE *fp, const char *str, size_t cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        if (console_out((int)str[i]) == EOF) {
            return EOF;
        }
    }
    return cnt;
}

void
console_echo(int on)
{
    echo = on;
}

void
console_printf(const char *fmt, ...)
{
    va_list args;

    if (console_get_ticks()) {
        /* Prefix each line with a timestamp. */
        if (!console_is_midline) {
            printf("%06lu ", (unsigned long)os_time_get());
        }
    }

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

size_t
console_file_write(void *arg, const char *str, size_t cnt)
{
    return stdout_write(NULL, str, cnt);
}

void
console_write(const char *str, int cnt)
{
    console_file_write(NULL, str, cnt);
}

static inline void
cursor_forward(unsigned int count)
{
    console_printf("\x1b[%uC", count);
}

static inline void
cursor_backward(unsigned int count)
{
    console_printf("\x1b[%uD", count);
}

static inline void
cursor_save(void)
{
    console_out(ESC);
    console_out('[');
    console_out('s');
}

static inline void
cursor_restore(void)
{
    console_out(ESC);
    console_out('[');
    console_out('u');
}

static void
insert_char(char *pos, char c, uint8_t end)
{
    char tmp;

    if (echo) {
        /* Echo back to console */
        console_out(c);
    }

    if (end == 0) {
        *pos = c;
        return;
    }

    tmp = *pos;
    *(pos++) = c;

    cursor_save();

    while (end-- > 0) {
        console_out(tmp);
        c = *pos;
        *(pos++) = tmp;
        tmp = c;
    }

    /* Move cursor back to right place */
    cursor_restore();
}

static void
del_char(char *pos, uint8_t end)
{
    console_out('\b');

    if (end == 0) {
        console_out(' ');
        console_out('\b');
        return;
    }

    cursor_save();

    while (end-- > 0) {
        *pos = *(pos + 1);
        console_out(*(pos++));
    }

    console_out(' ');

    /* Move cursor back to right place */
    cursor_restore();
}

static void
handle_ansi(uint8_t byte, char *line)
{
    if (esc_state & ESC_ANSI_FIRST) {
        esc_state &= ~ESC_ANSI_FIRST;
        if (!isdigit(byte)) {
            ansi_val = 1;
            goto ansi_cmd;
        }

        esc_state |= ESC_ANSI_VAL;
        ansi_val = byte - '0';
        ansi_val_2 = 0;
        return;
    }

    if (esc_state & ESC_ANSI_VAL) {
        if (isdigit(byte)) {
            if (esc_state & ESC_ANSI_VAL_2) {
                ansi_val_2 *= 10;
                ansi_val_2 += byte - '0';
            } else {
                ansi_val *= 10;
                ansi_val += byte - '0';
            }
            return;
        }

        /* Multi value sequence, e.g. Esc[Line;ColumnH */
        if (byte == ';' && !(esc_state & ESC_ANSI_VAL_2)) {
            esc_state |= ESC_ANSI_VAL_2;
            return;
        }

        esc_state &= ~ESC_ANSI_VAL;
        esc_state &= ~ESC_ANSI_VAL_2;
    }

ansi_cmd:
    switch (byte) {
    case ANSI_BACKWARD:
        if (ansi_val > cur) {
            break;
        }

        end += ansi_val;
        cur -= ansi_val;
        cursor_backward(ansi_val);
        break;
    case ANSI_FORWARD:
        if (ansi_val > end) {
            break;
        }

        end -= ansi_val;
        cur += ansi_val;
        cursor_forward(ansi_val);
        break;
    case ANSI_HOME:
        if (!cur) {
            break;
        }

        cursor_backward(cur);
        end += cur;
        cur = 0;
        break;
    case ANSI_END:
        if (!end) {
            break;
        }

        cursor_forward(end);
        cur += end;
        end = 0;
        break;
    case ANSI_DEL:
        if (!end) {
            break;
        }

        cursor_forward(1);
        del_char(&line[cur], --end);
        break;
    default:
        break;
    }

    esc_state &= ~ESC_ANSI;
}

int
console_handle_char(uint8_t byte)
{
#if !MYNEWT_VAL(CONSOLE_INPUT)
    return 0;
#endif

    static struct os_event *ev;
    static struct console_input *input;

    if (!avail_queue || !lines_queue) {
        return 0;
    }

    if (!ev) {
        ev = os_eventq_get_no_wait(avail_queue);
        if (!ev)
            return 0;
        input = ev->ev_arg;
    }

    /* Handle ANSI escape mode */
    if (esc_state & ESC_ANSI) {
        handle_ansi(byte, input->line);
        return 0;
    }

    /* Handle escape mode */
    if (esc_state & ESC_ESC) {
        esc_state &= ~ESC_ESC;
        handle_ansi(byte, input->line);
        switch (byte) {
        case ANSI_ESC:
            esc_state |= ESC_ANSI;
            esc_state |= ESC_ANSI_FIRST;
            break;
        default:
            break;
        }

        return 0;
    }

    /* Handle special control characters */
    if (!isprint(byte)) {
        handle_ansi(byte, input->line);
        switch (byte) {
        case DEL:
            if (cur > 0) {
                del_char(&input->line[--cur], end);
            }
            break;
        case ESC:
            esc_state |= ESC_ESC;
            break;
        case '\r':
            input->line[cur + end] = '\0';
            console_out('\r');
            console_out('\n');
            cur = 0;
            end = 0;
            os_eventq_put(lines_queue, ev);
            input = NULL;
            ev = NULL;
            break;
        case '\t':
            if (completion_cb && !end) {
                cur += completion_cb(input->line, cur);
            }
            break;
        default:
            break;
        }

        return 0;
    }

    /* Ignore characters if there's no more buffer space */
    if (cur + end < sizeof(input->line) - 1) {
        insert_char(&input->line[cur++], byte, end);
    }
    return 0;
}

int
console_is_init(void)
{
#if MYNEWT_VAL(CONSOLE_UART)
    return uart_console_is_init();
#endif
#if MYNEWT_VAL(CONSOLE_RTT)
    return rtt_console_is_init();
#endif
    return 0;
}

int
console_init(struct os_eventq *avail, struct os_eventq *lines,
             uint8_t (*completion)(char *str, uint8_t len))
{
    avail_queue = avail;
    lines_queue = lines;
    completion_cb = completion;
    return 0;
}

void
console_pkg_init(void)
{
    int rc = 0;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

#if MYNEWT_VAL(CONSOLE_UART)
    rc = uart_console_init();
#endif
#if MYNEWT_VAL(CONSOLE_RTT)
    rc = rtt_console_init();
#endif
    SYSINIT_PANIC_ASSERT(rc == 0);
}