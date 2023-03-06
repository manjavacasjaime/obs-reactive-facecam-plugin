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

#define RAWSCREENSHOT (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/test/raw_screenshot"

#define VALORANT_API_URL (char *)"http://127.0.0.1:8080/healthbar-reader-service/valorant/fullhd"
#define APEX_API_URL (char *)"http://127.0.0.1:8080/healthbar-reader-service/apex/fullhd"
#define SETTING_GAME_VALORANT 0
#define SETTING_GAME_APEX 1

extern struct json_object *parsed_json;
extern struct json_object *isImageIdentified;
extern struct json_object *errorMessage;
extern struct json_object *isLifeBarFound;
extern struct json_object *lifePercentage;

struct json_object *json_tokener_parse(const char *str);
bool json_object_object_get_ex(const struct json_object *obj, const char *key,
                                struct json_object **value);
const char *json_object_get_string(struct json_object *jso);
bool json_object_get_boolean(const struct json_object *jso);
int json_object_put(struct json_object *jso);

size_t got_data_from_api(char *buffer, size_t itemsize, size_t nitems, void* ignorethis);
size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
bool send_data_to_api(struct reactive_facecam_plugin *plugin);