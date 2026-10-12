#include "game/console.h"
#include "game/debug.h"
#include "game/ngps_file.h"
#include "game/parse.h"
#include "game/plainstring.h"
#include "game/platform_ngps.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Character classes used by the command line tokenizer.
enum {
    CHAR_IDENTIFIER = 0,
    CHAR_QUOTE = 1,
    CHAR_OTHER = 2,
    CHAR_END = 3,
    CHAR_NUMBER = 4,
};

// Token types.
enum {
    TOKEN_END = 0,
    TOKEN_WORD = 1,
    TOKEN_QUOTED = 3,
};

static void get_next_char(void);
static void init_char_types(void);
static void skip_whitespace(void);
static void make_lowercase_token(void);
static void read_word(void);
static void read_quoted_string(void);
static void get_token(void);
static void set_command_line(char* command_line);

char next_arg[256];
char original_arg[256];
console_command* Console_commands[MAX_COMMANDS];
jmp_buf console_error_jump;
char token_buffer[MAX_CONSOLE_LINE_LENGTH];
char lowercase_token[MAX_CONSOLE_LINE_LENGTH];
int char_types[256];

const char* command_line_ptr = "";
char* token_ptr = token_buffer;

s32 calling_a_command_function;
s32 doing_help_for_comand;
s32 checking_status_for_command;
u32 arg_type;
char* rest_of_command_line;
int command_argument_int;
float command_argument_float;
s32 current_command_count;
bool console_initialized;
BOOL debug_command_prefixed;
char current_char;
int token_type;

int print_commands_to_file(void) {
    char buffer[256];

    if (calling_a_command_function) {
        SysFile file;

        if (file.Open("console_commands.txt", 0x80000002, 9999999)) {
            return;
        }
        for (int i = 0; i < current_command_count; i++) {
            if (Console_commands[i]) {
                if (Console_commands[i]->description) {
                    sprintf(buffer, "%s - %s", Console_commands[i]->name, Console_commands[i]->description);
                } else {
                    strcpy(buffer, Console_commands[i]->name);
                }
                file.Write(buffer, -1);
            }
        }
        file.Close();
    }

    if (doing_help_for_comand) {
        print_to_console("Print out console commands to the text file console_commands.txt", false);
    }
}

int print_commands_to_tty(void) {
    s32 prev_len = -1;
    s32 msg_len;
    char buffer[256];
    int i;

    if (calling_a_command_function) {
        for (i = 0; i < current_command_count; i++) {
            if (Console_commands[i] && (msg_len = strlen(Console_commands[i]->name), prev_len < msg_len)) {
                prev_len = msg_len;
            }
        }
        for (i = 0; i < current_command_count; i++) {
            if (Console_commands[i]) {
                memset(buffer, ' ', 256);
                if (Console_commands[i]->description) {
                    sprintf(buffer, "%s", Console_commands[i]->name);
                    strlwr(buffer);
                    buffer[strlen(buffer)] = ' ';
                    sprintf(&buffer[prev_len + 3], "- %s", Console_commands[i]->description);
                } else {
                    strcpy(buffer, Console_commands[i]->name);
                    strlwr(buffer);
                }
                msg_len = strlen(buffer);
                if (msg_len >= 79) {
                    msg_len += prev_len + 5;
                    for (; msg_len >= prev_len + 84; msg_len--) {
                        buffer[msg_len] = buffer[msg_len - (prev_len + 5)];
                    }
                    for (; msg_len >= 79; msg_len--) {
                        buffer[msg_len] = ' ';
                    }
                    buffer[prev_len + 82] = '|';
                }
            }
        }
    }

    if (doing_help_for_comand) {
        print_to_console("Print out console commands to the tty window", false);
    }
}

static void get_next_char(void) {
    if (*command_line_ptr == '\0') {
        current_char = '\0';
    } else {
        current_char = *command_line_ptr;
        command_line_ptr++;
    }
}

