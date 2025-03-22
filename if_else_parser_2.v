module if_else_parser_2 (
    input  wire        clk,                   
    input  wire        rst,                   
    input  wire signed [31:0] x,              
    input  wire [6:0]  ascii_char,      
    input  wire        char_valid,            
    output reg signed  [31:0] p,
    output reg         [16*7-1:0] assignment_var,  // array to support multi-char variables
    output reg         [3:0] assignment_var_length, // Length of variable name  
    output reg         parsing_done,          
    output reg         error_flag,
    output reg [3:0]   error_code             
);  

    // Error codes
    parameter NO_ERROR            = 4'd0,
              INVALID_KEYWORD     = 4'd1,
              VAR_MISMATCH        = 4'd2, 
              INVALID_CHAR        = 4'd3,
              MISSING_SEMICOLON   = 4'd4,
              MISSING_OPERATOR    = 4'd5,
              SYNTAX_ERROR        = 4'd6;

    // State encoding
    parameter IDLE                      = 0,
              READ_IF                   = 1,  // Read "if" keyword
              READ_OPEN_PAREN           = 2,
              READ_VAR                  = 3,
              READ_COND_OPERATOR        = 4,  // Read first char of comparator
              READ_COND_OPERATOR2       = 5,  // Read second char of comparator (if any)
              READ_VALC                 = 6,
              READ_CLOSE_PAREN          = 7,
              READ_BEGIN                = 8,  // Read "begin" keyword
              READ_ASSIGNMENT_VAR       = 9,  // Read variable for assignment
              READ_ASSIGNMENT_OPERATOR  = 10, // Read assignment operator (<= or =)
              READ_CONST1               = 11, // Read constant for if branch
              READ_SEMICOLON1           = 12, // Expect semicolon after const1
              READ_END1                 = 13, // Read "end" keyword for if branch
              READ_ELSE                 = 14, // Expect "else" keyword
              READ_BEGIN2               = 16, // Read "begin" for else branch
              READ_ASSIGNMENT_VAR2      = 18, // Read variable for else branch
              READ_ASSIGNMENT_OPERATOR2 = 19, // Read assignment operator for else branch
              READ_CONST2               = 20, // Read constant for else branch
              READ_SEMICOLON2           = 21, // Expect semicolon after const2
              READ_END2                 = 22, // Read "end" keyword for else branch
              EVALUATE                  = 23;

    reg [4:0] state;

    // Keyword parsing support
    reg [31:0] keyword_buffer;
    reg [2:0]  keyword_index;
    reg        keyword_complete;

    // Variable name tracking - multi-character support
    reg [6:0]  cond_var[0:15];      // Condition variable (was 'x'), now supports up to 16 chars
    reg [3:0]  cond_var_length;     // Length of condition variable name
    reg [3:0]  cond_var_idx;        // Current index while reading variable
    
    // Internal storage for assignment variables (preserved from original)
    reg [6:0]  assignment_var_array[0:15]; // For internal processing
    reg [16*7-1:0] assignment_var2; // Assignment variable in else branch as packed array
    reg [3:0]  assignment_var2_length;
    reg [3:0]  assignment_var2_idx;
    reg [6:0]  assignment_var2_array[0:15]; // For internal processing
    reg        var_match;           // Flag to check if variables match
    reg        reading_var;         // Flag to indicate we're accumulating a variable name

    // Data registers
    integer    valC, const1, const2;
    integer    num_buffer;
    reg        parsing_number;

    integer paren_count = 0;

    // Assignment type flags
    reg        blocking_assignment1;  // = operator used in if branch
    reg        blocking_assignment2;  // = operator used in else branch

    // Operator detection for condition:
    // Supported comparators:
    //  "==" => EQ, "!=" => NE, "<=" => LE, ">=" => GE, "<"  => LT, ">"  => GT.
    reg [2:0] comparator;
    parameter EQ = 3'b000, NE = 3'b001, LE = 3'b100, GE = 3'b101, LT = 3'b010, GT = 3'b011;

    // Temporary register to store first operator character
    reg [6:0] op_first;

    // ASCII digit and letter checks
    wire is_digit = (ascii_char >= "0" && ascii_char <= "9");
    wire is_letter = ((ascii_char >= "a" && ascii_char <= "z") || 
                     (ascii_char >= "A" && ascii_char <= "Z"));
    wire is_underscore = (ascii_char == "_");                     
    wire is_id_start = is_letter;  // First character must be a letter
    wire is_id_char = is_letter || is_digit || is_underscore;  // Subsequent chars can be letter, digit, or underscore
    wire is_whitespace = (ascii_char == " " || ascii_char == "\t" || ascii_char == "\n");

    // Flags for negative integers valC, const1, const2
    reg is_valC_negative, is_const1_negative, is_const2_negative;

    wire new_char = char_valid;
    
    // Function to check if variable names match
    function var_names_match;
        input integer max_idx;
        integer i;
        begin
            if (assignment_var_length != assignment_var2_length) begin
                var_names_match = 0;  // Different lengths
            end else begin
                var_names_match = 1;  // Assume match, then check each char
                for (i = 0; i < assignment_var_length; i = i + 1) begin
                    if (assignment_var_array[i] != assignment_var2_array[i]) begin
                        var_names_match = 0;  // Mismatch found
                        $display("Mismatch at position %0d: '%c' vs '%c'", 
                                i, assignment_var_array[i], assignment_var2_array[i]);
                    end
                end
            end
        end
    endfunction

    // Debug function modified to handle arrays
    // task display_debug;
    //     integer i;
    //     reg [127:0] cond_var_str, assign_var_str, assign_var2_str;  // For display only
    //     begin
    //         cond_var_str = "";
    //         assign_var_str = "";
    //         assign_var2_str = "";
            
    //         for (i = 0; i < cond_var_length; i = i + 1)
    //             cond_var_str = {cond_var_str, cond_var[i]};
            
    //         for (i = 0; i < assignment_var_length; i = i + 1)
    //             assign_var_str = {assign_var_str, assignment_var[i]};
                
    //         for (i = 0; i < assignment_var2_length; i = i + 1)
    //             assign_var2_str = {assign_var2_str, assignment_var2[i]};
                
    //         $display("State: %d, curr_char: %c (%h), keyword_buffer: %h, keyword_index: %d", 
    //                  state, ascii_char, ascii_char, keyword_buffer, keyword_index);
    //         $display("cond_var: %s, assignment_var: %s, assignment_var2: %s, paren_count: %1d", 
    //                  cond_var_str, assign_var_str, assign_var2_str, paren_count);
    //         $display("valC: %d, const1: %d, const2: %d, error_code: %d", valC, const1, const2, error_code);
    //         $display("---------------------------------------------------------------------------------------------------------------------------------------------\n");
    //     end
    // endtask

    // In the "always" block for debug output:
    always @(posedge clk) begin
        $write("State: %2d, curr_char: %c (%h), keyword_buffer: %h, keyword_index: %0d, parsing_number: %1d\n", 
            state, ascii_char, ascii_char, keyword_buffer, keyword_index, parsing_number);

        // Print condition variable
        $write("cond_var: ");
        for (integer i = 0; i < cond_var_length; i = i + 1)
            $write("%c", cond_var[i]);
        for (integer i = cond_var_length; i < 16; i = i + 1)
            $write(" ");
            
        // Print assignment variables
        $write(", assignment_var: ");
        for (integer i = 0; i < assignment_var_length; i = i + 1)
            $write("%c", assignment_var_array[i]);
        for (integer i = assignment_var_length; i < 16; i = i + 1)
            $write(" ");
            
        $write(", assignment_var2: ");
        for (integer i = 0; i < assignment_var2_length; i = i + 1)
            $write("%c", assignment_var2_array[i]);
        for (integer i = assignment_var2_length; i < 16; i = i + 1)
            $write(" ");
            
        $write(", paren_count: %0d\n", paren_count);
        $write("valC: %11d, const1: %11d, const2: %11d, error_code: %2d\n", 
            valC, const1, const2, error_code);
        $write("---------------------------------------------------------------------------------------------------------------------------------------------\n\n");
    end

    always @(state) begin
        if (state == EVALUATE) begin
            // Pack assignment_var_array into assignment_var
            assignment_var = 0;
            for (integer i = 0; i < assignment_var_length; i = i + 1) begin
                assignment_var = assignment_var | (assignment_var_array[i] << (i*7));
            end
            
            // Pack assignment_var2_array into assignment_var2
            assignment_var2 = 0;
            for (integer i = 0; i < assignment_var2_length; i = i + 1) begin
                assignment_var2 = assignment_var2 | (assignment_var2_array[i] << (i*7));
            end
        end
    end

    // FSM
    always @(posedge clk or posedge rst) begin
        if(rst) begin
            state               <= IDLE;
            keyword_buffer      <= 0;
            keyword_index       <= 0;
            keyword_complete    <= 0;
            cond_var_length     <= 0;
            cond_var_idx        <= 0;
            assignment_var_length  <= 0;
            assignment_var2_length <= 0;
            assignment_var2_idx <= 0;
            var_match           <= 0;
            reading_var         <= 0;
            valC                <= 0;
            is_valC_negative    <= 0;
            const1              <= 0;
            is_const1_negative  <= 0;
            const2              <= 0;
            is_const2_negative  <= 0;
            num_buffer          <= 0;
            parsing_number      <= 0;
            parsing_done        <= 0;
            error_flag          <= 0;
            error_code          <= NO_ERROR;
            comparator          <= 0;
            op_first            <= 0;
            blocking_assignment1 <= 0;
            blocking_assignment2 <= 0;
            p                    <= 0;
            assignment_var       <= 0;
            assignment_var2      <= 0;
            // Clear variable arrays
            for (integer i = 0; i < 16; i = i + 1) begin
                cond_var[i] <= 0;
                assignment_var_array[i] <= 0;
                assignment_var2_array[i] <= 0;
            end
        end
        else begin
            // if(new_char) begin
            //     display_debug;
            // end

            case(state)
                IDLE: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= IDLE;
                        end
                        else if(ascii_char == "i") begin
                            keyword_index <= 1;
                            state <= READ_IF;
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_IF: begin
                    if(new_char) begin
                        if(ascii_char == "f") begin
                            keyword_index <= 0;
                            state <= READ_OPEN_PAREN;
                        end else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_OPEN_PAREN: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_OPEN_PAREN;
                        end
                        else if(ascii_char == "(") begin
                            paren_count <= paren_count + 1;
                            state <= READ_VAR;
                            cond_var_idx <= 0;
                            cond_var_length <= 0;
                            reading_var <= 0;
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_VAR: begin
                    if(new_char) begin
                        if(is_whitespace && !reading_var) begin
                            state <= READ_VAR;
                        end
                        else if(!reading_var && is_id_start) begin
                            // First character of identifier - must be a letter
                            cond_var[0] <= ascii_char; // Start at index 0
                            cond_var_idx <= 1;
                            cond_var_length <= 1;
                            reading_var <= 1;
                        end
                        else if(reading_var && is_id_char) begin
                            // Subsequent characters - can be letter, digit, or underscore
                            cond_var[cond_var_idx] <= ascii_char;
                            cond_var_idx <= cond_var_idx + 1;
                            cond_var_length <= cond_var_length + 1;
                            if(cond_var_idx == 15) begin  // Max length reached
                                state <= READ_COND_OPERATOR;
                                reading_var <= 0;
                            end
                        end
                        else if(ascii_char == ")" && reading_var) begin
                            // Special case: closing parenthesis without operator - error
                            paren_count <= paren_count - 1;
                        end
                        else if(reading_var && (is_whitespace || ascii_char == ">" || ascii_char == "<" || 
                                ascii_char == "=" || ascii_char == "!")) begin
                            // Variable name complete, ready for operator
                            state <= READ_COND_OPERATOR;
                            reading_var <= 0; 
                            
                            // Process operator right away if not whitespace
                            if(!is_whitespace) begin
                                if(paren_count == 0) begin
                                    error_flag <= 1;
                                    error_code <= SYNTAX_ERROR;
                                end
                                op_first <= ascii_char;
                                state <= READ_COND_OPERATOR2;
                            end
                        end
                        // else if(ascii_char == ")") begin
                        //     // Special case: closing parenthesis without operator - error
                        //     error_flag <= 1;
                        //     error_code <= MISSING_OPERATOR;
                        // end 
                        else if(ascii_char == "(") begin
                            // Opening nested parenthesis
                            paren_count <= paren_count + 1;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                // Read the first character of the condition operator.
                READ_COND_OPERATOR: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_COND_OPERATOR;
                        end
                        else if(ascii_char == ")") begin
                            if(paren_count == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            paren_count <= paren_count - 1;
                        end
                        else if(ascii_char == "<" || ascii_char == ">" || ascii_char == "=" || ascii_char == "!") begin
                            if(paren_count == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            op_first <= ascii_char;
                            state <= READ_COND_OPERATOR2;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_CHAR;
                        end
                    end
                end

                // Read the second character of the condition operator, if applicable.
                READ_COND_OPERATOR2: begin
                    if(new_char) begin
                        // check if there's a negative sign ("-") before the digit
                        if(ascii_char == "-") begin
                            is_valC_negative <= 1;
                            // state <= READ_VALC; // ???
                        end
                        case(op_first)
                            "<": begin
                                if(ascii_char=="=") begin
                                    comparator <= LE;
                                    state <= READ_VALC;
                                end
                                else if(is_digit) begin
                                    comparator <= LT; // single-character "<"
                                    // Start processing the digit immediately
                                    num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                                    parsing_number <= 1;
                                    state <= READ_VALC;
                                end 
                                else if(ascii_char == "(") begin
                                    paren_count <= paren_count + 1;
                                    state <= READ_VALC;
                                end
                                else begin
                                    error_flag <= 1;
                                    error_code <= SYNTAX_ERROR;
                                end
                            end
                            ">": begin
                                if(ascii_char=="=") begin
                                    comparator <= GE;
                                    state <= READ_VALC;
                                end
                                else if(is_digit) begin
                                    comparator <= GT; // single-character ">"
                                    // Start processing the digit immediately
                                    num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                                    parsing_number <= 1; 
                                    state <= READ_VALC;
                                end
                                else if(ascii_char == "(") begin
                                    paren_count <= paren_count + 1;
                                    state <= READ_VALC;
                                end
                                else begin
                                    error_flag <= 1;
                                    error_code <= SYNTAX_ERROR;
                                end
                            end
                            "=": begin
                                if(ascii_char=="=") begin
                                    comparator <= EQ;
                                    state <= READ_VALC;
                                end
                                else begin
                                    error_flag <= 1;
                                    error_code <= MISSING_OPERATOR;
                                end
                            end
                            "!": begin
                                if(ascii_char=="=") begin
                                    comparator <= NE;
                                    state <= READ_VALC;
                                end
                                else begin
                                    error_flag <= 1;
                                    error_code <= MISSING_OPERATOR;
                                end
                            end
                            default: begin
                                    error_flag <= 1;
                                    error_code <= SYNTAX_ERROR;
                            end
                        endcase
                        // $display("op_first: %c, ascii_char: %c, comparator: %b", op_first, ascii_char, comparator);
                    end
                end

                // Accumulate condition value (valC)
                READ_VALC: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_VALC;
                        end
                        // check if there's a negative sign ("-") before the digit   
                        else if(ascii_char == "-" && !parsing_number) begin
                            is_valC_negative <= 1;
                        end 
                        else if(ascii_char == "(" && !parsing_number) begin
                            paren_count <= paren_count + 1;
                        end
                        else if(is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end 
                        else if(parsing_number) begin
                            if(is_valC_negative)
                                valC <= -num_buffer;
                            else
                                valC <= num_buffer;
                            
                            num_buffer <= 0;
                            parsing_number <= 0;
                            
                            if(ascii_char == ")") begin
                                if(paren_count == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                                end
                                paren_count <= paren_count - 1;
                                state <= READ_CLOSE_PAREN;
                            end 
                            else begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        end 
                    end
                end

                READ_CLOSE_PAREN: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_CLOSE_PAREN;
                        end
                        else if(ascii_char == "b") begin
                            keyword_index <= 1;
                            keyword_buffer <= "b";
                            if(paren_count != 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            state <= READ_BEGIN;
                        end
                        else if(ascii_char == ")") begin
                            paren_count <= paren_count - 1;
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_BEGIN: begin
                    if(new_char) begin
                        case(keyword_index)
                            1: if(ascii_char == "e") begin
                                keyword_buffer <= (keyword_buffer << 8) | "e";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            2: if(ascii_char == "g") begin
                                keyword_buffer <= (keyword_buffer << 8) | "g";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end

                            3: if(ascii_char == "i") begin
                                keyword_buffer <= (keyword_buffer << 8) | "i";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            4: if(ascii_char == "n") begin
                                keyword_index <= 0;
                                state <= READ_ASSIGNMENT_VAR;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            default: begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        endcase
                    end
                end

                READ_ASSIGNMENT_VAR: begin
                    if(new_char) begin
                        if(is_whitespace && !reading_var) begin
                            state <= READ_ASSIGNMENT_VAR;
                        end
                        else if(!reading_var && is_id_start) begin
                            // First character of identifier - must be a letter
                            assignment_var_array[0] <= ascii_char; // Start at index 0
                            assignment_var_length <= 1;
                            cond_var_idx <= 1; // Reuse this counter for tracking position
                            reading_var <= 1;
                        end
                        else if(reading_var && is_id_char) begin
                            // Subsequent characters - can be letter, digit, or underscore
                            assignment_var_array[cond_var_idx] <= ascii_char;
                            cond_var_idx <= cond_var_idx + 1;
                            assignment_var_length <= assignment_var_length + 1;
                            if(cond_var_idx == 15) begin  // Max length reached
                                state <= READ_ASSIGNMENT_OPERATOR;
                                reading_var <= 0;
                            end
                        end
                        else if(ascii_char == ")" && reading_var) begin
                            // Special case: closing parenthesis without operator - error
                            paren_count <= paren_count - 1;
                        end
                        else if(reading_var && (is_whitespace || ascii_char == "=" || ascii_char == "<")) begin
                            // Variable name complete, ready for operator
                            state <= READ_ASSIGNMENT_OPERATOR;
                            reading_var <= 0;
                            
                            // Process operator right away if not whitespace
                            if(!is_whitespace) begin
                                if(ascii_char == "<") begin
                                    blocking_assignment1 <= 0;
                                    op_first <= "<";
                                end 
                                else if(ascii_char == "=") begin
                                    blocking_assignment1 <= 1;
                                    op_first <= 0;
                                    num_buffer <= 0;
                                    parsing_number <= 0;
                                    is_const1_negative <= 0;
                                    state <= READ_CONST1;
                                end
                            end
                        end
                        else if(ascii_char == "(") begin
                            // Opening nested parenthesis
                            paren_count <= paren_count + 1;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_ASSIGNMENT_OPERATOR: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_ASSIGNMENT_OPERATOR;
                        end
                        else if(ascii_char == ")") paren_count <= paren_count - 1;
                        else if(ascii_char == "<") begin
                            blocking_assignment1 <= 0;
                            state <= READ_ASSIGNMENT_OPERATOR;
                        end 
                        else if(ascii_char == "=") begin
                            if(op_first == "<") begin
                                blocking_assignment1 <= 0; // non-blocking (<=)
                            end else begin
                                blocking_assignment1 <= 1; // blocking (=)
                            end
                            op_first <= 0;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            is_const1_negative <= 0;
                            state <= READ_CONST1;
                        end
                        else begin
                            op_first <= ascii_char;
                        end
                    end
                end

                // Accumulate constant for if branch (const1)
                READ_CONST1: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            if(is_const1_negative || parsing_number) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            else begin 
                                state <= READ_CONST1;
                            end
                        end
                        else if(ascii_char == "(" && !parsing_number) begin
                            paren_count <= paren_count + 1;
                        end 
                        else if(ascii_char == ")") begin
                            if(num_buffer == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            else begin
                                paren_count <= paren_count - 1;
                            end
                        end
                        // check if there's a negative sign ("-") before the digit   
                        else if(ascii_char == "-" && !parsing_number) begin
                            is_const1_negative <= 1;
                        end 
                        else if(is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end 
                        else if(parsing_number) begin
                            if(is_const1_negative)
                                const1 <= -num_buffer;
                            else
                                const1 <= num_buffer;
                            
                            num_buffer <= 0;
                            parsing_number <= 0;
                            
                            if(ascii_char == ";") begin
                                state <= READ_SEMICOLON1;
                            end 
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_SEMICOLON1: begin
                    if(new_char) begin
                        if(paren_count != 0) begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                        if(is_whitespace) begin
                            state <= READ_SEMICOLON1;
                        end
                        else if(ascii_char == "e") begin
                            keyword_index <= 1;
                            keyword_buffer <= "e";
                            state <= READ_END1;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_END1: begin
                    if(new_char) begin
                        case(keyword_index)
                            1: if(ascii_char == "n") begin
                                keyword_buffer <= (keyword_buffer << 8) | "n";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            2: if(ascii_char == "d") begin
                                keyword_index <= 0;
                                state <= READ_ELSE;
                            end 
                            // else begin
                            //     error_flag <= 1;
                            //     error_code <= INVALID_KEYWORD;
                            // end
                            
                            default: begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        endcase
                    end
                end

                // Expect the "else" keyword (check for first letter "e")
                READ_ELSE: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_ELSE;
                        end
                        else if(ascii_char == "e") begin
                            keyword_index <= 1;
                            keyword_buffer <= "e";
                            state <= READ_ELSE + 1;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_ELSE + 1: begin
                    if(new_char) begin
                        case(keyword_index)
                            1: if(ascii_char == "l") begin
                                keyword_buffer <= (keyword_buffer << 8) | "l";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            2: if(ascii_char == "s") begin
                                keyword_buffer <= (keyword_buffer << 8) | "s";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end

                            3: if(ascii_char == "e") begin
                                keyword_index <= 0;
                                state <= READ_BEGIN2;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            default: begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        endcase
                    end
                end

                READ_BEGIN2: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_BEGIN2;
                        end
                        else if(ascii_char == "b") begin
                            keyword_index <= 1;
                            keyword_buffer <= "b";
                            state <= READ_BEGIN2 + 1;
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_BEGIN2 + 1: begin
                    if(new_char) begin
                        case(keyword_index)
                            1: if(ascii_char == "e") begin
                                keyword_buffer <= (keyword_buffer << 8) | "e";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            2: if(ascii_char == "g") begin
                                keyword_buffer <= (keyword_buffer << 8) | "g";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            3: if(ascii_char == "i") begin
                                keyword_buffer <= (keyword_buffer << 8) | "i";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end

                            4: if(ascii_char == "n") begin
                                keyword_index <= 0;
                                state <= READ_ASSIGNMENT_VAR2;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            // default: begin
                            //     error_flag <= 1;
                            //     error_code <= SYNTAX_ERROR;
                            // end
                        endcase
                    end
                end

                READ_ASSIGNMENT_VAR2: begin
                    if(new_char) begin
                        if(is_whitespace && !reading_var) begin
                            state <= READ_ASSIGNMENT_VAR2;
                        end
                        else if(!reading_var && is_id_start) begin
                            // First character of identifier - must be a letter
                            assignment_var2_array[0] <= ascii_char; // Start at index 0
                            assignment_var2_idx <= 1;
                            assignment_var2_length <= 1;
                            reading_var <= 1;
                        end
                        else if(reading_var && is_id_char) begin
                            // Subsequent characters - can be letter, digit, or underscore
                            assignment_var2_array[assignment_var2_idx] <= ascii_char;
                            assignment_var2_idx <= assignment_var2_idx + 1;
                            assignment_var2_length <= assignment_var2_length + 1;
                            if(assignment_var2_idx == 15) begin  // Max length reached
                                state <= READ_ASSIGNMENT_OPERATOR2;
                                reading_var <= 0;
                                
                                // Check if variables match
                                var_match <= var_names_match(15);
                                if(!var_names_match(15)) begin
                                    error_flag <= 1;
                                    error_code <= VAR_MISMATCH;
                                    $display("ERROR: Variables mismatch between branches!");
                                end
                            end
                        end
                        else if(ascii_char == ")" && reading_var) begin
                            // Special case: closing parenthesis without operator - error
                            paren_count <= paren_count - 1;
                        end
                        else if(reading_var && (is_whitespace || ascii_char == "=" || ascii_char == "<")) begin
                            // Variable name complete, ready for operator
                            state <= READ_ASSIGNMENT_OPERATOR2;
                            reading_var <= 0;
                            
                            // Check if variables match
                            var_match <= var_names_match(15);
                            if(!var_names_match(15)) begin
                                error_flag <= 1;
                                error_code <= VAR_MISMATCH;
                                $display("ERROR: Variables mismatch between branches!");
                            end
                            
                            // Process operator right away if not whitespace
                            if(!is_whitespace) begin
                                if(ascii_char == "<") begin
                                    blocking_assignment2 <= 0;
                                    op_first <= "<";
                                end 
                                else if(ascii_char == "=") begin
                                    blocking_assignment2 <= 1;
                                    op_first <= 0;
                                    num_buffer <= 0;
                                    parsing_number <= 0;
                                    is_const2_negative <= 0;
                                    state <= READ_CONST2;
                                end
                            end
                        end
                        else if(ascii_char == "(") begin
                            // Opening nested parenthesis
                            paren_count <= paren_count + 1;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_ASSIGNMENT_OPERATOR2: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            state <= READ_ASSIGNMENT_OPERATOR2;
                        end
                        else if(ascii_char == ")") paren_count <= paren_count - 1;
                        else if(ascii_char == "<") begin
                            blocking_assignment2 <= 0;
                            state <= READ_ASSIGNMENT_OPERATOR2;
                        end 
                        else if(ascii_char == "=") begin
                            if(op_first == "<") begin
                                blocking_assignment2 <= 0; // non-blocking (<=)
                            end else begin
                                blocking_assignment2 <= 1; // blocking (=)
                            end
                            op_first <= 0;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            is_const2_negative <= 0;
                            state <= READ_CONST2;
                        end
                        else begin
                            op_first <= ascii_char;
                        end
                    end
                end

                // Accumulate constant for else branch (const2)
                READ_CONST2: begin
                    if(new_char) begin
                        if(is_whitespace) begin
                            if(is_const2_negative || parsing_number) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            else begin 
                                state <= READ_CONST2;
                            end
                        end
                        else if(ascii_char == "(" && !parsing_number) begin
                            paren_count <= paren_count + 1;
                        end 
                        else if(ascii_char == ")") begin
                            if(num_buffer == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            else begin
                                paren_count <= paren_count - 1;
                            end
                        end
                        // check if there's a negative sign ("-") before the digit   
                        else if(ascii_char == "-" && !parsing_number) begin
                            is_const2_negative <= 1;
                        end 
                        else if(is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end 
                        else if(parsing_number) begin
                            if(is_const2_negative)
                                const2 <= -num_buffer;
                            else
                                const2 <= num_buffer;
                            
                            num_buffer <= 0;
                            parsing_number <= 0;
                            
                            if(ascii_char == ";") begin
                                state <= READ_SEMICOLON2;
                            end 
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_SEMICOLON2: begin
                    if(new_char) begin
                        if(paren_count != 0) begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                        if(is_whitespace) begin
                            state <= READ_SEMICOLON2;
                        end
                        else if(ascii_char == "e") begin
                            keyword_index <= 1;
                            keyword_buffer <= "e";
                            state <= READ_END2;
                        end
                        else begin
                            error_flag <= 1;
                            error_code <= INVALID_KEYWORD;
                        end
                    end
                end

                READ_END2: begin
                    if(new_char) begin
                        case(keyword_index)
                            1: if(ascii_char == "n") begin
                                keyword_buffer <= (keyword_buffer << 8) | "n";
                                keyword_index <= keyword_index + 1;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            2: if(ascii_char == "d") begin
                                keyword_index <= 0;
                                state <= EVALUATE;
                            end else begin
                                error_flag <= 1;
                                error_code <= INVALID_KEYWORD;
                            end
                            
                            default: begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        endcase
                    end
                end

                // Evaluate the condition using the selected comparator.
                EVALUATE: begin
                    if(parsing_done) begin
                        state <= IDLE;
                    end
                    
                    if(!parsing_done) begin
                        $display("EVALUATING: x=%d, valC=%d, comparator=%b, const1=%d, const2=%d",
                        x, valC, comparator, const1, const2);
                    end 

                    if(!error_flag) begin
                        case(comparator)
                            EQ:  if(x == valC) p <= const1; else p <= const2;
                            NE:  if(x != valC) p <= const1; else p <= const2;
                            LT:  if(x <  valC) p <= const1; else p <= const2;
                            GT:  if(x >  valC) p <= const1; else p <= const2;
                            LE:  if(x <= valC) p <= const1; else p <= const2;
                            GE:  if(x >= valC) p <= const1; else p <= const2;
                            default: p <= 0;
                        endcase
                        parsing_done <= 1;
                    end
                end

                default: state <= IDLE;
            endcase
        end
    end

endmodule
