# Copyright (c) 2024 tinyVision.ai Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""
Runner for the ecpprog programming tool for Lattice FPGAs.

https://github.com/gregdavill/ecpprog

This version with extra hooks before and after to reboot the board
which might be in "evaluation mode" and refuse to start until reset.

A license-related "protection feature" of some Lattice devices.
"""

from runners.core import BuildConfiguration, RunnerCaps, ZephyrBinaryRunner


class EcpprogHookBinaryRunner(ZephyrBinaryRunner):
    """Runner front-end for programming the FPGA flash at some offset."""

    def __init__(self, cfg, device=None, pre_cmd="true", post_cmd="true"):
        super().__init__(cfg)
        self.device = device
        self.pre_cmd = pre_cmd
        self.post_cmd = post_cmd

    @classmethod
    def name(cls):
        return "ecpprog_hook"

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={"flash"})

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument(
            "--device", dest="device", help="Device identifier such as i:<vid>:<pid>"
        )
        parser.add_argument(
            "--pre-cmd", dest="pre_cmd", help="Command to run as soon as flashing is done"
        )
        parser.add_argument(
            "--post-cmd", dest="post_cmd", help="Command to run as soon as flashing is done"
        )

    @classmethod
    def do_create(cls, cfg, args):
        return EcpprogHookBinaryRunner(cfg, device=args.device,
                                       pre_cmd=args.pre_cmd, post_cmd=args.post_cmd)

    def do_run(self, command, **kwargs):
        command = (self.pre_cmd,)
        self.logger.debug(" ".join(command))
        self.check_call(command)

        build_conf = BuildConfiguration(self.cfg.build_dir)
        load_offset = build_conf.get("CONFIG_FLASH_LOAD_OFFSET", 0)
        command = ("ecpprog", "-o", hex(load_offset), self.cfg.bin_file)
        self.logger.debug(" ".join(command))
        self.check_call(command)

        command = (self.post_cmd,)
        self.logger.debug(" ".join(command))
        self.check_call(command)
