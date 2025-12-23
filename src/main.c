#include "libs/eadk.h"
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

	for (;;) {
		size_t p = 0;
		bool found_any = false;
		while (p + 1 < dev.size) {
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

			eadk_timing_msleep(10);
			eadk_keyboard_state_t ks = eadk_keyboard_scan();
			if (eadk_keyboard_key_down(ks, eadk_key_back)) return 0;
		}
		if (!found_any) break;
	}
	eadk_display_push_rect_uniform (eadk_screen_rect, eadk_color_green);
	while (1) eadk_keyboard_scan();
	return 0;
}