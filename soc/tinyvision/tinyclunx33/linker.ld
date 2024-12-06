/*
 * Copyright (c) 2024 tinyVision.ai Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/linker/linker-tool.h>
#include <zephyr/devicetree.h>
#include <zephyr/linker/devicetree_regions.h>

MEMORY
{
	LINKER_DT_REGIONS()
}

SECTIONS
{
	SECTION_PROLOGUE(DWC3_DMA,,)
	{
		/* All symbols with this section are going to a memory region
		 * accessible to the DWC3 core performing DMA requests */

		. = ALIGN(16);
		/* Heuristics to match all buffers sent to the USB device controller */
		*(.bss.*_ep_pool);
		*(.noinit.*_ep_pool);
		*(*.kheap_buf_video_buffer_pool);

		/* Buffers explicitly marked to DWC3 DMA memory */
		*(.bss.dwc3_dma_*);

		/* UVC class */
		*(.bss.uvc_payload_header_*);
	} GROUP_LINK_IN(DWC3_DMA)
}

#include <zephyr/arch/riscv/common/linker.ld>
