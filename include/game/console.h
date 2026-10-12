#ifndef GAME_CONSOLE_H
#define GAME_CONSOLE_H

#include "types.h"

#define MAX_COMMANDS            300
#define MAX_CONSOLE_LINE_LENGTH 128

// Bits of arg_type, describing the argument most recently parsed by process_command().
#define ARG_TYPE_NONE   0x1   // end of the command line
#define ARG_TYPE_STRING 0x2
#define ARG_TYPE_QUOTED 0x4   // only while parsing; quotes are stripped and it becomes a string
#define ARG_TYPE_INT    0x8
#define ARG_TYPE_FLOAT  0x10
#define ARG_TYPE_HEX    0x20
#define ARG_TYPE_ON     0x40  // "on", "true" or a non-zero int
#define ARG_TYPE_OFF    0x80  // "off", "false" or zero
#define ARG_TYPE_PLUS   0x100
#define ARG_TYPE_MINUS  0x200
#define ARG_TYPE_COMMA  0x400

// Callbacks are int(void); CALL commands don't return a value (the game ignores it), which the
// compiler only warns about. Declaring them void and casting would reorder the static initializer.
typedef int (*CommandCallbackInt)(void);

enum command_type { TOGGLE, SET_INT, SET_FLOAT, CALL };

class console_command {
  public:
    console_command(const char* command, const char* description, command_type type, CommandCallbackInt command_function);
    BOOL add_command(const char* name, const char* description, command_type type);

    const char* name;
    const char* description;
    CommandCallbackInt callback;
    command_type type;
};

extern char next_arg[256];
extern char original_arg[256];
extern console_command* Console_commands[MAX_COMMANDS];
extern s32 calling_a_command_function;
extern s32 doing_help_for_comand;
extern s32 checking_status_for_command;
extern u32 arg_type;
extern char* rest_of_command_line;
extern int command_argument_int;
extern float command_argument_float;
extern s32 current_command_count;
extern bool console_initialized;
extern BOOL debug_command_prefixed;

int print_commands_to_file(void);
int print_commands_to_tty(void);
int execute_command_line(const char* command_line);
void print_to_console(const char* buffer, bool unk);
void process_command(int expected_types);
void console_initialize(void);
int run_script_file(void);
void load_script(char* script_file);

#endif // GAME_CONSOLE_H
