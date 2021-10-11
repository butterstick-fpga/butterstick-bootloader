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
        sigma = Signal(width+1)

        self.comb += out.eq(sigma[width])
        self.sync += sigma.eq(sigma + Cat(level, out, out))

class Leds(Module, AutoCSR):
    def __init__(self, anode, cathode):
        
        count = Signal(3, reset=1)
        prescale = Signal(max=10000)
        blanking_duration = Signal(8)

        self.sync += [
            If(prescale == 0,
                count.eq(Cat(count[1:],count[0])),
                prescale.eq(10000),
            ),
            prescale.eq(prescale - 1),

            If(prescale == 128,
                blanking_duration.eq(255)
            ),
            If(blanking_duration != 0,
                blanking_duration.eq(blanking_duration - 1)
            )
        ]

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
            ]

        self.comb += [
            cathode.eq(count)
        ]
