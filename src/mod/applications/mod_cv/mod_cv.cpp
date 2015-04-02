/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * mod_cv.cpp -- Detect Video motion
 *
 */


#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace std;
using namespace cv;

#include <switch.h>
#include <libyuv.h>

#include <cv.h>
#include "cvaux.h"
#include "cxmisc.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#define MY_EVENT_VIDEO_DETECT "cv::video_detect"

SWITCH_MODULE_LOAD_FUNCTION(mod_cv_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cv_shutdown);
SWITCH_MODULE_DEFINITION(mod_cv, mod_cv_load, mod_cv_shutdown, NULL);


static const int NCHANNELS = 3;

struct detect_stats {
    uint32_t last_score;
    uint32_t simo_count;
    uint32_t above_avg_simo_count;
    uint32_t sum;
    uint32_t itr;
    float avg;
};

typedef struct cv_context_s {
    CvBGCodeBookModel* model;
    bool ch[NCHANNELS];
    IplImage *ImaskCodeBook;
    IplImage *ImaskCodeBookCC;
    IplImage *rawImage;
    IplImage *yuvImage;
    CascadeClassifier *cascade;
    CascadeClassifier *nestedCascade;
    int train_frames;
    int w;
    int h;
    struct detect_stats detected;
    struct detect_stats nestDetected;
    int detect_event;
    int nest_detect_event;
} cv_context_t;


static void uninit_context(cv_context_t *context)
{
    cvReleaseBGCodeBookModel(&context->model);
    cvReleaseImage(&context->rawImage);
    cvReleaseImage(&context->yuvImage);
    cvReleaseImage(&context->ImaskCodeBook);
    cvReleaseImage(&context->ImaskCodeBookCC);

    if (context->cascade) {
        delete context->cascade;
    }

    if (context->nestedCascade) {
        delete context->nestedCascade;
    }
}

static void init_context(cv_context_t *context, const char *cascade_name, const char *nested_cascade_name)
{
    for (int i = 0; i < NCHANNELS; i++) {
        context->ch[i] = true;
    }

    if (cascade_name) {
        context->cascade = new CascadeClassifier;
        context->cascade->load(cascade_name);

        if (nested_cascade_name) {
            context->nestedCascade = new CascadeClassifier;
            context->nestedCascade->load(nested_cascade_name);
        }
    } else {
        context->model = cvCreateBGCodeBookModel();
        context->model->modMin[0] = 3;
        context->model->modMin[1] = context->model->modMin[2] = 3;
        context->model->modMax[0] = 10;
        context->model->modMax[1] = context->model->modMax[2] = 10;
        context->model->cbBounds[0] = context->model->cbBounds[1] = context->model->cbBounds[2] = 10;
        context->train_frames = 300;
    }
    
}

static const float coef1 = 0.3190;
static const float coef2 = -48.7187;

static void reset_stats(struct detect_stats *stats)
{
    memset(stats, 0, sizeof(*stats));
}

static void parse_stats(struct detect_stats *stats, uint32_t size)
{
    if (stats->itr >= 500) {
        reset_stats(stats);
    }

    if (stats->itr >= 60) {
        if (stats->last_score > stats->avg + 10) {
            stats->above_avg_simo_count++;
        } else if (stats->above_avg_simo_count) {
            stats->above_avg_simo_count = 0;
        }
    }


    if (size) {
        stats->simo_count++;
        stats->last_score = size;
        stats->sum += size;
    } else {
        stats->simo_count = 0;
        stats->itr = 0;
        stats->avg = 0;
    }

    stats->itr++;
    stats->avg = stats->sum / stats->itr;
}

