#include <src/defs.h>
#include <src/game-capture/game-capture.h>

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
