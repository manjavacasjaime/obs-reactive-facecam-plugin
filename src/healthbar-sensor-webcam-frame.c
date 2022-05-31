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

#define API_URL (char *)"http://127.0.0.1:8080/healthbar-reader-service/apex/fullhd"

#define NO_LIFEBAR_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/GreenMarco.webm"
#define HIGH_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/Marco01.webm"
#define MEDIUM_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/Marco02.webm"
#define LOW_HEALTH_DEFAULT_FRAME (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/Marco03.webm"

#define BUSCARJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/buscarJJ.png"
#define CURANDOJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/curandoJJ.png"
#define LEYENDASJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/leyendasJJ.png"
#define LLENABLANCOJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/llenaBlancoJJ.png"
#define LLENAJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/llenaJJ.png"
#define LLENANEGROJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/llenaNegroJJ.png"
#define POCAEMPTYJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/pocaEmptyJJ.png"
#define POCAROJAJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/pocaRojaJJ.png"
#define RAWSCREENSHOT (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/raw_screenshot"

#define CAYENDODUOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/cayendoDuos.png"
#define CAYENDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/cayendoTrios.png"
#define CURANDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/curandoTrios.png"
#define ESCUDODUOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/escudoDuos.png"
#define ESCUDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/escudoTrios.png"
#define POCAVIDADUOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/pocaVidaDuos.png"
#define POCAVIDATRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/pocaVidaTrios.png"
#define REANIMANDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/reanimandoTrios.png"

#define V_RAWIMAGE (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/valoTest/raw_image"


struct json_object *parsed_json;
struct json_object *isImageIdentified;
struct json_object *errorMessage;
struct json_object *isLifeBarFound;
struct json_object *lifePercentage;

struct healthbar_sensor_webcam_frame {
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
	char *medium_health_frame_path;
	char *low_health_frame_path;

	sem_t mutex;
	//0 is default, 1 is green, 2 is orange and 3 is red
	int current_frame;
	int no_lifebar_found_counter;
	bool is_on_destroy;
	bool is_on_sleep;
	
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
	blog(LOG_INFO, "HSWF - got_data_from_api: %zu bytes", bytes);

	blog(LOG_INFO, "HSWF - got_data_from_api: %s", buffer);

	parsed_json = json_tokener_parse(buffer);

	json_object_object_get_ex(parsed_json, "isImageIdentified", &isImageIdentified);
	json_object_object_get_ex(parsed_json, "errorMessage", &errorMessage);
	json_object_object_get_ex(parsed_json, "isLifeBarFound", &isLifeBarFound);
	json_object_object_get_ex(parsed_json, "lifePercentage", &lifePercentage);

	blog(LOG_INFO, "HSWF - Image identified: %i", json_object_get_boolean(isImageIdentified));
	blog(LOG_INFO, "HSWF - Error message: %s", json_object_get_string(errorMessage));
	blog(LOG_INFO, "HSWF - Life bar found: %i", json_object_get_boolean(isLifeBarFound));
	blog(LOG_INFO, "HSWF - Life percentage: %s", json_object_get_string(lifePercentage));

	json_object_put(parsed_json);

	return bytes;
}

size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	FILE *readhere = (FILE *)userdata;

	/* copy as much data as possible into the 'ptr' buffer, but no more than
	'size' * 'nmemb' bytes! */
	size_t bytes = fread(ptr, size, nmemb, readhere);

	blog(LOG_INFO, "HSWF - read_callback: %zu bytes", bytes);
	return bytes;
}

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
	if (sensor->frame_path && *sensor->frame_path) {
		struct mp_media_info info = {
			.opaque = sensor,
			.v_cb = get_frame,
			.v_preload_cb = preload_frame,
			.v_seek_cb = seek_frame,
			.a_cb = get_audio,
			.stop_cb = media_stopped,
			.path = sensor->frame_path,
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

void change_webcam_frame_to_file(struct healthbar_sensor_webcam_frame *sensor,
				char *new_frame_path)
{
	if (sensor->media_valid) {
		mp_media_free(&sensor->media);
		sensor->media_valid = false;
	}

	bfree(sensor->frame_path);
	sensor->frame_path = new_frame_path ? bstrdup(new_frame_path) : NULL;

	bool active = obs_source_active(sensor->context);
	if (active) {
		hswf_media_open(sensor);
		hswf_media_start(sensor);
	}
}

bool render_game_capture(struct healthbar_sensor_webcam_frame *sensor) 
{
	bool success = false;
	obs_enter_graphics();

	gs_texrender_reset(sensor->texrender);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(sensor->texrender, sensor->width, sensor->height)) {
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float) sensor->width, 0.0f,
			(float) sensor->height, -100.0f, 100.0f);

		
		if (obs_source_active(sensor->game_capture_source)) {
			obs_source_video_render(sensor->game_capture_source);
			success = true;
		}
		gs_texrender_end(sensor->texrender);
	}

	gs_blend_state_pop();

	gs_effect_t *effect2 = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_texture_t *tex = gs_texrender_get_texture(sensor->texrender);

	if (tex && obs_source_active(sensor->game_capture_source)) {
		gs_stage_texture(sensor->staging_texture, tex);

		uint8_t *data;
		uint32_t linesize;
		if (gs_stagesurface_map(sensor->staging_texture, &data, &linesize)) {
			memcpy(sensor->data, data, linesize * sensor->height);
			sensor->linesize = linesize;

			gs_stagesurface_unmap(sensor->staging_texture);
		}

		gs_eparam_t *image = gs_effect_get_param_by_name(effect2, "image");
		gs_effect_set_texture(image, tex);

		while (gs_effect_loop(effect2, "Draw"))
			gs_draw_sprite(tex, 0, sensor->width, sensor->height);
	} else {
		success = false;
	}

	obs_leave_graphics();
	return success;
}

