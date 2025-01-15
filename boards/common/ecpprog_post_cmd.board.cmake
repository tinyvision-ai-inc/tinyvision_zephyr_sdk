# SPDX-License-Identifier: Apache-2.0

board_set_flasher_ifnset(ecpprog_hook)

board_finalize_runner_args(ecpprog_hook
  "--pre-cmd=${ZEPHYR_TINYVISION_ZEPHYR_SDK_MODULE_DIR}/scripts/ecpprog_hook_pre_cmd.sh"
  "--post-cmd=${ZEPHYR_TINYVISION_ZEPHYR_SDK_MODULE_DIR}/scripts/ecpprog_hook_post_cmd.sh")
