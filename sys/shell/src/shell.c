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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "syscfg/syscfg.h"
#include "os/os.h"
#include "console/console.h"
#include "sysinit/sysinit.h"
#include "shell/shell.h"
#include "shell_priv.h"

#define SHELL_PROMPT "shell> "

#define MODULE_NAME_MAX_LEN     20

/* additional chars are "> " (include '\0' )*/
#define PROMPT_SUFFIX 3
#define PROMPT_MAX_LEN (MODULE_NAME_MAX_LEN + PROMPT_SUFFIX)

static struct shell_module shell_modules[MYNEWT_VAL(SHELL_MAX_MODULES)];
static size_t num_of_shell_entities;

static const char *prompt;
static char default_module_prompt[PROMPT_MAX_LEN];
static int default_module = -1;

static shell_cmd_function_t app_cmd_handler;
static shell_prompt_function_t app_prompt_handler;

static struct console_input buf[MYNEWT_VAL(SHELL_MAX_CMD_QUEUED)];

static struct os_eventq avail_queue;
static struct os_event shell_console_ev[MYNEWT_VAL(SHELL_MAX_CMD_QUEUED)];

static const char *
get_prompt(void)
{
    if (app_prompt_handler) {
        const char *str;

        str = app_prompt_handler();
        if (str) {
            return str;
        }
    }

    if (default_module != -1) {
        return default_module_prompt;
    }

    return prompt;
}

static size_t
line2argv(char *str, char *argv[], size_t size)
{
    size_t argc = 0;

    if (!strlen(str)) {
        return 0;
    }

    while (*str && *str == ' ') {
        str++;
    }

    if (!*str) {
        return 0;
    }

    argv[argc++] = str;

    while ((str = strchr(str, ' '))) {
        *str++ = '\0';

        while (*str && *str == ' ') {
            str++;
        }

        if (!*str) {
            break;
        }

        argv[argc++] = str;

        if (argc == size) {
            console_printf("Too many parameters (max %zu)\n", size - 1);
            return 0;
        }
    }

    /* keep it POSIX style where argv[argc] is required to be NULL */
    argv[argc] = NULL;

    return argc;
}

static int
get_destination_module(const char *module_str, uint8_t len)
{
    int i;

    for (i = 0; i < num_of_shell_entities; i++) {
        if (!strncmp(module_str,
                     shell_modules[i].module_name, len)) {
            return i;
        }
    }

    return -1;
}

/* For a specific command: argv[0] = module name, argv[1] = command name
 * If a default module was selected: argv[0] = command name
 */
static const char *
get_command_and_module(char *argv[], int *module)
{
    *module = -1;

    if (!argv[0]) {
        console_printf("Unrecognized command\n");
        return NULL;
    }

    if (default_module == -1) {
        if (!argv[1] || argv[1][0] == '\0') {
            console_printf("Unrecognized command: %s\n", argv[0]);
            return NULL;
        }

        *module = get_destination_module(argv[0], MODULE_NAME_MAX_LEN);
        if (*module == -1) {
            console_printf("Illegal module %s\n", argv[0]);
            return NULL;
        }

        return argv[1];
    }

    *module = default_module;
    return argv[0];
}

static int
show_cmd_help(char *argv[])
{
    const char *command = NULL;
    int module = -1;
    const struct shell_module *shell_module = NULL;
    int i;

    command = get_command_and_module(argv, &module);
    if ((module == -1) || (command == NULL)) {
        return 0;
    }

    shell_module = &shell_modules[module];
    for (i = 0; shell_module->commands[i].cmd_name; i++) {
        if (!strcmp(command, shell_module->commands[i].cmd_name)) {
            console_printf("%s %s\n",
                           shell_module->commands[i].cmd_name,
                           shell_module->commands[i].help &&
                           shell_module->commands[i].help->usage ?
                           shell_module->commands[i].help->usage : "");
            return 0;
        }
    }

    console_printf("Unrecognized command: %s\n", argv[0]);
    return 0;
}