bool send_data_to_api(struct healthbar_sensor_webcam_frame *sensor) 
{
	bool success = false;
	uint8_t *data = bzalloc(sensor->linesize * sensor->height);
	memcpy(data, sensor->data, sensor->linesize * sensor->height);

	FILE *of = fopen(RAWSCREENSHOT, "wb");
	if (of != NULL) {
		fwrite(data, 1, sensor->linesize * sensor->height, of);
		fclose(of);
	}

	bfree(data);

	FILE *fd;
  	fd = fopen(RAWSCREENSHOT, "rb");
	
	sensor->curl = curl_easy_init();

	if (sensor->curl) {
		curl_easy_setopt(sensor->curl, CURLOPT_URL, API_URL);
		curl_easy_setopt(sensor->curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(sensor->curl, CURLOPT_READFUNCTION, read_callback);
		curl_easy_setopt(sensor->curl, CURLOPT_READDATA, fd);
		curl_easy_setopt(sensor->curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(sensor->curl, CURLOPT_WRITEFUNCTION, got_data_from_api);

		CURLcode result = curl_easy_perform(sensor->curl);
		
		if (result == CURLE_OK) {
			success = true;
			blog(LOG_INFO, "HSWF - CURL result working");
		} else {
			blog(LOG_INFO, "HSWF - CURL error: %s", curl_easy_strerror(result));
		}
	} else {
		blog(LOG_INFO, "HSWF - CURL init failed");
	}

	fclose(fd);
	return success;
}

void check_for_current_scene(struct healthbar_sensor_webcam_frame *sensor)
{
	obs_source_t *current_scene_source = obs_frontend_get_current_scene();
	if (current_scene_source) {
		obs_scene_t *current_scene = obs_scene_from_source(current_scene_source);
		sensor->current_scene = current_scene;
		obs_source_release(current_scene_source);
	}
}

void check_for_game_capture_source(struct healthbar_sensor_webcam_frame *sensor)
{
	if (sensor->current_scene) {
		obs_sceneitem_t *game_capture_item = obs_scene_find_source(
				sensor->current_scene, sensor->game_capture_source_name);

		if (game_capture_item && obs_sceneitem_visible(game_capture_item)) {
			obs_source_t *game_capture_source = obs_sceneitem_get_source(game_capture_item);
			
			sensor->game_capture_source = game_capture_source;
			sensor->width = obs_source_get_width(game_capture_source);
			sensor->height = obs_source_get_height(game_capture_source);

			obs_enter_graphics();
			if (sensor->staging_texture)
				gs_stagesurface_destroy(sensor->staging_texture);

			sensor->staging_texture = gs_stagesurface_create(
					sensor->width, sensor->height, GS_RGBA);
			obs_leave_graphics();

			if (sensor->data) {
				bfree(sensor->data);
				sensor->data = NULL;
			}
			sensor->data = bzalloc((sensor->width + 32) * sensor->height * 4);
		} else if (sensor->game_capture_source) {
			sensor->game_capture_source = 0;
			sensor->width = 0;
			sensor->height = 0;
		}
	} else {
		check_for_current_scene(sensor);
	}
}

void *thread_take_screenshot_and_send_to_api(void *sensor)
{
    struct healthbar_sensor_webcam_frame *my_sensor =
		(struct healthbar_sensor_webcam_frame *) sensor;
	
	//wait
    sem_wait(&my_sensor->mutex);
	blog(LOG_INFO, "HSWF - semaphore: Entered...");

	bool success = false;

	if (obs_source_active(my_sensor->game_capture_source) &&
				my_sensor->width > 10 && my_sensor->height > 10 &&
				render_game_capture(my_sensor)) {
		success = send_data_to_api(my_sensor);
	} else {
		if (my_sensor->current_frame != 0) {
			change_webcam_frame_to_file(my_sensor, my_sensor->no_lifebar_frame_path);
			my_sensor->current_frame = 0;
			my_sensor->no_lifebar_found_counter = 0;
		}

		check_for_game_capture_source(my_sensor);
	}

	//if playing valorant success_sleep_time = 500
	//otherwise: 700
	int success_sleep_time = 700;
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
				if (my_sensor->no_lifebar_found_counter > 0)
					my_sensor->no_lifebar_found_counter = 0;
				
				if (lifePerc > 0.66 && my_sensor->current_frame != 1) {
					change_webcam_frame_to_file(my_sensor, my_sensor->high_health_frame_path);
					my_sensor->current_frame = 1;
				} else if (lifePerc > 0.33 && lifePerc <= 0.66 && my_sensor->current_frame != 2) {
					change_webcam_frame_to_file(my_sensor, my_sensor->medium_health_frame_path);
					my_sensor->current_frame = 2;
				} else if (lifePerc <= 0.33 && my_sensor->current_frame != 3) {
					change_webcam_frame_to_file(my_sensor, my_sensor->low_health_frame_path);
					my_sensor->current_frame = 3;
				}

			} else if (my_sensor->current_frame != 0) {
				my_sensor->no_lifebar_found_counter++;

				if (my_sensor->no_lifebar_found_counter > 6) {
					change_webcam_frame_to_file(my_sensor, my_sensor->no_lifebar_frame_path);
					my_sensor->current_frame = 0;
					my_sensor->no_lifebar_found_counter = 0;
				}
			}
		}
	}
      
    if (!my_sensor->is_on_destroy) {
		my_sensor->is_on_sleep = true;
		Sleep(sleep_time);
		my_sensor->is_on_sleep = false;
	}
	//signal
	blog(LOG_INFO, "HSWF - semaphore: Just Exiting...");
    sem_post(&my_sensor->mutex);
}

