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
#include <time.h>
#include <ftw.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define GREENCAMFRAME  (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/GreenMarco.webm"
#define REDCAMFRAME (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/RedMarco02.webm"
#define TESTIMAGE (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/testImage.png"
#define API_URL (char *)"http://127.0.0.1:8080/post"

#define BUSCARJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/buscarJJ.png"
#define CURANDOJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/curandoJJ.png"
#define LEYENDASJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/leyendasJJ.png"
#define LLENABLANCOJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/llenaBlancoJJ.png"
#define LLENAJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/llenaJJ.png"
#define LLENANEGROJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/llenaNegroJJ.png"
#define POCAEMPTYJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/pocaEmptyJJ.png"
#define POCAROJAJJ (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/test/pocaRojaJJ.png"

#define CAYENDODUOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/cayendoDuos.png"
#define CAYENDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/cayendoTrios.png"
#define CURANDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/curandoTrios.png"
#define ESCUDODUOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/escudoDuos.png"
#define ESCUDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/escudoTrios.png"
#define POCAVIDADUOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/pocaVidaDuos.png"
#define POCAVIDATRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/pocaVidaTrios.png"
#define REANIMANDOTRIOS (char *)"../../data/obs-plugins/obs-healthbar-sensor-webcam-frame/frames/newTest/reanimandoTrios.png"


struct json_object *parsed_json;
struct json_object *isImageIdentified;
struct json_object *errorMessage;
struct json_object *isLifeBarFound;
struct json_object *lifePercentage;

time_t newestFileTime = 0;
char newestFilePath[PATH_MAX];

struct healthbar_sensor_webcam_frame {
	obs_source_t *context;
	mp_media_t media;
	bool media_valid;

	char *framePath;
	
	char *text;
	int someNumber;

	sem_t mutex;
	//0 is green, 1 is red
	int currentFrame;
	
