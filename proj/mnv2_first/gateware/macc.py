#!/bin/env python
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from nmigen import Signal, signed

from nmigen_cfu import all_words, Sequencer, SimpleElaboratable, tree_sum

from .post_process import PostProcessor
from .registerfile import Xetter


class Madd4Pipeline(SimpleElaboratable):
    """A 4-wide Multiply Add pipeline.

    Pipeline takes 2 additional cycles.

    f_data and i_data each contain 4 signed 8 bit values. The
    calculation performed is:

    result = sum((i_data[n] + offset) * f_data[n] for n in range(4))

    Public Interface
    ----------------
    offset: Signal(signed(8)) input
        Offset to be added to all inputs.
    f_data: Signal(32) input
        4 bytes of filter data to use next
    i_data: Signal(32) input
        4 bytes of input data data to use next
    result: Signal(signed(32)) output
        Result of the multiply and add
    """
    PIPELINE_CYCLES = 2

    def __init__(self):
        super().__init__()
        self.offset = Signal(signed(9))
        self.f_data = Signal(32)
        self.i_data = Signal(32)
        self.result = Signal(signed(32))

    def elab(self, m):
        # Product is 17 bits: 8 bits * 9 bits = 17 bits
        products = [Signal(signed(17), name=f"product_{n}") for n in range(4)]
        for i_val, f_val, product in zip(
                all_words(self.i_data, 8), all_words(self.f_data, 8), products):
            f_tmp = Signal(signed(9))
            m.d.sync += f_tmp.eq(f_val.as_signed())
            i_tmp = Signal(signed(9))
            m.d.sync += i_tmp.eq(i_val.as_signed() + self.offset)
            m.d.comb += product.eq(i_tmp * f_tmp)

        m.d.sync += self.result.eq(tree_sum(products))


class Accumulator(SimpleElaboratable):
    """An accumulator for a Madd4Pipline

    Public Interface
    ----------------
    add_en: Signal() input
        When to add the input
    in_value: Signal(signed(32)) input
        The input data to add
    clear: Signal() input
        Zero accumulator.
    result: Signal(signed(32)) output
        Result of the multiply and add
    """

    def __init__(self):
        super().__init__()
        self.add_en = Signal()
        self.in_value = Signal(signed(32))
        self.clear = Signal()
        self.result = Signal(signed(32))

    def elab(self, m):
        accumulator = Signal(signed(32))
        m.d.comb += self.result.eq(accumulator)
        with m.If(self.add_en):
            m.d.sync += accumulator.eq(accumulator + self.in_value)
            m.d.comb += self.result.eq(accumulator + self.in_value)
        # clear always resets accumulator next cycle, even if add_en is high
        with m.If(self.clear):
            m.d.sync += accumulator.eq(0)


class Macc4Run1(Xetter):
    """Sequences a Madd4 to accumulate 1 input channel's worth of data.

    Parameters
    ----------------
    max_input_depth:
        Maximum number of input words (max input_depth)

    Public Interface
    ----------------
    input_depth: Signal(range(max_input_depth)) input
        Number of words in input

    madd4_start: Signal(1) output
        Notification that madd4 inputs have been read.
    madd4_inputs_ready: Signal() input
        Whether or not inputs for the madd4 are ready.

    acc_add_en: Signal() output
        Add to accumulator
    acc_clear: Signal() output
        Clear accumulator

    pp_start: Signal(1) output
        Notification that PP inputs have been read
    pp_accumulator: Signal(signed(32)) output
        Input to post processor. i.e the accumulated value.
    pp_result: Signal(signed(32))
        The post processed accumulator value - result of the post processor.
    """

    def __init__(self, max_input_depth):
        super().__init__()
        self.max_input_depth = max_input_depth
        self.input_depth = Signal(range(max_input_depth))
        self.madd4_start = Signal()
        self.madd4_inputs_ready = Signal()
        self.madd4_result = Signal(signed(32))
        self.pp_start = Signal()
        self.pp_accumulator = Signal(signed(32))
        self.pp_result = Signal(signed(32))

    def elab(self, m):
        m.submodules['madd_seq'] = madd4_seq = Sequencer(
            Madd4Pipeline.PIPELINE_CYCLES)
        m.submodules['pp_seq'] = pp_seq = Sequencer(
            PostProcessor.PIPELINE_CYCLES)

        started_madd4s = Signal(10)
        starting_last = Signal()
        retired_madd4s = Signal(10)
        retiring_last = Signal()
        accumulator = Signal(signed(32))

        input_depth_minus_1 = Signal.like(self.input_depth)
        m.d.sync += input_depth_minus_1.eq(self.input_depth - 1)

        # Puts 1 word of data into the pipeline, if it's ready
        def start_madd4():
            with m.If(self.madd4_inputs_ready):
                m.d.sync += started_madd4s.eq(started_madd4s + 1)
                m.d.comb += [
                    madd4_seq.inp.eq(1),
                    self.madd4_start.eq(1),
                    starting_last.eq(started_madd4s == input_depth_minus_1)
                ]

        def retire_madd4():
            with m.If(madd4_seq.sequence[-1]):
                m.d.sync += [
                    retired_madd4s.eq(retired_madd4s + 1),
                    accumulator.eq(accumulator + self.madd4_result),
                ]
                m.d.comb += [
                    retiring_last.eq(retired_madd4s == input_depth_minus_1)
                ]

        with m.FSM():
            with m.State("PREPARE"):
                m.d.sync += [
                    accumulator.eq(0),
                    started_madd4s.eq(0),
                    retired_madd4s.eq(0),
                ]
                m.next = "READY"
            with m.State("READY"):
                with m.If(self.start):
                    start_madd4()
                    m.next = "RUN"
            with m.State("RUN"):
                start_madd4()
                retire_madd4()
                with m.If(starting_last):
                    m.next = "WAIT_ACCUMULATE"
            with m.State("WAIT_ACCUMULATE"):
                retire_madd4()
                with m.If(retiring_last):
                    m.d.comb += [
                        self.pp_start.eq(1),
                        pp_seq.inp.eq(1),
                        self.pp_accumulator.eq(
                            accumulator + self.madd4_result),
                    ]
                    m.d.sync += [
                        accumulator.eq(0),
                        started_madd4s.eq(0),
                        retired_madd4s.eq(0),
                    ]
                    m.next = "WAIT_POST_PROCESS"
            with m.State("WAIT_POST_PROCESS"):
                with m.If(pp_seq.sequence[-1]):
                    m.d.comb += [
                        self.done.eq(1),
                        self.output.eq(self.pp_result),
                    ]
                    m.next = "READY"
