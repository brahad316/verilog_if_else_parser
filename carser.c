#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Error codes matching the Verilog implementation
#define NO_ERROR            0
#define INVALID_KEYWORD     1
#define VAR_MISMATCH        2
#define INVALID_CHAR        3
#define MISSING_SEMICOLON   4
#define MISSING_OPERATOR    5
#define SYNTAX_ERROR        6

// Comparator types
#define EQ 0
#define NE 1
#define LE 4
#define GE 5
#define LT 2
#define GT 3

// State definitions
typedef enum {
    STATE_IDLE,
    STATE_READ_IF,
    STATE_READ_OPEN_PAREN,
    STATE_READ_VAR,
    STATE_READ_COND_OPERATOR,
    STATE_READ_COND_OPERATOR2,
    STATE_READ_VALC,
    STATE_READ_CLOSE_PAREN,
    STATE_READ_BEGIN,
    STATE_READ_ASSIGNMENT_VAR,
    STATE_READ_ASSIGNMENT_OPERATOR,
    STATE_READ_CONST1,
    STATE_READ_SEMICOLON1,
    STATE_READ_END1,
    STATE_READ_ELSE,
    STATE_READ_ELSE2,
    STATE_READ_BEGIN2,
    STATE_READ_BEGIN2B,
    STATE_READ_ASSIGNMENT_VAR2,
    STATE_READ_ASSIGNMENT_OPERATOR2,
    STATE_READ_CONST2,
    STATE_READ_SEMICOLON2,
    STATE_READ_END2,
    STATE_EVALUATE
} ParserState;

// Structure to hold parser data
typedef struct {
    ParserState state;
    int error_code;
    bool error_flag;
    int paren_count;
    
    // Variable name storage
    char cond_var[16];
    int cond_var_length;
    
    char assignment_var[16];
    int assignment_var_length;
    
    char assignment_var2[16];
    int assignment_var2_length;
    
    // Condition and constants
    int valC;
    bool is_valC_negative;
    int comparator;
    int const1;
    bool is_const1_negative;
    int const2;
    bool is_const2_negative;
    
    // Parsing flags
    bool reading_var;
    bool parsing_number;
    int num_buffer;
    char op_first;
    
    // Assignment type flags
    bool blocking_assignment1;
    bool blocking_assignment2;
    
    // Result
    int p;
    bool parsing_done;
} Parser;

// Initialize parser
void parser_init(Parser* parser) {
    parser->state = STATE_IDLE;
    parser->error_code = NO_ERROR;
    parser->error_flag = false;
    parser->paren_count = 0;
    
    memset(parser->cond_var, 0, sizeof(parser->cond_var));
    parser->cond_var_length = 0;
    
    memset(parser->assignment_var, 0, sizeof(parser->assignment_var));
    parser->assignment_var_length = 0;
    
    memset(parser->assignment_var2, 0, sizeof(parser->assignment_var2));
    parser->assignment_var2_length = 0;
    
    parser->valC = 0;
    parser->is_valC_negative = false;
    parser->comparator = 0;
    parser->const1 = 0;
    parser->is_const1_negative = false;
    parser->const2 = 0;
    parser->is_const2_negative = false;
    
    parser->reading_var = false;
    parser->parsing_number = false;
    parser->num_buffer = 0;
    parser->op_first = 0;
    
    parser->blocking_assignment1 = false;
    parser->blocking_assignment2 = false;
    
    parser->p = 0;
    parser->parsing_done = false;
}

// Helper functions for character validation
bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_underscore(char c) {
    return c == '_';
}

bool is_id_start(char c) {
    return is_letter(c);
}

bool is_id_char(char c) {
    return is_letter(c) || is_digit(c) || is_underscore(c);
}

// Check if variable names match
bool var_names_match(Parser* parser) {
    if (parser->assignment_var_length != parser->assignment_var2_length) {
        return false;
    }
    
    for (int i = 0; i < parser->assignment_var_length; i++) {
        if (parser->assignment_var[i] != parser->assignment_var2[i]) {
            printf("Mismatch at position %d: '%c' vs '%c'\n", 
                   i, parser->assignment_var[i], parser->assignment_var2[i]);
            return false;
        }
    }
    return true;
}

