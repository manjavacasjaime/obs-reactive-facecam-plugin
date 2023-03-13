/*****************************************************************************
Copyright (C) 2022 manjavacasjaime <jaimezombis@gmail.com>
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

#include <src/defs.h>
#include <src/video-player/video-player.h>
#include <src/game-capture/game-capture.h>
#include <src/api-communication/api-communication.h>

#define NO_LIFEBAR_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Default.webm"
#define HIGH_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Verde.webm"
#define MEDIUM_HIGH_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Amarillo.webm"
#define MEDIUM_LOW_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Naranja.webm"
#define LOW_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Rojo.webm"
#define ZERO_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Negro.webm"

#define CAYENDODUOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/cayendoDuos.png"
#define CAYENDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/cayendoTrios.png"
#define CURANDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/curandoTrios.png"
#define ESCUDODUOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/escudoDuos.png"
#define ESCUDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/escudoTrios.png"
#define POCAVIDADUOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/pocaVidaDuos.png"
#define POCAVIDATRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/pocaVidaTrios.png"
#define REANIMANDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/testImages/reanimandoTrios.png"


void *thread_take_screenshot_and_send_to_api(void *plugin)
{
    struct reactive_facecam_plugin *my_plugin =
		(struct reactive_facecam_plugin *) plugin;
	
	//wait
    sem_wait(&my_plugin->mutex);
	blog(LOG_INFO, "RFP - semaphore: Entered...");

	bool success = false;

	if (obs_source_active(my_plugin->game_capture_source) &&
				my_plugin->width > 10 && my_plugin->height > 10 &&
				render_game_capture(my_plugin)) {
		success = send_data_to_api(my_plugin);
	} else {
		if (my_plugin->current_frame != 0) {
			change_facecam_frame_to_file(my_plugin, my_plugin->no_lifebar_frame_path);
			my_plugin->current_frame = 0;
			my_plugin->no_lifebar_found_counter = 0;
		}

		check_for_game_capture_source(my_plugin);
	}

	int success_sleep_time =
			my_plugin->game == SETTING_GAME_VALORANT ? 500 : 700;
	int sleep_time = success ? success_sleep_time : 4000;

	if (success) {
		char *ptr;
		bool isImgId = false;
		bool isLBFound = false;
		double lifePerc = 0.0;

		if (isImageIdentified && isLifeBarFound && lifePercentage) {
			isImgId = json_object_get_boolean(isImageIdentified);
			isLBFound = json_object_get_boolean(isLifeBarFound);
			lifePerc = strtod(json_object_get_string(lifePercentage), &ptr);
		}

		if (isImgId) {
			if (isLBFound) {
				if (my_plugin->no_lifebar_found_counter > 0)
					my_plugin->no_lifebar_found_counter = 0;
				
				if (lifePerc > 0.75 && my_plugin->current_frame != 1) {
					change_facecam_frame_to_file(my_plugin, my_plugin->high_health_frame_path);
					my_plugin->current_frame = 1;
				} else if (lifePerc > 0.5 && lifePerc <= 0.75 && my_plugin->current_frame != 2) {
					change_facecam_frame_to_file(my_plugin, my_plugin->medium_high_health_frame_path);
					my_plugin->current_frame = 2;
				} else if (lifePerc > 0.25 && lifePerc <= 0.5 && my_plugin->current_frame != 3) {
					change_facecam_frame_to_file(my_plugin, my_plugin->medium_low_health_frame_path);
					my_plugin->current_frame = 3;
				} else if (lifePerc > 0 && lifePerc <= 0.25 && my_plugin->current_frame != 4) {
					change_facecam_frame_to_file(my_plugin, my_plugin->low_health_frame_path);
					my_plugin->current_frame = 4;
				} else if (lifePerc == 0 && my_plugin->current_frame != 5) {
					change_facecam_frame_to_file(my_plugin, my_plugin->zero_health_frame_path);
					my_plugin->current_frame = 5;
				}

			} else if (my_plugin->current_frame != 0) {
				my_plugin->no_lifebar_found_counter++;

				if (my_plugin->no_lifebar_found_counter > 6) {
					change_facecam_frame_to_file(my_plugin, my_plugin->no_lifebar_frame_path);
					my_plugin->current_frame = 0;
					my_plugin->no_lifebar_found_counter = 0;
				}
			}
		}
	}
      
    if (!my_plugin->is_on_destroy) {
		my_plugin->is_on_sleep = true;
		Sleep(sleep_time);
		my_plugin->is_on_sleep = false;
	}
	//signal
	blog(LOG_INFO, "RFP - semaphore: Just Exiting...");
    sem_post(&my_plugin->mutex);
}

static const char *rfp_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Reactive Facecam Plugin");
}

static void rfp_update(void *data, obs_data_t *settings)
{
	blog(LOG_INFO, "RFP - ENTERING UPDATE");

	struct reactive_facecam_plugin *plugin = data;

	int game = obs_data_get_int(settings, "game");
	const char *game_capture_source_name =
			obs_data_get_string(settings, "game_capture_source_name");
	const char *no_lifebar_frame_path =
			obs_data_get_string(settings, "no_lifebar_frame_path");
	const char *high_health_frame_path =
			obs_data_get_string(settings, "high_health_frame_path");
	const char *medium_high_health_frame_path =
			obs_data_get_string(settings, "medium_high_health_frame_path");
	const char *medium_low_health_frame_path =
			obs_data_get_string(settings, "medium_low_health_frame_path");
	const char *low_health_frame_path =
			obs_data_get_string(settings, "low_health_frame_path");
	const char *zero_health_frame_path =
			obs_data_get_string(settings, "zero_health_frame_path");

	plugin->game = game;

	if (plugin->game_capture_source_name)
		bfree(plugin->game_capture_source_name);
	plugin->game_capture_source_name = bstrdup(game_capture_source_name);

	check_for_game_capture_source(plugin);

	if (plugin->frame_path)
		bfree(plugin->frame_path);
	plugin->frame_path = bstrdup(no_lifebar_frame_path);
	
	if (plugin->no_lifebar_frame_path)
		bfree(plugin->no_lifebar_frame_path);
	plugin->no_lifebar_frame_path = bstrdup(no_lifebar_frame_path);

	if (plugin->high_health_frame_path)
		bfree(plugin->high_health_frame_path);
	plugin->high_health_frame_path = bstrdup(high_health_frame_path);

	if (plugin->medium_high_health_frame_path)
		bfree(plugin->medium_high_health_frame_path);
	plugin->medium_high_health_frame_path = bstrdup(medium_high_health_frame_path);

	if (plugin->medium_low_health_frame_path)
		bfree(plugin->medium_low_health_frame_path);
	plugin->medium_low_health_frame_path = bstrdup(medium_low_health_frame_path);

	if (plugin->low_health_frame_path)
		bfree(plugin->low_health_frame_path);
	plugin->low_health_frame_path = bstrdup(low_health_frame_path);

	if (plugin->zero_health_frame_path)
		bfree(plugin->zero_health_frame_path);
	plugin->zero_health_frame_path = bstrdup(zero_health_frame_path);

	plugin->current_frame = 0;
	plugin->no_lifebar_found_counter = 0;

	if (plugin->media_valid) {
		mp_media_free(&plugin->media);
		plugin->media_valid = false;
	}

	bool active = obs_source_active(plugin->context);
	if (active) {
		rfp_media_open(plugin);
		rfp_media_start(plugin);
	}

	blog(LOG_INFO, "RFP - EXITING UPDATE");
}

static void *rfp_create(obs_data_t *settings, obs_source_t *context)
{
	UNUSED_PARAMETER(settings);

	blog(LOG_INFO, "RFP - ENTERING CREATE");

	struct reactive_facecam_plugin *plugin =
		bzalloc(sizeof(struct reactive_facecam_plugin));
	plugin->context = context;

	curl_global_init(0);
	
	sem_init(&plugin->mutex, 0, 1);
	plugin->is_on_destroy = false;
	plugin->is_on_sleep = false;

	check_for_current_scene(plugin);

	obs_enter_graphics();
	plugin->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();
	
	plugin->hotkey = obs_hotkey_register_source(
		context,
		"MediaSource.Restart",
		obs_module_text("RestartMedia"),
		rfp_restart_hotkey,
		plugin
	);
	plugin->play_pause_hotkey = obs_hotkey_pair_register_source(
		context,
		"MediaSource.Play",
		obs_module_text("Play"),
		"MediaSource.Pause",
		obs_module_text("Pause"),
		rfp_play_hotkey,
		rfp_pause_hotkey,
		plugin,
		plugin
	);
	plugin->stop_hotkey = obs_hotkey_register_source(
		context,
		"MediaSource.Stop",
		obs_module_text("Stop"),
		rfp_stop_hotkey,
		plugin
	);

	blog(LOG_INFO, "RFP - EXITING CREATE");

	rfp_update(plugin, settings);
	return plugin;
}

static void rfp_destroy(void *data)
{
	blog(LOG_INFO, "RFP - ENTERING DESTROY");
	struct reactive_facecam_plugin *plugin = data;

	plugin->is_on_destroy = true;
	if(!plugin->is_on_sleep)
		sem_wait(&plugin->mutex);

	if (plugin->hotkey)
		obs_hotkey_unregister(plugin->hotkey);

	if (plugin->media_valid)
		mp_media_free(&plugin->media);

	if (plugin->game_capture_source_name)
		bfree(plugin->game_capture_source_name);
	
	if (plugin->frame_path)
		bfree(plugin->frame_path);
	
	if (plugin->no_lifebar_frame_path)
		bfree(plugin->no_lifebar_frame_path);
	if (plugin->high_health_frame_path)
		bfree(plugin->high_health_frame_path);
	if (plugin->medium_high_health_frame_path)
		bfree(plugin->medium_high_health_frame_path);
	if (plugin->medium_low_health_frame_path)
		bfree(plugin->medium_low_health_frame_path);
	if (plugin->low_health_frame_path)
		bfree(plugin->low_health_frame_path);
	if (plugin->zero_health_frame_path)
		bfree(plugin->zero_health_frame_path);

	if (plugin->curl)
		curl_easy_cleanup(plugin->curl);
	curl_global_cleanup();

	obs_enter_graphics();
	gs_texrender_destroy(plugin->texrender);

	if (plugin->staging_texture)
		gs_stagesurface_destroy(plugin->staging_texture);

	obs_leave_graphics();

	if (plugin->data)
		bfree(plugin->data);

	if(!plugin->is_on_sleep)
		sem_post(&plugin->mutex);

	sem_destroy(&plugin->mutex);

	blog(LOG_INFO, "RFP - EXITING DESTROY");
	bfree(plugin);
}

static void rfp_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct reactive_facecam_plugin *plugin = data;

	int value;
    sem_getvalue(&plugin->mutex, &value);

	if (value == 1 && !plugin->is_on_destroy) {
		pthread_t thread;
    	pthread_create(&thread, NULL, thread_take_screenshot_and_send_to_api, (void*) plugin);
	}
}

static void rfp_activate(void *data)
{
	struct reactive_facecam_plugin *plugin = data;
	obs_source_media_restart(plugin->context);
}

static void rfp_deactivate(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	if (plugin->media_valid) {
		mp_media_stop(&plugin->media);
		obs_source_output_video(plugin->context, NULL);
	}
}

static const char *video_filter =
	" (*.mp4 *.ts *.mov *.flv *.mkv *.avi *.gif *.webm);;";

static obs_properties_t *rfp_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_list(props,
		"game", "Game", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "Valorant", SETTING_GAME_VALORANT);
	obs_property_list_add_int(p, "Apex Legends", SETTING_GAME_APEX);

	obs_properties_add_text(props, "game_capture_source_name",
			"Name of source where game is found", OBS_TEXT_DEFAULT);
	obs_properties_add_path(props, "no_lifebar_frame_path",
			"Facecam frame to show when no lifebar is found",
			OBS_PATH_FILE, video_filter, NO_LIFEBAR_DEFAULT_FRAME);
	obs_properties_add_path(props, "high_health_frame_path",
			"Facecam frame to show when health is 76-100%",
			OBS_PATH_FILE, video_filter, HIGH_HEALTH_DEFAULT_FRAME);
	obs_properties_add_path(props, "medium_high_health_frame_path",
			"Facecam frame to show when health is 51-75%",
			OBS_PATH_FILE, video_filter, MEDIUM_HIGH_HEALTH_DEFAULT_FRAME);
	obs_properties_add_path(props, "medium_low_health_frame_path",
			"Facecam frame to show when health is 26-50%",
			OBS_PATH_FILE, video_filter, MEDIUM_LOW_HEALTH_DEFAULT_FRAME);
	obs_properties_add_path(props, "low_health_frame_path",
			"Facecam frame to show when health is 1-25%",
			OBS_PATH_FILE, video_filter, LOW_HEALTH_DEFAULT_FRAME);
	obs_properties_add_path(props, "zero_health_frame_path",
			"Facecam frame to show when health is 0%",
			OBS_PATH_FILE, video_filter, ZERO_HEALTH_DEFAULT_FRAME);

	UNUSED_PARAMETER(data);
	return props;
}

static void rfp_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "game", SETTING_GAME_VALORANT);
	obs_data_set_default_string(settings, "game_capture_source_name",
			"Captura de juego");
	obs_data_set_default_string(settings,
			"no_lifebar_frame_path", NO_LIFEBAR_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"high_health_frame_path", HIGH_HEALTH_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"medium_high_health_frame_path", MEDIUM_HIGH_HEALTH_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"medium_low_health_frame_path", MEDIUM_LOW_HEALTH_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"low_health_frame_path", LOW_HEALTH_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"zero_health_frame_path", ZERO_HEALTH_DEFAULT_FRAME);
}


struct obs_source_info reactive_facecam_plugin = {
	.id = "reactive_facecam_plugin",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO |
			OBS_SOURCE_DO_NOT_DUPLICATE |
			OBS_SOURCE_CONTROLLABLE_MEDIA,
	.get_name = rfp_getname,
	.update = rfp_update,
	.create = rfp_create,
	.destroy = rfp_destroy,
	.video_tick = rfp_tick,
	.missing_files = rfp_missingfiles,
	.activate = rfp_activate,
	.deactivate = rfp_deactivate,
	.media_play_pause = rfp_play_pause,
	.media_restart = rfp_restart,
	.media_stop = rfp_stop,
	.media_get_duration = rfp_get_duration,
	.media_get_time = rfp_get_time,
	.media_set_time = rfp_set_time,
	.media_get_state = rfp_get_state,
	.get_properties = rfp_properties,
	.get_defaults = rfp_defaults,
	.icon_type = OBS_ICON_TYPE_UNKNOWN,
};
