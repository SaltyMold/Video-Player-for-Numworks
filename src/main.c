#include "libs/eadk.h"
#include "libs/storage.h"
#include "libs/TJpg_Decoder/tjpgd.h"
#include <string.h>
#include <stdlib.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Video Player";
const uint32_t eadk_api_level  __attribute__((section(".rodata.eadk_api_level"))) = 0;

typedef struct {
	const uint8_t* data;
	size_t size;
	size_t pos;
} memdev_t;

static size_t infunc(JDEC* jd, uint8_t* buff, size_t ndata) {
	memdev_t* dev = (memdev_t*)jd->device;
	if (!dev || !dev->data || dev->pos >= dev->size) return 0;
	size_t remain = dev->size - dev->pos;
	size_t rc = ndata < remain ? ndata : remain;
	if (buff) memcpy(buff, dev->data + dev->pos, rc);
	dev->pos += rc;
	return rc;
}

static int outfunc(JDEC* jd, void* bitmap, JRECT* rect) {
	if (!bitmap || !rect) return 0;
	uint16_t w = rect->right - rect->left + 1;
	uint16_t h = rect->bottom - rect->top + 1;

	static eadk_color_t linebuf[320];
	if (w > 320) return 0; 

	for (uint16_t row = 0; row < h; row++) {
		uint16_t* src16 = (uint16_t*)((uint8_t*)bitmap + (size_t)row * (size_t)w * 2);
		for (uint16_t x = 0; x < w; x++) {
			uint16_t pix = src16[x];
			if (jd && jd->swap) pix = (uint16_t)((pix >> 8) | (pix << 8));
			linebuf[x] = (eadk_color_t)pix;
		}
		eadk_rect_t rct = {(uint16_t)(rect->left), (uint16_t)(rect->top + row), w, 1};
		eadk_display_push_rect(rct, linebuf);
	}
	return 1;
}

static uint8_t jd_workbuf[48 * 1024];

extern const char* eadk_external_data;
extern size_t eadk_external_data_size;

int change_target_fps(int target_fps) {
	eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_white);
	eadk_display_draw_string("Change target FPS:", (eadk_point_t){0,0}, true, eadk_color_black, eadk_color_white);
	eadk_display_draw_string("Use UP/DOWN keys to adjust", (eadk_point_t){0,30}, false, eadk_color_black, eadk_color_white);
	eadk_display_draw_string("Press OK to confirm", (eadk_point_t){0,50}, false, eadk_color_black, eadk_color_white);

	while (1) {
		eadk_keyboard_state_t state = eadk_keyboard_scan();
		if (eadk_keyboard_key_down(state, eadk_key_up)) target_fps++;
		if (eadk_keyboard_key_down(state, eadk_key_down) && target_fps > 1) target_fps--;
		if (eadk_keyboard_key_down(state, eadk_key_ok)) break;

		char buf[32];
		eadk_display_push_rect_uniform((eadk_rect_t){0,100,320,20}, eadk_color_white);
		snprintf(buf, sizeof(buf), "Target FPS: %d", target_fps);
		eadk_display_draw_string(buf, (eadk_point_t){0,100}, true, eadk_color_black, eadk_color_white);
		eadk_timing_msleep(100);
	}

	return target_fps;
}

int main(void) {
	eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_white);

	if (!eadk_external_data || eadk_external_data_size == 0) {
		eadk_display_draw_string("No external data", (eadk_point_t){10,10}, true, eadk_color_white, eadk_color_black);
		while (1) eadk_keyboard_scan();
	}

	memdev_t dev;
	dev.data = (const uint8_t*)eadk_external_data;
	dev.size = eadk_external_data_size;
	dev.pos = 0;

	int target_fps = 10;
	uint64_t first_frame_ts_ms = eadk_timing_millis();

	for (;;) {
		size_t p = 0;
		bool found_any = false;

		while (p + 1 < dev.size) {
			bool debug = false;
			if (eadk_keyboard_key_down(eadk_keyboard_scan(), eadk_key_exe)) debug = true;
			if (eadk_keyboard_key_down(eadk_keyboard_scan(), eadk_key_shift)) target_fps = change_target_fps(target_fps);

			uint64_t start_frame_ts_ms = eadk_timing_millis();
			
			//----------------------------

			size_t soi = (size_t)-1;
			for (; p + 1 < dev.size; p++) {
				if (dev.data[p] == 0xFF && dev.data[p+1] == 0xD8) { soi = p; p += 2; break; }
			}
			if (soi == (size_t)-1) break;

			size_t eoi = (size_t)-1;
			for (; p + 1 < dev.size; p++) {
				if (dev.data[p] == 0xFF && dev.data[p+1] == 0xD9) { eoi = p + 1; p += 2; break; }
			}
			if (eoi == (size_t)-1) break;

			found_any = true;
			size_t frame_start = soi;
			size_t frame_len = eoi - soi + 1;

			memdev_t frame_dev;
			frame_dev.data = dev.data + frame_start;
			frame_dev.size = frame_len;
			frame_dev.pos = 0;

			JDEC jd;
			memset(&jd, 0, sizeof(jd));
			JRESULT res = jd_prepare(&jd, infunc, jd_workbuf, sizeof(jd_workbuf), &frame_dev);
			if (res == JDR_OK) {
				jd_decomp(&jd, outfunc, 0);
			}

			if (eadk_keyboard_key_down(eadk_keyboard_scan(), eadk_key_back)) return 0;

			//----------------------------

			uint64_t end_frame_ts_ms = eadk_timing_millis();

			uint64_t frame_duration_ms = end_frame_ts_ms - start_frame_ts_ms;

			int sleep_ms = (1000 / target_fps) > frame_duration_ms ? (1000 / target_fps) - frame_duration_ms : 0;

			if (debug){
				char buf[64];
				snprintf(buf, sizeof(buf), "start_frame_ts_ms: %d", (int)(start_frame_ts_ms));
				eadk_display_draw_string(buf, (eadk_point_t){0, 0}, false, eadk_color_black, eadk_color_white);
				
				snprintf(buf, sizeof(buf), "end_frame_ts_ms: %d", (int)(end_frame_ts_ms));
				eadk_display_draw_string(buf, (eadk_point_t){0, 12}, false, eadk_color_black, eadk_color_white);

				snprintf(buf, sizeof(buf), "frame_duration_ms: %d", (int)(frame_duration_ms));
				eadk_display_draw_string(buf, (eadk_point_t){0, 24}, false, eadk_color_black, eadk_color_white);

				snprintf(buf, sizeof(buf), "target_fps: %d", target_fps);
				eadk_display_draw_string(buf, (eadk_point_t){0, 36}, false, eadk_color_black, eadk_color_white);

				snprintf(buf, sizeof(buf), "sleep_ms: %d", (int)(sleep_ms));
				eadk_display_draw_string(buf, (eadk_point_t){0, 48}, false, eadk_color_black, eadk_color_white);
		
				snprintf(buf, sizeof(buf), "fps_no_cap: %d", (int)(1000 / frame_duration_ms));
				eadk_display_draw_string(buf, (eadk_point_t){0, 60}, false, eadk_color_black, eadk_color_white);

				snprintf(buf, sizeof(buf), "fps_capped: %d", (int)(1000 / (frame_duration_ms + sleep_ms)));
				eadk_display_draw_string(buf, (eadk_point_t){0, 72}, false, eadk_color_black, eadk_color_white);
			}

			eadk_timing_msleep(sleep_ms);
		}
		if (!found_any) break;
	}
	while (1) eadk_keyboard_scan();
	return 0;
}