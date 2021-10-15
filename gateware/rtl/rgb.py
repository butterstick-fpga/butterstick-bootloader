# Copyright (c) 2021 Gregory Davill <greg.davill@gmail.com> 
# SPDX-License-Identifier: BSD-2-Clause

from migen import *
from migen.genlib.misc import WaitTimer

from litex.soc.interconnect.csr import *

# Led Multiplex module ---------------------------------------------------------------------------------------

class PDM(Module):
    def __init__(self, width=8):
        self.level = level = Signal(width)
        self.out = out = Signal(1)
        sigma = Signal(width+2)

        self.comb += out.eq(sigma[width+1])
        self.sync += sigma.eq(sigma + Cat(level, out, out))

class Leds(Module, AutoCSR):
    def __init__(self, anode, cathode):
        
        count = Signal(3, reset=1)
        prescale = Signal(max=300)
        blanking_duration = Signal(max=63)

        self.sync += [
            If(prescale == 0,
                count.eq(Cat(count[1:],count[0])),
                prescale.eq(300),
            ),
            prescale.eq(prescale - 1),

            If(prescale == 32,
                blanking_duration.eq(63)
            ),
            If(blanking_duration != 0,
                blanking_duration.eq(blanking_duration - 1)
            )
        ]

        enable = Signal()
        for n in range(7):
            _csr,_pdm = CSRStorage(32, name="out{}".format(n)), ResetInserter()(PDM(10))
            self.submodules += _pdm
            setattr(self, "_out{}".format(n), _csr)
            self.comb += [
                anode[n].eq(_pdm.out),
                _pdm.reset.eq(blanking_duration != 0),
            ]
            self.sync += [
                If(count[0], _pdm.level.eq(_csr.storage[0:10])),
                If(count[1], _pdm.level.eq(_csr.storage[10:20])),
                If(count[2], _pdm.level.eq(_csr.storage[20:30])),
                
                If(_pdm.level,
                    enable.eq(enable | 1)
                ),
                If(_pdm.reset,
                    enable.eq(0)
                )
            ]

        self.comb += [
            If(enable,
                cathode.eq(count)
            )
        ]

## test
import unittest

class TestPDM(unittest.TestCase):
    
    def test_PDM_0(self):
        def generator(dut):
            yield from dut.led._out0.write(10)

            for _ in range(5000):
                yield


        class DUT(Module):
            def __init__(self):
                self.anode = Signal(7)
                self.cathode = Signal(3)
                self.submodules.led = Leds(self.anode, self.cathode)


                
        dut = DUT()
        run_simulation(dut, generator(dut), vcd_name='test.vcd')
