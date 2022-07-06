/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_LITE_KERNELS_INTERNAL_REFERENCE_INTEGER_OPS_CONV_H_
#define TENSORFLOW_LITE_KERNELS_INTERNAL_REFERENCE_INTEGER_OPS_CONV_H_

#include "tensorflow/lite/kernels/internal/common.h"
#include "perf.h"
#include<stdio.h>
#include "playground_util/print_params.h"
#include "cfu.h"

namespace tflite {
namespace reference_integer_ops {

// Fixed-point per-channel-quantization convolution reference kernel.
inline void ConvPerChannel(
    const ConvParams& params, const int32_t* output_multiplier,
    const int32_t* output_shift, const RuntimeShape& input_shape,
    const int8_t* input_data, const RuntimeShape& filter_shape,
    const int8_t* filter_data, const RuntimeShape& bias_shape,
    const int32_t* bias_data, const RuntimeShape& output_shape,
    int8_t* output_data) {
  //These are the parameters that are constant throughout the execution
  // Get parameters.
  /*const int32_t input_offset = params.input_offset;  // r = s(q - Z)
  const int stride_width = params.stride_width;
  const int stride_height = params.stride_height;
  const int dilation_width_factor = params.dilation_width_factor;
  const int dilation_height_factor = params.dilation_height_factor;
  const int pad_width = params.padding_values.width;
  const int pad_height = params.padding_values.height;*/
  const int32_t output_offset = params.output_offset;

  // Set min and max value of the output.
  const int32_t output_activation_min = params.quantized_activation_min;
  const int32_t output_activation_max = params.quantized_activation_max;

  // Consistency check.
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  TFLITE_DCHECK_EQ(input_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_EQ(filter_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_EQ(output_shape.DimensionsCount(), 4);
  const int batches = MatchingDim(input_shape, 0, output_shape, 0);
  const int input_depth = MatchingDim(input_shape, 3, filter_shape, 3);
  const int output_depth = MatchingDim(filter_shape, 0, output_shape, 3);
  if (bias_data) {
    TFLITE_DCHECK_EQ(bias_shape.FlatSize(), output_depth);
  }

  // Check dimensions of the tensors.
  const int input_height = input_shape.Dims(1);
  const int input_width = input_shape.Dims(2);
  //The filter_height and filter_width are replaced by their value i.e 3 below
  //const int filter_height = filter_shape.Dims(1);
  //const int filter_width = filter_shape.Dims(2);
  const int output_height = output_shape.Dims(1);
  const int output_width = output_shape.Dims(2);
 //Dividing the convolution computations into input depth of 1 and 12
 //Convolution layer with input depth of 12 - Unrolled with filter storage Implementation   
 if(input_depth == 12){
 //Block of code to access the filter values send them to cfu where it is stored	 
 //filter_count is to correspond the stored filter value to be used for computation in CFU
 //Since the innermost loop is unrolled by a factor of 4, 4 such counters are required
 int filter_count_1 = 0;
 int filter_count_2 = 0;
 int filter_count_3 = 0;
 int filter_count_4 = 0;
 int count = 0;		//this is to track the storage location of the filter value in cfu
	//These series of for loops are the ones required to access the filter values
	//To correspond to unrolling 4 units of filter storages are used 
	for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
	  for (int filter_y = 0; filter_y < 3; ++filter_y) {
	    for (int filter_x = 0; filter_x < 3; ++filter_x) {
	      for (int in_channel = 0; in_channel < input_depth; in_channel += 4) {
		//Accessing filter values		     
		int32_t filter_vals_1 = filter_data[Offset(
                      filter_shape, out_channel, filter_y, filter_x, in_channel)];
                int32_t filter_vals_2 = filter_data[Offset(
                      filter_shape, out_channel, filter_y, filter_x, in_channel+1)];
                int32_t filter_vals_3 = filter_data[Offset(
                      filter_shape, out_channel, filter_y, filter_x, in_channel+2)];
                int32_t filter_vals_4 = filter_data[Offset(
                      filter_shape, out_channel, filter_y, filter_x, in_channel+3)];          
		//sending the filter values along with storage location counter
		//each operation is sent along with a function id which is used in the CFU unit to store appropriately		      
                cfu_op0(3, count, filter_vals_1);
                cfu_op0(4, count, filter_vals_2);
                cfu_op0(5, count, filter_vals_3);
                cfu_op0(6, count, filter_vals_4);
		//incrementing the counter value to store the next set of filter values		      
		count += 1;
	      }
	    }
	  }
	}
    //End of the block of code used for storage of distinct filter values    	 
    for (int batch = 0; batch < batches; ++batch) {
      for (int out_y = 0; out_y < output_height; ++out_y) {
        const int in_y_origin = out_y;
        for (int out_x = 0; out_x < output_width; ++out_x) {
          const int in_x_origin = out_x;
          for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
	    //resetting the accumulator
            //the accumulation is done completely in CFU and the result is sent back 
      	    //This block of code is used for analysis as the accumulator results
      	    //are reset computed and sent back once wihtin this loop
            int32_t acc = cfu_op0(/* funct7= */ 1, 0, 0); // resets acc
            for (int filter_y = 0; filter_y < 3; ++filter_y) {
              const int in_y = in_y_origin + filter_y;
              for (int filter_x = 0; filter_x < 3; ++filter_x) {
                const int in_x = in_x_origin + filter_x;

                // Zero padding by omitting the areas outside the image.
                const bool is_point_inside_image =
                    (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height);

                if (!is_point_inside_image) {
                  continue;
                }
                for (int in_channel = 0; in_channel < input_depth; in_channel += 4) {
		  //innermost loop - involves accessing input values and sending them to CFU - 4 times due to unrolling
                  int32_t input_val = input_data[Offset(input_shape, batch, in_y,
                                                          in_x, in_channel)];
		  //along with input values the filter_counter tracks the filter value corresponding to the input value for the CFU			
                  acc = cfu_op0(/* funct7= */ 7, /* in0= */ input_val, /* in1= */ filter_count_1);
		  //Incrementing the filter_count unitl 324 and resetting after it
         	  //There are 324 filter values in each set involved for this computation			
                  filter_count_1 = (filter_count_1 + 1)%324;
                  
		  //Similarly 3 more sets of computations are carried out
                  input_val = input_data[Offset(input_shape, batch, in_y,
                                                          in_x, in_channel + 1)];
                  acc = cfu_op0(/* funct7= */ 8, /* in0= */ input_val, /* in1= */ filter_count_2);
                filter_count_2 = (filter_count_2 + 1)%324;

                  input_val = input_data[Offset(input_shape, batch, in_y,
                                                          in_x, in_channel + 2)];
                  acc = cfu_op0(/* funct7= */ 9, /* in0= */ input_val, /* in1= */ filter_count_3);
                filter_count_3 = (filter_count_3 + 1)%324;

                  input_val = input_data[Offset(input_shape, batch, in_y,
                                                          in_x, in_channel + 3)];
                  acc = cfu_op0(/* funct7= */ 10, /* in0= */ input_val, /* in1= */ filter_count_4);
                filter_count_4 = (filter_count_4 + 1)%324;
                }
              }
            }

            if (bias_data) {
              acc += bias_data[out_channel];
            }
            acc = MultiplyByQuantizedMultiplier(
                acc, output_multiplier[out_channel], output_shift[out_channel]);
            acc += output_offset;
            acc = std::max(acc, output_activation_min);
            acc = std::min(acc, output_activation_max);
            output_data[Offset(output_shape, batch, out_y, out_x, out_channel)] =
                static_cast<int8_t>(acc);
          }
        }
      }
    }
  }
  //The other case of the convolution computation for input depth of 1
  if(input_depth == 1) {
  //Block of code to access the filter values send them to cfu where it is stored	  
  int count_s = 0;		//this is to track the storage location of the filter value in cfu
  int filter_count_s = 0;	//this is to use the corresponding stored filter value for computation
  	//These series of for loops are the ones required to access the filter values	  
  	for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
	  for (int filter_y = 0; filter_y < 3; ++filter_y) {
	    for (int filter_x = 0; filter_x < 3; ++filter_x) {
		//Accessing filter values		    
                int32_t filter_vals_s = filter_data[Offset(
                      filter_shape, out_channel, filter_y, filter_x, 0)];
		//sending the filter values along with storage location counter		    
                cfu_op0(2, count_s, filter_vals_s);
		//incrementing the counter value to store the next filter value		    
		count_s += 1;
	    }
	  }
	}
    //End of the block of code used for storage of distinct filter values	  		  
    for (int batch = 0; batch < batches; ++batch) {
      for (int out_y = 0; out_y < output_height; ++out_y) {
        const int in_y_origin = out_y;
        for (int out_x = 0; out_x < output_width; ++out_x) {
          const int in_x_origin = out_x;
          for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
		int32_t acc = cfu_op0(/* funct7= */ 1, 0, 0); // resets acc
            for (int filter_y = 0; filter_y < 3; ++filter_y) {
              const int in_y = in_y_origin + filter_y;
              for (int filter_x = 0; filter_x < 3; ++filter_x) {
                const int in_x = in_x_origin + filter_x;

                // Zero padding by omitting the areas outside the image.
                const bool is_point_inside_image =
                    (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height);

                if (!is_point_inside_image) {
                  continue;
                }
		//innermost block - involves accessing input values and sending them to CFU		      		      
                int32_t input_val_1 = input_data[Offset(input_shape, batch, in_y,
                                                          in_x, 0)];
		//along with input values the filter_counter tracks the filter value corresponding to the input value for the CFU		      		      
                acc = cfu_op0(/* funct7= */ 0, /* in0= */ input_val_1, /* in1= */ filter_count_s);
		//Incrementing the filter_count unitl 108 and resetting after it
		//There are 108 filter values involved for this computation			      
                filter_count_s = (filter_count_s + 1)%108;
              }
            }
            if (bias_data) {
              acc += bias_data[out_channel];
            }
            acc = MultiplyByQuantizedMultiplier(
                acc, output_multiplier[out_channel], output_shift[out_channel]);
            acc += output_offset;
            acc = std::max(acc, output_activation_min);
            acc = std::min(acc, output_activation_max);
            output_data[Offset(output_shape, batch, out_y, out_x, out_channel)] =
                static_cast<int8_t>(acc);
          }
        }
      }
    }
  }  
}



