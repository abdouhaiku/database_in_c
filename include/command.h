//
// Created by Abdou on 25/07/2026.
//

#ifndef DATABASE_IN_C_COMMAND_H
#define DATABASE_IN_C_COMMAND_H
#include "command_result.h"
#include "repl.h"
#include "table.h"

CommandResult do_meta_command(InputBuffer *input_buffer);

CommandResult process_command(InputBuffer *input_buffer, Table *table);


#endif //DATABASE_IN_C_COMMAND_H
