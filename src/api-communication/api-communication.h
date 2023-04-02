/*
This module is responsible for sending the Game Capture Source screenshot to an API.
The API's response will be the player's health bar status and the facecam will
change according to it.
*/

#define RAWSCREENSHOT (char *)"../../data/obs-plugins/obs-reactive-facecam-plugin/frames/apiFiles-DO_NOT_DELETE/raw_screenshot"

#define VALORANT_API_URL (char *)"http://35.181.56.79/healthbar-reader-service/valorant/fullhd"
#define APEX_API_URL (char *)"http://35.181.56.79/healthbar-reader-service/apex/fullhd"
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