static void
print_modules(void)
{
    int module;

    for (module = 0; module < num_of_shell_entities; module++) {
        console_printf("%s\n", shell_modules[module].module_name);
    }
}

static void
print_module_commands(const int module)
{
    const struct shell_module *shell_module = &shell_modules[module];
    int i;

    console_printf("help\n");

    for (i = 0; shell_module->commands[i].cmd_name; i++) {
        console_printf("%s", shell_module->commands[i].cmd_name);
        if (shell_module->commands[i].help &&
            shell_module->commands[i].help->summary) {
        console_printf(" - %s", shell_module->commands[i].help->summary);
        }
        console_printf("\n");
    }
}

static int
show_help(int argc, char *argv[])
{
    int module;

    /* help per command */
    if ((argc > 2) || ((default_module != -1) && (argc == 2))) {
        return show_cmd_help(&argv[1]);
    }

    /* help per module */
    if ((argc == 2) || ((default_module != -1) && (argc == 1))) {
        if (default_module == -1) {
            module = get_destination_module(argv[1], MODULE_NAME_MAX_LEN);
            if (module == -1) {
                console_printf("Illegal module %s\n", argv[1]);
                return 0;
            }
        } else {
            module = default_module;
        }

        print_module_commands(module);
    } else { /* help for all entities */
        console_printf("Available modules:\n");
        print_modules();
        console_printf("To select a module, enter 'select <module name>'.\n");
    }

    return 0;
}

static int
set_default_module(const char *name)
{
    int module;

    if (strlen(name) > MODULE_NAME_MAX_LEN) {
        console_printf("Module name %s is too long, default is not changed\n",
                       name);
        return -1;
    }

    module = get_destination_module(name, MODULE_NAME_MAX_LEN);

    if (module == -1) {
        console_printf("Illegal module %s, default is not changed\n", name);
        return -1;
    }

    default_module = module;

    strncpy(default_module_prompt, name, MODULE_NAME_MAX_LEN);
    strcat(default_module_prompt, "> ");

    return 0;
}

static int
select_module(int argc, char *argv[])
{
    if (argc == 1) {
        default_module = -1;
    } else {
        set_default_module(argv[1]);
    }

    return 0;
}

static shell_cmd_function_t
get_cb(int argc, char *argv[])
{
    const char *first_string = argv[0];
    int module = -1;
    const struct shell_module *shell_module;
    const char *command;
    int i;

    if (!first_string || first_string[0] == '\0') {
        console_printf("Illegal parameter\n");
        return NULL;
    }

    if (!strcmp(first_string, "help")) {
        return show_help;
    }

    if (!strcmp(first_string, "select")) {
        return select_module;
    }

    if ((argc == 1) && (default_module == -1)) {
        console_printf("Missing parameter\n");
        return NULL;
    }

    command = get_command_and_module(argv, &module);
    if ((module == -1) || (command == NULL)) {
        return NULL;
    }

    shell_module = &shell_modules[module];
    console_printf("module: %d, command: %s\n", module, command);
    for (i = 0; shell_module->commands[i].cmd_name; i++) {
        if (!strcmp(command, shell_module->commands[i].cmd_name)) {
            return shell_module->commands[i].cb;
        }
    }

    return NULL;
}

