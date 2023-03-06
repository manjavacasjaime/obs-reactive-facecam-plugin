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

#include <src/defs.h>
#include <src/api-communication/api-communication.h>

struct json_object *parsed_json;
struct json_object *isImageIdentified;
struct json_object *errorMessage;
struct json_object *isLifeBarFound;
struct json_object *lifePercentage;

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