static void init_char_types(void) {
    int i;

    for (i = 0; i < 256; i++) {
        char_types[(u8)i] = CHAR_OTHER;
    }
    for (i = '0'; i <= '9'; i++) {
        char_types[(u8)i] = CHAR_NUMBER;
    }
    for (i = 'A'; i <= 'Z'; i++) {
        char_types[(u8)i] = CHAR_IDENTIFIER;
    }
    for (i = 'a'; i <= 'z'; i++) {
        char_types[(u8)i] = CHAR_IDENTIFIER;
    }
    char_types['_'] = CHAR_IDENTIFIER;
    char_types[':'] = CHAR_IDENTIFIER;
    char_types['.'] = CHAR_NUMBER;
    current_char = '\0';
    char_types['"'] = CHAR_QUOTE;
    char_types['\0'] = CHAR_END;
    char_types['-'] = CHAR_NUMBER;
    char_types['+'] = CHAR_NUMBER;
    char_types['\\'] = CHAR_IDENTIFIER;
}

static void skip_whitespace(void) {
    while (current_char == ' ' || current_char == '\t') {
        get_next_char();
    }
}

static void make_lowercase_token(void) {
    char* p;

    strcpy(lowercase_token, token_buffer);
    p = lowercase_token;
    do {
        *p = (*p >= 'A' && *p <= 'Z') ? *p + ('a' - 'A') : *p;
        p++;
    } while (*p);
}

static void read_word(void) {
    while (char_types[(u8)current_char] == CHAR_IDENTIFIER || char_types[(u8)current_char] == CHAR_NUMBER) {
        *token_ptr = current_char;
        token_ptr++;
        get_next_char();
    }
    *token_ptr = '\0';
    token_type = TOKEN_WORD;
}

static void read_quoted_string(void) {
    *token_ptr = '"';
    token_ptr++;
    get_next_char();
    while (current_char != '"' && current_char != '\0') {
        *token_ptr = current_char;
        token_ptr++;
        get_next_char();
    }
    get_next_char();
    *token_ptr = '"';
    token_ptr++;
    *token_ptr = '\0';
    token_type = TOKEN_QUOTED;
}

static void get_token(void) {
    skip_whitespace();
    token_ptr = token_buffer;
    *token_ptr = '\0';
    switch (char_types[(u8)current_char]) {
    case CHAR_QUOTE:
        read_quoted_string();
        break;
    case CHAR_END:
        token_type = TOKEN_END;
        break;
    case CHAR_NUMBER:
    case CHAR_IDENTIFIER:
        read_word();
        break;
    default:
        *token_ptr = current_char;
        token_ptr++;
        *token_ptr = '\0';
        get_next_char();
        token_type = TOKEN_WORD;
        break;
    }
    make_lowercase_token();
}

static void set_command_line(char* command_line) {
    command_line_ptr = command_line;
    get_next_char();
}

BOOL console_command::add_command(const char* name, const char* description, command_type type) {
    int i;
    int j;

    if (current_command_count >= MAX_COMMANDS) {
        return FALSE;
    }

    for (i = 0; i < current_command_count; i++) {
        int found = ngps_stricmp(Console_commands[i]->name, name);
        if (found == 0) {
            return FALSE;
        }
        if (found > 0) {
            break;
        }
    }

    if (i < current_command_count) {
        for (j = current_command_count; j > i; j--) {
            Console_commands[j] = Console_commands[j - 1];
        }
        Console_commands[i] = this;
        current_command_count++;
    } else {
        Console_commands[current_command_count] = this;
        current_command_count++;
    }

    this->name = name;
    this->description = description;
    this->type = type;
    return TRUE;
}

console_command::console_command(const char* command, const char* description, command_type type,
                                 CommandCallbackInt command_function) {
    if (add_command(command, description, type)) {
        callback = command_function;
    }
}

