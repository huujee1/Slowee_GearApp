/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "slowee.h"
#include "view.h"
#include <sensor.h>
#include <device/haptic.h>

#define STATE_GREEN		0
#define STATE_RED		1
#define STATE_ORANGE	2
#define STATE_BITING	3

//#define TIME_BITE		70		// 7000ms
//#define TIME_YELLOW	23		// 2300ms * 10
//#define TIME_RED		230		// 23000ms
#define TIME_BITE		30		// 3초
#define TIME_YELLOW		10		// 3초 300ms * 10
#define TIME_RED			30		// 3초 3000ms
const float THRE_GYRO =	60.0;

char* imageList[4][12] = {{"signal_green.png","signal_red.png","signal_yello.png",
		"signal_yello1.png","signal_yello2.png","signal_yello3.png","signal_yello4.png","signal_yello5.png",
		"signal_yello6.png","signal_yello7.png","signal_yello8.png","signal_yello9.png",},
		{"text_green.png","text_red.png","text_yello10.png",
		"text_yello9.png","text_yello8.png","text_yello7.png","text_yello6.png","text_yello5.png",
		"text_yello4.png","text_yello3.png","text_yello2.png","text_yello1.png",},
		{"clock_green.png","clock_red.png","clock_yello.png",
		"clock_yello.png","clock_yello.png","clock_yello.png","clock_yello.png","clock_yello.png",
		"clock_yello.png","clock_yello.png","clock_yello.png","clock_yello.png",},
		{"black.png","black.png","black.png",
		"black.png","black.png","black.png","black.png","black.png",
		"black.png","black.png","black.png","black.png",}};
int imageIdx = 0;

// 센서 리스너
int error;
sensor_type_e type = SENSOR_GYROSCOPE;
sensor_h sensor;
sensor_listener_h listener;

// 햅틱 변수
haptic_device_h hHaptic;
//haptic_effect_h effect_handle;

// 센서 리스너 변수
int nFirst = 0;
float fGyroX = 0, fGyroY = 0, fGyroZ = 0;
float fPreGyroX = 0, fPreGyroY = 0, fPreGyroZ = 0;
int nCurState = STATE_GREEN, nAccum = 0;
int nPassTime = 0;
int nYelloPassTime = 0;
int nRedPassTime = 0;

int nFeedbackMode = 0;

// (float) values[0]: X, values[1]: Y, values[2]: Z, values[3]: W
// Min = -573.0, Max 573.0 Degrees/s (°/s)

void on_sensor_event(sensor_h sensor, sensor_event_s *event, void *user_data)
{
   // Select a specific sensor with a sensor handle
   // This example uses sensor type, assuming there is only 1 sensor for each type
   sensor_type_e type;
   sensor_get_type(sensor, &type);

   if(type == SENSOR_GYROSCOPE)
   {
    	  if(nFirst == 1 && nPassTime == 0) {

			  fGyroZ = event->values[0];
			  float fTmp = 0.0;
			  fTmp = fGyroZ-fPreGyroZ;

			  if((fTmp > THRE_GYRO) || ((THRE_GYRO * -1) > fTmp )) {
				  nAccum++;
			  }
			  else {
				  nAccum = 0;
			  }

			  if(nAccum > 2) {
				  if(nCurState == STATE_GREEN) {
					  imageIdx = nCurState = STATE_ORANGE;
					  start_cairo_drawing();
				  }
				  else if(nCurState == STATE_ORANGE) {
					  if(nFeedbackMode != 2) {
						  error = device_haptic_vibrate(hHaptic, 2000, 80, 0);
					  }

					  imageIdx = nCurState = STATE_RED;
					  nYelloPassTime = 0;
					  nRedPassTime = TIME_RED;
					  start_cairo_drawing();
				  }
				  else if(nCurState == STATE_RED && nFeedbackMode != 2) {
					  error = device_haptic_vibrate(hHaptic, 2000, 80, 0);
				  }

				  nPassTime = TIME_BITE;
				  nAccum = 0;
			  }
			  else if ((nCurState == STATE_ORANGE) && (--nYelloPassTime <= 0)) {	// Yello LED 제어
				  if(++imageIdx >= 12) {
					  imageIdx = nCurState = STATE_GREEN;
				  }
				  else {
					  nYelloPassTime = TIME_YELLOW;
				  }
				  start_cairo_drawing();
			  }

			  if(nCurState == STATE_RED) {			// Red LED 제어
				  if(--nRedPassTime == 0) {
					  imageIdx = nCurState = STATE_GREEN;
					  start_cairo_drawing();
				  }
			  }

			  fPreGyroZ = fGyroZ;
    	  }
    	  else if(nPassTime > 0) {
    		  nPassTime--;
    	  }
    	  else if(nFirst == 0) {		// 처음 값 셋팅
    		  fPreGyroZ = event->values[0];
    		  nFirst = 1;
    	  }
   }
}

static struct view_info {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *img;
	Evas_Coord width;
	Evas_Coord height;
	cairo_t *cairo;
	cairo_surface_t *surface;
	unsigned char *pixels;
} s_info = {
	.win = NULL,
	.conform = NULL,
};