// Print parser state (for debugging)
void print_parser_state(Parser* parser, char curr_char) {
    printf("State: %2d, curr_char: %c (%02x), paren_count: %d\n", 
            parser->state, curr_char, curr_char, parser->paren_count);
    
    printf("cond_var: %.*s", parser->cond_var_length, parser->cond_var);
    for (int i = parser->cond_var_length; i < 16; i++)
        printf(" ");
        
    printf(", assignment_var: %.*s", parser->assignment_var_length, parser->assignment_var);
    for (int i = parser->assignment_var_length; i < 16; i++)
        printf(" ");
        
    printf(", assignment_var2: %.*s", parser->assignment_var2_length, parser->assignment_var2);
    for (int i = parser->assignment_var2_length; i < 16; i++)
        printf(" ");
    
    printf("\nvalC: %11d, const1: %11d, const2: %11d, error_code: %2d\n", 
           parser->valC, parser->const1, parser->const2, parser->error_code);
    printf("-----------------------------------------------------------------------------\n\n");
}

// Process a single character
void process_char(Parser* parser, char curr_char, int x_value) {
    // Debug output
    // print_parser_state(parser, curr_char);
    
    // If we've hit an error, don't process more characters
    if (parser->error_flag) {
        return;
    }
    
    // Process character based on current state
    switch (parser->state) {
        case STATE_IDLE:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_IDLE;
            } else if (curr_char == 'i') {
                parser->state = STATE_READ_IF;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_IF:
            if (curr_char == 'f') {
                parser->state = STATE_READ_OPEN_PAREN;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_OPEN_PAREN:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_OPEN_PAREN;
            } else if (curr_char == '(') {
                parser->paren_count++;
                parser->state = STATE_READ_VAR;
                parser->cond_var_length = 0;
                parser->reading_var = false;
            } else {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;
            
        case STATE_READ_VAR:
            if (is_whitespace(curr_char) && !parser->reading_var) {
                parser->state = STATE_READ_VAR;
            } else if (!parser->reading_var && is_id_start(curr_char)) {
                if (parser->cond_var_length < 15) {
                    parser->cond_var[parser->cond_var_length++] = curr_char;
                    parser->reading_var = true;
                }
            } else if (parser->reading_var && is_id_char(curr_char)) {
                if (parser->cond_var_length < 15) {
                    parser->cond_var[parser->cond_var_length++] = curr_char;
                }
                if (parser->cond_var_length >= 15) {
                    parser->state = STATE_READ_COND_OPERATOR;
                    parser->reading_var = false;
                }
            } else if (curr_char == ')' && parser->reading_var) {
                parser->paren_count--;
            } else if (parser->reading_var && (is_whitespace(curr_char) || curr_char == '>' || curr_char == '<' || 
                    curr_char == '=' || curr_char == '!')) {
                parser->state = STATE_READ_COND_OPERATOR;
                parser->reading_var = false;
                
                if (!is_whitespace(curr_char)) {
                    if (parser->paren_count == 0) {
                        parser->error_flag = true;
                        parser->error_code = SYNTAX_ERROR;
                    }
                    parser->op_first = curr_char;
                    parser->state = STATE_READ_COND_OPERATOR2;
                }
            } else if (curr_char == '(') {
                parser->paren_count++;
            } else {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;
            
        case STATE_READ_COND_OPERATOR:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_COND_OPERATOR;
            } else if (curr_char == ')') {
                if (parser->paren_count == 0) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                }
                parser->paren_count--;
            } else if (curr_char == '<' || curr_char == '>' || curr_char == '=' || curr_char == '!') {
                if (parser->paren_count == 0) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                }
                parser->op_first = curr_char;
                parser->state = STATE_READ_COND_OPERATOR2;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_CHAR;
            }
            break;
            
        case STATE_READ_COND_OPERATOR2:
            if (curr_char == '-') {
                parser->is_valC_negative = true;
            }
            
            switch (parser->op_first) {
                case '<': 
                    if (curr_char == '=') {
                        parser->comparator = LE;
                        parser->state = STATE_READ_VALC;
                    } else if (is_digit(curr_char)) {
                        parser->comparator = LT;
                        parser->num_buffer = (parser->num_buffer * 10) + (curr_char - '0');
                        parser->parsing_number = true;
                        parser->state = STATE_READ_VALC;
                    } else if (curr_char == '(') {
                        parser->comparator = LT;
                        parser->paren_count++;
                        parser->state = STATE_READ_VALC;
                    } else {
                        parser->error_flag = true;
                        parser->error_code = SYNTAX_ERROR;
                    }
                    break;
                    
                case '>': 
                    if (curr_char == '=') {
                        parser->comparator = GE;
                        parser->state = STATE_READ_VALC;
                    } else if (is_digit(curr_char)) {
                        parser->comparator = GT;
                        parser->num_buffer = (parser->num_buffer * 10) + (curr_char - '0');
                        parser->parsing_number = true;
                        parser->state = STATE_READ_VALC;
                    } else if (curr_char == '(') {
                        parser->comparator = GT;
                        parser->paren_count++;
                        parser->state = STATE_READ_VALC;
                    } else {
                        parser->error_flag = true;
                        parser->error_code = SYNTAX_ERROR;
                    }
                    break;
                    
                case '=': 
                    if (curr_char == '=') {
                        parser->comparator = EQ;
                        parser->state = STATE_READ_VALC;
                    } else {
                        parser->error_flag = true;
                        parser->error_code = MISSING_OPERATOR;
                    }
                    break;
                    
                case '!': 
                    if (curr_char == '=') {
                        parser->comparator = NE;
                        parser->state = STATE_READ_VALC;
                    } else {
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
            
        case STATE_READ_VALC:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_VALC;
            } else if (curr_char == '-' && !parser->parsing_number) {
                parser->is_valC_negative = true;
            } else if (curr_char == '(' && !parser->parsing_number) {
                parser->paren_count++;
            } else if (is_digit(curr_char)) {
                parser->num_buffer = (parser->num_buffer * 10) + (curr_char - '0');
                parser->parsing_number = true;
            } else if (parser->parsing_number) {
                if (parser->is_valC_negative)
                    parser->valC = -parser->num_buffer;
                else
                    parser->valC = parser->num_buffer;
                
                parser->num_buffer = 0;
                parser->parsing_number = false;
                
                if (curr_char == ')') {
                    if (parser->paren_count == 0) {
                        parser->error_flag = true;
                        parser->error_code = SYNTAX_ERROR;
                    }
                    parser->paren_count--;
                    parser->state = STATE_READ_CLOSE_PAREN;
                } else {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                }
            }
            break;
            
        case STATE_READ_CLOSE_PAREN:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_CLOSE_PAREN;
            } else if (curr_char == 'b') {
                if (parser->paren_count != 0) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                }
                parser->state = STATE_READ_BEGIN;
            } else if (curr_char == ')') {
                parser->paren_count--;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_BEGIN: {
            // Process "begin" keyword
            static const char* begin_keyword = "begin";
            static int begin_pos = 0;
            
            if (curr_char == begin_keyword[begin_pos]) {
                begin_pos++;
                if (begin_pos == 5) {  // "begin" has 5 characters
                    parser->state = STATE_READ_ASSIGNMENT_VAR;
                    begin_pos = 0;  // Reset for future use
                }
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
        }
            
        case STATE_READ_ASSIGNMENT_VAR:
            if (is_whitespace(curr_char) && !parser->reading_var) {
                parser->state = STATE_READ_ASSIGNMENT_VAR;
            } else if (!parser->reading_var && is_id_start(curr_char)) {
                if (parser->assignment_var_length < 15) {
                    parser->assignment_var[parser->assignment_var_length++] = curr_char;
                    parser->reading_var = true;
                }
            } else if (parser->reading_var && is_id_char(curr_char)) {
                if (parser->assignment_var_length < 15) {
                    parser->assignment_var[parser->assignment_var_length++] = curr_char;
                }
                if (parser->assignment_var_length >= 15) {
                    parser->state = STATE_READ_ASSIGNMENT_OPERATOR;
                    parser->reading_var = false;
                }
            } else if (curr_char == ')' && parser->reading_var) {
                parser->paren_count--;
            } else if (parser->reading_var && (is_whitespace(curr_char) || curr_char == '=' || curr_char == '<')) {
                parser->state = STATE_READ_ASSIGNMENT_OPERATOR;
                parser->reading_var = false;
                
                if (!is_whitespace(curr_char)) {
                    if (curr_char == '<') {
                        parser->blocking_assignment1 = false;
                        parser->op_first = '<';
                    } else if (curr_char == '=') {
                        parser->blocking_assignment1 = true;
                        parser->op_first = 0;
                        parser->num_buffer = 0;
                        parser->parsing_number = false;
                        parser->is_const1_negative = false;
                        parser->state = STATE_READ_CONST1;
                    }
                }
            } else if (curr_char == '(') {
                parser->paren_count++;
            } else {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;
            
        case STATE_READ_ASSIGNMENT_OPERATOR:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_ASSIGNMENT_OPERATOR;
            } else if (curr_char == ')') {
                parser->paren_count--;
            } else if (curr_char == '<') {
                parser->blocking_assignment1 = false;
                parser->state = STATE_READ_ASSIGNMENT_OPERATOR;
            } else if (curr_char == '=') {
                if (parser->op_first == '<') {
                    parser->blocking_assignment1 = false; // non-blocking (<=)
                } else {
                    parser->blocking_assignment1 = true; // blocking (=)
                }
                parser->op_first = 0;
                parser->num_buffer = 0;
                parser->parsing_number = false;
                parser->is_const1_negative = false;
                parser->state = STATE_READ_CONST1;
            } else {
                parser->op_first = curr_char;
            }
            break;
            
        case STATE_READ_CONST1:
            if (is_whitespace(curr_char)) {
                if (parser->is_const1_negative || parser->parsing_number) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                } else {
                    parser->state = STATE_READ_CONST1;
                }
            } else if (curr_char == '(' && !parser->parsing_number) {
                parser->paren_count++;
            } else if (curr_char == ')') {
                if (parser->num_buffer == 0) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                } else {
                    parser->paren_count--;
                }
            } else if (curr_char == '-' && !parser->parsing_number) {
                parser->is_const1_negative = true;
            } else if (is_digit(curr_char)) {
                parser->num_buffer = (parser->num_buffer * 10) + (curr_char - '0');
                parser->parsing_number = true;
            } else if (parser->parsing_number) {
                if (parser->is_const1_negative)
                    parser->const1 = -parser->num_buffer;
                else
                    parser->const1 = parser->num_buffer;
                
                parser->num_buffer = 0;
                parser->parsing_number = false;
                
                if (curr_char == ';') {
                    parser->state = STATE_READ_SEMICOLON1;
                }
            } else {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;
            
        case STATE_READ_SEMICOLON1:
            if (parser->paren_count != 0) {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_SEMICOLON1;
            } else if (curr_char == 'e') {
                parser->state = STATE_READ_END1;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_END1: {
            // Process "end" keyword
            static const char* end_keyword = "end";
            static int end_pos = 0;
            
            if (curr_char == end_keyword[end_pos]) {
                end_pos++;
                if (end_pos == 3) {  // "end" has 3 characters
                    parser->state = STATE_READ_ELSE;
                    end_pos = 0;  // Reset for future use
                }
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
        }
            
        case STATE_READ_ELSE:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_ELSE;
            } else if (curr_char == 'e') {
                parser->state = STATE_READ_ELSE2;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_ELSE2: {
            // Process "else" keyword
            static const char* else_keyword = "else";
            static int else_pos = 1;  // Start at 1 since 'e' was matched
            
            if (curr_char == else_keyword[else_pos]) {
                else_pos++;
                if (else_pos == 4) {  // "else" has 4 characters
                    parser->state = STATE_READ_BEGIN2;
                    else_pos = 1;  // Reset for future use
                }
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
        }
            
        case STATE_READ_BEGIN2:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_BEGIN2;
            } else if (curr_char == 'b') {
                parser->state = STATE_READ_BEGIN2B;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_BEGIN2B: {
            // Process "begin" keyword again
            static const char* begin2_keyword = "begin";
            static int begin2_pos = 1;  // Start at 1 since 'b' was matched
            
            if (curr_char == begin2_keyword[begin2_pos]) {
                begin2_pos++;
                if (begin2_pos == 5) {  // "begin" has 5 characters
                    parser->state = STATE_READ_ASSIGNMENT_VAR2;
                    begin2_pos = 1;  // Reset for future use
                }
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
        }
            
        case STATE_READ_ASSIGNMENT_VAR2:
            if (is_whitespace(curr_char) && !parser->reading_var) {
                parser->state = STATE_READ_ASSIGNMENT_VAR2;
            } else if (!parser->reading_var && is_id_start(curr_char)) {
                if (parser->assignment_var2_length < 15) {
                    parser->assignment_var2[parser->assignment_var2_length++] = curr_char;
                    parser->reading_var = true;
                }
            } else if (parser->reading_var && is_id_char(curr_char)) {
                if (parser->assignment_var2_length < 15) {
                    parser->assignment_var2[parser->assignment_var2_length++] = curr_char;
                }
                if (parser->assignment_var2_length >= 15) {
                    parser->state = STATE_READ_ASSIGNMENT_OPERATOR2;
                    parser->reading_var = false;
                }
            } else if (curr_char == ')' && parser->reading_var) {
                parser->paren_count--;
            } else if (parser->reading_var && (is_whitespace(curr_char) || curr_char == '=' || curr_char == '<')) {
                parser->state = STATE_READ_ASSIGNMENT_OPERATOR2;
                parser->reading_var = false;
                
                // Check if variables match
                if (!var_names_match(parser)) {
                    printf("ERROR: Variables mismatch between branches!\n");
                    parser->error_flag = true;
                    parser->error_code = VAR_MISMATCH;
                }
                
                if (!is_whitespace(curr_char)) {
                    if (curr_char == '<') {
                        parser->blocking_assignment2 = false;
                        parser->op_first = '<';
                    } else if (curr_char == '=') {
                        parser->blocking_assignment2 = true;
                        parser->op_first = 0;
                        parser->num_buffer = 0;
                        parser->parsing_number = false;
                        parser->is_const2_negative = false;
                        parser->state = STATE_READ_CONST2;
                    }
                }
            } else if (curr_char == '(') {
                parser->paren_count++;
            } else {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;
            
        case STATE_READ_ASSIGNMENT_OPERATOR2:
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_ASSIGNMENT_OPERATOR2;
            } else if (curr_char == ')') {
                parser->paren_count--;
            } else if (curr_char == '<') {
                parser->blocking_assignment2 = false;
                parser->state = STATE_READ_ASSIGNMENT_OPERATOR2;
            } else if (curr_char == '=') {
                if (parser->op_first == '<') {
                    parser->blocking_assignment2 = false; // non-blocking (<=)
                } else {
                    parser->blocking_assignment2 = true; // blocking (=)
                }
                parser->op_first = 0;
                parser->num_buffer = 0;
                parser->parsing_number = false;
                parser->is_const2_negative = false;
                parser->state = STATE_READ_CONST2;
            } else {
                parser->op_first = curr_char;
            }
            break;
            
        case STATE_READ_CONST2:
            if (is_whitespace(curr_char)) {
                if (parser->is_const2_negative || parser->parsing_number) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                } else {
                    parser->state = STATE_READ_CONST2;
                }
            } else if (curr_char == '(' && !parser->parsing_number) {
                parser->paren_count++;
            } else if (curr_char == ')') {
                if (parser->num_buffer == 0) {
                    parser->error_flag = true;
                    parser->error_code = SYNTAX_ERROR;
                } else {
                    parser->paren_count--;
                }
            } else if (curr_char == '-' && !parser->parsing_number) {
                parser->is_const2_negative = true;
            } else if (is_digit(curr_char)) {
                parser->num_buffer = (parser->num_buffer * 10) + (curr_char - '0');
                parser->parsing_number = true;
            } else if (parser->parsing_number) {
                if (parser->is_const2_negative)
                    parser->const2 = -parser->num_buffer;
                else
                    parser->const2 = parser->num_buffer;
                
                parser->num_buffer = 0;
                parser->parsing_number = false;
                
                if (curr_char == ';') {
                    parser->state = STATE_READ_SEMICOLON2;
                }
            } else {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            break;
            
        case STATE_READ_SEMICOLON2:
            if (parser->paren_count != 0) {
                parser->error_flag = true;
                parser->error_code = SYNTAX_ERROR;
            }
            if (is_whitespace(curr_char)) {
                parser->state = STATE_READ_SEMICOLON2;
            } else if (curr_char == 'e') {
                parser->state = STATE_READ_END2;
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
            
        case STATE_READ_END2: {
            // Process final "end" keyword
            static const char* end2_keyword = "end";
            static int end2_pos = 1;  // Start at 1 since 'e' was matched
            
            if (curr_char == end2_keyword[end2_pos]) {
                end2_pos++;
                if (end2_pos == 3) {  // "end" has 3 characters
                    parser->state = STATE_EVALUATE;
                    end2_pos = 1;  // Reset for future use
                }
            } else {
                parser->error_flag = true;
                parser->error_code = INVALID_KEYWORD;
            }
            break;
        }
            
        case STATE_EVALUATE:
            // Evaluate the condition if we haven't done so yet
            if (!parser->parsing_done && !parser->error_flag) {
                // Debug output
                printf("EVALUATING: x=%d, valC=%d, comparator=%d, const1=%d, const2=%d\n",
                    x_value, parser->valC, parser->comparator, parser->const1, parser->const2);
                
                // Evaluate the condition using the selected comparator
                switch (parser->comparator) {
                    case EQ: parser->p = (x_value == parser->valC) ? parser->const1 : parser->const2; break;
                    case NE: parser->p = (x_value != parser->valC) ? parser->const1 : parser->const2; break;
                    case LT: parser->p = (x_value <  parser->valC) ? parser->const1 : parser->const2; break;
                    case GT: parser->p = (x_value >  parser->valC) ? parser->const1 : parser->const2; break;
                    case LE: parser->p = (x_value <= parser->valC) ? parser->const1 : parser->const2; break;
                    case GE: parser->p = (x_value >= parser->valC) ? parser->const1 : parser->const2; break;
                }
                
                parser->parsing_done = true;
            }
            break;
    }
}

// Get error description
const char* get_error_description(int error_code) {
    switch (error_code) {
        case NO_ERROR:          return "No Error";
        case INVALID_KEYWORD:   return "Invalid Keyword";
        case VAR_MISMATCH:      return "Variable Mismatch";
        case INVALID_CHAR:      return "Invalid Character";
        case MISSING_SEMICOLON: return "Missing Semicolon";
        case MISSING_OPERATOR:  return "Missing Operator";
        case SYNTAX_ERROR:      return "Syntax Error";
        default:                return "Unknown Error";
    }
}

int main() {
    int x_value;
    Parser parser;
    
    // Read file content
    FILE *file = fopen("input.v", "r");
    if (!file) {
        printf("Error opening file\n");
        return 1;
    }
    
    // Get file size and allocate buffer
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        printf("Memory allocation failed\n");
        fclose(file);
        return 1;
    }
    
    // Read file contents
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);
    
    // Get user input for x
    printf("Enter the value of x: ");
    scanf("%d", &x_value);
    
    // Initialize parser
    parser_init(&parser);
    
    // Process each character in the file
    for (int i = 0; i < file_size; i++) {
        process_char(&parser, buffer[i], x_value);
    }
    
    // Check final state and print results
    if (parser.parsing_done && !parser.error_flag) {
        printf("\nParsing successful!\n");
        printf("Condition variable: %.*s\n", parser.cond_var_length, parser.cond_var);
        printf("Assignment variable: %.*s\n", parser.assignment_var_length, parser.assignment_var);
        printf("Comparison: %.*s %s %d\n", parser.cond_var_length, parser.cond_var, 
               parser.comparator == EQ ? "==" :
               parser.comparator == NE ? "!=" :
               parser.comparator == LT ? "<" :
               parser.comparator == GT ? ">" :
               parser.comparator == LE ? "<=" :
               parser.comparator == GE ? ">=" : "??",
               parser.valC);
        printf("Given x = %d, %.*s <= %d\n", x_value, 
               parser.assignment_var_length, parser.assignment_var, parser.p);
    } else if (parser.error_flag) {
        printf("\nError: %s (%d)\n", get_error_description(parser.error_code), parser.error_code);
    } else {
        printf("\nParsing incomplete\n");
    }
    
    // Clean up
    free(buffer);
    
    return 0;
}
