# This file is Copyright (c) 2021 Greg Davill <greg.davill@gmail.com>
# License: BSD


from migen import *

from litex.soc.interconnect import wishbone

from litex.build.io import DDROutput
from rtl.nmigen_rtl.eptri import LunaEpTri


from litex.soc.interconnect.csr_eventmanager import *

class LunaEpTriWrapper(Module):

    def __init__(self, platform):
        self.platform = platform
        
        ulpi_pads = platform.request('ulpi')
        ulpi_data = TSTriple(8)
        reset = Signal()
        self.comb += ulpi_pads.rst.eq(~ResetSignal("usb"))

        self.specials += DDROutput(~reset ,0, ulpi_pads.clk, ClockSignal("usb"))
        self.specials += ulpi_data.get_tristate(ulpi_pads.data)
        
        self.wrapper("LunaEpTri", LunaEpTri())

        self.params = dict(
            # Clock / Reset
            i_usb_clk   = ClockSignal("usb"),
            #o_usb_rst   = ResetSignal("usb"),
            i_clk   = ClockSignal("sys"),
            i_rst   = ResetSignal("sys"),

            o_ulpi__data__o = ulpi_data.o,
            o_ulpi__data__oe = ulpi_data.oe,
            i_ulpi__data__i = ulpi_data.i,
            #o_ulpi__clk__o = clk,
            o_ulpi__stp = ulpi_pads.stp,
            i_ulpi__nxt__i = ulpi_pads.nxt,
            i_ulpi__dir__i = ulpi_pads.dir,
            o_ulpi__rst = reset,
        )

        self.bus = bus = wishbone.Interface()

        self.params.update( 
            i__bus__adr = bus.adr[:24],
            i__bus__stb = bus.stb,
            i__bus__cyc = bus.cyc,
            i__bus__we = bus.we,
            i__bus__sel = bus.sel,
            i__bus__dat_w = bus.dat_w,
            o__bus__dat_r = bus.dat_r,
            o__bus__ack = bus.ack,
        )


        self.irqs = irqs = {}
        irqs['device_controller'] = Signal()
        irqs['setup'] = Signal()
        irqs['in_ep'] = Signal()
        irqs['out_ep'] = Signal()

        self.params.update( 
            o_usb_device_controller_ev_irq = irqs['device_controller'],
            o_usb_setup_ev_irq = irqs['setup'],
            o_usb_in_ep_ev_irq = irqs['in_ep'],
            o_usb_out_ep_ev_irq = irqs['out_ep'],
        )


        self.specials += Instance("LunaEpTri",
            **self.params
        )

    def finalize(self):
        import os
        verilog_file = f"wrapper_{self.verilog_name}.v"
    
        vdir = os.path.join(os.getcwd(), "build", self.platform.name, "gateware")
        os.makedirs(vdir, exist_ok=True)

        verilog_file = os.path.join(vdir, verilog_file)
        with open(verilog_file, "w") as f:
            f.write(self.verilog)

        self.platform.add_source(verilog_file)






    ## Wrapper 
    def wrapper(self, name, elaboratable):
        from nmigen import Record, Signal
        from nmigen.back import verilog

        ports = []

        # Patch through all Records/Ports
        for port_name, port in vars(elaboratable).items():
            if not port_name.startswith("_") and isinstance(port, (Signal, Record)):
                ports += port._lhs_signals()

        self.verilog = verilog.convert(elaboratable, name=name, ports=ports, strip_internal_attrs=False)
        self.verilog_name = name
        # verilog_file = f"build/wrapper_{name}.v"
        
        # vdir = os.path.join(os.getcwd(), "build", platform.name)
        # os.makedirs(vdir, exist_ok=True)

        # with open(verilog_file, "w") as f:
        #     f.write(verilog_text)

        # platform.add_source(os.path.join(vdir, f"wrapper_{name}.v"))

