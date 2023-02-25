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
#include <obs-frontend-api.h>
#include <curl/curl.h>
#include <windows.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define VALORANT_API_URL (char *)"http://127.0.0.1:8080/healthbar-reader-service/valorant/fullhd"
#define APEX_API_URL (char *)"http://127.0.0.1:8080/healthbar-reader-service/apex/fullhd"

#define NO_LIFEBAR_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Default.webm"
#define HIGH_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Verde.webm"
#define MEDIUM_HIGH_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Amarillo.webm"
#define MEDIUM_LOW_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Naranja.webm"
#define LOW_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Rojo.webm"
#define ZERO_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/Marco-Negro.webm"

#define CAYENDODUOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/cayendoDuos.png"
#define CAYENDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/cayendoTrios.png"
#define CURANDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/curandoTrios.png"
#define ESCUDODUOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/escudoDuos.png"
#define ESCUDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/escudoTrios.png"
#define POCAVIDADUOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/pocaVidaDuos.png"
#define POCAVIDATRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/pocaVidaTrios.png"
#define REANIMANDOTRIOS (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/newTest/reanimandoTrios.png"

#define C_VALOFULLVIDA (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/charlieTest/charlieValoFullVida.png"
#define C_VALOPOCAVIDA (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/charlieTest/charlieValoPocaVida.png"
#define C_APEXFULLVIDA (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/charlieTest/charlieApexFullVida.png"
#define C_APEXPOCAVIDA (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/charlieTest/charlieApexPocaVida.png"

#define V_RAWIMAGE (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/valoTest/raw_image"
#define RAWSCREENSHOT (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/test/raw_screenshot"

#define SETTING_GAME_VALORANT 0
#define SETTING_GAME_APEX 1


struct json_object *parsed_json;
struct json_object *isImageIdentified;
struct json_object *errorMessage;
struct json_object *isLifeBarFound;
struct json_object *lifePercentage;

struct reactive_facecam_plugin {
	obs_source_t *context;
	mp_media_t media;
	bool media_valid;

	uint8_t *data;
	uint32_t linesize;

	uint32_t width;
	uint32_t height;
	gs_texrender_t *texrender;
	gs_stagesurf_t *staging_texture;

	char *frame_path;
	
	char *no_lifebar_frame_path;
	char *high_health_frame_path;
	char *medium_high_health_frame_path;
	char *medium_low_health_frame_path;
	char *low_health_frame_path;
	char *zero_health_frame_path;

	sem_t mutex;
	//0 is default, 1 is green, 2 is yellow,
	//3 is orange, 4 is red and 5 is black
	int current_frame;
	int no_lifebar_found_counter;
	bool is_on_destroy;
	bool is_on_sleep;
	
	int game;
	obs_scene_t *current_scene;
	char *game_capture_source_name;
	obs_source_t *game_capture_source;
	CURL *curl;

	enum obs_media_state state;
	obs_hotkey_id hotkey;
	obs_hotkey_pair_id play_pause_hotkey;
	obs_hotkey_id stop_hotkey;
};

struct json_object *json_tokener_parse(const char *str);
bool json_object_object_get_ex(const struct json_object *obj, const char *key,
                                struct json_object **value);
const char *json_object_get_string(struct json_object *jso);
bool json_object_get_boolean(const struct json_object *jso);
int json_object_put(struct json_object *jso);

size_t got_data_from_api(char *buffer, size_t itemsize, size_t nitems, void* ignorethis)
{
	UNUSED_PARAMETER(ignorethis);

	size_t bytes = itemsize * nitems;
	blog(LOG_INFO, "RFP - got_data_from_api: %zu bytes", bytes);

	blog(LOG_INFO, "RFP - got_data_from_api: %s", buffer);

	parsed_json = json_tokener_parse(buffer);

	json_object_object_get_ex(parsed_json, "isImageIdentified", &isImageIdentified);
	json_object_object_get_ex(parsed_json, "errorMessage", &errorMessage);
	json_object_object_get_ex(parsed_json, "isLifeBarFound", &isLifeBarFound);
	json_object_object_get_ex(parsed_json, "lifePercentage", &lifePercentage);

	blog(LOG_INFO, "RFP - Image identified: %i", json_object_get_boolean(isImageIdentified));
	blog(LOG_INFO, "RFP - Error message: %s", json_object_get_string(errorMessage));
	blog(LOG_INFO, "RFP - Life bar found: %i", json_object_get_boolean(isLifeBarFound));
	blog(LOG_INFO, "RFP - Life percentage: %s", json_object_get_string(lifePercentage));

	json_object_put(parsed_json);

	return bytes;
}

