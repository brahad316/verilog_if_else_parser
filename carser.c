#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int main() {
    int x;

    FILE *file = fopen("input.v", "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    char buffer[4096] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);

    // Normalize whitespace for easier parsing
    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\r') {
            buffer[i] = ' ';
        }
    }

    char comparison_op[3] = "";
    int condition_num = -1;
    int true_branch_num = -1;
    int false_branch_num = -1;

    // Find 'if(' and extract only the condition inside parentheses
    char *if_start = strstr(buffer, "if(");
    if (if_start) {
        char *if_end = strstr(if_start, ")");
        if (if_end) {
            // Temporarily null-terminate to isolate the condition inside the parentheses
            *if_end = '\0';
            char *condition_part = if_start + 3; // Skip "if("

            // Extract comparison operator
            char *op_pos = NULL;
            if ((op_pos = strstr(condition_part, "<=")) != NULL) strcpy(comparison_op, "<=");
            else if ((op_pos = strstr(condition_part, ">=")) != NULL) strcpy(comparison_op, ">=");
            else if ((op_pos = strstr(condition_part, "==")) != NULL) strcpy(comparison_op, "==");
            else if ((op_pos = strstr(condition_part, "!=")) != NULL) strcpy(comparison_op, "!=");
            else if ((op_pos = strstr(condition_part, "<")) != NULL) strcpy(comparison_op, "<");
            else if ((op_pos = strstr(condition_part, ">")) != NULL) strcpy(comparison_op, ">");

            // Extract condition number from 'if(x <op> num)'
            if (op_pos) {
                char *num_start = op_pos + strlen(comparison_op);
                while (*num_start && isspace(*num_start)) num_start++; // Skip spaces
                if (isdigit(*num_start) || *num_start == '-') { // Handle negative numbers too
                    condition_num = strtoll(num_start, NULL, 10);
                }
            }

            // Restore the full buffer
            *if_end = ')';
        }
    }

    // Find true branch value (before else)
    char *begin_pos = strstr(buffer, "begin");
    char *else_pos = strstr(buffer, "else");
    if (begin_pos && else_pos && begin_pos < else_pos) {
        char *p_pos = strstr(begin_pos, "p <=");
        if (p_pos && p_pos < else_pos) {
            p_pos += 4; // Move past "p <="
            while (*p_pos && isspace(*p_pos)) p_pos++; // Skip spaces
            if (isdigit(*p_pos)) {
                true_branch_num = atoi(p_pos);
            }
        }
    }

    // Find false branch value (after else)
    if (else_pos) {
        char *p_pos = strstr(else_pos, "p <=");
        if (p_pos) {
            p_pos += 4; // Move past "p <="
            while (*p_pos && isspace(*p_pos)) p_pos++; // Skip spaces
            if (isdigit(*p_pos)) {
                false_branch_num = atoi(p_pos);
            }
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
    if (strcmp(comparison_op, "<=") == 0) {
        printf("p <= %d\n", (x <= condition_num) ? true_branch_num : false_branch_num);
    } else if (strcmp(comparison_op, ">=") == 0) {
        printf("p <= %d\n", (x >= condition_num) ? true_branch_num : false_branch_num);
    } else if (strcmp(comparison_op, "==") == 0) {
        printf("p <= %d\n", (x == condition_num) ? true_branch_num : false_branch_num);
    } else if (strcmp(comparison_op, "!=") == 0) {
        printf("p <= %d\n", (x != condition_num) ? true_branch_num : false_branch_num);
    } else if (strcmp(comparison_op, "<") == 0) {
        printf("p <= %d\n", (x < condition_num) ? true_branch_num : false_branch_num);
    } else if (strcmp(comparison_op, ">") == 0) {
        printf("p <= %d\n", (x > condition_num) ? true_branch_num : false_branch_num);
    }

    return 0;
}
