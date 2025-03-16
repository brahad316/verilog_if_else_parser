#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function to remove all whitespace characters
void remove_whitespace(char *str) {
    char *dst = str;
    for (char *src = str; *src; src++) {
        if (!isspace(*src)) {
            *dst++ = *src;
        }
    }
    *dst = '\0';  // Null-terminate the compacted string
}

// Function to extract the integer value for assignment "p<="
int extract_assignment_value(char *start) {
    while (*start && (*start == '=' || isspace(*start))) start++; // Skip '=' and spaces
    return strtol(start, NULL, 10); // strtol() converts str to long int, handles negative numbers
}

int main() {
    int x;

    FILE *file = fopen("input.v", "r");
    if (!file) {
        printf("Error opening file\n");
        return 1;
    }

    char buffer[4096] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);

    remove_whitespace(buffer); // remove all whitespace characters

    char comparison_op[3] = "";
    int condition_num = -1, true_branch_num = -1, false_branch_num = -1;

    // Extract condition inside "if(...)"
    char *if_start = strstr(buffer, "if(");
    if (if_start) {
        char *if_end = strstr(if_start, ")");
        if (if_end) {
            *if_end = '\0'; // Temporarily null-terminate
            char *condition_part = if_start + 3; // Skip "if("

            // Extract comparison operator
            char *op_pos = NULL;
            if ((op_pos = strstr(condition_part, "<="))) strcpy(comparison_op, "<=");
            else if ((op_pos = strstr(condition_part, ">="))) strcpy(comparison_op, ">=");
            else if ((op_pos = strstr(condition_part, "=="))) strcpy(comparison_op, "==");
            else if ((op_pos = strstr(condition_part, "!="))) strcpy(comparison_op, "!=");
            else if ((op_pos = strstr(condition_part, "<"))) strcpy(comparison_op, "<");
            else if ((op_pos = strstr(condition_part, ">"))) strcpy(comparison_op, ">");

            // Extract condition number (valC)
            if (op_pos) {
                char *num_start = op_pos + strlen(comparison_op);
                condition_num = strtol(num_start, NULL, 10);
            }
            *if_end = ')'; // restore buffer
        }
    }

    // Extract true branch assignment (first "p<=" before "else")
    char *else_pos = strstr(buffer, "else"); // strstr() returns pointer to first occurrence of string match b/w args
    char *p_pos = strstr(buffer, "p<=");
    if (p_pos && (!else_pos || p_pos < else_pos)) {
        true_branch_num = extract_assignment_value(p_pos + 2);
    }

    // Extract false branch assignment (first "p<=" after "else")
    if (else_pos) {
        p_pos = strstr(else_pos, "p<=");
        if (p_pos) {
            false_branch_num = extract_assignment_value(p_pos + 2);
        }
    }

    // Get user input for x
    printf("Enter the value of x:\n");
    scanf("%d", &x);
    printf("x = %d;\n", x);

    printf("comparison_op = %s\n", comparison_op);
    printf("condition_num = %d\n", condition_num);
    printf("true_branch_num = %d\n", true_branch_num);
    printf("false_branch_num = %d\n", false_branch_num);

    // Evaluate condition and print the result
    int result = (strcmp(comparison_op, "<=") == 0) ? (x <= condition_num) ? true_branch_num : false_branch_num
               : (strcmp(comparison_op, ">=") == 0) ? (x >= condition_num) ? true_branch_num : false_branch_num
               : (strcmp(comparison_op, "==") == 0) ? (x == condition_num) ? true_branch_num : false_branch_num
               : (strcmp(comparison_op, "!=") == 0) ? (x != condition_num) ? true_branch_num : false_branch_num
               : (strcmp(comparison_op, "<") == 0) ? (x < condition_num) ? true_branch_num : false_branch_num
               : (strcmp(comparison_op, ">") == 0) ? (x > condition_num) ? true_branch_num : false_branch_num
               : 0;

    printf("p <= %d\n", result);

    return 0;
}