int execute_command_line(const char* command_line) {
    int i;
    int mode;
    char buffer[256];

    if (strlen(command_line) == 0 || strlen(command_line) >= 255) {
        return -1;
    }

    strcpy(buffer, command_line);
    debug_command_prefixed = FALSE;
    rest_of_command_line = buffer;
    set_command_line(buffer);
    if (setjmp(console_error_jump)) {
        return -1;
    }

    mode = 0;
    process_command(-1);
    if (ngps_stricmp(next_arg, "debug") == 0) {
        debug_command_prefixed = TRUE;
        print_to_console("Command line:", false);
        print_to_console(rest_of_command_line, false);
        process_command(-1);
    }
    if (ngps_stricmp(next_arg, "?") == 0) {
        mode = 1;
        process_command(-1);
        if (arg_type & ARG_TYPE_NONE) {
            return 0;
        }
    }
    if (ngps_stricmp(next_arg, "help") == 0 || ngps_stricmp(next_arg, "man") == 0) {
        mode = 2;
        process_command(-1);
        if (arg_type & ARG_TYPE_NONE) {
            return 0;
        }
    }
    if (strstr(rest_of_command_line, "?")) {
        mode = 2;
    }
    if (!(arg_type & ARG_TYPE_STRING)) {
        sprintf(string_buffer, "Invalid keyword '%s'\n", next_arg);
        print_to_console(string_buffer, false);
        return -1;
    }
    if (debug_command_prefixed) {
        sprintf(string_buffer, "Searching for command '%s'\n", next_arg);
        print_to_console(string_buffer, false);
    }

    for (i = 0; i < current_command_count; i++) {
        if (ngps_stricmp(next_arg, Console_commands[i]->name) == 0) {
            if (mode == 0) {
                if (debug_command_prefixed) {
                    sprintf(string_buffer, "Calling function '%s'\n", next_arg);
                    print_to_console(string_buffer, false);
                }
                doing_help_for_comand = 0;
                calling_a_command_function = 1;
                checking_status_for_command = 1;
            } else if (mode == 1) {
                if (debug_command_prefixed) {
                    sprintf(string_buffer, "Checking status for '%s'\n", next_arg);
                    print_to_console(string_buffer, false);
                }
                calling_a_command_function = 0;
                checking_status_for_command = 1;
                doing_help_for_comand = 0;
            } else {
                if (debug_command_prefixed) {
                    sprintf(string_buffer, "Doing help for '%s'\n", next_arg);
                    print_to_console(string_buffer, false);
                }
                calling_a_command_function = 0;
                doing_help_for_comand = 1;
                checking_status_for_command = 0;
            }

            switch (Console_commands[i]->type) {
            case CALL:
                Console_commands[i]->callback();
                break;
            case TOGGLE:
                Console_commands[i]->callback();
                break;
            case SET_INT:
                Console_commands[i]->callback();
                break;
            case SET_FLOAT:
                Console_commands[i]->callback();
                break;
            }

            if (mode == 0) {
                process_command(-1);
                if (!(arg_type & ARG_TYPE_NONE)) {
                    sprintf(string_buffer, "Ignoring the unused command line tail '%s %s'\n", original_arg,
                            rest_of_command_line);
                    print_to_console(string_buffer, false);
                }
            }
            return 0;
        }
    }

    sprintf(string_buffer, "Unknown command '%s'\n", next_arg);
    print_to_console(string_buffer, false);
    return -1;
}

void print_to_console(const char* buffer, bool unk) {}

