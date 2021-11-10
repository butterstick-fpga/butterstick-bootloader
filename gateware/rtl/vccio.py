# Copyright (c) 2021 Gregory Davill <greg.davill@gmail.com> 
# SPDX-License-Identifier: BSD-2-Clause

from migen import *
from migen.genlib.misc import WaitTimer

from litex.soc.interconnect.csr import *

class PDM(Module):
    def __init__(self, width=8):
        self.level = level = Signal(width)
        self.out = out = Signal(1)
        sigma = Signal(width+2)

        self.comb += out.eq(sigma[width+1])
        self.sync += sigma.eq(sigma + Cat(level, out, out))

# VccIo module ---------------------------------------------------------------------------------------

class VccIo(Module, AutoCSR):
    def __init__(self, vccio_pins):
        for i, p in enumerate(vccio_pins.pdm):
            pdm = PDM(16)
            csr = CSRStorage(16, name='ch{0}'.format(i))

            setattr(self, csr.name, csr)
            self.submodules += pdm

            self.comb += [
                pdm.level.eq(csr.storage),
                p[0].eq(pdm.out),
            ]

        self._vccio_en = CSRStorage(1, name='enable')
        counter = Signal(32)

        self.sync += [
            If(counter[10] == 0,
                counter.eq(counter + 1),
            ).Else(
                vccio_pins.en.eq(self._vccio_en.storage),
            )
        ]
