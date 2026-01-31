#pragma once

/*
 * UI:
 * 
 *  H264 BPS:  /\__|\_____/\__ | H264 FPS:  /\__|\_____/\__ |
 * ----------------------------------------------------------
 *  SDWR BPS:  /\__|\_____/\__ | H264 BUFS_USED/BUFS_RELEASED
 */

#define GAP_X 2
#define TEXT_SIZE_X 60
#define PLOT_SIZE_X 80

#define H264_BPS_X 0
#define H264_BPS_Y 0
#define H264_BPS_TEXT_SIZE_X TEXT_SIZE_X
#define H264_BPS_SIZE_X (TEXT_SIZE_X + PLOT_SIZE_X)
#define H264_BPS_SIZE_Y 20
#define H264_BPS_MAX_VALUE 30000000

#define H264_FPS_X 0
#define H264_FPS_Y 20
#define H264_FPS_TEXT_SIZE_X TEXT_SIZE_X
#define H264_FPS_SIZE_X (TEXT_SIZE_X + PLOT_SIZE_X)
#define H264_FPS_SIZE_Y 20
#define H264_FPS_MAX_VALUE (CAM_VIDEO_FRAMES_PER_SEC * 1.2)

#define SD_WR_BPS_X (H264_BPS_X + H264_BPS_SIZE_X)
#define SD_WR_BPS_Y 0
#define SD_WR_BPS_TEXT_SIZE_X TEXT_SIZE_X
#define SD_WR_BPS_SIZE_X (TEXT_SIZE_X + PLOT_SIZE_X)
#define SD_WR_BPS_SIZE_Y 20
#define SD_WR_BPS_MAX_VALUE 4000000


