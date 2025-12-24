#include "libs/eadk.h"
#include "libs/TJpg_Decoder/tjpgd.h"
#include <string.h>
#include <stdlib.h>

#define ENABLE_STORAGE 1 // No storage for simulator

#if ENABLE_STORAGE
#include "libs/storage.h"
#endif

#define ENABLE_VBLANK 0 // Don't use it. It add delay between push that generates tearing effect.

#define SVG_FILE "video.svg"
/*
1st byte = target fps
*/

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Video Player";
const uint32_t eadk_api_level  __attribute__((section(".rodata.eadk_api_level"))) = 0;

typedef struct {
	const uint8_t* data;
	size_t size;
	size_t pos;
} memdev_t;

static void fill_composed_buf(eadk_color_t *buf, size_t n, eadk_color_t val) {
	if (!buf || n == 0) return;
	size_t i = 0;
	uint32_t v32 = ((uint32_t)val << 16) | (uint32_t)val;
	uint32_t *p32 = (uint32_t *)buf;
	size_t n32 = n / 2;
	for (i = 0; i < n32; i++) p32[i] = v32;
	if (n & 1) buf[n - 1] = val;
}

static size_t infunc(JDEC* jd, uint8_t* buff, size_t ndata) {
	memdev_t* dev = (memdev_t*)jd->device;
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

	enum { BLOCK_W = 16, BLOCK_H = 16, COLS = 20, ROWS = 5 };
	enum { COMPOSE_W = COLS * BLOCK_W, COMPOSE_H = ROWS * BLOCK_H };

	if (w > BLOCK_W || h > BLOCK_H) return 0;

	uint16_t* src = (uint16_t*)bitmap;

	enum { BANDS = 3 };
	static eadk_color_t composed_buf[COMPOSE_W * COMPOSE_H];
	static int composed_blocks = 0; 
	static int current_band = 0;

		int rect_band = ((uint16_t)rect->top) / COMPOSE_H;
		if (rect_band >= BANDS) rect_band = BANDS - 1;
		if (rect_band != current_band && composed_blocks > 0) {
			eadk_rect_t rct = { 0, (uint16_t)(current_band * COMPOSE_H), COMPOSE_W, COMPOSE_H };

			#if ENABLE_VBLANK
			eadk_display_wait_for_vblank();
			#endif
			
			eadk_display_push_rect(rct, composed_buf);
			fill_composed_buf(composed_buf, (COMPOSE_W * COMPOSE_H), eadk_color_white);
			composed_blocks = 0;
			current_band = rect_band;
		}

		bool swap = (jd && jd->swap) ? true : false;
		{
			uint16_t left = (uint16_t)rect->left;
			uint16_t top = (uint16_t)rect->top;
			uint16_t band_top = (uint16_t)(current_band * COMPOSE_H);
			uint16_t max_col_bound = (left >= COMPOSE_W) ? 0 : (COMPOSE_W - left);
			for (uint16_t row = 0; row < h; row++) {
				uint16_t dy = top + row;
				if (dy < band_top || dy >= (band_top + COMPOSE_H)) continue;
				if (max_col_bound == 0) continue;
				uint16_t band_y = dy - band_top;
				eadk_color_t *dest_row = &composed_buf[band_y * COMPOSE_W];
				uint16_t *src_row = &src[row * w];
				uint16_t col_max = w < max_col_bound ? w : max_col_bound;
				if (!swap) {
					memcpy(&dest_row[left], src_row, (size_t)col_max * sizeof(uint16_t));
				} else {
					for (uint16_t col = 0; col < col_max; col++) {
						uint16_t pix = src_row[col];
						pix = (pix >> 8) | (pix << 8);
						dest_row[left + col] = (eadk_color_t)pix;
					}
				}
			}
		}

		if (w == BLOCK_W && h == BLOCK_H) {
			composed_blocks++;
			if (composed_blocks >= (COLS * ROWS)) {
				eadk_rect_t rct = { 0, (uint16_t)(current_band * COMPOSE_H), COMPOSE_W, COMPOSE_H };
				#if ENABLE_VBLANK
				eadk_display_wait_for_vblank();
				#endif
					eadk_display_push_rect(rct, composed_buf);
					fill_composed_buf(composed_buf, (COMPOSE_W * COMPOSE_H), eadk_color_white);
				composed_blocks = 0;
				if (current_band < (BANDS - 1)) current_band++;
				else current_band = 0;
			}
		}

	return 1;
}


static uint8_t jd_workbuf[TJPGD_WORKSPACE_SIZE];

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

	#if ENABLE_STORAGE
		extapp_fileErase(SVG_FILE);
		extapp_fileWrite(SVG_FILE, (const char*)&target_fps, 1);
	#endif

	return target_fps;
}

void menu(){
	eadk_display_draw_string("This screen will no longer show up.", (eadk_point_t){0, 0}, false, eadk_color_white, eadk_color_black);
	eadk_display_draw_string("Press BACK to quit the app.", (eadk_point_t){0, 20}, false, eadk_color_white, eadk_color_black);
	eadk_display_draw_string("Press shift to change FPS", (eadk_point_t){0, 40}, false, eadk_color_white, eadk_color_black);
	eadk_display_draw_string("Press EXE to enable debug info", (eadk_point_t){0, 60}, false, eadk_color_white, eadk_color_black);
	eadk_display_draw_string("Press OK to continue", (eadk_point_t){0, 80}, true, eadk_color_white, eadk_color_black);
	
	eadk_timing_msleep(2000);
	while (!eadk_keyboard_key_down(eadk_keyboard_scan(), eadk_key_ok));
}

int main(void) {
	eadk_display_push_rect_uniform(eadk_screen_rect, eadk_color_white);

	memdev_t dev;
	dev.data = (const uint8_t*)eadk_external_data;
	dev.size = eadk_external_data_size;
	dev.pos = 0;

	int target_fps = 15;

	#if ENABLE_STORAGE 
	if (extapp_fileExists(SVG_FILE)) {
		size_t fps_len = 0;
		const char* fps_data = extapp_fileRead(SVG_FILE, &fps_len);
		if (fps_data != NULL && fps_len > 0) {
			target_fps = (int)((uint8_t)fps_data[0]);
		}
	}
	else {
		menu();
		extapp_fileWrite(SVG_FILE, (const char*)&target_fps, 1);
	}
	#endif

	for (;;) {
		size_t p = 0;
		while (p + 1 < dev.size) {
			bool debug = false;
			eadk_keyboard_state_t state = eadk_keyboard_scan();
			if (eadk_keyboard_key_down(state, eadk_key_exe)) debug = true;
			if (eadk_keyboard_key_down(state, eadk_key_shift)) target_fps = change_target_fps(target_fps);
			if (eadk_keyboard_key_down(state, eadk_key_back)) return 0;

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

				int fps_no_cap = frame_duration_ms ? (int)(1000 / frame_duration_ms) : 0;
				int fps_capped = (frame_duration_ms + sleep_ms) ? (int)(1000 / (frame_duration_ms + sleep_ms)) : 0;
				snprintf(buf, sizeof(buf), "fps_no_cap: %d", fps_no_cap);
				eadk_display_draw_string(buf, (eadk_point_t){0, 60}, false, eadk_color_black, eadk_color_white);

				snprintf(buf, sizeof(buf), "fps_capped: %d", fps_capped);
				eadk_display_draw_string(buf, (eadk_point_t){0, 72}, false, eadk_color_black, eadk_color_white);
			}

			eadk_timing_msleep(sleep_ms);
		}
	}
	return 0;
}