// Fixed-point per-channel-quantization convolution reference kernel.
// 16-bit data and 8-bit filter
template <typename AccumScalar>
inline void ConvPerChannel(
    const ConvParams& params, const int32_t* output_multiplier,
    const int32_t* output_shift, const RuntimeShape& input_shape,
    const int16_t* input_data, const RuntimeShape& filter_shape,
    const int8_t* filter_data, const RuntimeShape& bias_shape,
    const AccumScalar* bias_data, const RuntimeShape& output_shape,
    int16_t* output_data) {
  // Get parameters.
  const int stride_width = params.stride_width;
  const int stride_height = params.stride_height;
  const int dilation_width_factor = params.dilation_width_factor;
  const int dilation_height_factor = params.dilation_height_factor;
  const int pad_width = params.padding_values.width;
  const int pad_height = params.padding_values.height;

  // Set min and max value of the output.
  const int32_t output_activation_min = params.quantized_activation_min;
  const int32_t output_activation_max = params.quantized_activation_max;

  // Consistency check.
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  TFLITE_DCHECK_EQ(input_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_EQ(filter_shape.DimensionsCount(), 4);
  TFLITE_DCHECK_EQ(output_shape.DimensionsCount(), 4);
  const int batches = MatchingDim(input_shape, 0, output_shape, 0);
  const int input_depth = MatchingDim(input_shape, 3, filter_shape, 3);
  const int output_depth = MatchingDim(filter_shape, 0, output_shape, 3);
  if (bias_data) {
    TFLITE_DCHECK_EQ(bias_shape.FlatSize(), output_depth);
  }

  // Check dimensions of the tensors.
  const int input_height = input_shape.Dims(1);
  const int input_width = input_shape.Dims(2);
  const int filter_height = filter_shape.Dims(1);
  const int filter_width = filter_shape.Dims(2);
  const int output_height = output_shape.Dims(1);
  const int output_width = output_shape.Dims(2);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      const int in_y_origin = (out_y * stride_height) - pad_height;
      for (int out_x = 0; out_x < output_width; ++out_x) {
        const int in_x_origin = (out_x * stride_width) - pad_width;
        for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
          AccumScalar acc = 0;
          for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
            const int in_y = in_y_origin + dilation_height_factor * filter_y;
            for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
              const int in_x = in_x_origin + dilation_width_factor * filter_x;

              // Zero padding by omitting the areas outside the image.
              const bool is_point_inside_image =
                  (in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                  (in_y < input_height);

              if (!is_point_inside_image) {
                continue;
              }

              for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                int32_t input_val = input_data[Offset(input_shape, batch, in_y,
                                                      in_x, in_channel)];
                int32_t filter_val = filter_data[Offset(
                    filter_shape, out_channel, filter_y, filter_x, in_channel)];
                // Accumulate with 64 bits accumulator.
                // int64_t += int8_t * int16_t so the highest value we can
                // get from each accumulation is [-127, 127] * ([-32768,
                // 32767] -
                // [-32768, 32767]), which is [-8322945, 8322945].
                // log2(8322945) = 22.99.
                acc += filter_val * input_val;
              }
            }
          }
          if (bias_data) {
            acc += bias_data[out_channel];
          }
          int32_t scaled_acc = MultiplyByQuantizedMultiplier(
              acc, output_multiplier[out_channel], output_shift[out_channel]);
          scaled_acc = std::max(scaled_acc, output_activation_min);
          scaled_acc = std::min(scaled_acc, output_activation_max);
          output_data[Offset(output_shape, batch, out_y, out_x, out_channel)] =
              static_cast<int16_t>(scaled_acc);
        }
      }
    }
  }
}

}  // namespace reference_integer_ops
}  // namespace tflite

#endif  // TENSORFLOW_LITE_KERNELS_INTERNAL_REFERENCE_INTEGER_OPS_CONV_H_
