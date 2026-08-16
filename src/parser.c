//
// Created by Abdou on 08/08/2026.
//

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

static void parser_set_error(Parser *parser, const char *expected) {
    snprintf(parser->error, sizeof(parser->error),
             "Parse error at line %d, column %d: expected %s, found %s",
             parser->current.line, parser->current.column,
             expected, token_type_name(&parser->current));
}

void parser_init(Parser *parser, const char *sql) {
    tokenizer_init(&parser->tokenizer, sql);
    parser->error[0] = '\0';
    get_next_token(&parser->tokenizer, &parser->current);
}

void parser_advance(Parser *parser) {
    parser->current.type = TOKEN_NONE;
    get_next_token(&parser->tokenizer, &parser->current);
}

static AstNode *parse_primary_expression(Parser *parser) {
    AstNode *node = malloc(sizeof(AstNode));

    switch (parser->current.type) {
        case TOKEN_INTEGER:
            node->type = EXPR_LITERAL_INT;
            node->as.literal_int = parser->current.int_value;
            break;
        case TOKEN_STRING:
            node->type = EXPR_LITERAL_STRING;
            node->as.literal_string.text = parser->current.text;
            node->as.literal_string.length = parser->current.text_length;
            break;
        case TOKEN_IDENTIFIER:
            node->type = EXPR_COLUMN;
            node->as.column.name = parser->current.text;
            node->as.column.length = parser->current.text_length;
            break;
        case TOKEN_TRUE:
            node->type = EXPR_LITERAL_BOOLEAN;
            node->as.literal_boolean.bool_value = true;
            break;
        case TOKEN_FALSE:
            node->type = EXPR_LITERAL_BOOLEAN;
            node->as.literal_boolean.bool_value = false;
            break;
        default:
            parser_set_error(parser, "a literal or column reference");
            free(node);
            return NULL;
    }
    parser_advance(parser);
    return node;
}

static ColumnType parse_column_type(Parser *parser) {
    switch (parser->current.type) {
        case TOKEN_TYPE_TEXT:
            return COLUMN_TEXT;
        case TOKEN_TYPE_BOOLEAN:
            return COLUMN_BOOLEAN;
        case TOKEN_TYPE_INTEGER:
            return COLUMN_INTEGER;
        default:
            // Unreachable: the caller only calls this after confirming
            // current.type is one of the three cases above.
            return COLUMN_INTEGER;
    }
}

AstNode *parse_expression(Parser *parser) {
    AstNode *left = parse_primary_expression(parser);
    if (left == NULL) {
        return NULL; // parse_primary_expression already set the error
    }

    // peek at whatever parse_primary_expression's advance left us sitting on.
    if (parser->current.type != TOKEN_EQUAL) {
        return left;
    }
    parser_advance(parser); // consume '='

    AstNode *right = parse_primary_expression(parser);
    if (right == NULL) {
        ast_destroy(left);
        return NULL; // parse_primary_expression already set the error
    }

    AstNode *equals = malloc(sizeof(AstNode));
    equals->type = EXPR_EQUALS;
    equals->as.equals.left = left;
    equals->as.equals.right = right;
    return equals;
}

