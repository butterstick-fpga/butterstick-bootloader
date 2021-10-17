#!/usr/bin/env python3

# This file is Copyright (c) Greg Davill <greg.davill@gmail.com>
# License: BSD


# This variable defines all the external programs that this module
# relies on.  lxbuildenv reads this variable in order to ensure
# the build will finish without exiting due to missing third-party
# programs.
LX_DEPENDENCIES = ["riscv", "nextpnr-ecp5", "yosys"]

# Import lxbuildenv to integrate the deps/ directory
import lxbuildenv



import os
import shutil
import argparse
import subprocess


from migen import *
from migen.genlib.resetsync import AsyncResetSynchronizer


from litex.build.lattice.trellis import trellis_args, trellis_argdict

from litex.soc.integration.soc_core import *
from litex.soc.integration.builder import *

from litex.soc.interconnect.csr import *

from litex.soc.cores.clock import *
from litex.soc.cores.clock.common import period_ns
from litex.soc.cores.gpio import GPIOOut, GPIOIn
from litex.soc.cores.spi_flash import SpiFlashDualQuad

from rtl.platform import butterstick_r1d0
from rtl.eptri import LunaEpTriWrapper
from rtl.rgb import Leds


# CRG ---------------------------------------------------------------------------------------------

class CRG(Module):
    def __init__(self, platform, sys_clk_freq):
        self.clock_domains.cd_init     = ClockDomain()
        self.clock_domains.cd_por      = ClockDomain(reset_less=True)
        self.clock_domains.cd_sys      = ClockDomain()
        self.clock_domains.cd_usb      = ClockDomain()
        self.clock_domains.cd_sys2x    = ClockDomain()
        self.clock_domains.cd_sys2x_i  = ClockDomain()


        # # #

        self.stop = Signal()
        self.reset = Signal()

        
        # Use OSCG for generating por clocks.
        osc_g = Signal()
        self.specials += Instance("OSCG",
            p_DIV=7, # 38MHz
            o_OSC=osc_g
        )

        # Clk
        clk30 = platform.request("clk30")
        por_done  = Signal()
        platform.add_period_constraint(clk30, period_ns(30e6))

        sys2x_clk_ecsout = Signal()
        self.submodules.pll = pll = ECP5PLL()

        # Power on reset 10ms.
        por_count = Signal(24, reset=int(30e6 * 10e-3))
        self.comb += self.cd_por.clk.eq(osc_g)
        self.comb += por_done.eq(pll.locked & (por_count == 0))
        self.sync.por += If(~por_done, por_count.eq(por_count - 1))

        usb_por_done = Signal()
        usb_por_count = Signal(24, reset=int(60e6 * 10e-3))
        self.comb += usb_por_done.eq(usb_por_count == 0)
        self.comb += self.cd_usb.clk.eq(self.cd_sys.clk)
        self.comb += self.cd_usb.rst.eq(~usb_por_done)

        self.sync.init += If(~usb_por_done, usb_por_count.eq(usb_por_count - 1))


        # PLL
        pll.register_clkin(clk30, 30e6)
        pll.create_clkout(self.cd_sys2x_i, 2*sys_clk_freq, with_reset=False)
        pll.create_clkout(self.cd_init, 30e6, with_reset=False)
        self.specials += [
            Instance("ECLKBRIDGECS",
                i_CLK0   = self.cd_sys2x_i.clk,
                i_SEL    = 0,
                o_ECSOUT = sys2x_clk_ecsout),
            Instance("ECLKSYNCB",
                i_ECLKI = sys2x_clk_ecsout,
                i_STOP  = self.stop,
                o_ECLKO = self.cd_sys2x.clk),
            Instance("CLKDIVF",
                p_DIV     = "2.0",
                i_ALIGNWD = 0,
                i_CLKI    = self.cd_sys2x.clk,
                i_RST     = self.reset,
                o_CDIVX   = self.cd_sys.clk),
            AsyncResetSynchronizer(self.cd_init,  ~por_done | ~pll.locked),
            AsyncResetSynchronizer(self.cd_sys,   ~por_done | ~pll.locked | self.reset),
            AsyncResetSynchronizer(self.cd_sys2x, ~por_done | ~pll.locked | self.reset),
            AsyncResetSynchronizer(self.cd_sys2x_i, ~por_done | ~pll.locked | self.reset),
        ]

    