void detectAndDraw(cv_context_t *context)
{
    double scale = 1;
    Mat img(context->rawImage);

    if (context->rawImage->width >= 1080) {
        scale = 2;
    } else if (context->rawImage->width >= 720) {
        scale = 1.5;
    }


    int i = 0;
    vector<Rect> detectedObjs, detectedObjs2;
    const static Scalar colors[] =  { CV_RGB(0,0,255),
                                      CV_RGB(0,128,255),
                                      CV_RGB(0,255,255),
                                      CV_RGB(0,255,0),
                                      CV_RGB(255,128,0),
                                      CV_RGB(255,255,0),
                                      CV_RGB(255,0,0),
                                      CV_RGB(255,0,255)} ;

    Mat gray, smallImg( cvRound (img.rows/scale), cvRound(img.cols/scale), CV_8UC1 );

    const int max_neighbors = MAX(0, cvRound((float)coef1*smallImg.cols + coef2));

    cvtColor( img, gray, CV_BGR2GRAY );
    resize( gray, smallImg, smallImg.size(), 0, 0, INTER_LINEAR );
    equalizeHist( smallImg, smallImg );

    context->cascade->detectMultiScale( smallImg, detectedObjs,
                                        1.1, 2, 0
                                        |CV_HAAR_FIND_BIGGEST_OBJECT
                                        |CV_HAAR_DO_ROUGH_SEARCH
                                        |CV_HAAR_SCALE_IMAGE
                                        ,
                                        Size(20, 20) );


    parse_stats(&context->detected, detectedObjs.size());

    //printf("SCORE: %d %f %d\n", context->detected.simo_count, context->detected.avg, context->detected.last_score);

    for( vector<Rect>::iterator r = detectedObjs.begin(); r != detectedObjs.end(); r++, i++ ) {
        Mat smallImgROI;
        vector<Rect> nestedObjects;
        Point center;
        Scalar color = colors[i%8];
        int radius;

        double aspect_ratio = (double)r->width/r->height;
        if(0.75 < aspect_ratio && aspect_ratio < 1.3 ) {
            center.x = cvRound((r->x + r->width*0.5)*scale);
            center.y = cvRound((r->y + r->height*0.5)*scale);
            radius = cvRound((r->width + r->height)*0.25*scale);
            circle( img, center, radius, color, 3, 8, 0 );
        } else {
            rectangle( img, cvPoint(cvRound(r->x*scale), cvRound(r->y*scale)),
                       cvPoint(cvRound((r->x + r->width-1)*scale), cvRound((r->y + r->height-1)*scale)),
                       color, 3, 8, 0);
        }

        if(!context->nestedCascade || context->nestedCascade->empty() ) {
            continue;
        }

        const int half_height=cvRound((float)r->height/2);

        r->y = r->y + half_height;
        r->height = half_height;
        smallImgROI = smallImg(*r);
        context->nestedCascade->detectMultiScale( smallImgROI, nestedObjects,
                                                  1.1, 0, 0
                                                  //|CV_HAAR_FIND_BIGGEST_OBJECT
                                                  //|CV_HAAR_DO_ROUGH_SEARCH
                                                  //|CV_HAAR_DO_CANNY_PRUNING
                                                  |CV_HAAR_SCALE_IMAGE
                                                  ,
                                                  Size(30, 30) );
        


        // Draw rectangle reflecting confidence
        const int object_neighbors = nestedObjects.size();
        //cout << "Detected " << object_neighbors << " object neighbors" << endl;
        const int rect_height = cvRound((float)img.rows * object_neighbors / max_neighbors);
        CvScalar col = CV_RGB((float)255 * object_neighbors / max_neighbors, 0, 0);
        rectangle(img, cvPoint(0, img.rows), cvPoint(img.cols/10, img.rows - rect_height), col, -1);

        parse_stats(&context->nestDetected, nestedObjects.size());
        
        
        //printf("NEST: %d %f %d\n", context->nestDetected.simo_count, context->nestDetected.avg, context->nestDetected.last_score);
    }
    
}


