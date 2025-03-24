#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Error codes
#define NO_ERROR 0
#define INVALID_KEYWORD 1
#define VAR_MISMATCH 2
#define INVALID_CHAR 3
#define MISSING_SEMICOLON 4
#define MISSING_OPERATOR 5
#define SYNTAX_ERROR 6

// State encoding (matching the Verilog implementation)
#define IDLE 0
#define READ_IF 1 // Read "if" keyword
#define READ_OPEN_PAREN 2
#define READ_VAR 3
#define READ_COND_OPERATOR 4  // Read first char of comparator
#define READ_COND_OPERATOR2 5 // Read second char of comparator (if any)
#define READ_VALC 6
#define READ_CLOSE_PAREN 7
#define READ_BEGIN 8                 // Read "begin" keyword
#define READ_ASSIGNMENT_VAR 9        // Read variable for assignment
#define READ_ASSIGNMENT_OPERATOR 10  // Read assignment operator (<= or =)
#define READ_CONST1 11               // Read constant for true branch
#define READ_SEMICOLON1 12           // Expect ; after const1
#define READ_END1 13                 // Read "end" keyword for true branch
#define READ_ELSE 14                 // Expect "else" keyword
#define READ_BEGIN2 16               // Read "begin" for false branch
#define READ_ASSIGNMENT_VAR2 18      // Read variable for false branch
#define READ_ASSIGNMENT_OPERATOR2 19 // Read assignment operator for false branch
#define READ_CONST2 20               // Read constant for false branch
#define READ_SEMICOLON2 21           // Expect ; after const2
#define READ_END2 22                 // Read "end" keyword for false branch
#define EVALUATE 23

// Comparator types
#define EQ 0
#define NE 1
#define LT 2
#define GT 3
#define LE 4
#define GE 5

typedef struct
{
    int state;

    // Keyword parsing
    char keyword_buffer[32];
    int keyword_index;
    bool keyword_complete;

    char cond_var[16];
    int cond_var_length;
    int cond_var_idx;

    char assignment_var[16];
    int assignment_var_length;

    char assignment_var2[16];
    int assignment_var2_length;
    int assignment_var2_idx;

    bool var_match;
    bool reading_var;

    // Data registers
    int x;    // Input value
    int valC; // Comparison value
    bool is_valC_negative;
    int const1; 
    bool is_const1_negative;
    int const2; 
    bool is_const2_negative;
    int num_buffer;
    bool parsing_number;

    int paren_count;

    bool blocking_assignment1;
    bool blocking_assignment2;

    int comparator;
    char op_first;

    // Result value
    int p;

    // Status flags
    bool parsing_done;
    bool error_flag;
    int error_code;

    // for debugging
    bool debug_mode;
} Parser;

void parser_init(Parser *parser)
{
    parser->state = IDLE;
    parser->keyword_index = 0;
    parser->keyword_complete = false;

    parser->cond_var_length = 0;
    parser->cond_var_idx = 0;
    parser->assignment_var_length = 0;
    parser->assignment_var2_length = 0;
    parser->assignment_var2_idx = 0;
    parser->var_match = false;
    parser->reading_var = false;

    parser->x = 0;
    parser->valC = 0;
    parser->is_valC_negative = false;
    parser->const1 = 0;
    parser->is_const1_negative = false;
    parser->const2 = 0;
    parser->is_const2_negative = false;
    parser->num_buffer = 0;
    parser->parsing_number = false;

    parser->paren_count = 0;

    parser->blocking_assignment1 = false;
    parser->blocking_assignment2 = false;

    parser->comparator = 0;
    parser->op_first = 0;

    parser->p = 0;
    parser->parsing_done = false;
    parser->error_flag = false;
    parser->error_code = NO_ERROR;

    memset(parser->cond_var, 0, sizeof(parser->cond_var));
    memset(parser->assignment_var, 0, sizeof(parser->assignment_var));
    memset(parser->assignment_var2, 0, sizeof(parser->assignment_var2));

    parser->debug_mode = false;
}