static void
shell(struct os_event *ev)
{
    char *argv[MYNEWT_VAL(SHELL_CMD_ARGC_MAX) + 1];
    size_t argc;
    struct console_input *cmd;
    shell_cmd_function_t cb;
    size_t argc_offset = 0;

    if (!ev) {
        console_printf("%s", get_prompt());
        return;
    }

    cmd = ev->ev_arg;
    if (!cmd) {
        console_printf("%s", get_prompt());
        return;
    }

    argc = line2argv(cmd->line, argv, MYNEWT_VAL(SHELL_CMD_ARGC_MAX) + 1);
    if (!argc) {
        os_eventq_put(&avail_queue, ev);
        console_printf("%s", get_prompt());
        return;
    }

    cb = get_cb(argc, argv);
    if (!cb) {
        if (app_cmd_handler != NULL) {
            cb = app_cmd_handler;
        } else {
            console_printf("Unrecognized command: %s\n", argv[0]);
            console_printf("Type 'help' for list of available commands\n");
            os_eventq_put(&avail_queue, ev);
            console_printf("%s", get_prompt());
            return;
        }
    }

    /* Allow invoking a cmd with module name as a prefix; a command should
     * not know how it was invoked (with or without prefix)
     */
    if (default_module == -1 && cb != select_module && cb != show_help) {
        argc_offset = 1;
    }

    /* Execute callback with arguments */
    if (cb(argc - argc_offset, &argv[argc_offset]) < 0) {
        show_cmd_help(argv);
    }

    os_eventq_put(&avail_queue, ev);
    console_printf("%s", get_prompt());
}

#if MYNEWT_VAL(SHELL_COMPLETION)

static void
print_command_params(const int module, const int command)
{
    const struct shell_module *shell_module = &shell_modules[module];
    const struct shell_cmd *shell_cmd = &shell_module->commands[command];
    int i;

    if (!(shell_cmd->help && shell_cmd->help->params)) {
        return;
    }

    for (i = 0; shell_cmd->help->params[i].param_name; i++) {
        console_printf("%s - %s\n", shell_cmd->help->params[i].param_name,
                       shell_cmd->help->params[i].help);
    }
}

static int
get_command_from_module(const char *command, int len, int module)
{
    int i;
    const struct shell_module *shell_module;

    shell_module = &shell_modules[module];
    for (i = 0; shell_module->commands[i].cmd_name; i++) {
        if (!strncmp(command, shell_module->commands[i].cmd_name, len)) {
            return i;
        }
    }
    return -1;
}

static int
get_token(char **cur, int *null_terminated)
{
    char *str = *cur;

    *null_terminated = 0;
    /* remove ' ' at the beginning */
    while (*str && *str == ' ') {
        str++;
    }

    if (!str) {
        *null_terminated = 1;
        return 0;
    }

    *cur = str;
    str = strchr(str, ' ');

    if (str == NULL) {
        *null_terminated = 1;
        return strlen(*cur);
    }

    return str - *cur;
}

static int
get_last_token(char **cur)
{
    *cur = strrchr(*cur, ' ');
    if (*cur == NULL) {
        return 0;
    }
    (*cur)++;
    return strlen(*cur);
}

static int
complete_param(char *line, uint8_t len, const char *param_prefix,
                 int param_len, int module_idx, int command_idx)
{
    const char *first_match = NULL;
    int i, common_chars = -1;
    const struct shell_cmd *command;

    command = &shell_modules[module_idx].commands[command_idx];

    if (!(command->help && command->help->params)) {
        return 0;
    }

    for (i = 0; command->help->params[i].param_name; i++) {
        int j;

        if (strncmp(param_prefix,
            command->help->params[i].param_name, param_len)) {
            continue;
        }

        if (!first_match) {
            first_match = command->help->params[i].param_name;
            continue;
        }

        /* more commands match, print first match */
        if (first_match && (common_chars < 0)) {
            console_printf("\n");
            console_printf("%s\n", first_match);
            common_chars = strlen(first_match);
        }

        /* cut common part of matching names */
        for (j = 0; j < common_chars; j++) {
            if (first_match[j] != command->help->params[i].param_name[j]) {
                break;
            }
        }

        common_chars = j;

        console_printf("%s\n", command->help->params[i].param_name);
    }

    /* no match, do nothing */
    if (!first_match) {
        return 0;
    }

    if (common_chars >= 0) {
        /* multiple match, restore prompt */
        console_printf("%s", get_prompt());
        console_printf("%s", line);
    } else {
        common_chars = strlen(first_match);
    }

    /* complete common part */
    for (i = param_len; i < common_chars; i++) {
        console_printf("%c", first_match[i]);
        line[len++] = first_match[i];
    }

    return common_chars - param_len;
}