/*
 * @brief Create Essential Object window, conformant and layout
 */
void view_create(void)
{
	/* Create window */
	s_info.win = view_create_win(PACKAGE);
	if (s_info.win == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a window.");
		return;
	}

	/* Show window after main view is set up */
	evas_object_show(s_info.win);
}

/*
 * @brief Make a basic window named package
 * @param[in] pkg_name Name of the window
 */
Evas_Object *view_create_win(const char *pkg_name)
{
	Evas_Object *win = NULL;

	/*
	 * Window
	 * Create and initialize elm_win.
	 * elm_win is mandatory to manipulate window
	 */
	win = elm_win_util_standard_add(pkg_name, pkg_name);
	elm_win_conformant_set(win, EINA_TRUE);
	elm_win_autodel_set(win, EINA_TRUE);

	/* Rotation setting */
	if (elm_win_wm_rotation_supported_get(win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(win, EEXT_CALLBACK_BACK, win_back_cb, NULL);
	evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, win_resize_cb, NULL);
	evas_object_show(win);

	/* Create image */
	s_info.img = evas_object_image_filled_add(evas_object_evas_get(win));
	evas_object_show(s_info.img);

	error = sensor_get_default_sensor(type, &sensor);
	error = sensor_create_listener (sensor, &listener);
	error = sensor_listener_set_event_cb(listener, 100, on_sensor_event, 0);
	//error = sensor_listener_set_interval(listener, 100);
	error = sensor_listener_set_option(listener, SENSOR_OPTION_ALWAYS_ON);
	error = sensor_listener_start(listener);
	sensor_event_s event;
	error = sensor_listener_read_data(listener, &event);

	error = device_haptic_open(0, &hHaptic);

	return win;
}

/*
 * @brief Destroy window and free important data to finish this application
 */
void view_destroy(void)
{
	if (s_info.win == NULL)
		return;

	error = sensor_listener_unset_event_cb(listener);
	error = sensor_listener_stop(listener);
	error = sensor_destroy_listener(listener);

	error = device_haptic_stop(hHaptic, 0);
	error = device_haptic_close(hHaptic);

	/* Destroy cairo surface and device */
	cairo_surface_destroy(s_info.surface);
	cairo_destroy(s_info.cairo);

	evas_object_del(s_info.win);
}

void win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	view_destroy();
	ui_app_exit();
}

void win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	/* Let window go to hide state. */
	//elm_win_lower(s_info.win);
	nFeedbackMode = (++nFeedbackMode) % 4;
	start_cairo_drawing();
}

void win_resize_cb(void *data, Evas *e , Evas_Object *obj , void *event_info)
{
	/* When window resize event occurred
		Check first cairo surface already exist
		If cairo surface exist, destroy it */
	if (s_info.surface) {
		/* Destroy previous cairo canvas */
		cairo_surface_destroy(s_info.surface);
		cairo_destroy(s_info.cairo);
		s_info.surface = NULL;
		s_info.cairo = NULL;
	}

	/* When window resize event occurred
		If no cairo surface exist or destroyed
		Create cairo surface with resized Window size */
	if (!s_info.surface) {
		/* Get screen size */
		evas_object_geometry_get(obj, NULL, NULL, &s_info.width, &s_info.height);

		/* Set image size */
		evas_object_image_size_set(s_info.img, s_info.width, s_info.height);
		evas_object_resize(s_info.img, s_info.width, s_info.height);
		evas_object_show(s_info.img);

		/* Create new cairo canvas for resized window */
		s_info.pixels = (unsigned char*)evas_object_image_data_get(s_info.img, 1);
		s_info.surface = cairo_image_surface_create_for_data(s_info.pixels,
						CAIRO_FORMAT_ARGB32, s_info.width, s_info.height, s_info.width * 4);
		s_info.cairo = cairo_create(s_info.surface);

		/* Get image data from png file and paint as default background */
		start_cairo_drawing();
	}
}

/* In this function, first get path a png file as a resource
	After get image data from png file, use the data as source surface
	This source could be painted on cairo context and displayed */
void start_cairo_drawing(void)
{
	/* Get the sample stored png image file path to use as resource */
	char sample_filepath[256];
	//char *source_filename = "signal_green.png";
	char *resource_path = app_get_resource_path();
	snprintf(sample_filepath, 256, "%s/%s", resource_path, imageList[nFeedbackMode][imageIdx]);
	free(resource_path);

	/* Get image data from png file as image source surface */
	cairo_surface_t *image = cairo_image_surface_create_from_png(sample_filepath);
	cairo_set_source_surface(s_info.cairo, image, 18, 0);

	/* Use operator as CAIRO_OPERATOR_SOURCE to enable to use image source */
	cairo_set_operator(s_info.cairo, CAIRO_OPERATOR_SOURCE);

	/* Paint image source */
	cairo_paint(s_info.cairo);

	/* Destroy the image source */
	cairo_surface_destroy(image);

	/* Render stacked cairo APIs on cairo context's surface */
	cairo_surface_flush(s_info.surface);

	/* Display this cairo image painting on screen */
	evas_object_image_data_update_add(s_info.img, 0, 0, s_info.width, s_info.height);
}


