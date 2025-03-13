module if_else_parser (
    input  wire        clk,                   
    input  wire        rst,                   
    input  wire [31:0] x,              
    input  wire [6:0]  ascii_char,      
    input  wire        char_valid,            
    output reg  [31:0] p,              
    output reg         parsing_done,          
    output reg         error_flag             
);

    // State encoding
    parameter IDLE                      = 0,
              READ_IF                   = 1,
              READ_VAR                  = 2,
              READ_COND_OPERATOR        = 3,  // Read first char of comparator
              READ_COND_OPERATOR2       = 4,  // Read second char of comparator (if any)
              READ_VALC                 = 5,
              READ_ASSIGNMENT           = 6,  // expecting letter "p"
              READ_ASSIGNMENT_OPERATOR  = 7,  // expecting "<=" for if branch
              READ_CONST1               = 8,
              READ_ELSE                 = 9,  // expecting "else" keyword (first letter "e")
              READ_ASSIGNMENT2          = 10, // expecting letter "p" for else branch
              READ_ASSIGNMENT_OPERATOR2 = 11, // expecting "<=" for else branch
              READ_CONST2               = 12,
              EVALUATE                  = 13,
              OUTPUT                    = 14;

    reg [3:0] state;

    // Data registers
    reg [31:0] valC, const1, const2;
    reg [31:0] num_buffer;
    reg        parsing_number;
    reg        num_done; // flag: indicates that we've latched the current number

    // Operator detection for condition:
    // Supported comparators:
    //  "==" => EQ, "!=" => NE, "<=" => LE, ">=" => GE,
    //  "<"  => LT, ">"  => GT.
    reg [2:0] comparator;
    parameter EQ = 3'b000, NE = 3'b001, LT = 3'b010, GT = 3'b011, LE = 3'b100, GE = 3'b101;

    // Temporary register to store first operator character
    reg [6:0] op_first;

    // For verifying that the assignment letter is detected
    reg p_detected_if, p_detected_else;

    // ASCII digit check ("0" = 7'h30, "9" = 7'h39)
    wire is_digit = (ascii_char >= "0" && ascii_char <= "9");

    // Edge-detect char_valid: process each character only once.
    reg char_valid_d;
    always @(posedge clk or posedge rst)
        if (rst)
            char_valid_d <= 0;
        else
            char_valid_d <= char_valid;
    wire new_char = char_valid & ~char_valid_d;

    // FSM
    always @(posedge clk or posedge rst) begin
        if(rst) begin
            state           <= IDLE;
            valC            <= 0;
            const1          <= 0;
            const2          <= 0;
            num_buffer      <= 0;
            parsing_number  <= 0;
            num_done        <= 0;
            parsing_done    <= 0;
            error_flag      <= 0;
            comparator      <= 0;
            p_detected_if   <= 0;
            p_detected_else <= 0;
            op_first        <= 0;
        end
        else begin
            $display("State: %d, char: %c (%h), x: %d, valC: %d, p: %d, done: %b, error: %b", 
                      state, ascii_char, ascii_char, x, valC, p, parsing_done, error_flag);
            case(state)
                IDLE: begin
                    if(new_char && ascii_char=="i")
                        state <= READ_IF;
                end

                READ_IF: begin
                    if(new_char && ascii_char=="f")
                        state <= READ_VAR;
                end

                READ_VAR: begin
                    if(new_char && ascii_char=="x")
                        state <= READ_COND_OPERATOR;
                end

                // Read the first character of the condition operator.
                READ_COND_OPERATOR: begin
                    if(new_char) begin
                        // Accept one of these as the first character: <, >, =, or !
                        if(ascii_char=="<" || ascii_char==">" || ascii_char=="=" || ascii_char=="!")
                        begin
                            op_first <= ascii_char;
                            state <= READ_COND_OPERATOR2;
                        end
                        else begin
                            error_flag <= 1;
                        end
                    end
                end

                // Read the second character of the condition operator, if applicable.
                READ_COND_OPERATOR2: begin
                    if(new_char) begin
                        case(op_first)
                            "<": begin
                                if(ascii_char=="=")
                                    comparator <= LE;
                                else
                                    comparator <= LT; // single-character "<"
                                // Prepare for number parsing:
                                num_buffer     <= 0;
                                parsing_number <= 0;
                                num_done       <= 0;
                                state          <= READ_VALC;
                            end
                            ">": begin
                                if(ascii_char=="=")
                                    comparator <= GE;
                                else if(is_digit)
                                    comparator <= GT; // single-character ">" 
                                num_buffer     <= 0;
                                parsing_number <= 0;
                                num_done       <= 0;
                                state          <= READ_VALC;
                            end
                            "=": begin
                                if(ascii_char=="=")
                                    comparator <= EQ;
                                else begin
                                    error_flag <= 1;
                                end
                                num_buffer     <= 0;
                                parsing_number <= 0;
                                num_done       <= 0;
                                state          <= READ_VALC;
                            end
                            "!": begin
                                if(ascii_char=="=")
                                    comparator <= NE;
                                else begin
                                    error_flag <= 1;
                                end
                                num_buffer     <= 0;
                                parsing_number <= 0;
                                num_done       <= 0;
                                state          <= READ_VALC;
                            end
                            default: state <= IDLE;
                        endcase
                    end
                end

                // Accumulate condition value (valC)
                READ_VALC: begin
                    if(new_char) begin
                        if(is_digit) begin
                            num_buffer     <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end
                        else if(parsing_number && !num_done) begin
                            valC       <= num_buffer;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            num_done   <= 1;
                            state      <= READ_ASSIGNMENT;
                        end
                    end
                end

                // Expect letter "p" for the if-branch assignment.
                READ_ASSIGNMENT: begin
                    if(new_char) begin
                        if(ascii_char=="p") begin
                            p_detected_if <= 1;
                            state <= READ_ASSIGNMENT_OPERATOR;
                        end
                    end
                end

                // Expect assignment operator ("<=") for if-branch constant.
                READ_ASSIGNMENT_OPERATOR: begin
                    if(new_char) begin
                        if(ascii_char=="<")
                            state <= READ_ASSIGNMENT_OPERATOR; // stay until "=" comes
                        else if(ascii_char=="=") begin
                            // Got "<="; prepare to read constant digits.
                            num_buffer     <= 0;
                            parsing_number <= 0;
                            num_done       <= 0;
                            state          <= READ_CONST1;
                        end
                    end
                end

                // Accumulate constant for if branch (const1)
                READ_CONST1: begin
                    if(new_char) begin
                        if(is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end
                        else if(parsing_number && !num_done) begin
                            const1 <= num_buffer;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            num_done <= 1;
                            state <= READ_ELSE;
                        end
                    end
                end

                // Expect the "else" keyword (check for first letter "e")
                READ_ELSE: begin
                    if(new_char && ascii_char=="e")
                        state <= READ_ASSIGNMENT2;
                end

                // Expect letter "p" for the else branch assignment.
                READ_ASSIGNMENT2: begin
                    if(new_char) begin
                        if(ascii_char=="p") begin
                            p_detected_else <= 1;
                            state <= READ_ASSIGNMENT_OPERATOR2;
                        end
                    end
                end

                // Expect assignment operator ("<=") for else branch.
                READ_ASSIGNMENT_OPERATOR2: begin
                    if(new_char) begin
                        if(ascii_char=="<")
                            state <= READ_ASSIGNMENT_OPERATOR2; // wait for "="
                        else if(ascii_char=="=") begin
                            num_buffer     <= 0;
                            parsing_number <= 0;
                            num_done       <= 0;
                            state          <= READ_CONST2;
                        end
                    end
                end

                // Accumulate constant for else branch (const2)
                READ_CONST2: begin
                    if(new_char) begin
                        if(is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end
                        else if(parsing_number && !num_done) begin
                            const2 <= num_buffer;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            num_done <= 1;
                            state <= EVALUATE;
                        end
                    end
                end

                // Evaluate the condition using the selected comparator.
                EVALUATE: begin
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
                    state <= OUTPUT;
                end

                OUTPUT: begin
                    parsing_done <= 1;
                    // Final state; outputs are held.
                end

                default: state <= IDLE;
            endcase
        end
    end

endmodule
