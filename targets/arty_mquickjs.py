#!/usr/bin/env python3

#
# This file is part of mquickjs on LiteX.
#
# Copyright (c) 2026 EnjoyDigital
# SPDX-License-Identifier: BSD-2-Clause

from migen import Cat

from litex.build.parser import LiteXArgumentParser
from litex.soc.cores.gpio import GPIOIn
from litex.soc.integration.builder import Builder

from litex_boards.platforms import digilent_arty
from litex_boards.targets.digilent_arty import BaseSoC


class MQuickJSSoC(BaseSoC):
    def __init__(self, *args, with_buttons=True, with_switches=True, **kwargs):
        super().__init__(*args, with_buttons=with_buttons, **kwargs)

        if with_switches:
            self.switches = GPIOIn(
                pads     = Cat(*self.platform.request_all("user_sw")),
                with_irq = self.irq.enabled
            )


def main():
    parser = LiteXArgumentParser(platform=digilent_arty.Platform, description="mquickjs on LiteX Arty A7 SoC.")
    parser.add_target_argument("--flash",          action="store_true",       help="Flash bitstream.")
    parser.add_target_argument("--variant",        default="a7-35",           help="Board variant (a7-35 or a7-100).")
    parser.add_target_argument("--sys-clk-freq",   default=100e6, type=float, help="System clock frequency.")
    parser.add_target_argument("--with-xadc",      action="store_true",       help="Enable 7-Series XADC.")
    parser.add_target_argument("--with-dna",       action="store_true",       help="Enable 7-Series DNA.")
    parser.add_target_argument("--with-usb",       action="store_true",       help="Enable USB Host.")
    parser.add_target_argument("--with-ethernet",  action="store_true",       help="Enable Ethernet support.")
    parser.add_target_argument("--with-etherbone", action="store_true",       help="Enable Etherbone support.")
    parser.add_target_argument("--eth-ip",         default="192.168.1.50",    help="Ethernet/Etherbone IP address.")
    parser.add_target_argument("--remote-ip",      default="192.168.1.100",   help="Remote IP address of TFTP server.")
    parser.add_target_argument("--eth-dynamic-ip", action="store_true",       help="Enable dynamic Ethernet IP assignment.")
    parser.add_target_argument("--without-buttons", action="store_true",      help="Disable user button CSR.")
    parser.add_target_argument("--without-switches", action="store_true",     help="Disable user switch CSR.")

    sdopts = parser.target_group.add_mutually_exclusive_group()
    sdopts.add_argument("--with-spi-sdcard", action="store_true", help="Enable SPI-mode SDCard support.")
    sdopts.add_argument("--with-sdcard",     action="store_true", help="Enable SDCard support.")
    parser.add_target_argument("--sdcard-adapter", help="SDCard PMOD adapter (digilent or numato).")

    parser.add_target_argument("--with-spi-flash", action="store_true", help="Enable memory-mapped SPI flash.")
    parser.add_target_argument("--with-pmod-gpio", action="store_true", help="Enable GPIOs through PMOD.")
    parser.add_target_argument("--with-can",       action="store_true", help="Enable CAN support through CTU-CAN-FD.")
    args = parser.parse_args()

    assert not (args.with_etherbone and args.eth_dynamic_ip)

    soc = MQuickJSSoC(
        variant        = args.variant,
        toolchain      = args.toolchain,
        sys_clk_freq   = args.sys_clk_freq,
        with_xadc      = args.with_xadc,
        with_dna       = args.with_dna,
        with_ethernet  = args.with_ethernet,
        with_etherbone = args.with_etherbone,
        eth_ip         = args.eth_ip,
        remote_ip      = args.remote_ip,
        eth_dynamic_ip = args.eth_dynamic_ip,
        with_usb       = args.with_usb,
        with_spi_flash = args.with_spi_flash,
        with_buttons   = not args.without_buttons,
        with_switches  = not args.without_switches,
        with_pmod_gpio = args.with_pmod_gpio,
        with_can       = args.with_can,
        **parser.soc_argdict
    )

    if args.sdcard_adapter == "numato":
        soc.platform.add_extension(digilent_arty._numato_sdcard_pmod_io)
    else:
        soc.platform.add_extension(digilent_arty._sdcard_pmod_io)
    if args.with_spi_sdcard:
        soc.add_spi_sdcard()
    if args.with_sdcard:
        soc.add_sdcard()

    builder = Builder(soc, **parser.builder_argdict)
    if args.build:
        builder.build(**parser.toolchain_argdict)

    if args.load:
        prog = soc.platform.create_programmer()
        prog.load_bitstream(builder.get_bitstream_filename(mode="sram"))

    if args.flash:
        prog = soc.platform.create_programmer()
        prog.flash(0, builder.get_bitstream_filename(mode="flash"))


if __name__ == "__main__":
    main()