AstNode *parse_select_statement(Parser *parser) {
    if (parser->current.type != TOKEN_SELECT) {
        parser_set_error(parser, "SELECT");
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    node->type = AST_SELECT;
    node->as.select.is_star = false;
    node->as.select.column_count = 0;
    node->as.select.where = NULL;
    parser_advance(parser);

    if (parser->current.type == TOKEN_STAR) {
        node->as.select.is_star = true;
        parser_advance(parser);
    } else {
        // Column list
        while (parser->current.type != TOKEN_EOF && parser->current.type != TOKEN_FROM) {
            if (parser->current.type == TOKEN_COMMA) {
                parser_advance(parser);
                continue;
            }
            if (parser->current.type != TOKEN_IDENTIFIER) {
                // Unexpected token where a column name or comma was expected.
                parser_set_error(parser, "a column name or ','");
                ast_destroy(node);
                return NULL;
            }
            if (node->as.select.column_count >= MAX_TABLE_COLUMNS) {
                parser_set_error(parser, "at most 8 columns");
                ast_destroy(node);
                return NULL;
            }
            node->as.select.columns[node->as.select.column_count] = parse_primary_expression(parser);
            node->as.select.column_count++;
        }
    }

    if (parser->current.type != TOKEN_FROM) {
        // FROM is required.
        parser_set_error(parser, "FROM");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current.type != TOKEN_IDENTIFIER) {
        // Table name is required after FROM.
        parser_set_error(parser, "a table name");
        ast_destroy(node);
        return NULL;
    }
    node->as.select.table = parser->current.text;
    node->as.select.table_length = parser->current.text_length;
    parser_advance(parser);

    if (parser->current.type == TOKEN_WHERE) {
        parser_advance(parser); // consume WHERE itself before parsing the condition
        node->as.select.where = parse_expression(parser);
        if (node->as.select.where == NULL) {
            // parse_expression already set the error
            ast_destroy(node);
            return NULL;
        }
    }

    return node;
}

/*
UPDATE table_name
SET column1 = value1, column2 = value2
WHERE condition;
*/
AstNode *parse_update_statement(Parser *parser) {
    if (parser->current.type != TOKEN_UPDATE) {
        parser_set_error(parser, "UPDATE");
        return NULL;
    }
    AstNode *node = malloc(sizeof(AstNode));
    node->type = AST_UPDATE;
    node->as.update_table.column_count = 0;
    node->as.update_table.values_count = 0;
    parser_advance(parser);

    if (parser->current.type !=TOKEN_IDENTIFIER) {
        parser_set_error(parser, "a table name");
        ast_destroy(node);
        return NULL;
    }
    node->as.update_table.table = parser->current.text; // was as.create_table.table (wrong union member)
    node->as.update_table.table_length = parser->current.text_length;
    parser_advance(parser);

    if (parser->current.type != TOKEN_SET) {
        parser_set_error(parser, "SET keyword");
        ast_destroy(node);
        return NULL;
    }

    parser_advance(parser);

    while (parser->current.type != TOKEN_EOF && parser->current.type != TOKEN_WHERE) {
        if (parser->current.type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }
        if (parser->current.type != TOKEN_IDENTIFIER) {
            // Unexpected token where a column name or comma was expected.
            parser_set_error(parser, "a column name or ','");
            ast_destroy(node);
            return NULL;
        }
        if (node->as.update_table.column_count >= MAX_TABLE_COLUMNS) {
            parser_set_error(parser, "at most 8 columns");
            ast_destroy(node);
            return NULL;
        }
        node->as.update_table.columns[node->as.update_table.column_count] = parse_primary_expression(parser);
        node->as.update_table.column_count++;
        if (parser->current.type != TOKEN_EQUAL) {
            parser_set_error(parser, "=");
            ast_destroy(node);
            return NULL;
        }

        parser_advance(parser);
        if (node->as.update_table.values_count >= MAX_TABLE_COLUMNS) {
            parser_set_error(parser, "at most 8 columns");
            ast_destroy(node);
            return NULL;
        }

        AstNode *value = parse_primary_expression(parser);
        if (value == NULL) {
            ast_destroy(node);
            return NULL;
        }
        node->as.update_table.values[node->as.update_table.values_count] = value;
        node->as.update_table.values_count++;

    }

    if (parser->current.type != TOKEN_WHERE) {
        parser_set_error(parser, "WHERE keyword");
        ast_destroy(node);
        return NULL;
    }

    parser_advance(parser); // consume WHERE itself before parsing the condition
    node->as.update_table.where = parse_expression(parser);
    if (node->as.update_table.where == NULL) {
        // parse_expression already set the error
        ast_destroy(node);
        return NULL;
    }

    return node;
}

/*
DELETE FROM table_name
WHERE condition;
*/
AstNode *parse_delete_statement(Parser *parser) {
    if (parser->current.type != TOKEN_DELETE) {
        parser_set_error(parser, "DELETE");
        return NULL;
    }
    AstNode *node = malloc(sizeof(AstNode));
    node->type = AST_DELETE;
    node->as.delete_table.where = NULL;
    parser_advance(parser);

    if (parser->current.type != TOKEN_FROM) {
        parser_set_error(parser, "FROM");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current.type != TOKEN_IDENTIFIER) {
        parser_set_error(parser, "a table name");
        ast_destroy(node);
        return NULL;
    }
    node->as.delete_table.table = parser->current.text;
    node->as.delete_table.table_length = parser->current.text_length;
    parser_advance(parser);

    if (parser->current.type == TOKEN_WHERE) {
        parser_advance(parser); // consume WHERE itself before parsing the condition
        node->as.delete_table.where = parse_expression(parser);
        if (node->as.delete_table.where == NULL) {
            // parse_expression already set the error
            ast_destroy(node);
            return NULL;
        }
    }
    return node;
}

AstNode *parse_insert_statement(Parser *parser) {
    if (parser->current.type != TOKEN_INSERT) {
        parser_set_error(parser, "INSERT");
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    node->type = AST_INSERT;
    node->as.insert.values_count = 0;
    parser_advance(parser);

    // INSERT INTO users VALUES (1, 'Alice', 'alice@example.com');

    if (parser->current.type != TOKEN_INTO) {
        parser_set_error(parser, "INTO");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current.type != TOKEN_IDENTIFIER) {
        // Table name is required after INTO.
        parser_set_error(parser, "a table name");
        ast_destroy(node);
        return NULL;
    }
    node->as.insert.table = parser->current.text;
    node->as.insert.table_length = parser->current.text_length;
    parser_advance(parser);

    if (parser->current.type != TOKEN_VALUE) {
        // VALUES keyword is required after the table name.
        parser_set_error(parser, "VALUES");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current.type != TOKEN_LPAREN) {
        // '(' is required to start the value list.
        parser_set_error(parser, "'('");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    int i = 0;
    while (parser->current.type != TOKEN_EOF && parser->current.type != TOKEN_RPAREN) {
        if (parser->current.type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }
        if (i >= MAX_TABLE_COLUMNS) {
            parser_set_error(parser, "at most 8 values");
            ast_destroy(node);
            return NULL;
        }
        AstNode *value = parse_primary_expression(parser);
        if (value == NULL) {
            ast_destroy(node);
            return NULL;
        }
        node->as.insert.values[i] = value;
        node->as.insert.values_count++;
        i++;
    }

    if (parser->current.type != TOKEN_RPAREN) {
        // ')' is required to close the value list.
        parser_set_error(parser, "')'");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);
    return node;
}


// CREATE TABLE users (
/*
id INTEGER PRIMARY KEY,
name TEXT,
age INTEGER
);
*/
AstNode *parse_create_statement(Parser *parser) {
    if (parser->current.type != TOKEN_CREATE) {
        parser_set_error(parser, "CREATE");
        return NULL;
    }
    AstNode *node = malloc(sizeof(AstNode));
    node->type = AST_CREATE_TABLE;
    node->as.create_table.column_count = 0;
    parser_advance(parser);

    if (parser->current.type != TOKEN_TABLE) {
        parser_set_error(parser, "TABLE");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    if (parser->current.type != TOKEN_IDENTIFIER) {
        parser_set_error(parser, "a table name");
        ast_destroy(node);
        return NULL;
    }
    node->as.create_table.table = parser->current.text;
    node->as.create_table.table_length = parser->current.text_length;
    parser_advance(parser);

    if (parser->current.type != TOKEN_LPAREN) {
        parser_set_error(parser, "'('");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    while (parser->current.type != TOKEN_EOF && parser->current.type != TOKEN_RPAREN) {
        if (node->as.create_table.column_count > 0) {
            // Every column after the first must be separated by a comma.
            if (parser->current.type != TOKEN_COMMA) {
                parser_set_error(parser, "',' or ')'");
                ast_destroy(node);
                return NULL;
            }
            parser_advance(parser);
        }

        if (node->as.create_table.column_count >= MAX_TABLE_COLUMNS) {
            parser_set_error(parser, "at most 8 columns");
            ast_destroy(node);
            return NULL;
        }

        AstNode *column_def = malloc(sizeof(AstNode));
        column_def->type = EXPR_COLUMN_DEFINITION;
        column_def->as.column_definition.is_primary = false;

        if (parser->current.type != TOKEN_IDENTIFIER) {
            parser_set_error(parser, "a column name");
            ast_destroy(node);
            free(column_def);
            return NULL;
        }
        column_def->as.column_definition.name = parser->current.text;
        column_def->as.column_definition.length = parser->current.text_length;
        parser_advance(parser);

        if (parser->current.type != TOKEN_TYPE_TEXT && parser->current.type != TOKEN_TYPE_INTEGER
            && parser->current.type != TOKEN_TYPE_BOOLEAN) {
            parser_set_error(parser, "TEXT, INTEGER, or BOOLEAN");
            ast_destroy(node);
            free(column_def);
            return NULL;
        }
        column_def->as.column_definition.columnType = parse_column_type(parser);
        parser_advance(parser);

        if (parser->current.type == TOKEN_PRIMARY) {
            parser_advance(parser);
            if (parser->current.type != TOKEN_KEY) {
                parser_set_error(parser, "KEY");
                ast_destroy(node);
                free(column_def);
                return NULL;
            }
            column_def->as.column_definition.is_primary = true;
            parser_advance(parser);
        }

        node->as.create_table.columns_definitions[node->as.create_table.column_count] = column_def;
        node->as.create_table.column_count++;
    }

    if (parser->current.type != TOKEN_RPAREN) {
        parser_set_error(parser, "')'");
        ast_destroy(node);
        return NULL;
    }
    parser_advance(parser);

    // Every table must have exactly one integer primary key.
    size_t primary_key_count = 0;
    bool primary_key_is_integer = true;
    for (size_t i = 0; i < node->as.create_table.column_count; i++) {
        AstNode *column_def = node->as.create_table.columns_definitions[i];
        if (column_def->as.column_definition.is_primary) {
            primary_key_count++;
            primary_key_is_integer = column_def->as.column_definition.columnType == COLUMN_INTEGER;
        }
    }
    if (primary_key_count != 1) {
        parser_set_error(parser, "exactly one PRIMARY KEY column");
        ast_destroy(node);
        return NULL;
    }
    if (!primary_key_is_integer) {
        parser_set_error(parser, "an INTEGER type for the PRIMARY KEY column");
        ast_destroy(node);
        return NULL;
    }

    return node;
}
