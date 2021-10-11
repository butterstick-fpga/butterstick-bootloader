#
# This file is part of LiteX.
#
# Copyright (c) 2021 
# SPDX-License-Identifier: BSD-2-Clause

from migen import *
from migen.genlib.misc import WaitTimer

from litex.soc.interconnect.csr import *

# Led Chaser ---------------------------------------------------------------------------------------

_CHASER_MODE  = 0
_CONTROL_MODE = 1

class PDM(Module):
    def __init__(self, width=8):
        self.level = level = Signal(width)
        self.out = out = Signal(1)

        # Gamma correction
        sigma = Signal(width+1)

        self.comb += out.eq(sigma[width])
        self.sync += sigma.eq(sigma + Cat(level, out, out))


class Leds(Module, AutoCSR):
    def __init__(self, anode, cathode):
        #self.pads = pads
        
        count = Signal(3, reset=1)
        prescale = Signal(max=10000)

        self.sync += [
            If(prescale == 0,
                count.eq(Cat(count[1:],count[0])),
                prescale.eq(10000),
            ),
            prescale.eq(prescale - 1),
        ]

        for n in range(7):
            _csr,_pdm = CSRStorage(32, name="out{}".format(n)), PDM(10)
            self.submodules += _pdm
            setattr(self, "_out{}".format(n), _csr)
            self.comb += [
                anode[n].eq(_pdm.out),
            ]
            self.sync += [
                If(count[0], _pdm.level.eq(_csr.storage[0:10])),
                If(count[1], _pdm.level.eq(_csr.storage[10:20])),
                If(count[2], _pdm.level.eq(_csr.storage[20:30])),
            ]

        self.comb += [
            cathode.eq(count)
        ]