static void hswf_update(void *data, obs_data_t *settings)
{
	blog(LOG_INFO, "HSWF - ENTERING UPDATE");

	struct healthbar_sensor_webcam_frame *sensor = data;

	const char *game_capture_source_name =
			obs_data_get_string(settings, "game_capture_source_name");
	const char *no_lifebar_frame_path =
			obs_data_get_string(settings, "no_lifebar_frame_path");
	const char *high_health_frame_path =
			obs_data_get_string(settings, "high_health_frame_path");
	const char *medium_health_frame_path =
			obs_data_get_string(settings, "medium_health_frame_path");
	const char *low_health_frame_path =
			obs_data_get_string(settings, "low_health_frame_path");

	if (sensor->game_capture_source_name)
		bfree(sensor->game_capture_source_name);
	sensor->game_capture_source_name = bstrdup(game_capture_source_name);

	check_for_game_capture_source(sensor);

	if (sensor->frame_path)
		bfree(sensor->frame_path);
	sensor->frame_path = bstrdup(no_lifebar_frame_path);
	
	if (sensor->no_lifebar_frame_path)
		bfree(sensor->no_lifebar_frame_path);
	sensor->no_lifebar_frame_path = bstrdup(no_lifebar_frame_path);

	if (sensor->high_health_frame_path)
		bfree(sensor->high_health_frame_path);
	sensor->high_health_frame_path = bstrdup(high_health_frame_path);

	if (sensor->medium_health_frame_path)
		bfree(sensor->medium_health_frame_path);
	sensor->medium_health_frame_path = bstrdup(medium_health_frame_path);

	if (sensor->low_health_frame_path)
		bfree(sensor->low_health_frame_path);
	sensor->low_health_frame_path = bstrdup(low_health_frame_path);

	sensor->current_frame = 0;
	sensor->no_lifebar_found_counter = 0;

	if (sensor->media_valid) {
		mp_media_free(&sensor->media);
		sensor->media_valid = false;
	}

	bool active = obs_source_active(sensor->context);
	if (active) {
		hswf_media_open(sensor);
		hswf_media_start(sensor);
	}

	blog(LOG_INFO, "HSWF - EXITING UPDATE");
}