// Function to check if variable names match
bool var_names_match(Parser *parser)
{
    if (parser->assignment_var_length != parser->assignment_var2_length)
    {
        return false;
    }

    // Compare each character
    for (int i = 0; i < parser->assignment_var_length; i++)
    {
        if (parser->assignment_var[i] != parser->assignment_var2[i])
        {
            return false; 
        }
    }

    return true; 
}

void debug_print(Parser *parser, char ascii_char)
{
    if (!parser->debug_mode)
        return;

    printf("State: %2d, curr_char: %c (0x%02x), parsing_number: %d\n",
           parser->state, ascii_char, ascii_char, parser->parsing_number);

    printf("cond_var: %s", parser->cond_var);
    for (int i = strlen(parser->cond_var); i < 16; i++)
        printf(" ");

    printf(", assignment_var: %s", parser->assignment_var);
    for (int i = strlen(parser->assignment_var); i < 16; i++)
        printf(" ");

    printf(", assignment_var2: %s", parser->assignment_var2);
    for (int i = strlen(parser->assignment_var2); i < 16; i++)
        printf(" ");

    printf(", paren_count: %d\n", parser->paren_count);
    printf("valC: %11d, const1: %11d, const2: %11d, error_code: %2d\n",
           parser->valC, parser->const1, parser->const2, parser->error_code);
    printf("---------------------------------------------------------------------------------------------------------------------------------------------\n\n");
}

