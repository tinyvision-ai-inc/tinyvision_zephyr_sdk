#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

static const struct device *const source_dev_0 = DEVICE_DT_GET(DT_NODELABEL(debayer0));
static const struct device *const sensor_dev_0 = DEVICE_DT_GET(DT_NODELABEL(imx219ch0));
static const struct device *const source_dev_1 = DEVICE_DT_GET(DT_NODELABEL(debayer1));
static const struct device *const sensor_dev_1 = DEVICE_DT_GET(DT_NODELABEL(imx219ch0));

void app_stats_convert(struct video_stats *stats_in, uint16_t stats_wanted)
{
	struct video_channel_stats *stats = (void *)stats_in;

	/* Average the RGB channels into the Y (luma) channel */
	if ((stats_wanted & VIDEO_STATS_CHANNELS_Y) &&
	    (stats_in->type_flags & VIDEO_STATS_CHANNELS_Y) == 0 &&
	    (stats_in->type_flags & VIDEO_STATS_CHANNELS_RGB)) {
		stats->ch0 = (stats->ch1 + stats->ch2 + stats->ch3) / 3;
		stats->base.type_flags |= VIDEO_STATS_CHANNELS_Y;
	}

	/* Convert from YUV to RGB */
	if ((stats_wanted & VIDEO_STATS_CHANNELS_YUV) &&
	    (stats_in->type_flags & VIDEO_STATS_CHANNELS_RGB) == 0 &&
	    (stats_in->type_flags & VIDEO_STATS_CHANNELS_YUV)) {
		/* TODO implementation of a library, used inline here */
	}
}

int app_ipa_aec(const struct device *dev, struct video_stats *stats_in)
{
	struct video_channel_stats *stats = (void *)stats_in;
	uint32_t goal_y = 160;
	int exposure_min = 0;
	int exposure_max = 8000;
	int exposure_prev = 0;
	int exposure_next = 0;
	int ret;

	app_stats_convert(stats_in, VIDEO_STATS_CHANNELS_Y);

	if ((stats_in->type_flags & VIDEO_STATS_CHANNELS_Y) == 0) {
		LOG_WRN("Missing Y channel average, cannot compute IPA");
		return -ENOTSUP;
	}

	ret = video_get_ctrl(dev, VIDEO_CID_EXPOSURE, (void *)&exposure_prev);
	if (ret < 0) {
		LOG_ERR("Cannot get the current control value from %s", dev->name);
		return ret;
	}

	/* Apply the correction. */
	if (stats->ch0 < goal_y * 90 / 100) {
		LOG_INF("Under-exposure at CH0 %d, increasing exposure %d of %s",
			stats->ch0, exposure_prev, dev->name);
		exposure_next = exposure_prev * 103 / 100 + 1;
	} else if (stats->ch0 > goal_y * 120 / 100) {
		LOG_INF("Over-exposure at CH0 %d, decreasing exposure %d of %s",
			stats->ch0, exposure_prev, dev->name);
		exposure_next = exposure_prev * 98 / 100;
	} else {
		exposure_next = exposure_prev;
	}

	exposure_next = CLAMP(exposure_next, exposure_min, exposure_max);

	LOG_DBG("CH0 (Y) is %u, updating from %d to %d", stats->ch0, exposure_prev, exposure_next);

	if (exposure_prev == exposure_next) {
		LOG_DBG("Previous and new values are the same (%d), not updating", exposure_prev);
		return 0;
	}

	video_set_ctrl(dev, VIDEO_CID_EXPOSURE, (void *)exposure_next);
	if (ret < 0) {
		LOG_ERR("Cannot set the new control value to %d for %s", exposure_next, dev->name);
		return ret;
	}

	LOG_DBG("Updated ISP to new value %d, work is done", exposure_next);
	return 0;
}

int main(void)
{
	struct video_channel_stats stats = {0};
	struct video_stats *stats_out = (void *)&stats;
	int ret;

	ret = video_set_ctrl(sensor_dev_0, VIDEO_CID_GAIN, (void *)555);
	if (ret < 0) {
		LOG_ERR("Cannot set the default gain for %s", sensor_dev_0->name);
		return ret;
	}

	while (true) {
		video_get_stats(source_dev_0, VIDEO_EP_OUT, VIDEO_STATS_ASK_CHANNELS, stats_out);
		app_ipa_aec(sensor_dev_0, stats_out);

		k_sleep(K_MSEC(10));
	}

	return 0;
}
