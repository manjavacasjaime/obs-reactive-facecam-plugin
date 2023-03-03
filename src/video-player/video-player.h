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

void rfp_restart_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			   bool pressed);
bool rfp_play_hotkey(void *data, obs_hotkey_pair_id id,
				      obs_hotkey_t *hotkey, bool pressed);
bool rfp_pause_hotkey(void *data, obs_hotkey_pair_id id,
				       obs_hotkey_t *hotkey, bool pressed);
void rfp_stop_hotkey(void *data, obs_hotkey_id id,
				      obs_hotkey_t *hotkey, bool pressed);

void rfp_media_open(struct reactive_facecam_plugin *plugin);
void rfp_media_start(struct reactive_facecam_plugin *plugin);
void change_facecam_frame_to_file(struct reactive_facecam_plugin *plugin,
				char *new_frame_path);

obs_missing_files_t *rfp_missingfiles(void *data);

void rfp_play_pause(void *data, bool pause);
void rfp_restart(void *data);
void rfp_stop(void *data);
int64_t rfp_get_duration(void *data);
int64_t rfp_get_time(void *data);
void rfp_set_time(void *data, int64_t ms);
enum obs_media_state rfp_get_state(void *data);
