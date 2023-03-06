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

bool render_game_capture(struct reactive_facecam_plugin *plugin);

void check_for_current_scene(struct reactive_facecam_plugin *plugin);
void check_for_game_capture_source(struct reactive_facecam_plugin *plugin);
