/*****************************************************************************
Copyright (C) 2017 by Eric Rasmuson <erasmuson@gmail.com>

This file is part of OBS.

OBS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include <obs-module.h>
#include <obs-source.h>
#include <obs.h>
#include <util/platform.h>
#include <media-playback/media.h>


struct healthbar_sensor_webcam_frame {
	obs_source_t *context;
	mp_media_t media;
	bool media_valid;

	char *input;
	
	char *text;
	int someNumber;

	enum obs_media_state state;
	obs_hotkey_id hotkey;
	obs_hotkey_pair_id play_pause_hotkey;
	obs_hotkey_id stop_hotkey;
};

static const char *hswf_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Healthbar Sensor Webcam Frame");
}

static void hswf_restart_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			   bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	struct healthbar_sensor_webcam_frame *sensor = data;
	if (obs_source_showing(sensor->context))
		obs_source_media_restart(sensor->context);
}

static bool hswf_play_hotkey(void *data, obs_hotkey_pair_id id,
				      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return false;

	struct healthbar_sensor_webcam_frame *sensor = data;

	if (sensor->state == OBS_MEDIA_STATE_PLAYING ||
	    !obs_source_showing(sensor->context))
		return false;

	obs_source_media_play_pause(sensor->context, false);
	return true;
}

static bool hswf_pause_hotkey(void *data, obs_hotkey_pair_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return false;

	struct healthbar_sensor_webcam_frame *sensor = data;

	if (sensor->state != OBS_MEDIA_STATE_PLAYING ||
	    !obs_source_showing(sensor->context))
		return false;

	obs_source_media_play_pause(sensor->context, true);
	return true;
}

static void hswf_stop_hotkey(void *data, obs_hotkey_id id,
				      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	struct healthbar_sensor_webcam_frame *sensor = data;

	if (obs_source_showing(sensor->context))
		obs_source_media_stop(sensor->context);
}

static void get_frame(void *opaque, struct obs_source_frame *f)
{
	struct healthbar_sensor_webcam_frame *sensor = opaque;
	obs_source_output_video(sensor->context, f);
}

static void preload_frame(void *opaque, struct obs_source_frame *f)
{
	struct healthbar_sensor_webcam_frame *sensor = opaque;
	obs_source_preload_video(sensor->context, f);
}

static void seek_frame(void *opaque, struct obs_source_frame *f)
{
	struct healthbar_sensor_webcam_frame *sensor = opaque;
	obs_source_set_video_frame(sensor->context, f);
}

static void get_audio(void *opaque, struct obs_source_audio *a)
{
	struct healthbar_sensor_webcam_frame *sensor = opaque;
	obs_source_output_audio(sensor->context, a);
}

static void media_stopped(void *opaque)
{
	struct healthbar_sensor_webcam_frame *sensor = opaque;
	obs_source_output_video(sensor->context, NULL);

	sensor->state = OBS_MEDIA_STATE_ENDED;
	obs_source_media_ended(sensor->context);
}

static void hswf_media_open(struct healthbar_sensor_webcam_frame *sensor)
{
	if (sensor->input && *sensor->input) {
		struct mp_media_info info = {
			.opaque = sensor,
			.v_cb = get_frame,
			.v_preload_cb = preload_frame,
			.v_seek_cb = seek_frame,
			.a_cb = get_audio,
			.stop_cb = media_stopped,
			.path = sensor->input,
			.format = NULL,
			.buffering = 2 * 1024 * 1024,
			.speed = 100,
			.force_range = VIDEO_RANGE_DEFAULT,
			.is_linear_alpha = false,
			.hardware_decoding = false,
			.is_local_file = true,
			.reconnecting = false,
		};

		sensor->media_valid = mp_media_init(&sensor->media, &info);
	}
}

static void hswf_media_start(struct healthbar_sensor_webcam_frame *sensor)
{
	if (!sensor->media_valid)
		hswf_media_open(sensor);

	mp_media_play(&sensor->media, true, false);
	obs_source_show_preloaded_video(sensor->context);
	
	sensor->state = OBS_MEDIA_STATE_PLAYING;
	obs_source_media_started(sensor->context);
}

static void hswf_update(void *data, obs_data_t *settings)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	bfree(sensor->input);
	char *input =
		(char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/GreenMarco.webm";

	const char *text = obs_data_get_string(settings, "text");
	const int someNumber = obs_data_get_int(settings, "someNumber");

	sensor->input = input ? bstrdup(input) : NULL;
	if (sensor->text)
		bfree(sensor->text);
	sensor->text = bstrdup(text);
	sensor->someNumber = someNumber;

	if (sensor->media_valid) {
		mp_media_free(&sensor->media);
		sensor->media_valid = false;
	}

	bool active = obs_source_active(sensor->context);
	if(active) {
		hswf_media_open(sensor);
		hswf_media_start(sensor);
	}
}

static void *hswf_create(obs_data_t *settings, obs_source_t *context)
{
	UNUSED_PARAMETER(settings);

	struct healthbar_sensor_webcam_frame *sensor =
		bzalloc(sizeof(struct healthbar_sensor_webcam_frame));
	sensor->context = context;

	sensor->hotkey = obs_hotkey_register_source(
		context,
		"MediaSource.Restart",
		obs_module_text("RestartMedia"),
		hswf_restart_hotkey,
		sensor
	);
	sensor->play_pause_hotkey = obs_hotkey_pair_register_source(
		context,
		"MediaSource.Play",
		obs_module_text("Play"),
		"MediaSource.Pause",
		obs_module_text("Pause"),
		hswf_play_hotkey,
		hswf_pause_hotkey,
		sensor,
		sensor
	);
	sensor->stop_hotkey = obs_hotkey_register_source(
		context,
		"MediaSource.Stop",
		obs_module_text("Stop"),
		hswf_stop_hotkey,
		sensor
	);

	hswf_update(sensor, settings);
	return sensor;
}

static void hswf_destroy(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (sensor->hotkey)
		obs_hotkey_unregister(sensor->hotkey);

	if (sensor->media_valid)
		mp_media_free(&sensor->media);

	if (sensor->text)
		bfree(sensor->text);

	bfree(sensor->input);
	bfree(sensor);
}

static void hswf_tick(void *data, float seconds)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	UNUSED_PARAMETER(seconds);
}

static obs_missing_files_t *hswf_missingfiles(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(sensor->input, "") != 0 && !os_file_exists(sensor->input)) {
		blog(LOG_INFO, "HSWF - ERROR: missing media file");
	}

	return files;
}

static void hswf_activate(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	obs_source_media_restart(sensor->context);
}

static void hswf_deactivate(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (sensor->media_valid) {
		mp_media_stop(&sensor->media);
		obs_source_output_video(sensor->context, NULL);
	}
}

static void hswf_play_pause(void *data, bool pause)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (!sensor->media_valid)
		hswf_media_open(sensor);

	mp_media_play_pause(&sensor->media, pause);

	if (pause) {
		sensor->state = OBS_MEDIA_STATE_PAUSED;
	} else {
		sensor->state = OBS_MEDIA_STATE_PLAYING;
		obs_source_media_started(sensor->context);
	}
}

static void hswf_restart(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (obs_source_showing(sensor->context))
		hswf_media_start(sensor);

	sensor->state = OBS_MEDIA_STATE_PLAYING;
}

static void hswf_stop(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (sensor->media_valid) {
		mp_media_stop(&sensor->media);
		obs_source_output_video(sensor->context, NULL);
		sensor->state = OBS_MEDIA_STATE_STOPPED;
	}
}

static int64_t hswf_get_duration(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	int64_t dur = 0;

	if (sensor->media.fmt)
		dur = sensor->media.fmt->duration / INT64_C(1000);

	return dur;
}

static int64_t hswf_get_time(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	return mp_get_current_time(&sensor->media);
}

static void hswf_set_time(void *data, int64_t ms)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (!sensor->media_valid)
		return;

	mp_media_seek_to(&sensor->media, ms);
}

static enum obs_media_state hswf_get_state(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	return sensor->state;
}

static obs_properties_t *hswf_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "text", "Texto a mostrar", OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "someNumber", "Número", 400, 600, 5);

	UNUSED_PARAMETER(data);
	return props;
}

static void hswf_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "text", "Str por defecto");
	obs_data_set_default_int(settings, "someNumber", 400);
}


//.output_flags = OBS_SOURCE_ASYNC_VIDEO,
//solo este flag hace que se crashee a veces
//quizas es porque necesita los hotkey methods
struct obs_source_info healthbar_sensor_webcam_frame = {
	.id = "healthbar_sensor_webcam_frame",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO |
			OBS_SOURCE_DO_NOT_DUPLICATE |
			OBS_SOURCE_CONTROLLABLE_MEDIA,
	.get_name = hswf_getname,
	.update = hswf_update,
	.create = hswf_create,
	.destroy = hswf_destroy,
	.video_tick = hswf_tick,
	.missing_files = hswf_missingfiles,
	.activate = hswf_activate,
	.deactivate = hswf_deactivate,
	.media_play_pause = hswf_play_pause,
	.media_restart = hswf_restart,
	.media_stop = hswf_stop,
	.media_get_duration = hswf_get_duration,
	.media_get_time = hswf_get_time,
	.media_set_time = hswf_set_time,
	.media_get_state = hswf_get_state,
	.get_properties = hswf_properties,
	.get_defaults = hswf_defaults,
	.icon_type = OBS_ICON_TYPE_UNKNOWN,
};