void  detect(IplImage* img_8uc1,IplImage* img_8uc3) {
    
	//cvThreshold( img_8uc1, img_edge, 128, 255, CV_THRESH_BINARY );
	CvMemStorage* storage = cvCreateMemStorage();
	CvSeq* first_contour = NULL;
	CvSeq* maxitem=NULL;
	double area=0,areamax=0;
	int maxn=0;
	int Nc = cvFindContours(
							img_8uc1,
							storage,
							&first_contour,
							sizeof(CvContour),
							CV_RETR_LIST // Try all four values and see what happens
							);
	int n=0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Total Contours Detected: %d\n", Nc );
    
	if (Nc > 0) {
		
        for( CvSeq* c=first_contour; c!=NULL; c=c->h_next ) {
            //cvCvtColor( img_8uc1, img_8uc3, CV_GRAY2BGR );

            area=cvContourArea(c,CV_WHOLE_SEQ );

            if(area>areamax)
                {areamax=area;
                    maxitem=c;
                    maxn=n;
                }

            n++;

        }

        CvMemStorage* storage3 = cvCreateMemStorage(0);
        //if (maxitem) maxitem = cvApproxPoly( maxitem, sizeof(maxitem), storage3, CV_POLY_APPROX_DP, 3, 1 );  



        if (areamax>5000) {
				
            maxitem = cvApproxPoly( maxitem, sizeof(CvContour), storage3, CV_POLY_APPROX_DP, 10, 1 );
                
            CvPoint pt0;

            CvMemStorage* storage1 = cvCreateMemStorage(0);
            CvMemStorage* storage2 = cvCreateMemStorage(0);
            CvSeq* ptseq = cvCreateSeq( CV_SEQ_KIND_GENERIC|CV_32SC2, sizeof(CvContour),
                                        sizeof(CvPoint), storage1 );
            CvSeq* hull;
            CvSeq* defects;

            for(int i = 0; i < maxitem->total; i++ ) {
                CvPoint* p = CV_GET_SEQ_ELEM( CvPoint, maxitem, i );
                pt0.x = p->x;
                pt0.y = p->y;
                cvSeqPush( ptseq, &pt0 );
            }
            hull = cvConvexHull2( ptseq, 0, CV_CLOCKWISE, 0 );
            int hullcount = hull->total;
        
            defects= cvConvexityDefects(ptseq,hull,storage2  );

            //printf(" defect no %d \n",defects->total);


        

            CvConvexityDefect* defectArray;  

    
            int j=0;  
            //int m_nomdef=0;
            // This cycle marks all defects of convexity of current contours.  
            for(;defects;defects = defects->h_next)  {
                  
                int nomdef = defects->total; // defect amount  
                //outlet_float( m_nomdef, nomdef );  
            
                //printf(" defect no %d \n",nomdef);
              
                if(nomdef == 0)  
                    continue;  
               
                // Alloc memory for defect set.     
                //fprintf(stderr,"malloc\n");  
                defectArray = (CvConvexityDefect*)malloc(sizeof(CvConvexityDefect)*nomdef);  
              
                // Get defect set.  
                //fprintf(stderr,"cvCvtSeqToArray\n");  
                cvCvtSeqToArray(defects,defectArray, CV_WHOLE_SEQ);  
            
                // Draw marks for all defects.  
                for(int i=0; i<nomdef; i++)  {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " defect depth for defect %d %f \n",i,defectArray[i].depth);
                    cvLine(img_8uc3, *(defectArray[i].start), *(defectArray[i].depth_point),CV_RGB(255,255,0),1, CV_AA, 0 );  
                    cvCircle( img_8uc3, *(defectArray[i].depth_point), 5, CV_RGB(0,0,164), 2, 8,0);  
                    cvCircle( img_8uc3, *(defectArray[i].start), 5, CV_RGB(0,0,164), 2, 8,0);  
                    cvLine(img_8uc3, *(defectArray[i].depth_point), *(defectArray[i].end),CV_RGB(255,255,0),1, CV_AA, 0 );  
       
                } 
                char txt[]="0";
                txt[0]='0'+nomdef-1;
                CvFont font;
                cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1.0, 1.0, 0, 5, CV_AA);
                cvPutText(img_8uc3, txt, cvPoint(50, 50), &font, cvScalar(0, 0, 255, 0)); 
  
                j++;  
               
                // Free memory.         
                free(defectArray);  
            } 


            cvReleaseMemStorage( &storage );
            cvReleaseMemStorage( &storage1 );
            cvReleaseMemStorage( &storage2 );
            cvReleaseMemStorage( &storage3 );
            //return 0;
        }
    }
}