# BaseSoC ------------------------------------------------------------------------------------------

class BaseSoC(SoCCore):
    mem_map = {
        "rom":      0x00000000,  # (default shadow @0x80000000)
        "sram":     0x10000000,  # (default shadow @0xa0000000)
        "spiflash": 0x20000000,  # (default shadow @0xa0000000)
        "main_ram": 0x40000000,  # (default shadow @0xc0000000)
        "csr":      0xf0000000,  # (default shadow @0xe0000000)
        "usb":      0xf0010000,
    }
    mem_map.update(SoCCore.mem_map)

    interrupt_map = {
        "timer0": 0,
        "uart": 1,
    }
    interrupt_map.update(SoCCore.interrupt_map)

    def __init__(self, sys_clk_freq=int(60e6), toolchain="trellis", **kwargs):
        # Board Revision ---------------------------------------------------------------------------
        revision = kwargs.get("revision", "0.2")
        device = kwargs.get("device", "25F")

        platform = butterstick_r1d0.ButterStickPlatform()

        # Serial -----------------------------------------------------------------------------------
        platform.add_extension(butterstick_r1d0._uart_debug)

        # SoCCore ----------------------------------------------------------------------------------
        SoCCore.__init__(self, platform, clk_freq=sys_clk_freq, csr_data_width=32, integrated_rom_size=32*1024, integrated_sram_size=16*1024, uart_baudrate=1000000)        
        
        # CRG --------------------------------------------------------------------------------------
        self.submodules.crg = crg = CRG(platform, sys_clk_freq)

        # VCCIO Control ----------------------------------------------------------------------------
        vccio_pins = platform.request("vccio_ctrl")
        pwm_timer = Signal(14)
        self.sync += pwm_timer.eq(pwm_timer + 1)
        self.comb += [
            vccio_pins.pdm[0].eq(pwm_timer < int(2**14 * (0.13))),  # 3.3v
            vccio_pins.pdm[1].eq(pwm_timer < int(2**14 * (0.13))),  # 3.3v
            vccio_pins.pdm[2].eq(pwm_timer < int(2**14 * (0.70))),  # 1.8v
        ]
        counter = Signal(32)
        self.sync += [
            If(counter[16] == 0,
                counter.eq(counter + 1),
            ).Else(
                vccio_pins.en.eq(1),
            )
        ]

        # SPI Flash --------------------------------------------------------------------------------
        from litespi.modules import W25Q128JV
        from litespi.opcodes import SpiNorFlashOpCodes as Codes
        self.add_spi_flash(mode="4x", module=W25Q128JV(Codes.READ_1_1_4), with_master=True)


        # Leds -------------------------------------------------------------------------------------
        led = platform.request("led_rgb_multiplex")
        self.submodules.leds = Leds(led.a, led.c)
        self.add_csr("leds")

        # Self Reset -------------------------------------------------------------------------------
        rst = Signal()
        self.submodules.reset = GPIOOut(rst)
        self.comb += platform.request("rst_n").eq(~rst)

        # Buttons ----------------------------------------------------------------------------------
        self.submodules.button = GPIOIn(platform.request("user_btn"))

        # USB --------------------------------------------------------------------------------------
        self.submodules.usb = LunaEpTriWrapper(self.platform, base_addr=self.mem_map['usb'])
        self.add_memory_region("usb", self.mem_map['usb'], 0x10000, type="");
        self.add_wb_slave(self.mem_map['usb'], self.usb.bus)
        for name, irq in self.usb.irqs.items():
            name = 'usb_{}'.format(name)
            class DummyIRQ(Module):
                def __init__(self, irq):
                    class DummyEV(Module):
                        def __init__(self, irq):
                            self.irq = irq
                    self.submodules.ev = DummyEV(irq)

            setattr(self.submodules, name, DummyIRQ(irq))
            self.add_interrupt(name)


        #Add GIT repo to the firmware
        git_rev_cmd = subprocess.Popen("git describe --tags --first-parent --always".split(),
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
        (git_stdout, _) = git_rev_cmd.communicate()
        self.add_constant('CONFIG_REPO_GIT_DESC',git_stdout.decode('ascii').strip('\n'))

    # This function will build our software and create a oc-fw.init file that can be patched directly into blockram in the FPGA
    def PackageFirmware(self, builder):  
        self.finalize()

        os.makedirs(builder.output_dir, exist_ok=True)

        src_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "firmware"))
        builder.add_software_package("fw", src_dir)

        builder._prepare_rom_software()
        builder._generate_includes()
        builder._generate_rom_software(compile_bios=False)

        firmware_file = os.path.join(builder.output_dir, "software", "fw","oc-fw.bin")
        firmware_data = get_mem_data(firmware_file, self.cpu.endianness)
        self.initialize_rom(firmware_data)

        # lock out compiling firmware during build steps
        builder.compile_software = False