void process_char(Parser *parser, char ascii_char)
{
    debug_print(parser, ascii_char);

    bool is_digit = isdigit(ascii_char);
    bool is_letter = isalpha(ascii_char);
    bool is_underscore = (ascii_char == '_');
    bool is_id_start = is_letter;
    bool is_id_char = is_letter || is_digit || is_underscore;
    bool is_whitespace = (ascii_char == ' ' || ascii_char == '\t' || ascii_char == '\n' || ascii_char == '\r');

    switch (parser->state)
    {
    case IDLE:
        if (is_whitespace)
        {
            parser->state = IDLE;
        }
        else if (ascii_char == 'i')
        {
            parser->keyword_index = 1;
            parser->state = READ_IF;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_IF:
        if (ascii_char == 'f')
        {
            parser->keyword_index = 0;
            parser->state = READ_OPEN_PAREN;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_OPEN_PAREN:
        if (is_whitespace)
        {
            parser->state = READ_OPEN_PAREN;
        }
        else if (ascii_char == '(')
        {
            parser->paren_count++;
            parser->state = READ_VAR;
            parser->cond_var_idx = 0;
            parser->cond_var_length = 0;
            parser->reading_var = false;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_VAR:
        if (is_whitespace && !parser->reading_var)
        {
            parser->state = READ_VAR;
        }
        else if (!parser->reading_var && is_id_start)
        {
            // First character of identifier - must be a letter
            parser->cond_var[0] = ascii_char; // Start at index 0
            parser->cond_var_idx = 1;
            parser->cond_var_length = 1;
            parser->reading_var = true;
        }
        else if (parser->reading_var && is_id_char)
        {
            // Subsequent characters - can be letter, digit, or underscore
            if (parser->cond_var_idx < 15)
            { // Prevent buffer overflow
                parser->cond_var[parser->cond_var_idx] = ascii_char;
                parser->cond_var_idx++;
                parser->cond_var_length++;
            }

            if (parser->cond_var_idx == 15)
            { // Max length reached
                parser->state = READ_COND_OPERATOR;
                parser->reading_var = false;
                parser->cond_var[15] = '\0'; // Null-terminate
            }
        }
        else if (parser->reading_var && (is_whitespace || ascii_char == '>' || ascii_char == '<' ||
                                         ascii_char == '=' || ascii_char == '!'))
        {
            // Variable name complete, ready for operator
            parser->cond_var[parser->cond_var_length] = '\0'; // Null-terminate
            parser->state = READ_COND_OPERATOR;
            parser->reading_var = false;

            // Process operator right away if not whitespace
            if (!is_whitespace)
            {
                if (parser->paren_count == 0)
                {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                }
                parser->op_first = ascii_char;
                parser->state = READ_COND_OPERATOR2;
            }
        }
        else if (ascii_char == '(')
        {
            // Opening nested parenthesis
            parser->paren_count++;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_COND_OPERATOR:
        if (is_whitespace)
        {
            parser->state = READ_COND_OPERATOR;
        }
        else if (ascii_char == ')')
        {
            if (parser->paren_count == 0)
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            parser->paren_count--;
        }
        else if (ascii_char == '<' || ascii_char == '>' || ascii_char == '=' || ascii_char == '!')
        {
            if (parser->paren_count == 0)
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            parser->op_first = ascii_char;
            parser->state = READ_COND_OPERATOR2;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_CHAR;
        }
        break;

    case READ_COND_OPERATOR2:
        // Handle whitespace first
        if (is_whitespace)
        {
            // Just stay in the same state
            break;
        }
        // Check if there's a negative sign ("-") before the digit
        else if (ascii_char == '-')
        {
            parser->is_valC_negative = true;
        }

        switch (parser->op_first)
        {
        case '<':
            if (ascii_char == '=')
            {
                parser->comparator = LE;
                parser->state = READ_VALC;
            }
            else if (is_digit)
            {
                parser->comparator = LT; // single-character "<"
                // Start processing the digit immediately
                parser->num_buffer = (parser->num_buffer * 10) + (ascii_char - '0');
                parser->parsing_number = true;
                parser->state = READ_VALC;
            }
            else if (ascii_char == '(')
            {
                parser->comparator = LT;
                parser->paren_count++;
                parser->state = READ_VALC;
            }
            else if (!is_whitespace) 
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;

        case '>':
            if (ascii_char == '=')
            {
                parser->comparator = GE;
                parser->state = READ_VALC;
            }
            else if (is_digit)
            {
                parser->comparator = GT; // single-character ">"
                // Start processing the digit immediately
                parser->num_buffer = (parser->num_buffer * 10) + (ascii_char - '0');
                parser->parsing_number = true;
                parser->state = READ_VALC;
            }
            else if (ascii_char == '(')
            {
                parser->comparator = GT;
                parser->paren_count++;
                parser->state = READ_VALC;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;

        case '=':
            if (ascii_char == '=')
            {
                parser->comparator = EQ;
                parser->state = READ_VALC;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = MISSING_OPERATOR;
            }
            break;

        case '!':
            if (ascii_char == '=')
            {
                parser->comparator = NE;
                parser->state = READ_VALC;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = MISSING_OPERATOR;
            }
            break;

        default:
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
            break;
        }
        break;

    case READ_VALC:
        if (is_whitespace)
        {
            parser->state = READ_VALC;
        }
        // Check if there's a negative sign ("-") before the digit
        else if (ascii_char == '-' && !parser->parsing_number)
        {
            parser->is_valC_negative = true;
        }
        else if (ascii_char == '(' && !parser->parsing_number)
        {
            parser->paren_count++;
        }
        else if (is_digit)
        {
            parser->num_buffer = (parser->num_buffer * 10) + (ascii_char - '0');
            parser->parsing_number = true;
        }
        else if (parser->parsing_number)
        {
            if (parser->is_valC_negative)
                parser->valC = -parser->num_buffer;
            else
                parser->valC = parser->num_buffer;

            parser->num_buffer = 0;
            parser->parsing_number = false;

            if (ascii_char == ')')
            {
                if (parser->paren_count == 0)
                {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                }
                parser->paren_count--;
                parser->state = READ_CLOSE_PAREN;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
        }
        break;

    case READ_CLOSE_PAREN:
        if (is_whitespace)
        {
            parser->state = READ_CLOSE_PAREN;
        }
        else if (ascii_char == 'b')
        {
            parser->keyword_index = 1;
            strcpy(parser->keyword_buffer, "b");
            if (parser->paren_count != 0)
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            parser->state = READ_BEGIN;
        }
        else if (ascii_char == ')')
        {
            parser->paren_count--;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_BEGIN:
        switch (parser->keyword_index)
        {
        case 1:
            if (ascii_char == 'e')
            {
                strcat(parser->keyword_buffer, "e");
                parser->keyword_index++;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        case 2:
            if (ascii_char == 'g')
            {
                strcat(parser->keyword_buffer, "g");
                parser->keyword_index++;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        case 3:
            if (ascii_char == 'i')
            {
                strcat(parser->keyword_buffer, "i");
                parser->keyword_index++;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        case 4:
            if (ascii_char == 'n')
            {
                parser->keyword_index = 0;
                parser->state = READ_ASSIGNMENT_VAR;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        default:
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
            break;
        }
        break;

    case READ_ASSIGNMENT_VAR:
        if (is_whitespace && !parser->reading_var)
        {
            parser->state = READ_ASSIGNMENT_VAR;
        }
        else if (!parser->reading_var && is_id_start)
        {
            // First character of identifier - must be a letter
            parser->assignment_var[0] = ascii_char; // Start at index 0
            parser->assignment_var_length = 1;
            parser->cond_var_idx = 1; // Reuse this counter for tracking position
            parser->reading_var = true;
        }
        else if (parser->reading_var && is_id_char)
        {
            // Subsequent characters - can be letter, digit, or underscore
            if (parser->cond_var_idx < 15)
            { 
                parser->assignment_var[parser->cond_var_idx] = ascii_char;
                parser->cond_var_idx++;
                parser->assignment_var_length++;
            }

            if (parser->cond_var_idx == 15)
            { // Max length reached
                parser->state = READ_ASSIGNMENT_OPERATOR;
                parser->reading_var = false;
                parser->assignment_var[15] = '\0'; // terminate with null char
            }
        }
        else if (parser->reading_var && (is_whitespace || ascii_char == '=' || ascii_char == '<'))
        {
            // Variable name complete, ready for operator
            parser->assignment_var[parser->assignment_var_length] = '\0'; 
            parser->state = READ_ASSIGNMENT_OPERATOR;
            parser->reading_var = false;

            // Process operator rightaway if not whitespace
            if (!is_whitespace)
            {
                if (ascii_char == '<')
                {
                    parser->blocking_assignment1 = false;
                    parser->op_first = '<';
                }
                else if (ascii_char == '=')
                {
                    parser->blocking_assignment1 = true;
                    parser->op_first = 0;
                    parser->num_buffer = 0;
                    parser->parsing_number = false;
                    parser->is_const1_negative = false;
                    parser->state = READ_CONST1;
                }
            }
        }
        else if (ascii_char == '(')
        {
            parser->paren_count++;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_ASSIGNMENT_OPERATOR:
        if (is_whitespace)
        {
            parser->state = READ_ASSIGNMENT_OPERATOR;
        }
        else if (ascii_char == ')')
            parser->paren_count--;
        else if (ascii_char == '<')
        {
            parser->blocking_assignment1 = false;
            parser->state = READ_ASSIGNMENT_OPERATOR;
        }
        else if (ascii_char == '=')
        {
            if (parser->op_first == '<')
            {
                parser->blocking_assignment1 = false; // non-blocking (<=)
            }
            else
            {
                parser->blocking_assignment1 = true; // blocking (=)
            }
            parser->op_first = 0;
            parser->num_buffer = 0;
            parser->parsing_number = false;
            parser->is_const1_negative = false;
            parser->state = READ_CONST1;
        }
        else
        {
            parser->op_first = ascii_char;
        }
        break;

    case READ_CONST1:
        if (is_whitespace)
        {
            if (parser->is_const1_negative || parser->parsing_number)
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            else
            {
                parser->state = READ_CONST1;
            }
        }
        else if (ascii_char == '(' && !parser->parsing_number)
        {
            parser->paren_count++;
        }
        else if (ascii_char == ')')
        {
            if (parser->num_buffer == 0)
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            else
            {
                parser->paren_count--;
            }
        }
        // Check if there's a negative sign ("-") before the digit
        else if (ascii_char == '-' && !parser->parsing_number)
        {
            parser->is_const1_negative = true;
        }
        else if (is_digit)
        {
            parser->num_buffer = (parser->num_buffer * 10) + (ascii_char - '0');
            parser->parsing_number = true;
        }
        else if (parser->parsing_number)
        {
            if (parser->is_const1_negative)
                parser->const1 = -parser->num_buffer;
            else
                parser->const1 = parser->num_buffer;

            parser->num_buffer = 0;
            parser->parsing_number = false;

            if (ascii_char == ';')
            {
                parser->state = READ_SEMICOLON1;
            }
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_SEMICOLON1:
        if (parser->paren_count != 0)
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        if (is_whitespace)
        {
            parser->state = READ_SEMICOLON1;
        }
        else if (ascii_char == 'e')
        {
            parser->keyword_index = 1;
            strcpy(parser->keyword_buffer, "e");
            parser->state = READ_END1;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_END1:
        switch (parser->keyword_index)
        {
        case 1:
            if (ascii_char == 'n')
            {
                strcat(parser->keyword_buffer, "n");
                parser->keyword_index++;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        case 2:
            if (ascii_char == 'd')
            {
                parser->keyword_index = 0;
                parser->state = READ_ELSE;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        default:
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
            break;
        }
        break;

    case READ_ELSE:
        if (is_whitespace)
        {
            parser->state = READ_ELSE;
        }
        else if (ascii_char == 'e' && parser->keyword_index == 0)
        {
            parser->keyword_index = 1;
            strcpy(parser->keyword_buffer, "e");
        }
        else if (parser->keyword_index == 1 && ascii_char == 'l')
        {
            strcat(parser->keyword_buffer, "l");
            parser->keyword_index++;
        }
        else if (parser->keyword_index == 2 && ascii_char == 's')
        {
            strcat(parser->keyword_buffer, "s");
            parser->keyword_index++;
        }
        else if (parser->keyword_index == 3 && ascii_char == 'e')
        {
            parser->keyword_index = 0;
            parser->state = READ_BEGIN2;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_BEGIN2:
        if (is_whitespace)
        {
            parser->state = READ_BEGIN2;
        }
        else if (ascii_char == 'b')
        {
            parser->keyword_index = 1;
            strcpy(parser->keyword_buffer, "b");
            parser->state = READ_BEGIN2;
        }
        else if (parser->keyword_index == 1 && ascii_char == 'e')
        {
            strcat(parser->keyword_buffer, "e");
            parser->keyword_index++;
        }
        else if (parser->keyword_index == 2 && ascii_char == 'g')
        {
            strcat(parser->keyword_buffer, "g");
            parser->keyword_index++;
        }
        else if (parser->keyword_index == 3 && ascii_char == 'i')
        {
            strcat(parser->keyword_buffer, "i");
            parser->keyword_index++;
        }
        else if (parser->keyword_index == 4 && ascii_char == 'n')
        {
            parser->keyword_index = 0;
            parser->state = READ_ASSIGNMENT_VAR2;
            parser->reading_var = false;
            parser->assignment_var2_idx = 0;
            parser->assignment_var2_length = 0;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_ASSIGNMENT_VAR2:
        if (is_whitespace && !parser->reading_var)
        {
            parser->state = READ_ASSIGNMENT_VAR2;
        }
        else if (!parser->reading_var && is_id_start)
        {
            // First character of identifier - must be a letter
            parser->assignment_var2[0] = ascii_char; // Start at index 0
            parser->assignment_var2_length = 1;
            parser->assignment_var2_idx = 1; // Tracking position
            parser->reading_var = true;
        }
        else if (parser->reading_var && is_id_char)
        {
            // Subsequent characters - can be letter, digit, or underscore
            if (parser->assignment_var2_idx < 15)
            { 
                parser->assignment_var2[parser->assignment_var2_idx] = ascii_char;
                parser->assignment_var2_idx++;
                parser->assignment_var2_length++;
            }

            if (parser->assignment_var2_idx == 15)
            { 
                parser->state = READ_ASSIGNMENT_OPERATOR2;
                parser->reading_var = false;
                parser->assignment_var2[15] = '\0'; 
            }
        }
        else if (parser->reading_var && (is_whitespace || ascii_char == '=' || ascii_char == '<'))
        {
            // Variable name complete, ready for operator
            parser->assignment_var2[parser->assignment_var2_length] = '\0'; 
            parser->state = READ_ASSIGNMENT_OPERATOR2;
            parser->reading_var = false;

            // Process operator rightaway if not whitespace
            if (!is_whitespace)
            {
                if (ascii_char == '<')
                {
                    parser->blocking_assignment2 = false;
                    parser->op_first = '<';
                }
                else if (ascii_char == '=')
                {
                    parser->blocking_assignment2 = true;
                    parser->op_first = 0;
                    parser->num_buffer = 0;
                    parser->parsing_number = false;
                    parser->is_const2_negative = false;
                    parser->state = READ_CONST2;
                }
            }
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_ASSIGNMENT_OPERATOR2:
        if (is_whitespace)
        {
            parser->state = READ_ASSIGNMENT_OPERATOR2;
        }
        else if (ascii_char == ')')
            parser->paren_count--;
        else if (ascii_char == '<')
        {
            parser->blocking_assignment2 = false;
            parser->op_first = '<';
        }
        else if (ascii_char == '=')
        {
            if (parser->op_first == '<')
            {
                parser->blocking_assignment2 = false; // non-blocking (<=)
            }
            else
            {
                parser->blocking_assignment2 = true; // blocking (=)
            }
            parser->op_first = 0;
            parser->num_buffer = 0;
            parser->parsing_number = false;
            parser->is_const2_negative = false;
            parser->state = READ_CONST2;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_CONST2:
        if (is_whitespace)
        {
            if (parser->is_const2_negative || parser->parsing_number)
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            else
            {
                parser->state = READ_CONST2;
            }
        }
        else if (ascii_char == '(' && !parser->parsing_number)
        {
            parser->paren_count++;
        }
        else if (ascii_char == ')')
        {
            if (parser->parsing_number)
            {
                parser->paren_count--;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
        }
        // Check if there's a negative sign ("-") before the digit
        else if (ascii_char == '-' && !parser->parsing_number)
        {
            parser->is_const2_negative = true;
        }
        else if (is_digit)
        {
            parser->num_buffer = (parser->num_buffer * 10) + (ascii_char - '0');
            parser->parsing_number = true;
        }
        else if (parser->parsing_number)
        {
            if (parser->is_const2_negative)
                parser->const2 = -parser->num_buffer;
            else
                parser->const2 = parser->num_buffer;

            parser->num_buffer = 0;
            parser->parsing_number = false;

            if (ascii_char == ';')
            {
                parser->state = READ_SEMICOLON2;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        break;

    case READ_SEMICOLON2:
        if (parser->paren_count != 0)
        {
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
        }
        if (is_whitespace)
        {
            parser->state = READ_SEMICOLON2;
        }
        else if (ascii_char == 'e')
        {
            parser->keyword_index = 1;
            strcpy(parser->keyword_buffer, "e");
            parser->state = READ_END2;
        }
        else
        {
            parser->error_flag = true;
            parser->error_code = INVALID_KEYWORD;
        }
        break;

    case READ_END2:
        switch (parser->keyword_index)
        {
        case 1:
            if (ascii_char == 'n')
            {
                strcat(parser->keyword_buffer, "n");
                parser->keyword_index++;
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        case 2:
            if (ascii_char == 'd')
            {
                // Check if variable names match between true and false branches
                parser->var_match = var_names_match(parser);
                if (!parser->var_match)
                {
                    parser->error_flag = true;
                    parser->error_code = VAR_MISMATCH;
                }
                else
                {
                    parser->state = EVALUATE;
                    parser->parsing_done = true;
                }
            }
            else
            {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;

        default:
            parser->error_flag = true;
            parser->error_code = SYNTAX_ERROR;
            break;
        }
        break;

    case EVALUATE:
        // This state is reached when the parser has successfully parsed the if-else construct
        // Evaluation logic is handled in another function
        break;

    default:
        parser->error_flag = true;
        parser->error_code = SYNTAX_ERROR;
        break;
    }
}

// Function to evaluate the condition with the given input value
bool evaluate_condition(Parser *parser, int input_value) 
{
    bool condition_result = false;
    
    printf("EVALUATING: %s=%d %s %d\n", 
        parser->cond_var, input_value, 
        parser->comparator == EQ ? "==" :
        parser->comparator == NE ? "!=" :
        parser->comparator == LT ? "<" :
        parser->comparator == GT ? ">" :
        parser->comparator == LE ? "<=" :
        parser->comparator == GE ? ">=" : "??",
        parser->valC);
    
    switch (parser->comparator) {
        case EQ: condition_result = (input_value == parser->valC); break;
        case NE: condition_result = (input_value != parser->valC); break;
        case LT: condition_result = (input_value < parser->valC); break;
        case GT: condition_result = (input_value > parser->valC); break;
        case LE: condition_result = (input_value <= parser->valC); break;
        case GE: condition_result = (input_value >= parser->valC); break;
        default: condition_result = false; break;
    }
    
    return condition_result;
}

int main(int argc, char *argv[])
{   
    FILE *fp;
    char ch;
    Parser parser;
    int input_value;

    // Initialize parser
    parser_init(&parser);

    // Enable debug mode if command line argument is provided
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        parser.debug_mode = true;
    }

    // Open input file
    fp = fopen("input.v", "r");
    if (fp == NULL)
    {
        printf("Error: Could not open input.v\n");
        return 1;
    }

    // Process file character by character
    while ((ch = fgetc(fp)) != EOF && !parser.parsing_done && !parser.error_flag)
    {
        process_char(&parser, ch);
    }

    fclose(fp);

    // Check for errors and print results
    if (parser.error_flag)
    {
        printf("Error code %d: ", parser.error_code);
        switch (parser.error_code)
        {
        case INVALID_KEYWORD:
            printf("Invalid keyword encountered\n");
            break;
        case VAR_MISMATCH:
            printf("Variable names don't match between if and else branches\n");
            break;
        case INVALID_CHAR:
            printf("Invalid character encountered\n");
            break;
        case MISSING_SEMICOLON:
            printf("Missing semicolon\n");
            break;
        case MISSING_OPERATOR:
            printf("Invalid or missing operator\n");
            break;
        case SYNTAX_ERROR:
            printf("Syntax error\n");
            break;
        default:
            printf("Unknown error\n");
            break;
        }
        return 1;
    }

    // If parsing was successful, prompt user for variable value and evaluate
    if (parser.parsing_done)
    {
        // Print basic information about the parsed structure
        printf("\nParsing successful!\n");
        printf("Condition: %s %s %d\n", 
            parser.cond_var,
            parser.comparator == EQ ? "==" :
            parser.comparator == NE ? "!=" :
            parser.comparator == LT ? "<" :
            parser.comparator == GT ? ">" :
            parser.comparator == LE ? "<=" :
            parser.comparator == GE ? ">=" : "??",
            parser.valC);
        printf("If true, %s = %d\n", parser.assignment_var, parser.const1);
        printf("If false, %s = %d\n", parser.assignment_var, parser.const2);
        
        // Get user input for condition variable
        printf("\nEnter value for '%s': ", parser.cond_var);
        scanf("%d", &input_value);
        
        // Evaluate the condition
        bool condition_result = evaluate_condition(&parser, input_value);
        
        // Assign the correct value based on the condition result
        if (condition_result) {
            parser.p = parser.const1;
            printf("\nCondition is TRUE\n");
            printf("Assigned %s = %d.\n", 
                parser.assignment_var, parser.p);
        } else {
            parser.p = parser.const2;
            printf("\nCondition is FALSE\n");
            printf("Assigned %s = %d.\n", 
                parser.assignment_var, parser.p);
        }
    }
    else {
        printf("\nParsing incomplete - could not evaluate\n");
    }

    return 0;
}
