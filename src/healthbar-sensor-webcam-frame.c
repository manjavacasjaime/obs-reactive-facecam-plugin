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

//#include <media-playback/media.h>


struct healthbar_sensor_webcam_frame {
	obs_source_t *context;

	char *text;
	int someNumber;
};

static const char *hswf_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Healthbar Sensor Webcam Frame");
}

static void hswf_update(void *data, obs_data_t *settings)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	const char *text = obs_data_get_string(settings, "text");
	const int someNumber = obs_data_get_int(settings, "someNumber");

	if (sensor->text)
		bfree(sensor->text);
	sensor->text = bstrdup(text);
	sensor->someNumber = someNumber;
}

static void *hswf_create(obs_data_t *settings, obs_source_t *context)
{
	struct healthbar_sensor_webcam_frame *sensor =
		bzalloc(sizeof(struct healthbar_sensor_webcam_frame));

	sensor->context = context;
	hswf_update(sensor, settings);

	return sensor;
}

static void hswf_destroy(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	if (sensor->text)
		bfree(sensor->text);
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
	return files;
}

static void hswf_activate(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static void hswf_deactivate(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static void hswf_play_pause(void *data, bool pause)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static void hswf_restart(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static void hswf_stop(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static int64_t hswf_get_duration(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	int64_t dur = 0;

	return dur;
}

static int64_t hswf_get_time(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	//return mp_get_current_time(&s->media);
	return 0;
}

static void hswf_set_time(void *data, int64_t ms)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static enum obs_media_state hswf_get_state(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	//return sensor->state;
	return OBS_MEDIA_STATE_ENDED;
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
