# OBS Reactive Facecam Plugin

OBS plugin that provides a fancy facecam frame which changes depending on your health bar status. Some default frames come with the plugin, but you can add yours if you want to.

Right now, Reactive Facecam Plugin only works with Valorant and Apex Legends, but please feel free to escalate it to other games.
Note that the plugin depends on [healthbar-reader-service](https://github.com/manjavacasjaime/healthbar-reader-service) API, so check the API's page in order to know if it is up or down.

This project has been coded using mooware's [OBS-Studio-Flash-Filter](https://github.com/puri-puri/OBS-Studio-Flash-Filter) as a reference.

### Sample video (click to view on Youtube)

[![Youtube video: OBS Reactive Facecam Plugin sample](https://img.youtube.com/vi/Hx1ezYTnC2Q/0.jpg)](https://www.youtube.com/watch?v=Hx1ezYTnC2Q)

# Installation

Download from the [Releases page](https://github.com/manjavacasjaime/obs-reactive-facecam-plugin/releases).

Extract the zip archive into the root folder of your OBS installation.

### Some resources coming with the plugin

Inside *obs-reactive-facecam-plugin/data/obs-plugins/obs-reactive-facecam-plugin/frames* folder, you will find the default facecam frames and an Alpha Mask that you can use as an Effect Filter for your webcam.

# Download and modify the code

Please make sure to download and build OBS from its source code as it's explained [here](https://obsproject.com/wiki/install-instructions). This step has to be done before specifying this plugin's CMake variables.

Once you have downloaded Reactive Facecam Plugin's source code, you'll need to build it using CMake in order to generate its binaries.
CMake variables that need to be specified:

| Name                  | Value                                                                           |
| --------------------- | ------------------------------------------------------------------------------- |
| LIBOBS_INCLUDE_DIR    | .../obs-studio/libobs                                                           |
| LIBOBS_LIB            | .../obs-studio/build/libobs/Release/obs.lib                                     |
| DepsPath              | .../obs-studio-deps/win64                                                       |
| OBS_FRONTEND_LIB      | .../obs-studio/build/UI/obs-frontend-api/Release/obs-frontend-api.lib           |
| CURL_INCLUDE_DIR      | .../obs-studio-deps/win64/include                                               |
| CURL_LIBRARY          | .../obs-studio-deps/win64/bin/libcurl.lib                                       |
| JSON_C_INCLUDE_DIR    | obs-reactive-facecam-plugin/deps/json-c-json-c-0.15-20200726                    |
| JSON_C_LIBRARY        | obs-reactive-facecam-plugin/deps/json-c-json-c-0.15-20200726/build/libjson-c.a  |

This project was my university thesis :) If you need more info about its details (references, installation tips, diagrams, usage), the thesis itself can be found within the *deps* folder.