static switch_status_t video_thread_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
    cv_context_t *context = (cv_context_t *) user_data;
    switch_channel_t *channel = switch_core_session_get_channel(session);

    if (!switch_channel_ready(channel)) {
        return SWITCH_STATUS_FALSE;
    }

    if (frame->img) {
        if (frame->img->d_w != context->w || frame->img->d_h != context->h && context->rawImage) {
            cvReleaseImage(&context->rawImage);
            cvReleaseImage(&context->yuvImage);
        }

        if (!context->rawImage) {
            context->rawImage = cvCreateImage(cvSize(frame->img->d_w, frame->img->d_h), IPL_DEPTH_8U, 3);
            context->yuvImage = cvCreateImage(cvSize(frame->img->d_w, frame->img->d_h), IPL_DEPTH_8U, 3);
            switch_assert(context->rawImage);
            switch_assert(context->rawImage->width * 3 == context->rawImage->widthStep);


            context->ImaskCodeBook = cvCreateImage( cvGetSize(context->rawImage), IPL_DEPTH_8U, 1 );
            context->ImaskCodeBookCC = cvCreateImage( cvGetSize(context->rawImage), IPL_DEPTH_8U, 1 );
            cvSet(context->ImaskCodeBook, cvScalar(255));
        }

        //printf("context->rawImage: %dx%d stride: %d size: %d color:%s\n", context->rawImage->width, context->rawImage->height, context->rawImage->widthStep, context->rawImage->imageSize, context->rawImage->colorModel);

        libyuv::I420ToRGB24(frame->img->planes[0], frame->img->stride[0],
                            frame->img->planes[1], frame->img->stride[1],
                            frame->img->planes[2], frame->img->stride[2],
                            (uint8_t *)context->rawImage->imageData, context->rawImage->widthStep,
                            context->rawImage->width, context->rawImage->height);


        if (context->model) {
            cvCvtColor(context->rawImage, context->yuvImage, CV_RGB2YCrCb);
        
            if (context->train_frames > 0) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "training please wait %d\n", context->train_frames);
                cvBGCodeBookUpdate( context->model, context->yuvImage );
                context->train_frames--;
            } else if (context->train_frames == 0) {
                cvBGCodeBookClearStale( context->model, context->model->t/2 );
                context->train_frames--;
            } else {
                cvBGCodeBookDiff(context->model, context->yuvImage, context->ImaskCodeBook);
                cvCopy(context->ImaskCodeBook, context->ImaskCodeBookCC);	
                cvSegmentFGMask( context->ImaskCodeBookCC );
                detect(context->ImaskCodeBookCC, context->rawImage);
            }
        }
        
        if (context->cascade) {
            switch_event_t *event;

            detectAndDraw(context);
            
            if (context->detected.simo_count > 20) {
                if (!context->detect_event) {
                    context->detect_event = 1;
                    
                    if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_VIDEO_DETECT) == SWITCH_STATUS_SUCCESS) {
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Type", "primary");
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Disposition", "start");
                        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Simo-Count", "%d", context->detected.simo_count);
                        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Average", "%f", context->detected.avg);
                        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Last-Score", "%f", context->detected.last_score);
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
                        //switch_channel_event_set_data(channel, event);
                        DUMP_EVENT(event);
                        switch_event_fire(&event);
                    }
                    
                    switch_channel_execute_on(channel, "execute_on_cv_detect_primary");
                    
                }
            } else {
                if (context->detect_event) {
                    if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_VIDEO_DETECT) == SWITCH_STATUS_SUCCESS) {
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Type", "primary");
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Disposition", "stop");
                        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Simo-Count", "%d", context->detected.simo_count);
                        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Average", "%f", context->detected.avg);
                        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Last-Score", "%f", context->detected.last_score);
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
                        //switch_channel_event_set_data(channel, event);
                        DUMP_EVENT(event);
                        switch_event_fire(&event);
                    }

                    switch_channel_execute_on(channel, "execute_on_cv_detect_off_primary");
                }

                context->detect_event = 0;

            }

            if (context->nestedCascade && context->detected.simo_count > 20) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CHECKING: %d %d %f %d\n", context->nestDetected.itr, context->nestDetected.last_score, context->nestDetected.avg, context->nestDetected.above_avg_simo_count);

                if (context->nestDetected.simo_count > 20 && context->nestDetected.last_score > context->nestDetected.avg && 
                    context->nestDetected.above_avg_simo_count > 5) {
                    if (!context->nest_detect_event) {
                        context->nest_detect_event = 1;

                        if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_VIDEO_DETECT) == SWITCH_STATUS_SUCCESS) {
                            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Type", "nested");
                            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Disposition", "start");
                            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Simo-Count", "%d", context->nestDetected.simo_count);
                            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Average", "%f", context->nestDetected.avg);
                            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Last-Score", "%f", context->nestDetected.last_score);
                            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
                            //switch_channel_event_set_data(channel, event);
                            DUMP_EVENT(event);
                            switch_event_fire(&event);
                        }

                        switch_channel_execute_on(channel, "execute_on_cv_detect_nested");
                    }
                } else if (context->nestDetected.above_avg_simo_count == 0) {
                    if (context->nest_detect_event) {
                        if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_VIDEO_DETECT) == SWITCH_STATUS_SUCCESS) {
                            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Type", "nested");
                            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Detect-Disposition", "stop");
                            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Simo-Count", "%d", context->nestDetected.simo_count);
                            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Average", "%f", context->nestDetected.avg);
                            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Detect-Last-Score", "%f", context->nestDetected.last_score);
                            switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
                            //switch_channel_event_set_data(channel, event);
                            DUMP_EVENT(event);
                            switch_event_fire(&event);
                        }
                        switch_channel_execute_on(channel, "execute_on_cv_detect_off_nested");
                    }
                    
                    context->nest_detect_event = 0;
                }
            }
        }

        int w = context->rawImage->width;
        int h = context->rawImage->height;

        libyuv::RGB24ToI420((uint8_t *)context->rawImage->imageData, w * 3,
                            frame->img->planes[0], frame->img->stride[0],
                            frame->img->planes[1], frame->img->stride[1],
                            frame->img->planes[2], frame->img->stride[2],
                            context->rawImage->width, context->rawImage->height);
                            
    }
}


