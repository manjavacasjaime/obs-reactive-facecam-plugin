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

struct reactive_facecam_plugin {
	obs_source_t *context;
	uint32_t width;
	uint32_t height;

	sem_t mutex;
	bool is_on_destroy;
	bool is_on_sleep;

	/*video-player variables*/

	mp_media_t media;
	bool media_valid;

	enum obs_media_state state;
	obs_hotkey_id hotkey;
	obs_hotkey_pair_id play_pause_hotkey;
	obs_hotkey_id stop_hotkey;

	char *frame_path;
	
	char *no_lifebar_frame_path;
	char *high_health_frame_path;
	char *medium_high_health_frame_path;
	char *medium_low_health_frame_path;
	char *low_health_frame_path;
	char *zero_health_frame_path;

	//0 is no_lifebar, 1 is high, 2 is medium_high,
	//3 is medium_low, 4 is low and 5 is zero_health
	int current_frame;

	/*game-capture variables*/

	uint8_t *data;
	uint32_t linesize;
	gs_texrender_t *texrender;
	gs_stagesurf_t *staging_texture;

	obs_scene_t *current_scene;
	char *game_capture_source_name;
	obs_source_t *game_capture_source;

	/*api-communication variables*/
	
	int game;
	CURL *curl;

	int no_lifebar_found_counter;
};
