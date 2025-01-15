# SPDX-License-Identifier: Apache-2.0

board_set_flasher_ifnset(ecpprog_hook)

board_finalize_runner_args(ecpprog_hook
  "--pre-cmd=scripts/reboot.sh"
  "--post-cmd=scripts/ecpprog_post_cmd.sh")
