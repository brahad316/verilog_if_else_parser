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
    size_t bytes_read = 0;
    bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    // Normalize whitespace for easier parsing
    for (int i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\r') {
            buffer[i] = ' ';
        }
    }
    
    char comparison_op[3] = "";
    long long condition_num = -1;
    int true_branch_num = -1;
    int false_branch_num = -1;
    
    // Find if statement
    char *if_pos = strstr(buffer, "if");
    if (if_pos) {
        // Find comparison operator
        char *x_pos = strstr(if_pos, "x");
        if (x_pos) {
            char *op_pos = NULL;
            if ((op_pos = strstr(x_pos, "<=")) != NULL) strcpy(comparison_op, "<=");
            else if ((op_pos = strstr(x_pos, ">=")) != NULL) strcpy(comparison_op, ">=");
            else if ((op_pos = strstr(x_pos, "==")) != NULL) strcpy(comparison_op, "==");
            else if ((op_pos = strstr(x_pos, "!=")) != NULL) strcpy(comparison_op, "!=");
            else if ((op_pos = strstr(x_pos, "<")) != NULL) strcpy(comparison_op, "<");
            else if ((op_pos = strstr(x_pos, ">")) != NULL) strcpy(comparison_op, ">");
            
            // Extract condition number
            if (op_pos) {
                char *num_start = op_pos + strlen(comparison_op);
                while (*num_start && (isspace(*num_start) || !isdigit(*num_start))) num_start++;
                if (isdigit(*num_start)) {
                    condition_num = strtoll(num_start, NULL, 10);
                }
            }
        }
        
        // Find true branch value (before else)
        char *begin_pos = strstr(if_pos, "begin");
        char *else_pos = strstr(if_pos, "else");
        if (begin_pos && else_pos && begin_pos < else_pos) {
            char *p_pos = strstr(begin_pos, "p <=");
            if (p_pos && p_pos < else_pos) {
                p_pos += 4;
                while (*p_pos && (isspace(*p_pos) || !isdigit(*p_pos))) p_pos++;
                if (isdigit(*p_pos)) {
                    true_branch_num = atoi(p_pos);
                }
            }
        }
        
        // Find false branch value (after else)
        if (else_pos) {
            char *p_pos = strstr(else_pos, "p <=");
            if (p_pos) {
                p_pos += 4;
                while (*p_pos && (isspace(*p_pos) || !isdigit(*p_pos))) p_pos++;
                if (isdigit(*p_pos)) {
                    false_branch_num = atoi(p_pos);
                }
            }
        }
    }
    
        printf("Enter the value of x:\n");
        scanf("%d", &x);
        printf("x = %d;\n", x);

        printf("comparison_op = %s\n", comparison_op);
        printf("condition_num = %lld\n", condition_num);
        printf("true_branch_num = %d\n", true_branch_num);
        printf("false_branch_num = %d\n", false_branch_num);

        if(strcmp(comparison_op, "<=") == 0) {
            if(x <= condition_num) {
                printf("p <= %d\n", true_branch_num);
            } else {
                printf("p <= %d\n", false_branch_num);
            }
        } else if(strcmp(comparison_op, ">=") == 0) {
            if(x >= condition_num) {
                printf("p <= %d\n", true_branch_num);
            } else {
                printf("p <= %d\n", false_branch_num);
            }
        } else if(strcmp(comparison_op, "==") == 0) {
            if(x == condition_num) {
                printf("p <= %d\n", true_branch_num);
            } else {
                printf("p <= %d\n", false_branch_num);
            }
        } else if(strcmp(comparison_op, "!=") == 0) {
            if(x != condition_num) {
                printf("p <= %d\n", true_branch_num);
            } else {
                printf("p <= %d\n", false_branch_num);
            }
        } else if(strcmp(comparison_op, "<") == 0) {
            if(x < condition_num) {
                printf("p <= %d\n", true_branch_num);
            } else {
                printf("p <= %d\n", false_branch_num);
            }
        } else if(strcmp(comparison_op, ">") == 0) {
            if(x > condition_num) {
                printf("p <= %d\n", true_branch_num);
            } else {
                printf("p <= %d\n", false_branch_num);
            }
        }
    return 0;
}