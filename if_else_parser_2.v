module if_else_parser_2 (
    input  wire        clk,                   
    input  wire        rst,                   
    input  wire signed [31:0] x,              
    input  wire [6:0]  ascii_char,      
    input  wire        char_valid,            
    output reg signed  [31:0] p,
    output reg         [6:0] name_p,              
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

    // Variable name tracking
    reg [6:0]  cond_var;      // Condition variable (was 'x')
    reg [6:0]  assignment_var; // Assignment variable (was 'p')
    reg [6:0]  assignment_var2; // Assignment variable in else branch
    reg        var_match;      // Flag to check if variables match

    // Data registers
    integer    valC, const1, const2;
    integer    num_buffer;
    reg        parsing_number;

    integer paran_count = 0;

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

    // For verifying that the assignment letter is detected
    reg p_detected_if, p_detected_else;

    // ASCII digit and letter checks
    wire is_digit = (ascii_char >= "0" && ascii_char <= "9");
    wire is_letter = ((ascii_char >= "a" && ascii_char <= "z") || 
                     (ascii_char >= "A" && ascii_char <= "Z"));

    // Flags for negative integers valC, const1, const2
    reg is_valC_negative, is_const1_negative, is_const2_negative;

    // Edge-detect char_valid: process each character only once.
    // not needed anymore - doing continuous assignment now.
    // wire char_valid_d;
    // always @(posedge clk or posedge rst)
    //     if (rst)
    //         char_valid_d <= 0;
    //     else
    //         char_valid_d <= char_valid;
    // assign char_valid_d = char_valid;
    // wire new_char = char_valid & ~char_valid_d;
    wire new_char = char_valid;

    // Debug function
    task display_debug;
        begin
            $display("State: %d, curr_char: %c (%h), keyword_buffer: %h, keyword_index: %d, cond_var: %c, assignment_var: %c, assignment_var2: %c, var_match: %b", 
                    state, ascii_char, ascii_char, keyword_buffer, keyword_index, cond_var, assignment_var, assignment_var2, var_match);
            $display("valC: %d, const1: %d, const2: %d, error_code: %d", valC, const1, const2, error_code);
            $display("---------------------------------------------------------------------------------------------------------------------------------------------");
        end
    endtask

    // Task for keyword parsing
    task parse_keyword;
        input [6:0] expected_char;
        input [4:0] next_state;
        input [4:0] error_state;
        begin
            if (ascii_char == expected_char) begin
                keyword_index <= keyword_index + 1;
                state <= next_state;
            end 
            else begin
                error_flag <= 1;
                error_code <= INVALID_KEYWORD;
                state <= error_state;
            end
        end
    endtask

    // FSM
    always @(posedge clk or posedge rst) begin
        if(rst) begin
            state               <= IDLE;
            keyword_buffer      <= 0;
            keyword_index       <= 0;
            keyword_complete    <= 0;
            cond_var            <= 0;
            assignment_var      <= 0;
            assignment_var2     <= 0;
            var_match           <= 0;
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
        end
        else begin
            if(new_char) begin
                display_debug;
            end

            case(state)
                IDLE: begin
                    if(new_char) begin
                        if(ascii_char == "i") begin
                            keyword_index <= 1;
                            state <= READ_IF;
                        end else begin
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
                        if(ascii_char == "(") begin
                            // paran_count <= paran_count + 1;
                            state <= READ_VAR;
                        end else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_VAR: begin
                    if(new_char) begin
                        if(is_letter) begin
                            cond_var <= ascii_char;
                            state <= READ_COND_OPERATOR;
                            $display("Condition variable received: %c", ascii_char);
                        end 
                        else if(ascii_char == "(") begin
                            paran_count <= paran_count + 1;
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
                        if(ascii_char == "<" || ascii_char == ">" || ascii_char == "=" || ascii_char == "!") begin
                            op_first <= ascii_char;
                            state <= READ_COND_OPERATOR2;
                        end
                        else if(ascii_char == ")") begin
                            if(paran_count == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            paran_count <= paran_count - 1;
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
                                    paran_count <= paran_count + 1;
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
                                    paran_count <= paran_count + 1;
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
                    // $display("new_char: %b, ascii_char: %c, is_digit: %b", new_char, ascii_char, is_digit);
                    // check if there's a negative sign ("-") before the digit
                    if(new_char) begin
                        if(ascii_char == "(" && !parsing_number) begin
                            paran_count <= paran_count + 1;
                        end   
                        if(ascii_char == "-" && !parsing_number) begin
                            is_valC_negative <= 1;
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
                                if(paran_count == 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                                end
                                // paran_count <= paran_count - 1;
                                state <= READ_CLOSE_PAREN;
                            end else begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        end 
                    end
                end

                READ_CLOSE_PAREN: begin
                    if(new_char) begin
                        if(ascii_char == "b") begin
                            keyword_index <= 1;
                            keyword_buffer <= "b";
                            if(paran_count != 0) begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                            state <= READ_BEGIN;
                        end
                        else if(ascii_char == ")") begin
                            paran_count <= paran_count - 1;
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
                        if(is_letter) begin
                            assignment_var <= ascii_char;
                            $display("Assignment variable (if branch): %c", ascii_char);
                            state <= READ_ASSIGNMENT_OPERATOR;
                        end else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                // Expect assignment operator ("<=") for if-branch constant.
                READ_ASSIGNMENT_OPERATOR: begin
                    if(new_char) begin
                        if(ascii_char == "<") begin
                            blocking_assignment1 <= 0;
                            state <= READ_ASSIGNMENT_OPERATOR;
                        end else if(ascii_char == "=") begin
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
                        end else begin
                            op_first <= ascii_char;
                        end
                    end
                end

                // Accumulate constant for if branch (const1)
                READ_CONST1: begin
                    if(new_char) begin
                        // check if there's a negative sign ("-") before the digit
                        if(ascii_char == "-") begin
                            is_const1_negative <= 1;
                        end
                        if(is_digit) begin
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
                            end else begin
                                error_flag <= 1;
                                error_code <= MISSING_SEMICOLON;
                            end
                        end else if(ascii_char == ";") begin
                            state <= READ_SEMICOLON1;
                        end else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_SEMICOLON1: begin
                    if(new_char) begin
                        if(ascii_char == "e") begin
                            keyword_index <= 1;
                            keyword_buffer <= "e";
                            state <= READ_END1;
                        end else begin
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

                // Expect the "else" keyword (check for first letter "e")
                READ_ELSE: begin
                    if(new_char) begin
                        if(ascii_char == "e") begin
                            keyword_index <= 1;
                            keyword_buffer <= "e";
                            state <= READ_ELSE + 1;
                        end else begin
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
                        if(ascii_char == "b") begin
                            keyword_index <= 1;
                            keyword_buffer <= "b";
                            state <= READ_BEGIN2 + 1;
                        end else begin
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
                            
                            default: begin
                                error_flag <= 1;
                                error_code <= SYNTAX_ERROR;
                            end
                        endcase
                    end
                end

                READ_ASSIGNMENT_VAR2: begin
                    if(new_char) begin
                        if(is_letter) begin
                            assignment_var2 <= ascii_char;
                            $display("Assignment variable (else branch): %c", ascii_char);
                            
                            // Check if variables match
                            if(assignment_var != ascii_char) begin
                                error_flag <= 1;
                                error_code <= VAR_MISMATCH;
                                $display("ERROR: Variables mismatch between branches!");
                            end
                            
                            state <= READ_ASSIGNMENT_OPERATOR2;
                        end else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                // Expect assignment operator ("<=") for else branch.
                READ_ASSIGNMENT_OPERATOR2: begin
                    if(new_char) begin
                        if(ascii_char == "<") begin
                            blocking_assignment2 <= 0;
                            state <= READ_ASSIGNMENT_OPERATOR2;
                        end else if(ascii_char == "=") begin
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
                        // check if there's a negative sign ("-") before the digit
                        if(ascii_char == "-" && !parsing_number) begin
                            is_const2_negative <= 1;
                        end
                        if(is_digit) begin
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
                            end else begin
                                error_flag <= 1;
                                error_code <= MISSING_SEMICOLON;
                            end
                        end 
                        else if(ascii_char == ";") begin
                            state <= READ_SEMICOLON2;
                        end 
                        else begin
                            error_flag <= 1;
                            error_code <= SYNTAX_ERROR;
                        end
                    end
                end

                READ_SEMICOLON2: begin
                    if(new_char) begin
                        if(ascii_char == "e") begin
                            keyword_index <= 1;
                            keyword_buffer <= "e";
                            state <= READ_END2;
                        end else begin
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