static void *hswf_create(obs_data_t *settings, obs_source_t *context)
{
	UNUSED_PARAMETER(settings);

	blog(LOG_INFO, "HSWF - ENTERING CREATE");

	struct healthbar_sensor_webcam_frame *sensor =
		bzalloc(sizeof(struct healthbar_sensor_webcam_frame));
	sensor->context = context;

	curl_global_init(0);
	
	sem_init(&sensor->mutex, 0, 1);
	sensor->is_on_destroy = false;
	sensor->is_on_sleep = false;

	check_for_current_scene(sensor);

	obs_enter_graphics();
	sensor->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();
	
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

	blog(LOG_INFO, "HSWF - EXITING CREATE");

	hswf_update(sensor, settings);
	return sensor;
}

static void hswf_destroy(void *data)
{
	blog(LOG_INFO, "HSWF - ENTERING DESTROY");
	struct healthbar_sensor_webcam_frame *sensor = data;

	sensor->is_on_destroy = true;
	if(!sensor->is_on_sleep)
		sem_wait(&sensor->mutex);

	if (sensor->hotkey)
		obs_hotkey_unregister(sensor->hotkey);

	if (sensor->media_valid)
		mp_media_free(&sensor->media);

	if (sensor->game_capture_source_name)
		bfree(sensor->game_capture_source_name);
	
	if (sensor->frame_path)
		bfree(sensor->frame_path);
	
	if (sensor->no_lifebar_frame_path)
		bfree(sensor->no_lifebar_frame_path);
	if (sensor->high_health_frame_path)
		bfree(sensor->high_health_frame_path);
	if (sensor->medium_health_frame_path)
		bfree(sensor->medium_health_frame_path);
	if (sensor->low_health_frame_path)
		bfree(sensor->low_health_frame_path);

	if (sensor->curl)
		curl_easy_cleanup(sensor->curl);
	curl_global_cleanup();

	obs_enter_graphics();
	gs_texrender_destroy(sensor->texrender);

	if (sensor->staging_texture)
		gs_stagesurface_destroy(sensor->staging_texture);

	obs_leave_graphics();

	if (sensor->data)
		bfree(sensor->data);

	if(!sensor->is_on_sleep)
		sem_post(&sensor->mutex);

	sem_destroy(&sensor->mutex);

	blog(LOG_INFO, "HSWF - EXITING DESTROY");
	bfree(sensor);
}

static void hswf_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct healthbar_sensor_webcam_frame *sensor = data;

	int value;
    sem_getvalue(&sensor->mutex, &value);

	if (value == 1 && !sensor->is_on_destroy) {
		pthread_t thread;
    	pthread_create(&thread, NULL, thread_take_screenshot_and_send_to_api, (void*) sensor);
	}
}

static obs_missing_files_t *hswf_missingfiles(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(sensor->frame_path, "") != 0 && !os_file_exists(sensor->frame_path)) {
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

static const char *video_filter =
	" (*.mp4 *.ts *.mov *.flv *.mkv *.avi *.gif *.webm);;";

static obs_properties_t *hswf_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "game_capture_source_name",
			"Name of source where lifebar is found", OBS_TEXT_DEFAULT);
	obs_properties_add_path(props, "no_lifebar_frame_path",
			"Webcam frame to show when no lifebar is found",
			OBS_PATH_FILE, video_filter, NO_LIFEBAR_DEFAULT_FRAME);
	obs_properties_add_path(props, "high_health_frame_path",
			"Webcam frame to show when health is high",
			OBS_PATH_FILE, video_filter, HIGH_HEALTH_DEFAULT_FRAME);
	obs_properties_add_path(props, "medium_health_frame_path",
			"Webcam frame to show when health is medium",
			OBS_PATH_FILE, video_filter, MEDIUM_HEALTH_DEFAULT_FRAME);
	obs_properties_add_path(props, "low_health_frame_path",
			"Webcam frame to show when health is low",
			OBS_PATH_FILE, video_filter, LOW_HEALTH_DEFAULT_FRAME);

	UNUSED_PARAMETER(data);
	return props;
}

static void hswf_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "game_capture_source_name",
			"Captura de juego");
	obs_data_set_default_string(settings,
			"no_lifebar_frame_path", NO_LIFEBAR_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"high_health_frame_path", HIGH_HEALTH_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"medium_health_frame_path", MEDIUM_HEALTH_DEFAULT_FRAME);
	obs_data_set_default_string(settings,
			"low_health_frame_path", LOW_HEALTH_DEFAULT_FRAME);
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