size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	FILE *readhere = (FILE *)userdata;

	/* copy as much data as possible into the 'ptr' buffer, but no more than
	'size' * 'nmemb' bytes! */
	size_t bytes = fread(ptr, size, nmemb, readhere);

	blog(LOG_INFO, "RFP - read_callback: %zu bytes", bytes);
	return bytes;
}

static const char *rfp_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Reactive Facecam Plugin");
}

static void rfp_restart_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
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

static bool rfp_play_hotkey(void *data, obs_hotkey_pair_id id,
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

static bool rfp_pause_hotkey(void *data, obs_hotkey_pair_id id,
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

static void rfp_stop_hotkey(void *data, obs_hotkey_id id,
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

static void rfp_media_open(struct reactive_facecam_plugin *plugin)
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

static void rfp_media_start(struct reactive_facecam_plugin *plugin)
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

bool render_game_capture(struct reactive_facecam_plugin *plugin) 
{
	bool success = false;
	obs_enter_graphics();

	gs_texrender_reset(plugin->texrender);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(plugin->texrender, plugin->width, plugin->height)) {
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float) plugin->width, 0.0f,
			(float) plugin->height, -100.0f, 100.0f);

		
		if (obs_source_active(plugin->game_capture_source)) {
			obs_source_video_render(plugin->game_capture_source);
			success = true;
		}
		gs_texrender_end(plugin->texrender);
	}

	gs_blend_state_pop();

	gs_effect_t *effect2 = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_texture_t *tex = gs_texrender_get_texture(plugin->texrender);

	if (tex && obs_source_active(plugin->game_capture_source)) {
		gs_stage_texture(plugin->staging_texture, tex);

		uint8_t *data;
		uint32_t linesize;
		if (gs_stagesurface_map(plugin->staging_texture, &data, &linesize)) {
			memcpy(plugin->data, data, linesize * plugin->height);
			plugin->linesize = linesize;

			gs_stagesurface_unmap(plugin->staging_texture);
		}

		gs_eparam_t *image = gs_effect_get_param_by_name(effect2, "image");
		gs_effect_set_texture(image, tex);

		while (gs_effect_loop(effect2, "Draw"))
			gs_draw_sprite(tex, 0, plugin->width, plugin->height);
	} else {
		success = false;
	}

	obs_leave_graphics();
	return success;
}

