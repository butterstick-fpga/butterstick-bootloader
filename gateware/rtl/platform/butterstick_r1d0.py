# This file is Copyright (c) Greg Davill <greg.davill@gmail.com>
# License: BSD

from litex.build.generic_platform import *
from litex.build.lattice import LatticePlatform

_butterstick_r1d0_io = [
    
    ("clk30", 0,   Pins("B12"), IOStandard("LVCMOS18")),
    ("rst_n", 0,   Pins("R3"), IOStandard("LVCMOS33"), Misc("OPENDRAIN=ON")),

    ("user_btn", 0, Pins("U16"), IOStandard("SSTL135_I")),
    ("user_btn", 1, Pins("T17"), IOStandard("SSTL135_I")),
    
    ("led_rgb_multiplex", 0, 
        Subsignal("a", Pins("C13 D12 U2 T3 D13 E13 C16")),
        Subsignal("c", Pins("T1 R1 U1")),
        IOStandard("LVCMOS33")
    ),

    ("ulpi", 0,
        Subsignal("data",  Pins("B9 C6 A7 E9 A8 D9 C10 C7")),
        Subsignal("clk",   Pins("B6")),
        Subsignal("dir",   Pins("A6")),
        Subsignal("nxt",   Pins("B8")),
        Subsignal("stp",   Pins("C8")),
        Subsignal("rst",   Pins("C9")),
        IOStandard("LVCMOS18"),Misc("SLEWRATE=FAST")
    ),   

    ("eth_clk", 0,
        Subsignal("tx", Pins("E15")),
        Subsignal("rx", Pins("D11")),
        IOStandard("LVCMOS33"),Misc("SLEWRATE=FAST"),
    ),

    ("eth", 0,
        Subsignal("rst_n",   Pins("B20")),
        Subsignal("mdio",    Pins("D16")),
        Subsignal("mdc",     Pins("A19")),
        Subsignal("rx_data", Pins("A16 C17 B17 A17")),
        Subsignal("tx_ctl",  Pins("D15")),
        Subsignal("rx_ctl",  Pins("B18")),
        Subsignal("tx_data", Pins("C15 B16 A18 B19")),
        IOStandard("LVCMOS33"),Misc("SLEWRATE=FAST")
    ),   


    ("ddram", 0,
        Subsignal("a", Pins(
            "G16 E19 E20 F16 F19 E16 F17 L20 "
            "M20 E18 G18 D18 H18 C18 D17 G20 "),
            IOStandard("SSTL135_I")),
        Subsignal("ba",    Pins("H16 F20 H20"), IOStandard("SSTL135_I"),),
        Subsignal("ras_n", Pins("K18"), IOStandard("SSTL135_I")),
        Subsignal("cas_n", Pins("J17"), IOStandard("SSTL135_I")),
        Subsignal("we_n",  Pins("G19"), IOStandard("SSTL135_I")),
        Subsignal("cs_n",  Pins("J20 J16"), IOStandard("SSTL135_I")),
        Subsignal("dm", Pins("U20 L18"), IOStandard("SSTL135_I")),
        Subsignal("dq", Pins(
            "U19 T18 U18 R20 P18 P19 P20 N20 ",
            "L19 L17 L16 R16 N18 R17 N17 P17 "),
            IOStandard("SSTL135_I"),
            Misc("TERMINATION=OFF")),
        Subsignal("dqs_p", Pins("T19 N16"), IOStandard("SSTL135D_I"),
            Misc("TERMINATION=OFF"),
            Misc("DIFFRESISTOR=100")),
        Subsignal("clk_p", Pins("C20 J19"), IOStandard("SSTL135D_I")),
        Subsignal("cke",   Pins("F18 J18"), IOStandard("SSTL135_I")),
        Subsignal("odt",   Pins("K20 H17"), IOStandard("SSTL135_I")),
        Subsignal("reset_n", Pins("E17"), IOStandard("SSTL135_I")),
        Subsignal("vccio", Pins("K16 K17 M19 M18 N19 T20"), IOStandard("SSTL135_II")), # Virtual VCCIO pins
        Misc("SLEWRATE=FAST")
    ),

    ("vccio_ctrl", 0, 
        Subsignal("pdm", Pins("V1 E11 T2")),
        Subsignal("en", Pins("E12"))
    ),

    ("spiflash", 0, # Clock needs to be accessed through USRMCLK
        Subsignal("cs_n", Pins("R2")),
        Subsignal("mosi", Pins("W2")),
        Subsignal("miso", Pins("V2")),
        Subsignal("wp",   Pins("Y2")),
        Subsignal("hold", Pins("W1")),
        IOStandard("LVCMOS33")
    ),

    ("spiflash4x", 0, # Clock needs to be accessed through USRMCLK
        Subsignal("cs_n", Pins("R2")),
        Subsignal("dq",   Pins("W2 V2 Y2 W1")),
        IOStandard("LVCMOS33")
    ),
]

_connectors = [
    ("SYZYGY1", {
        # single ended
        
        # diff pairs
        "D0P":"E4", "D0N":"D5",
        "D1P":"A4", "D1N":"A5",
        "D2P":"C4", "D2N":"B4",
        "D3P":"B2", "D3N":"C2",
        "D4P":"A2", "D4N":"B1",
        "D5P":"C1", "D5N":"D1",
        "D6P":"F4", "D6N":"E3",
        "D7P":"D2", "D7N":"E1",
        }
    ),
    ("SYZYGY0", {
        # single ended
        "S0":"G2",  "S1":"J3",
        "S2":"F1",  "S3":"K3",
        "S4":"J4",  "S5":"K2",
        "S6":"J5",  "S7":"J1",
        "S8":"N2",  "S9":"L3",
        "S10":"M1", "S11":"L2",
        "S12":"N3", "S13":"N4",
        "S14":"M3", "S15":"P5",
        "S16":"H1", "S17":"K5",
        "S18":"K4", "S19":"K1",
        "S20":"L4", "S21":"L1",
        "S22":"L5", "S23":"M4",
        "S24":"N1", "S25":"N5",
        "S26":"P3", "S27":"P4",
        "S28":"H2", "S29":"P1",
        "S30":"G1", "S31":"P2",
        # diff pairs

        }
    ),
]

_uart_debug = [
    ("serial",0,
        Subsignal("tx", Pins("SYZYGY1:D0N"), IOStandard("LVCMOS33")),
        Subsignal("rx", Pins("SYZYGY1:D0P"), IOStandard("LVCMOS33")),
    ),
]

_i2c =[
    ("i2c",0,
        Subsignal("sda", Pins("C14"), IOStandard("LVCMOS33")),
        Subsignal("scl", Pins("E14"), IOStandard("LVCMOS33")),
    )
]

class ButterStickPlatform(LatticePlatform):
    default_clk_name = "clk_sys"
    default_clk_period = 1e9 / 30e6

    def __init__(self, **kwargs):
        LatticePlatform.__init__(self, "LFE5UM5G-85F-8BG381C", _butterstick_r1d0_io, _connectors, toolchain="trellis", **kwargs)
        self.toolchain.build_template[2] += ' --compress'
        self.toolchain.build_template[1] += ' --log {build_name}.tim'
       
    def do_finalize(self, fragment):
        LatticePlatform.do_finalize(self, fragment)
        