static int
complete_command(char *line, uint8_t len, char *command_prefix,
                 int command_len, int module_idx)
{
    const char *first_match = NULL;
    int i, common_chars = -1, space = 0;
    const struct shell_module *module;

    module = &shell_modules[module_idx];

    for (i = 0; module->commands[i].cmd_name; i++) {
        int j;

        if (strncmp(command_prefix,
            module->commands[i].cmd_name, command_len)) {
            continue;
        }

        if (!first_match) {
            first_match = module->commands[i].cmd_name;
            continue;
        }

        /* more commands match, print first match */
        if (first_match && (common_chars < 0)) {
            console_printf("\n");
            console_printf("%s\n", first_match);
            common_chars = strlen(first_match);
        }

        /* cut common part of matching names */
        for (j = 0; j < common_chars; j++) {
            if (first_match[j] != module->commands[i].cmd_name[j]) {
                break;
            }
        }

        common_chars = j;

        console_printf("%s\n", module->commands[i].cmd_name);
    }

    /* no match, do nothing */
    if (!first_match) {
        return 0;
    }

    if (common_chars >= 0) {
        /* multiple match, restore prompt */
        console_printf("%s", get_prompt());
        console_printf("%s", line);
    } else {
        common_chars = strlen(first_match);
        space = 1;
    }

    /* complete common part */
    for (i = command_len; i < common_chars; i++) {
        console_printf("%c", first_match[i]);
        line[len++] = first_match[i];
    }

    /* for convenience add space after command */
    if (space) {
        console_printf(" ");
        line[len] = ' ';
    }

    return common_chars - command_len + space;
}

static int
complete_module(char *line, int len, char *module_prefix, int module_len)
{
    int i;
    const char *first_match = NULL;
    int common_chars = -1, space = 0;

    if (!module_len) {
        console_printf("\n");
        for (i = 0; i < num_of_shell_entities; i++) {
            console_printf("%s\n", shell_modules[i].module_name);
        }
        console_printf("%s", get_prompt());
        console_printf("%s", line);
        return 0;
    }

    for (i = 0; i < num_of_shell_entities; i++) {
        int j;

        if (strncmp(module_prefix,
                     shell_modules[i].module_name,
                     module_len)) {
            continue;
        }

        if (!first_match) {
            first_match = shell_modules[i].module_name;
            continue;
        }

        /* more commands match, print first match */
        if (first_match && (common_chars < 0)) {
            console_printf("\n");
            console_printf("%s\n", first_match);
            common_chars = strlen(first_match);
        }

        /* cut common part of matching names */
        for (j = 0; j < common_chars; j++) {
            if (first_match[j] != shell_modules[i].module_name[j]) {
                break;
            }
        }

        common_chars = j;

        console_printf("%s\n", shell_modules[i].module_name);
    }

    /* no match, do nothing */
    if (!first_match) {
        return 0;
    }

    if (common_chars >= 0) {
        /* multiple match, restore prompt */
        console_printf("%s", get_prompt());
        console_printf("%s", line);
    } else {
        common_chars = strlen(first_match);
        space = 1;
    }

    /* complete common part */
    for (i = module_len; i < common_chars; i++) {
        console_printf("%c", first_match[i]);
        line[len++] = first_match[i];
    }

    /* for convenience add space after command */
    if (space) {
        console_printf(" ");
        line[len] = ' ';
    }

    return common_chars - module_len + space;
}

static int
complete_select(char *line, int len, char *cur, int tok_len)
{
    int null_terminated = 0;
    cur += tok_len + 1;
    tok_len = get_token(&cur, &null_terminated);
    if (tok_len == 0) {
        if (default_module != -1) {
            return 0;
        }
        console_printf("\n");
        print_modules();
        console_printf("%s", get_prompt());
        console_printf("%s", line);
        return 0;
    }

    if (null_terminated) {
        if (default_module == -1) {
            return complete_module(line, len, cur, tok_len);
        }
    }
    return 0;
}

