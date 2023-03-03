#include <obs-module.h>
#include <obs-source.h>
#include <obs.h>
#include <util/platform.h>
#include <media-playback/media.h>
#include <obs-frontend-api.h>
#include <curl/curl.h>
#include <windows.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include <src/defs.h>
#include <src/video-player/video-player.h>

void rfp_restart_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			   bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	struct reactive_facecam_plugin *plugin = data;
	if (obs_source_showing(plugin->context))
		obs_source_media_restart(plugin->context);
}

bool rfp_play_hotkey(void *data, obs_hotkey_pair_id id,
				      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return false;

	struct reactive_facecam_plugin *plugin = data;

	if (plugin->state == OBS_MEDIA_STATE_PLAYING ||
	    !obs_source_showing(plugin->context))
		return false;

	obs_source_media_play_pause(plugin->context, false);
	return true;
}

bool rfp_pause_hotkey(void *data, obs_hotkey_pair_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return false;

	struct reactive_facecam_plugin *plugin = data;

	if (plugin->state != OBS_MEDIA_STATE_PLAYING ||
	    !obs_source_showing(plugin->context))
		return false;

	obs_source_media_play_pause(plugin->context, true);
	return true;
}

void rfp_stop_hotkey(void *data, obs_hotkey_id id,
				      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	struct reactive_facecam_plugin *plugin = data;

	if (obs_source_showing(plugin->context))
		obs_source_media_stop(plugin->context);
}

static void get_frame(void *opaque, struct obs_source_frame *f)
{
	struct reactive_facecam_plugin *plugin = opaque;
	obs_source_output_video(plugin->context, f);
}

static void preload_frame(void *opaque, struct obs_source_frame *f)
{
	struct reactive_facecam_plugin *plugin = opaque;
	obs_source_preload_video(plugin->context, f);
}

static void seek_frame(void *opaque, struct obs_source_frame *f)
{
	struct reactive_facecam_plugin *plugin = opaque;
	obs_source_set_video_frame(plugin->context, f);
}

static void get_audio(void *opaque, struct obs_source_audio *a)
{
	struct reactive_facecam_plugin *plugin = opaque;
	obs_source_output_audio(plugin->context, a);
}

static void media_stopped(void *opaque)
{
	struct reactive_facecam_plugin *plugin = opaque;
	obs_source_output_video(plugin->context, NULL);

	plugin->state = OBS_MEDIA_STATE_ENDED;
	obs_source_media_ended(plugin->context);
}

void rfp_media_open(struct reactive_facecam_plugin *plugin)
{
	if (plugin->frame_path && *plugin->frame_path) {
		struct mp_media_info info = {
			.opaque = plugin,
			.v_cb = get_frame,
			.v_preload_cb = preload_frame,
			.v_seek_cb = seek_frame,
			.a_cb = get_audio,
			.stop_cb = media_stopped,
			.path = plugin->frame_path,
			.format = NULL,
			.buffering = 2 * 1024 * 1024,
			.speed = 100,
			.force_range = VIDEO_RANGE_DEFAULT,
			.is_linear_alpha = false,
			.hardware_decoding = false,
			.is_local_file = true,
			.reconnecting = false,
		};

		plugin->media_valid = mp_media_init(&plugin->media, &info);
	}
}

void rfp_media_start(struct reactive_facecam_plugin *plugin)
{
	if (!plugin->media_valid)
		rfp_media_open(plugin);

	mp_media_play(&plugin->media, true, false);
	obs_source_show_preloaded_video(plugin->context);
	
	plugin->state = OBS_MEDIA_STATE_PLAYING;
	obs_source_media_started(plugin->context);
}

void change_facecam_frame_to_file(struct reactive_facecam_plugin *plugin,
				char *new_frame_path)
{
	if (plugin->media_valid) {
		mp_media_free(&plugin->media);
		plugin->media_valid = false;
	}

	bfree(plugin->frame_path);
	plugin->frame_path = new_frame_path ? bstrdup(new_frame_path) : NULL;

	bool active = obs_source_active(plugin->context);
	if (active) {
		rfp_media_open(plugin);
		rfp_media_start(plugin);
	}
}

obs_missing_files_t *rfp_missingfiles(void *data)
{
	struct reactive_facecam_plugin *plugin = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(plugin->frame_path, "") != 0 && !os_file_exists(plugin->frame_path)) {
		blog(LOG_INFO, "RFP - ERROR: missing media file");
	}

	return files;
}

void rfp_play_pause(void *data, bool pause)
{
	struct reactive_facecam_plugin *plugin = data;

	if (!plugin->media_valid)
		rfp_media_open(plugin);

	mp_media_play_pause(&plugin->media, pause);

	if (pause) {
		plugin->state = OBS_MEDIA_STATE_PAUSED;
	} else {
		plugin->state = OBS_MEDIA_STATE_PLAYING;
		obs_source_media_started(plugin->context);
	}
}

void rfp_restart(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	if (obs_source_showing(plugin->context))
		rfp_media_start(plugin);

	plugin->state = OBS_MEDIA_STATE_PLAYING;
}

void rfp_stop(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	if (plugin->media_valid) {
		mp_media_stop(&plugin->media);
		obs_source_output_video(plugin->context, NULL);
		plugin->state = OBS_MEDIA_STATE_STOPPED;
	}
}

int64_t rfp_get_duration(void *data)
{
	struct reactive_facecam_plugin *plugin = data;
	int64_t dur = 0;

	if (plugin->media.fmt)
		dur = plugin->media.fmt->duration / INT64_C(1000);

	return dur;
}

int64_t rfp_get_time(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	return mp_get_current_time(&plugin->media);
}

void rfp_set_time(void *data, int64_t ms)
{
	struct reactive_facecam_plugin *plugin = data;

	if (!plugin->media_valid)
		return;

	mp_media_seek_to(&plugin->media, ms);
}

enum obs_media_state rfp_get_state(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	return plugin->state;
}
