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
  reg [31:0] filt_vals_1[0:107];

  wire signed [31:0] sum_prods_1;
  assign sum_prods_1 = ($signed(cmd_payload_inputs_0) + InputOffset)
                  * $signed(filt_vals_1[cmd_payload_inputs_1]);

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
      // Accumulate step:
      if(cmd_payload_function_id[9:3] == 1) begin
       rsp_payload_outputs_0 <= 32'b0;
      end
      else if(cmd_payload_function_id[9:3] == 2) begin
      	//filter value are stored here
      	filt_vals_1[cmd_payload_inputs_0] <= cmd_payload_inputs_1;
      end
      else begin
      	rsp_payload_outputs_0 <= rsp_payload_outputs_0 + sum_prods_1;
      end
      
    end
  end
endmodule