static uint8_t
completion(char *line, uint8_t len)
{
    char *cur;
    int tok_len;
    int module, command;
    int null_terminated = 0;

    /*
     * line to completion is not ended by '\0' as the line that gets from
     * os_eventq_get function
     */
    line[len] = '\0';

    cur = line;
    tok_len = get_token(&cur, &null_terminated);

    /* empty token - print options */
    if (tok_len == 0) {
        console_printf("\n");
        if (default_module == -1) {
            print_modules();
        } else {
            print_module_commands(default_module);
        }
        console_printf("%s", get_prompt());
        console_printf("%s", line);
        return 0;
    }

    /* token can be completed */
    if (null_terminated) {
        if (default_module == -1) {
            return complete_module(line, len, cur, tok_len);
        }
        return complete_command(line, len, cur, tok_len, default_module);
    }

    if (strncmp("select", cur, tok_len) == 0) {
        return complete_select(line, len, cur, tok_len);
    }

    if (default_module != -1) {
        module = default_module;
    } else {
        module = get_destination_module(cur, tok_len);

        if (module == -1) {
            return 0;
        }

        cur += tok_len + 1;
        tok_len = get_token(&cur, &null_terminated);

        if (tok_len == 0) {
            console_printf("\n");
            print_module_commands(module);
            console_printf("%s", get_prompt());
            console_printf("%s", line);
            return 0;
        }

        if (null_terminated) {
            return complete_command(line, len, cur, tok_len, module);
        }
    }



    command = get_command_from_module(cur, tok_len, module);
    if (command == -1) {
        return 0;
    }

    cur += tok_len;
    tok_len = get_last_token(&cur);
    if (tok_len == 0) {
        console_printf("\n");
        print_command_params(module, command);
        console_printf("%s", get_prompt());
        console_printf("%s", line);
        return 0;
    }
    return complete_param(line, len, cur, tok_len, module, command);
}
#endif /* MYNEWT_VAL(SHELL_COMPLETION) */

void
shell_register_app_cmd_handler(shell_cmd_function_t handler)
{
    app_cmd_handler = handler;
}

void
shell_register_prompt_handler(shell_prompt_function_t handler)
{
    app_prompt_handler = handler;
}

void
shell_register_default_module(const char *name)
{
    int result = set_default_module(name);

    if (result != -1) {
        console_printf("\n");
        console_printf("%s", default_module_prompt);
    }
}

static void
line_queue_init(void)
{
    int i;

    for (i = 0; i < MYNEWT_VAL(SHELL_MAX_CMD_QUEUED); i++) {
        shell_console_ev[i].ev_cb = shell;
        shell_console_ev[i].ev_arg = &buf[i];
        os_eventq_put(&avail_queue, &shell_console_ev[i]);
    }
}

int
shell_register(const char *module_name, const struct shell_cmd *commands)
{
    if (num_of_shell_entities >= MYNEWT_VAL(SHELL_MAX_MODULES)) {
        return -1;
    }

    shell_modules[num_of_shell_entities].module_name = module_name;
    shell_modules[num_of_shell_entities].commands = commands;
    ++num_of_shell_entities;

    return 0;
}

void
shell_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

#if !MYNEWT_VAL(SHELL_TASK)
    return;
#endif

    os_eventq_init(&avail_queue);
    line_queue_init();
    prompt = SHELL_PROMPT;
#if MYNEWT_VAL(SHELL_COMPLETION)
    console_init(&avail_queue, os_eventq_dflt_get(), completion);
#else
    console_init(&avail_queue, os_eventq_dflt_get(), NULL);
#endif

#if MYNEWT_VAL(SHELL_OS_MODULE)
    shell_os_register(shell_register);
#endif
#if MYNEWT_VAL(SHELL_PROMPT_MODULE)
    shell_prompt_register(shell_register);
#endif
}