def CreateFirmwareInit(init, output_file):
    content = ""
    for d in init:
        content += "{:08x}\n".format(d)
    with open(output_file, "w") as o:
        o.write(content)    


# Build --------------------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="LiteX based Bootloader on ButterStick")
    builder_args(parser)
    trellis_args(parser)
    # parser.add_argument("--device", default="25F",
    #                     help="ECP5 device (default=25F)")
    # parser.add_argument("--sdram-device", default="MT41K64M16",
    #                     help="ECP5 device (default=MT41K64M16)")
    parser.add_argument(
        "--update-firmware", default=False, action='store_true',
        help="compile firmware and update existing gateware"
    )
    args = parser.parse_args()

    soc = BaseSoC(**argdict(args))
    builder = Builder(soc, **builder_argdict(args))
    


    # Build firmware
    soc.PackageFirmware(builder)
    #generate_docs(soc, "build/documentation/", project_name="OrangeCrab Test SoC", author="Greg Davill")
        
    # Check if we have the correct files
    firmware_file = os.path.join(builder.output_dir, "software", "fw", "oc-fw.bin")
    firmware_data = get_mem_data(firmware_file, soc.cpu.endianness)
    firmware_init = os.path.join(builder.output_dir, "software", "fw", "oc-fw.init")
    CreateFirmwareInit(firmware_data, firmware_init)
    
    rand_rom = os.path.join(builder.output_dir, "gateware", "rand.data")

    

    # If we don't have a random file, create one, and recompile gateware
    if (os.path.exists(rand_rom) == False) or (args.update_firmware == False):
        os.makedirs(os.path.join(builder.output_dir,'gateware'), exist_ok=True)
        os.makedirs(os.path.join(builder.output_dir,'software'), exist_ok=True)

        os.system(f"ecpbram  --generate {rand_rom} --seed {0} --width {32} --depth {soc.integrated_rom_size // 4}")

        # patch random file into BRAM
        data = []
        with open(rand_rom, 'r') as inp:
            for d in inp.readlines():
                data += [int(d, 16)]
        soc.initialize_rom(data)

        # Build gateware
        builder_kargs = trellis_argdict(args)
        vns = builder.build(**builder_kargs)
        soc.do_exit(vns)   
    
    from litex.soc.doc import generate_docs
    generate_docs(soc, "build/docs/")
    os.system("sphinx-build -M html build/docs/ build/docs/_build")

    input_config = os.path.join(builder.output_dir, "gateware", f"{soc.platform.name}.config")
    output_config = os.path.join(builder.output_dir, "gateware", f"{soc.platform.name}_patched.config")

    # Insert Firmware into Gateware
    os.system(f"ecpbram  --input {input_config} --output {output_config} --from {rand_rom} --to {firmware_init}")


    # create compressed config (ECP5 specific)
    output_bitstream = os.path.join(builder.gateware_dir, f"{soc.platform.name}.bit")
    #os.system(f"ecppack --freq 38.8 --spimode qspi --compress --input {output_config} --bit {output_bitstream}")
    os.system(f"ecppack --freq 38.8 --bootaddr 0x200000 --compress --input {output_config} --bit {output_bitstream}")

    dfu_file = os.path.join(builder.gateware_dir, f"{soc.platform.name}.dfu")
    shutil.copyfile(output_bitstream, dfu_file)
    os.system(f"dfu-suffix -v 1209 -p 5bf0 -a {dfu_file}")


def argdict(args):
    r = soc_core_argdict(args)
    for a in ["device", "revision", "sdram_device"]:
        arg = getattr(args, a, None)
        if arg is not None:
            r[a] = arg
    return r

if __name__ == "__main__":
    main()
