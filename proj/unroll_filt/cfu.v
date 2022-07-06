module Cfu (
  input               cmd_valid,
  output              cmd_ready,
  input      [9:0]    cmd_payload_function_id,
  input      [31:0]   cmd_payload_inputs_0,
  input      [31:0]   cmd_payload_inputs_1,
  output reg          rsp_valid,
  input               rsp_ready,
  output reg [31:0]   rsp_payload_outputs_0,
  input               reset,
  input               clk
);
  localparam InputOffset = $signed(9'd128);
  //4 sets of registers to store the distinct filter values
  reg [31:0] filt_vals_1[0:323];
  reg [31:0] filt_vals_2[0:323];
  reg [31:0] filt_vals_3[0:323];
  reg [31:0] filt_vals_4[0:323];
  //registers to store the filter values for input depth of 1
  reg [31:0] filt_vals_s[0:107];
  //products pertaining to each filter set for accumulation
  wire signed [15:0] prod_0_1, prod_1_1, prod_2_1, prod_3_1;
  wire signed [15:0] prod_0_2, prod_1_2, prod_2_2, prod_3_2;
  wire signed [15:0] prod_0_3, prod_1_3, prod_2_3, prod_3_3;
  wire signed [15:0] prod_0_4, prod_1_4, prod_2_4, prod_3_4;
                  
  //MAC operation for each of the filter sets
  assign prod_0_1 =  ($signed(cmd_payload_inputs_0[7 : 0]) + InputOffset)
                  * $signed(filt_vals_1[cmd_payload_inputs_1][7 : 0]);
  assign prod_1_1 =  ($signed(cmd_payload_inputs_0[15: 8]) + InputOffset)
                  * $signed(filt_vals_1[cmd_payload_inputs_1][15: 8]);
  assign prod_2_1 =  ($signed(cmd_payload_inputs_0[23:16]) + InputOffset)
                  * $signed(filt_vals_1[cmd_payload_inputs_1][23:16]);
  assign prod_3_1 =  ($signed(cmd_payload_inputs_0[31:24]) + InputOffset)
                  * $signed(filt_vals_1[cmd_payload_inputs_1][31:24]);
                  
  assign prod_0_2 =  ($signed(cmd_payload_inputs_0[7 : 0]) + InputOffset)
                  * $signed(filt_vals_2[cmd_payload_inputs_1][7 : 0]);
  assign prod_1_2 =  ($signed(cmd_payload_inputs_0[15: 8]) + InputOffset)
                  * $signed(filt_vals_2[cmd_payload_inputs_1][15: 8]);
  assign prod_2_2 =  ($signed(cmd_payload_inputs_0[23:16]) + InputOffset)
                  * $signed(filt_vals_2[cmd_payload_inputs_1][23:16]);
  assign prod_3_2 =  ($signed(cmd_payload_inputs_0[31:24]) + InputOffset)
                  * $signed(filt_vals_2[cmd_payload_inputs_1][31:24]);
                  
  assign prod_0_3 =  ($signed(cmd_payload_inputs_0[7 : 0]) + InputOffset)
                  * $signed(filt_vals_3[cmd_payload_inputs_1][7 : 0]);
  assign prod_1_3 =  ($signed(cmd_payload_inputs_0[15: 8]) + InputOffset)
                  * $signed(filt_vals_3[cmd_payload_inputs_1][15: 8]);
  assign prod_2_3 =  ($signed(cmd_payload_inputs_0[23:16]) + InputOffset)
                  * $signed(filt_vals_3[cmd_payload_inputs_1][23:16]);
  assign prod_3_3 =  ($signed(cmd_payload_inputs_0[31:24]) + InputOffset)
                  * $signed(filt_vals_3[cmd_payload_inputs_1][31:24]);
                  
  assign prod_0_4 =  ($signed(cmd_payload_inputs_0[7 : 0]) + InputOffset)
                  * $signed(filt_vals_4[cmd_payload_inputs_1][7 : 0]);
  assign prod_1_4 =  ($signed(cmd_payload_inputs_0[15: 8]) + InputOffset)
                  * $signed(filt_vals_4[cmd_payload_inputs_1][15: 8]);
  assign prod_2_4 =  ($signed(cmd_payload_inputs_0[23:16]) + InputOffset)
                  * $signed(filt_vals_4[cmd_payload_inputs_1][23:16]);
  assign prod_3_4 =  ($signed(cmd_payload_inputs_0[31:24]) + InputOffset)
                  * $signed(filt_vals_4[cmd_payload_inputs_1][31:24]);

  wire signed [31:0] sum_prods_1;
  wire signed [31:0] sum_prods_2;
  wire signed [31:0] sum_prods_3;
  wire signed [31:0] sum_prods_4;
  wire signed [31:0] sum_prods_s;
  //Accumulating all the products
  assign sum_prods_1 = prod_0_1 + prod_1_1 + prod_2_1 + prod_3_1;
  assign sum_prods_2 = prod_0_2 + prod_1_2 + prod_2_2 + prod_3_2;
  assign sum_prods_3 = prod_0_3 + prod_1_3 + prod_2_3 + prod_3_3;
  assign sum_prods_4 = prod_0_4 + prod_1_4 + prod_2_4 + prod_3_4;
  //MAC operation for input depth of 1
  assign sum_prods_s = ($signed(cmd_payload_inputs_0) + InputOffset)
                  * $signed(filt_vals_s[cmd_payload_inputs_1]);

  // Only not ready for a command when we have a response.
  assign cmd_ready = ~rsp_valid;

  always @(posedge clk) begin
    if (reset) begin
      rsp_payload_outputs_0 <= 32'b0;
      rsp_valid <= 1'b0;
    end else if (rsp_valid) begin
      // Waiting to hand off response to CPU.
      rsp_valid <= ~rsp_ready;
    end else if (cmd_valid) begin
      rsp_valid <= 1'b1;
      if(cmd_payload_function_id[9:3] == 1) begin
       rsp_payload_outputs_0 <= 32'b0;        //resetting the accumulator
      end
      //filter value are stored here at the corresponding position given by the software code
      //each set is stored according to the funcition id used in the software code
      else if(cmd_payload_function_id[9:3] == 3) begin
      	filt_vals_1[cmd_payload_inputs_0] <= cmd_payload_inputs_1;
      end
      else if(cmd_payload_function_id[9:3] == 4) begin
      	filt_vals_2[cmd_payload_inputs_0] <= cmd_payload_inputs_1;
      end
      else if(cmd_payload_function_id[9:3] == 5) begin
      	filt_vals_3[cmd_payload_inputs_0] <= cmd_payload_inputs_1;
      end
      else if(cmd_payload_function_id[9:3] == 6) begin
      	filt_vals_4[cmd_payload_inputs_0] <= cmd_payload_inputs_1;
      end
      //filter storage for input depth of 1
      else if(cmd_payload_function_id[9:3] == 2) begin
      	filt_vals_s[cmd_payload_inputs_0] <= cmd_payload_inputs_1;
      end
      //Accumulate step for input depth of 1
      else if(cmd_payload_function_id[9:3] == 0) begin
      	rsp_payload_outputs_0 <= rsp_payload_outputs_0 + sum_prods_s;
      end
      //Accumulate step for each of the sets of filter values
      else if(cmd_payload_function_id[9:3] == 7) begin
      	rsp_payload_outputs_0 <= rsp_payload_outputs_0 + sum_prods_1;
      end
      else if(cmd_payload_function_id[9:3] == 8) begin
      	rsp_payload_outputs_0 <= rsp_payload_outputs_0 + sum_prods_2;
      end
      else if(cmd_payload_function_id[9:3] == 9) begin
      	rsp_payload_outputs_0 <= rsp_payload_outputs_0 + sum_prods_3;
      end
      else if(cmd_payload_function_id[9:3] == 10) begin
      	rsp_payload_outputs_0 <= rsp_payload_outputs_0 + sum_prods_4;
      end
      
    end
  end
endmodule