// Parses the next argument and longjmps back to execute_command_line if it isn't one of
// expected_types. Nonmatching: identical instructions, but in the digit-counting loop the game
// has digits/i/next_arg/char_types/4 in t2/t1/t0/a3/a2 where this gets t0/t1/a3/a2/a1.
void process_command(int expected_types) {
    get_token();
    rest_of_command_line = (char*)command_line_ptr;
    strcpy(original_arg, token_buffer);
    strcpy(next_arg, lowercase_token);

    if (debug_command_prefixed) {
        sprintf(string_buffer, "next arg is '%s', was originally '%s'\n", next_arg, original_arg);
        print_to_console(string_buffer, false);
        print_to_console("Rest of the command line is ", false);
        print_to_console(rest_of_command_line, false);
        print_to_console("\n", false);
    }

    if (token_type == TOKEN_END) {
        arg_type = ARG_TYPE_NONE;
    } else if (token_type == TOKEN_WORD) {
        arg_type = ARG_TYPE_STRING;
    } else if (token_type == TOKEN_QUOTED) {
        arg_type = ARG_TYPE_QUOTED;
    } else {
        arg_type = ARG_TYPE_STRING;
    }

    if (arg_type & ARG_TYPE_QUOTED) {
        int len = strlen(next_arg);
        memmove(next_arg, next_arg + 1, len - 2);
        next_arg[len - 2] = '\0';
        arg_type = ARG_TYPE_STRING;
    }

    if (arg_type & ARG_TYPE_STRING) {
        int len = strlen(next_arg);
        int digits = 0;
        int i;

        for (i = 0; i < len; i++) {
            if (char_types[(u8)next_arg[i]] == CHAR_NUMBER) {
                digits++;
            }
        }

        if (digits == len) {
            arg_type |= ARG_TYPE_FLOAT;
            command_argument_float = atof(next_arg);
            if (ngps_strchr(next_arg, '.') == NULL) {
                arg_type |= ARG_TYPE_INT;
                command_argument_int = atoi(next_arg);
            }
        } else if (next_arg[0] == '0' && next_arg[1] == 'x') {
            char* end;
            int value = strtol(next_arg, &end, 0);
            if (*end == '\0') {
                command_argument_int = value;
                arg_type |= ARG_TYPE_HEX | ARG_TYPE_INT;
            }
        }

        if (debug_command_prefixed) {
            if (arg_type & ARG_TYPE_FLOAT) {
                sprintf(string_buffer, "Found float number! %f\n", command_argument_float);
                print_to_console(string_buffer, false);
            }
            if (arg_type & ARG_TYPE_INT) {
                sprintf(string_buffer, "Found int number! %f\n", command_argument_int);
                print_to_console(string_buffer, false);
            }
            if (arg_type & ARG_TYPE_HEX) {
                sprintf(string_buffer, "Found hex number! %x\n", command_argument_int);
                print_to_console(string_buffer, false);
            }
        }

        if (ngps_stricmp(next_arg, "on") == 0) {
            arg_type |= ARG_TYPE_ON;
        }
        if (ngps_stricmp(next_arg, "true") == 0) {
            arg_type |= ARG_TYPE_ON;
        }
        if (ngps_stricmp(next_arg, "off") == 0) {
            arg_type |= ARG_TYPE_OFF;
        }
        if (ngps_stricmp(next_arg, "false") == 0) {
            arg_type |= ARG_TYPE_OFF;
        }
        if (ngps_stricmp(next_arg, "+") == 0) {
            arg_type |= ARG_TYPE_PLUS;
        }
        if (ngps_stricmp(next_arg, "-") == 0) {
            arg_type |= ARG_TYPE_MINUS;
        }
        if (ngps_stricmp(next_arg, ",") == 0) {
            arg_type |= ARG_TYPE_COMMA;
        }
    }

    if (arg_type & ARG_TYPE_INT) {
        if (command_argument_int) {
            arg_type |= ARG_TYPE_ON;
        } else {
            arg_type |= ARG_TYPE_OFF;
        }
    }

    if (!(arg_type & expected_types)) {
        if ((arg_type & ARG_TYPE_NONE) && !(expected_types & ARG_TYPE_NONE)) {
            print_to_console("Error: Not enough parameters.\n", false);
        } else {
            sprintf(string_buffer, "Error: '%s' invalid type\n", next_arg);
            print_to_console(string_buffer, false);
        }
        longjmp(console_error_jump, 1);
    }
}

void console_initialize(void) {
    if (console_initialized != true) {
        init_char_types();
    }
}

int run_script_file(void) {
    if (calling_a_command_function) {
        process_command(ARG_TYPE_STRING);
        if (strlen(next_arg)) {
            load_script(next_arg);
        } else {
            doing_help_for_comand = 1;
        }
    }

    if (doing_help_for_comand) {
        print_to_console("Usage: script <scriptfilename>\n", false);
    }
}

void load_script(char* script_file) {
    TextFile file;
    char line[256];
    char filename[256];
    int line_number;
    BOOL in_comment;

    strcpy(filename, script_file);
    change_extension(filename, filename, ".txt");
    if (file.Open(filename, 9999999) < 0) {
        sprintf(string_buffer, "Couldn't find a script file '%s'\n", filename);
        print_to_console(string_buffer, false);
        return;
    }

    line_number = 1;
    in_comment = FALSE;
    while (file.ReadLine(line, 256)) {
        trim_whitespace(line, line, -1);
        if (line[0] == '/' && line[1] == '/') {
            continue;
        }
        if (strstr(line, "/*") && !in_comment) {
            in_comment = TRUE;
            continue;
        }
        if (strstr(line, "*/") && in_comment) {
            in_comment = FALSE;
            continue;
        }
        if (!in_comment) {
            if (strlen(line) && execute_command_line(line)) {
                sprintf(string_buffer, "Error processing script line %d: '%s'\n", line_number, line);
            }
            line_number++;
        }
    }
    file.Close();
}

console_command printCommandsToFile("print_to_file", "Print out console commands to the text file console_commands.txt", CALL,
                                    print_commands_to_file);
console_command printCommandsToTTY("print_to_tty", "Print out console commands to the tty window", CALL,
                                   print_commands_to_tty);
console_command runScript("script", "Runs a console script file (.vcs)", CALL, run_script_file);