SWITCH_STANDARD_APP(cv_start_function)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_frame_t *read_frame;
    cv_context_t context = { 0 };
    char *lbuf;
    char *cascade_name;
    char *nested_cascade_name;
	char *argv[6];
	int argc;
    

	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
        cascade_name = argv[0];
        nested_cascade_name = argv[1];
    }

    init_context(&context, cascade_name, nested_cascade_name);
    
    switch_channel_answer(channel);
    switch_channel_set_flag_recursive(channel, CF_VIDEO_DECODED_READ);
    switch_channel_set_flag_recursive(channel, CF_VIDEO_ECHO);

    switch_core_session_raw_read(session);

    switch_core_session_set_video_read_callback(session, video_thread_callback, &context);

	while (switch_channel_ready(channel)) {
		switch_status_t status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

        memset(read_frame->data, 0, read_frame->datalen);
        switch_core_session_write_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);
    }

    switch_core_session_set_video_read_callback(session, NULL, NULL);
    
    uninit_context(&context);

    switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);
}


///////
struct cv_bug_helper {
	switch_core_session_t *session;
    cv_context_t context;
    char *cascade_name;
    char *nested_cascade_name;
};

static switch_bool_t cv_bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	struct cv_bug_helper *cvh = (struct cv_bug_helper *) user_data;
    switch_channel_t *channel = switch_core_session_get_channel(cvh->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		{
            switch_channel_set_flag_recursive(channel, CF_VIDEO_DECODED_READ);
            init_context(&cvh->context, cvh->cascade_name, cvh->nested_cascade_name);
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
            switch_channel_clear_flag_recursive(channel, CF_VIDEO_DECODED_READ);
            uninit_context(&cvh->context);
		}
		break;
	case SWITCH_ABC_TYPE_READ_VIDEO_PING: 
        {
            switch_frame_t *frame = switch_core_media_bug_get_video_ping_frame(bug);
            video_thread_callback(cvh->session, frame, &cvh->context);
        }
        break;
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_STANDARD_APP(cv_bug_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct cv_bug_helper *cvh;
	char *lbuf = NULL;
	int x, n;
    char *cascade_name;
    char *nested_cascade_name;
	char *argv[6];
	int argc;

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_cv_bug_"))) {
		if (!zstr(data) && !strcasecmp(data, "stop")) {
			switch_channel_set_private(channel, "_cv_bug_", NULL);
			switch_core_media_bug_remove(session, &bug);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
		}
		return;
	}

	cvh = (struct cv_bug_helper *) switch_core_session_alloc(session, sizeof(*cvh));
	assert(cvh != NULL);

	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
        cascade_name = argv[1];
        nested_cascade_name = argv[2];
    }

	cvh->session = session;

    if (cascade_name) {
        cvh->cascade_name = switch_core_session_strdup(session, cascade_name);
    }

    if (nested_cascade_name) {
        cvh->cascade_name = switch_core_session_strdup(session, nested_cascade_name);
    }

	if ((status = switch_core_media_bug_add(session, "cv_bug", NULL, cv_bug_callback, cvh, 0, SMBF_READ_VIDEO_PING, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure!\n");
		return;
	}

	switch_channel_set_private(channel, "_cv_bug_", bug);

}

/* API Interface Function */
#define CV_BUG_API_SYNTAX "<uuid> [start|stop]"
SWITCH_STANDARD_API(cv_bug_api_function)
{
	switch_core_session_t *rsession = NULL;
	switch_channel_t *channel = NULL;
	switch_media_bug_t *bug;
	switch_status_t status;
	struct cv_bug_helper *cvh;
	char *mycmd = NULL;
	int argc = 0;
	char *argv[10] = { 0 };
	char *uuid = NULL;
	char *action = NULL;
    char *cascade_name = NULL;
    char *nested_cascade_name = NULL;
	char *lbuf = NULL;
	int x, n;

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		goto usage;
	}

	uuid = argv[0];
	action = argv[1];
    cascade_name = argv[2];
    nested_cascade_name = argv[3];

	if (!(rsession = switch_core_session_locate(uuid))) {
		stream->write_function(stream, "-ERR Cannot locate session!\n");
		goto done;
	}

	channel = switch_core_session_get_channel(rsession);

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_cv_bug_"))) {
		if (!zstr(action) && !strcasecmp(action, "stop")) {
			switch_channel_set_private(channel, "_cv_bug_", NULL);
			switch_core_media_bug_remove(rsession, &bug);
			stream->write_function(stream, "+OK Success\n");
		} else {
			stream->write_function(stream, "-ERR Cannot run 2 at once on the same channel!\n");
		}
		goto done;
	}

	if (!zstr(action) && strcasecmp(action, "start")) {
		goto usage;
	}

	cvh = (struct cv_bug_helper *) switch_core_session_alloc(rsession, sizeof(*cvh));
	assert(cvh != NULL);

	cvh->session = rsession;
    if (cascade_name) {
        cvh->cascade_name = switch_core_session_strdup(rsession, cascade_name);
    }
    if (nested_cascade_name) {
        cvh->nested_cascade_name = switch_core_session_strdup(rsession, nested_cascade_name);
    }
    
	if ((status = switch_core_media_bug_add(rsession, "cv_bug", NULL, cv_bug_callback, cvh, 0, SMBF_READ_VIDEO_PING, &bug)) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR Failure!\n");
		goto done;
	} else {
		switch_channel_set_private(channel, "_cv_bug_", bug);
		stream->write_function(stream, "+OK Success\n");
		goto done;
	}


 usage:
	stream->write_function(stream, "-USAGE: %s\n", CV_BUG_API_SYNTAX);

 done:
	if (rsession) {
		switch_core_session_rwunlock(rsession);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

///////


SWITCH_MODULE_LOAD_FUNCTION(mod_cv_load)
{
    switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "cv", "", "", cv_start_function, "", SAF_NONE);

	SWITCH_ADD_APP(app_interface, "cv_bug", "connect cv", "connect cv", 
                   cv_bug_start_function, "[</path/to/haar.xml>]", SAF_NONE);

	SWITCH_ADD_API(api_interface, "cv_bug", "cv_bug", cv_bug_api_function, CV_BUG_API_SYNTAX);

	switch_console_set_complete("add cv_bug ::console::list_uuid ::[start:stop");


    
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cv_shutdown)
{
    return SWITCH_STATUS_UNLOAD;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