bool send_data_to_api(struct reactive_facecam_plugin *plugin) 
{
	bool success = false;
	uint8_t *data = bzalloc(plugin->linesize * plugin->height);
	memcpy(data, plugin->data, plugin->linesize * plugin->height);

	errno = 0;

	FILE *of = fopen(RAWSCREENSHOT, "wb");
	if (of != NULL) {
		fwrite(data, 1, plugin->linesize * plugin->height, of);
		blog(LOG_INFO, "RFP - FOPEN WRITE success");
	} else {
		blog(LOG_INFO, "RFP - FOPEN WRITE returns NULL: %d", errno);
	}

	fclose(of);
	bfree(data);

	FILE *fd;
  	fd = fopen(RAWSCREENSHOT, "rb");
	if (fd != NULL) {
		blog(LOG_INFO, "RFP - FOPEN READ success");
	} else {
		blog(LOG_INFO, "RFP - FOPEN READ returns NULL");
	}
	
	plugin->curl = curl_easy_init();

	if (plugin->curl) {
		const char *api_url =
			plugin->game == SETTING_GAME_VALORANT ? VALORANT_API_URL : APEX_API_URL;

		curl_easy_setopt(plugin->curl, CURLOPT_URL, api_url);
		curl_easy_setopt(plugin->curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(plugin->curl, CURLOPT_READFUNCTION, read_callback);
		curl_easy_setopt(plugin->curl, CURLOPT_READDATA, fd);
		curl_easy_setopt(plugin->curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(plugin->curl, CURLOPT_WRITEFUNCTION, got_data_from_api);

		CURLcode result = curl_easy_perform(plugin->curl);
		
		if (result == CURLE_OK) {
			success = true;
			blog(LOG_INFO, "RFP - CURL result working");
		} else {
			blog(LOG_INFO, "RFP - CURL error: %s", curl_easy_strerror(result));
		}
	} else {
		blog(LOG_INFO, "RFP - CURL init failed");
	}

	fclose(fd);
	return success;
}

void check_for_current_scene(struct reactive_facecam_plugin *plugin)
{
	obs_source_t *current_scene_source = obs_frontend_get_current_scene();
	if (current_scene_source) {
		obs_scene_t *current_scene = obs_scene_from_source(current_scene_source);
		plugin->current_scene = current_scene;
		obs_source_release(current_scene_source);
	}
}

void check_for_game_capture_source(struct reactive_facecam_plugin *plugin)
{
	if (plugin->current_scene) {
		obs_sceneitem_t *game_capture_item = obs_scene_find_source(
				plugin->current_scene, plugin->game_capture_source_name);

		if (game_capture_item && obs_sceneitem_visible(game_capture_item)) {
			obs_source_t *game_capture_source = obs_sceneitem_get_source(game_capture_item);
			
			plugin->game_capture_source = game_capture_source;
			plugin->width = obs_source_get_width(game_capture_source);
			plugin->height = obs_source_get_height(game_capture_source);

			obs_enter_graphics();
			if (plugin->staging_texture)
				gs_stagesurface_destroy(plugin->staging_texture);

			plugin->staging_texture = gs_stagesurface_create(
					plugin->width, plugin->height, GS_RGBA);
			obs_leave_graphics();

			if (plugin->data) {
				bfree(plugin->data);
				plugin->data = NULL;
			}
			plugin->data = bzalloc((plugin->width + 32) * plugin->height * 4);
		} else if (plugin->game_capture_source) {
			plugin->game_capture_source = 0;
			plugin->width = 0;
			plugin->height = 0;
		}
	} else {
		check_for_current_scene(plugin);
	}
}

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

static obs_missing_files_t *rfp_missingfiles(void *data)
{
	struct reactive_facecam_plugin *plugin = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(plugin->frame_path, "") != 0 && !os_file_exists(plugin->frame_path)) {
		blog(LOG_INFO, "RFP - ERROR: missing media file");
	}

	return files;
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

static void rfp_play_pause(void *data, bool pause)
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

static void rfp_restart(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	if (obs_source_showing(plugin->context))
		rfp_media_start(plugin);

	plugin->state = OBS_MEDIA_STATE_PLAYING;
}

static void rfp_stop(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	if (plugin->media_valid) {
		mp_media_stop(&plugin->media);
		obs_source_output_video(plugin->context, NULL);
		plugin->state = OBS_MEDIA_STATE_STOPPED;
	}
}

static int64_t rfp_get_duration(void *data)
{
	struct reactive_facecam_plugin *plugin = data;
	int64_t dur = 0;

	if (plugin->media.fmt)
		dur = plugin->media.fmt->duration / INT64_C(1000);

	return dur;
}

static int64_t rfp_get_time(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	return mp_get_current_time(&plugin->media);
}

static void rfp_set_time(void *data, int64_t ms)
{
	struct reactive_facecam_plugin *plugin = data;

	if (!plugin->media_valid)
		return;

	mp_media_seek_to(&plugin->media, ms);
}

static enum obs_media_state rfp_get_state(void *data)
{
	struct reactive_facecam_plugin *plugin = data;

	return plugin->state;
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


//.output_flags = OBS_SOURCE_ASYNC_VIDEO,
//solo este flag hace que se crashee a veces
//quizas es porque necesita los hotkey methods
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