	obs_source_t *currentScene;
	char *screenshotPath;
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

int check_if_newer_file(const char *path, const struct stat *sb, int typeflag)
{
    if (typeflag == FTW_F && sb->st_mtime > newestFileTime) {
        newestFileTime = sb->st_mtime;
        strncpy(newestFilePath, path, PATH_MAX);
    }

	return 0;
}

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
	if (sensor->framePath && *sensor->framePath) {
		struct mp_media_info info = {
			.opaque = sensor,
			.v_cb = get_frame,
			.v_preload_cb = preload_frame,
			.v_seek_cb = seek_frame,
			.a_cb = get_audio,
			.stop_cb = media_stopped,
			.path = sensor->framePath,
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
				char *newFramePath)
{
	if (sensor->media_valid) {
		mp_media_free(&sensor->media);
		sensor->media_valid = false;
	}

	bfree(sensor->framePath);
	sensor->framePath = newFramePath ? bstrdup(newFramePath) : NULL;

	bool active = obs_source_active(sensor->context);
	if (active) {
		hswf_media_open(sensor);
		hswf_media_start(sensor);
	}
}

void *thread_take_screenshot_and_send_to_api(void *sensor)
{
    struct healthbar_sensor_webcam_frame *my_sensor =
		(struct healthbar_sensor_webcam_frame *) sensor;
	
	//wait
    sem_wait(&my_sensor->mutex);
	blog(LOG_INFO, "HSWF - semaphore: Entered...");
  
    //critical section
    obs_frontend_take_source_screenshot(my_sensor->currentScene);
	
	//get last file from screenshotPath
	ftw(my_sensor->screenshotPath, check_if_newer_file, 1);
	blog(LOG_INFO, "HSWF - Newest file: %s", newestFilePath);

	FILE *fd;
  	fd = fopen(newestFilePath, "rb");

	my_sensor->curl = curl_easy_init();

	if (my_sensor->curl) {
		curl_easy_setopt(my_sensor->curl, CURLOPT_URL, API_URL);
		curl_easy_setopt(my_sensor->curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(my_sensor->curl, CURLOPT_READFUNCTION, read_callback);
		curl_easy_setopt(my_sensor->curl, CURLOPT_READDATA, fd);
		curl_easy_setopt(my_sensor->curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(my_sensor->curl, CURLOPT_WRITEFUNCTION, got_data_from_api);

		CURLcode result = curl_easy_perform(my_sensor->curl);
		
		if (result == CURLE_OK) {
			blog(LOG_INFO, "HSWF - CURL result working");
		} else {
			blog(LOG_INFO, "HSWF - CURL error: %s", curl_easy_strerror(result));
		}
	} else {
		blog(LOG_INFO, "HSWF - CURL init failed");
	}

	fclose(fd);
	remove(newestFilePath);

	char *ptr;
	bool isImgId = false;
	bool isLBFound = false;
	double lifePerc = 0.0;

	if (isImageIdentified && isLifeBarFound && lifePercentage) {
		isImgId = json_object_get_boolean(isImageIdentified);
		isLBFound = json_object_get_boolean(isLifeBarFound);
		lifePerc = strtod(json_object_get_string(lifePercentage), &ptr);
	}

	if (isImgId && isLBFound) {
		if (lifePerc > 0.5 && my_sensor->currentFrame != 0) {
			change_webcam_frame_to_file(my_sensor, GREENCAMFRAME);
			my_sensor-> currentFrame = 0;
		} else if (lifePerc <= 0.5 && my_sensor->currentFrame != 1) {
			change_webcam_frame_to_file(my_sensor, REDCAMFRAME);
			my_sensor-> currentFrame = 1;
		}
	}
      
    sleep(4);
	//signal
	blog(LOG_INFO, "HSWF - semaphore: Just Exiting...");
    sem_post(&my_sensor->mutex);
}

static void hswf_update(void *data, obs_data_t *settings)
{
	blog(LOG_INFO, "HSWF - ENTERING UPDATE");

	struct healthbar_sensor_webcam_frame *sensor = data;
	bfree(sensor->framePath);
	char *framePath = GREENCAMFRAME;

	const char *text = obs_data_get_string(settings, "text");
	const int someNumber = obs_data_get_int(settings, "someNumber");

	sensor->framePath = framePath ? bstrdup(framePath) : NULL;
	if (sensor->text)
		bfree(sensor->text);
	sensor->text = bstrdup(text);
	sensor->someNumber = someNumber;

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

	sem_init(&sensor->mutex, 0, 1);
	sensor->currentFrame = 0;

	obs_source_t *currentScene = obs_frontend_get_current_scene();
	sensor->currentScene = currentScene;
	obs_source_release(currentScene);

	const char *screenshotPath = obs_frontend_get_current_record_output_path();
	if (sensor->screenshotPath)
		bfree(sensor->screenshotPath);
	sensor->screenshotPath = bstrdup(screenshotPath);
	bfree((void *) screenshotPath);
	
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

	if (sensor->hotkey)
		obs_hotkey_unregister(sensor->hotkey);

	if (sensor->media_valid)
		mp_media_free(&sensor->media);

	if (sensor->text)
		bfree(sensor->text);

	if (sensor->screenshotPath)
		bfree(sensor->screenshotPath);

	if (sensor->curl)
		curl_easy_cleanup(sensor->curl);

	bfree(sensor->framePath);
	sem_destroy(&sensor->mutex);

	blog(LOG_INFO, "HSWF - EXITING DESTROY");
	bfree(sensor);
}

static void hswf_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct healthbar_sensor_webcam_frame *sensor = data;

	pthread_t thread;
    pthread_create(&thread, NULL, thread_take_screenshot_and_send_to_api, (void*) sensor);
}

static obs_missing_files_t *hswf_missingfiles(void *data)
{
	struct healthbar_sensor_webcam_frame *sensor = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(sensor->framePath, "") != 0 && !os_file_exists(sensor->framePath)) {
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

static void hswf_mouse_click(void *data, const struct obs_mouse_event *event,
				int32_t type, bool mouse_up, uint32_t click_count)
{
	UNUSED_PARAMETER(event);
	UNUSED_PARAMETER(type);
	UNUSED_PARAMETER(click_count);
	
	if (mouse_up)
		return;

	struct healthbar_sensor_webcam_frame *sensor = data;
	char *framePath;

	if (strcmp(sensor->framePath, GREENCAMFRAME) == 0) {
		framePath = REDCAMFRAME;
	} else {
		framePath = GREENCAMFRAME;
	}

	if (sensor->media_valid) {
		mp_media_free(&sensor->media);
		sensor->media_valid = false;
	}

	bfree(sensor->framePath);
	sensor->framePath = framePath ? bstrdup(framePath) : NULL;

	bool active = obs_source_active(sensor->context);
	if (active) {
		hswf_media_open(sensor);
		hswf_media_start(sensor);
	}
}


//.output_flags = OBS_SOURCE_ASYNC_VIDEO,
//solo este flag hace que se crashee a veces
//quizas es porque necesita los hotkey methods
//En cuanto quite el mouse_click, OBS_SOURCE_INTERACTION se va
struct obs_source_info healthbar_sensor_webcam_frame = {
	.id = "healthbar_sensor_webcam_frame",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO |
			OBS_SOURCE_DO_NOT_DUPLICATE |
			OBS_SOURCE_CONTROLLABLE_MEDIA |
			OBS_SOURCE_INTERACTION,
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
	.mouse_click = hswf_mouse_click,
};
