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
	int width;
	int height;
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
	const int width = obs_data_get_int(settings, "width");
	const int height = obs_data_get_int(settings, "height");

	if (sensor->text)
		bfree(sensor->text);
	sensor->text = bstrdup(text);
	sensor->width = width;
	sensor->height = height;
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

static void hswf_render(void *data, gs_effect_t *effect)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	UNUSED_PARAMETER(effect);
}

static void hswf_tick(void *data, float seconds)
{
	struct healthbar_sensor_webcam_frame *sensor = data;

	UNUSED_PARAMETER(seconds);
}

static void hswf_show(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static void hswf_hide(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static void hswf_activate(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
}

static uint32_t hswf_getwidth(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	return sensor->width;
}

static uint32_t hswf_getheight(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	return sensor->height;
}

static obs_properties_t *hswf_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "text", "Texto a mostrar", OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "width", "Ancho", 400, 600, 5);
	obs_properties_add_int(props, "height", "Alto", 400, 600, 5);

	UNUSED_PARAMETER(data);
	return props;
}

static void hswf_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "text", "Str por defecto");
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 400);
}


//.output_flags = OBS_SOURCE_ASYNC_VIDEO,
//solo este flag hace que se crashee a veces
//quizas es porque necesita los hotkey methods
struct obs_source_info healthbar_sensor_webcam_frame = {
	.id = "healthbar_sensor_webcam_frame",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = hswf_getname,
	.update = hswf_update,
	.create = hswf_create,
	.destroy = hswf_destroy,
	.video_render = hswf_render,
	.video_tick = hswf_tick,
	.show = hswf_show,
	.hide = hswf_hide,
	.activate = hswf_activate,
	.get_width = hswf_getwidth,
	.get_height = hswf_getheight,
	.get_properties = hswf_properties,
	.get_defaults = hswf_defaults,
	.icon_type = OBS_ICON_TYPE_UNKNOWN,
};
