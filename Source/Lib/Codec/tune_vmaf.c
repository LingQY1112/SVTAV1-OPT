/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "tune_vmaf.h"
#include "definitions.h"
#include "pic_buffer_desc.h"
#include "restoration.h"
#include "convolve.h"
#include "utility.h"
#include "enc_dec_process.h"
#include <libvmaf/libvmaf.h>

//#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#ifdef ARCH_X86_64
extern void RunEmms();
#define aom_clear_system_state() RunEmms()
#else
#define aom_clear_system_state() \
    {}
#endif
#define PI 3.1415926535897

typedef struct FrameData {
    const EbPictureBufferDesc *source;
    const Yv12BufferConfig *   distorted;
    int                        frame_set;
} FrameData;

typedef struct FrameData_ORI {
    const EbPictureBufferDesc *source;
    const EbPictureBufferDesc *distorted;
    int                        frame_set;
} FrameData_ORI;

//RDO
typedef struct FrameData_RDO {
    const EbPictureBufferDesc *source;
    const Yv12BufferConfig *blurred;
    int   block_w, block_h, num_rows, num_cols, row, col;
} FrameData_RDO;
//RDO

// A callback function used to pass data to VMAF.
// Returns 0 after reading a frame.
// Returns 2 when there is no more frame to read.
static int read_frame_8bd(float *ref_data, float *main_data, float *temp_data, int stride, void *user_data) {
    FrameData *frames = (FrameData *)user_data;

    if (!frames->frame_set) {
        const int width  = frames->source->width;
        const int height = frames->source->height;
        assert(width == frames->distorted->y_width);
        assert(height == frames->distorted->y_height);
        uint8_t *ref_ptr = frames->source->buffer_y + (frames->source->org_y) * (frames->source->stride_y) +
            (frames->source->org_x);
        uint8_t *main_ptr = frames->distorted->y_buffer;

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) { ref_data[col] = (float)ref_ptr[col]; }
            ref_ptr += frames->source->stride_y;
            ref_data += stride / sizeof(*ref_data);
        }

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) { main_data[col] = (float)main_ptr[col]; }
            main_ptr += frames->distorted->y_stride;
            main_data += stride / sizeof(*main_data);
        }
        frames->frame_set = 1;
        return 0;
    }

    (void)temp_data;
    return 2;
}

static int read_frame_8bd_ori(float *ref_data, float *main_data, float *temp_data, int stride, void *user_data) {
    FrameData_ORI *frames = (FrameData_ORI *)user_data;

    if (!frames->frame_set) {
        const int width  = frames->source->width;
        const int height = frames->source->height;
        assert(width == frames->distorted->width);
        assert(height == frames->distorted->height);
        uint8_t *ref_ptr = frames->source->buffer_y + (frames->source->org_y) * (frames->source->stride_y) +
            (frames->source->org_x);
        uint8_t *main_ptr = frames->distorted->buffer_y + (frames->distorted->org_y) * (frames->distorted->stride_y) +
            (frames->distorted->org_x);

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) { ref_data[col] = (float)ref_ptr[col];}
            ref_ptr += frames->source->stride_y;
            ref_data += stride / sizeof(*ref_data);
        }
        
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) { main_data[col] = (float)main_ptr[col]; }
            main_ptr += frames->distorted->stride_y;
            main_data += stride / sizeof(*main_data);
        }
        frames->frame_set = 1;
        return 0;
    }

    (void)temp_data;
    return 2;
}

void aom_calc_vmaf_dif(const char *model_path,const EbPictureBufferDesc *source, const Yv12BufferConfig *distorted,
                       double *vmaf) {
    aom_clear_system_state();
    const int width  = source->width;
    const int height = source->height;
    FrameData frames = {source, distorted, 0};
    double    vmaf_score;
    int (*read_frame)(float *reference_data, float *distorted_data, float *temp_data, int stride, void *s);
    read_frame    = read_frame_8bd;
    int ret = compute_vmaf(&vmaf_score,
                            (char *)"yuv420p",
                            width,
                            height,
                            read_frame,
          /*user_data=*/&frames, (char *)model_path,
         /*log_path=*/NULL, /*log_fmt=*/NULL, /*disable_clip=*/1,
          /*disable_avx=*/0, /*enable_transform=*/0,
          /*phone_model=*/0, /*do_psnr=*/0, /*do_ssim=*/0,
          /*do_ms_ssim=*/0, /*pool_method=*/NULL, /*n_thread=*/0,
          /*n_subsample=*/1, /*enable_conf_interval=*/0);

    aom_clear_system_state();
    *vmaf = vmaf_score;
    if (ret) fprintf(stderr,"Fatal:Failed to compute VMAF scores.\n");
}

void aom_calc_vmaf_ori(const char *model_path,const EbPictureBufferDesc *source, const EbPictureBufferDesc *distorted,
                       double *vmaf) {
    aom_clear_system_state();
    const int     width  = source->width;
    const int     height = source->height;
    FrameData_ORI frames = {source, distorted, 0};
    double        vmaf_score;
    int (*read_frame)(float *reference_data, float *distorted_data, float *temp_data, int stride, void *s);
    read_frame    = read_frame_8bd_ori;
    int ret = compute_vmaf(&vmaf_score,
                   (char *)"yuv420p",
                   width,
                   height,
                   read_frame,
          /*user_data=*/&frames, (char *)model_path,
          /*log_path=*/NULL, /*log_fmt=*/NULL, /*disable_clip=*/1,
         /*disable_avx=*/0, /*enable_transform=*/0,
          /*phone_model=*/0, /*do_psnr=*/0, /*do_ssim=*/0,
         /*do_ms_ssim=*/0, /*pool_method=*/NULL, /*n_thread=*/0,
          /*n_subsample=*/1, /*enable_conf_interval=*/0);
    
    aom_clear_system_state();
    *vmaf = vmaf_score;
    if (ret)
        fprintf(stderr, "Fatal:Failed to compute VMAF scores.\n");
}

//RDO:
void aom_calc_vmaf_multi_frame(void *user_data, const char *model_path,
                               int (*read_frame)(float *ref_data, float *main_data, float *temp_data, int stride_byte,
                                                 void *user_data),
                               int frame_width, int frame_height, double *vmaf) {
    aom_clear_system_state();

    double    vmaf_score;
    
    const int ret      = compute_vmaf(&vmaf_score,
                                 (char *)"yuv420p",
                                 frame_width,
                                 frame_height,
                                 read_frame,
                                 /*user_data=*/user_data,
                                 (char *)model_path,
                                 /*log_path=*/"vmaf_scores.xml",
                                 /*log_fmt=*/NULL,
                                 /*disable_clip=*/0,
                                 /*disable_avx=*/0,
                                 /*enable_transform=*/0,
                                 /*phone_model=*/0,
                                 /*do_psnr=*/0,
                                 /*do_ssim=*/0,
                                 /*do_ms_ssim=*/0,
                                 /*pool_method=*/NULL,
                                 /*n_thread=*/0,
                                 /*n_subsample=*/1,
                                 /*enable_conf_interval=*/0);
    
    FILE *    vmaf_log = fopen("vmaf_scores.xml", "r");
    if (vmaf_log == NULL || ret)
        fprintf(stderr,"Failed to compute VMAF scores.\n");
    int  frame_index = 0;
    char buf[512];
    while (fgets(buf, 511, vmaf_log) != NULL) {
        if (memcmp(buf, "    <frame ", 11) == 0) {
            char *p = strstr(buf, "vmaf=");
            if (p != NULL && p[5] == '"') {
                char *p2           = strstr(&p[6], "\"");
                *p2                = '\0';
                const double score = atof(&p[6]);
                if (score < 0.0 || score > 100.0) {
                    fprintf(stderr,"Failed to compute VMAF scores.");
                }
                vmaf[frame_index++] = score;
            }
        }
    }
    fclose(vmaf_log);

    aom_clear_system_state();
}


void unsharp_rect(const uint8_t *source, int source_stride, const uint8_t *blurred, int blurred_stride,uint8_t *dst, int dst_stride, int w, int h, double amount) {
    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            const double val = (double)source[j] + amount * ((double)source[j] - (double)blurred[j]);
            dst[j]           = (uint8_t)clamp((int)(val + 0.5), 0, 255);
        }
        source += source_stride;
        blurred += blurred_stride;
        dst += dst_stride;
    }

}


void unsharp_rect_float(const float *source, int source_stride, const uint8_t *blurred,
                                          int blurred_stride, float *dst, int dst_stride, int w, int h, float amount) {
    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            dst[j] = source[j] + amount * (source[j] - (float)blurred[j]);
            if (dst[j] < 0.0f)
                dst[j] = 0.0f;
            if (dst[j] > 255.0)
                dst[j] = 255.0f;
        }
        source += source_stride;
        blurred += blurred_stride;
        dst += dst_stride;
    }
}

void unsharp(const EbPictureBufferDesc *source, const Yv12BufferConfig *blurred,
                             const Yv12BufferConfig *dst, double amount) { 
    unsharp_rect(source->buffer_y + source->org_y * source->stride_y + source->org_x,
                 source->stride_y,
                 blurred->y_buffer,
                 blurred->y_stride,
                 dst->y_buffer,
                 dst->y_stride,
                 source->width,
                 source->height,
                 amount);

}
void frame_unsharp(const EbPictureBufferDesc *source, const Yv12BufferConfig *blurred,
                                   const EbPictureBufferDesc *dst, double amount) {
    unsharp_rect(source->buffer_y + source->org_y * source->stride_y + source->org_x,
                 source->stride_y,
                 blurred->y_buffer,
                 blurred->y_stride,
                 dst->buffer_y+dst->org_y*dst->stride_y+dst->org_x,
                 dst->stride_y,
                 source->width,
                 source->height,
                 amount);
}

// 8-tap Gaussian convolution filter with sigma = 1.0, sums to 128,
// all co-efficients must be even.
DECLARE_ALIGNED(16, static const int16_t, gauss_filter[8]) = { 0, 8, 30, 52, 30, 8, 0, 0};
void gaussian_blur(PictureParentControlSet *pcs,
                                   const Yv12BufferConfig *source, const Yv12BufferConfig *dst) { 
    const Av1Common *cm = pcs->av1_cm;
    const int block_size = BLOCK_128X128;

    const int num_mi_w = mi_size_wide[block_size];
    const int num_mi_h = mi_size_high[block_size];
    const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
    const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
    int       row, col;
    const int use_hbd = source->flags & YV12_FLAG_HIGHBITDEPTH;

    ConvolveParams conv_params = get_conv_params(0,0, 0, cm->bit_depth);
    InterpFilterParams filter      = {
        .filter_ptr = gauss_filter, .taps = 8, .subpel_shifts = 0, .interp_filter = EIGHTTAP_REGULAR};

    for (row = 0; row < num_rows; ++row) {
        for (col = 0; col < num_cols; ++col) {
            const int mi_row = row * num_mi_h;
            const int mi_col = col * num_mi_w;

            const int row_offset_y = mi_row << 2;
            const int col_offset_y = mi_col << 2;

            uint8_t *src_buf = source->y_buffer + row_offset_y * source->y_stride + col_offset_y;
            uint8_t *dst_buf = dst->y_buffer + row_offset_y * dst->y_stride + col_offset_y;

            if (use_hbd) {
                
                svt_av1_highbd_convolve_2d_sr(CONVERT_TO_SHORTPTR(src_buf),
                                          source->y_stride,
                                          CONVERT_TO_SHORTPTR(dst_buf),
                                          dst->y_stride,
                                          num_mi_w << 2,
                                          num_mi_h << 2,
                                          &filter,
                                          &filter,
                                          0,
                                          0,
                                          &conv_params,
                                          cm->bit_depth);
            } else {
                svt_av1_convolve_2d_sr(src_buf,
                                   source->y_stride,
                                   dst_buf,
                                   dst->y_stride,
                                   num_mi_w << 2,
                                   num_mi_h << 2,
                                   &filter,
                                   &filter,
                                   0,
                                   0,
                                   &conv_params);
            }
        }
    }
}


int closest_even(double num) {
    int rounded_num = (int)round(num);
    if (rounded_num % 2 == 1) {
        return rounded_num + (num < rounded_num ? -1 : 1);
    }
    return rounded_num;
}



void cal_guassian_filter(double sigma,int16_t *re_new) {
    int    t[]       = {-3, -2, -1, 0, 1, 2, 3};
    //int16_t    re_new[8] = {0};
    double re[8]     = {0};
    double sum_re    = 0.0;

    
    for (int i = 0; i < 7; i++) {
        re[i] = 1.0 / (sqrt(2 * PI) * sigma) * exp(-1.0 * t[i] * t[i] / (2 * sigma * sigma));
        sum_re += re[i];
    }
    re[7] = 0; 
    sum_re += re[7];
    
    for (int i = 0; i < 8; i++) { re_new[i] = closest_even(re[i] / sum_re * 128); }

    int sum_re_new = 0;
    for (int i = 0; i < 8; i++) { sum_re_new += re_new[i]; }

    if (sum_re_new == 124) {
        double s[3];
        for (int i = 0; i < 3; i++) { s[i] = fabs(re_new[i] + 2 - re[i] / sum_re * 128); }
        int ind = 0;
        for (int i = 1; i < 3; i++) {
            if (s[i] < s[ind]) {
                ind = i;
            }
        }
        re_new[ind] += 2;
        re_new[6 - ind] += 2;
    } else if (sum_re_new == 126) {
        double s[3];
        for (int i = 0; i < 3; i++) { s[i] = fabs(re_new[i] + 2 - re[i] / sum_re * 128); }
        int ind = 0;
        for (int i = 1; i < 3; i++) {
            if (s[i] < s[ind]) {
                ind = i;
            }
        }
        re_new[ind] += 2;
        re_new[6 - ind] += 2;
        re_new[3] -= 2;
    } else if (sum_re_new == 130) {
        double s[3];
        for (int i = 0; i < 3; i++) { s[i] = fabs(re_new[i] - 2 - re[i] / sum_re * 128); }
        int ind = 0;
        for (int i = 1; i < 3; i++) {
            if (s[i] < s[ind]) {
                ind = i;
            }
        }
        re_new[ind] -= 2;
        re_new[6 - ind] -= 2;
        re_new[3] += 2;
    } else if (sum_re_new == 132) {
        double s[4];
        for (int i = 0; i < 4; i++) { s[i] = fabs(re_new[i] - 2 - re[i] / sum_re * 128); }
        int ind = 0;
        for (int i = 1; i < 4; i++) {
            if (s[i] < s[ind]) {
                ind = i;
            }
        }
        re_new[ind] -= 2;
        re_new[6 - ind] -= 2;
    } else if (sum_re_new != 128) {
        fprintf(stderr,"Error\n");
    }
}


//SSIM_K
//DECLARE_ALIGNED(16, static const int16_t, gauss_filter_adapt[8]) ;
void gaussian_blur_adapt(PictureParentControlSet *pcs,
                                   const Yv12BufferConfig *source, const Yv12BufferConfig *dst) { 
    const Av1Common *cm = pcs->av1_cm;
    const int block_size = BLOCK_128X128;

    const int num_mi_w = mi_size_wide[block_size];
    const int num_mi_h = mi_size_high[block_size];
    const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
    const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
    int       row, col;
    const int use_hbd = source->flags & YV12_FLAG_HIGHBITDEPTH;

    ConvolveParams conv_params = get_conv_params(0,0, 0, cm->bit_depth);
    
    uint8_t             qindex      = (uint8_t)pcs->scs->static_config.qp;
    
    double temp=exp(0.0288*qindex-2.3108)-exp(-2.3108);
    if(temp<0.05)temp=0.05;
    if(temp>0.95)temp=0.95;
    double sigma  = 0.7255*temp*temp*temp-1.1086*temp*temp+0.7344*temp+0.3168 ;
    
    
    int16_t                gauss_filter_coeff[8]; 
    cal_guassian_filter(sigma,gauss_filter_coeff);
    
    InterpFilterParams filter      = {
        .filter_ptr = gauss_filter_coeff, .taps = 8, .subpel_shifts = 0, .interp_filter = EIGHTTAP_REGULAR};

    for (row = 0; row < num_rows; ++row) {
        for (col = 0; col < num_cols; ++col) {
            const int mi_row = row * num_mi_h;
            const int mi_col = col * num_mi_w;

            const int row_offset_y = mi_row << 2;
            const int col_offset_y = mi_col << 2;

            uint8_t *src_buf = source->y_buffer + row_offset_y * source->y_stride + col_offset_y;
            uint8_t *dst_buf = dst->y_buffer + row_offset_y * dst->y_stride + col_offset_y;

            if (use_hbd) {
                svt_av1_highbd_convolve_2d_sr(CONVERT_TO_SHORTPTR(src_buf),
                                          source->y_stride,
                                          CONVERT_TO_SHORTPTR(dst_buf),
                                          dst->y_stride,
                                          num_mi_w << 2,
                                          num_mi_h << 2,
                                          &filter,
                                          &filter,
                                          0,
                                          0,
                                          &conv_params,
                                          cm->bit_depth);
            } else {
                svt_av1_convolve_2d_sr(src_buf,
                                   source->y_stride,
                                   dst_buf,
                                   dst->y_stride,
                                   num_mi_w << 2,
                                   num_mi_h << 2,
                                   &filter,
                                   &filter,
                                   0,
                                   0,
                                   &conv_params);
            }
        }
    }
}

//RDO:
void gaussian_blur_RDO(PictureParentControlSet *pcs, const EbPictureBufferDesc *source, const Yv12BufferConfig *dst) {
    const Av1Common *cm         = pcs->av1_cm;
    const int        block_size = BLOCK_128X128;

    const int num_mi_w = mi_size_wide[block_size];
    const int num_mi_h = mi_size_high[block_size];
    const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
    const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
    int       row, col;
    const int use_hbd = cm->use_highbitdepth;


    //////revise RDO////////
    uint8_t             qindex      = (uint8_t)pcs->scs->static_config.qp;
    double temp=exp(0.0288*qindex-2.3108)-exp(-2.3108);
    if(temp<0.05)temp=0.05;
    if(temp>0.95)temp=0.95;
    double sigma  = 0.7255*temp*temp*temp-1.1086*temp*temp+0.7344*temp+0.3168 ;
    int16_t                gauss_filter_coeff[8]; 
    cal_guassian_filter(sigma,gauss_filter_coeff);
    ///////

    ConvolveParams     conv_params = get_conv_params(0, 0, 0, cm->bit_depth);
    InterpFilterParams filter      = {
        .filter_ptr = gauss_filter_coeff, .taps = 8, .subpel_shifts = 0, .interp_filter = EIGHTTAP_REGULAR};

    for (row = 0; row < num_rows; ++row) {
        for (col = 0; col < num_cols; ++col) {
            const int mi_row = row * num_mi_h;
            const int mi_col = col * num_mi_w;

            const int row_offset_y = mi_row << 2;
            const int col_offset_y = mi_col << 2;

            uint8_t *src_buf = source->buffer_y + source->org_y * source->stride_y + source->org_x +
                row_offset_y * source->stride_y + col_offset_y;
            uint8_t *dst_buf = dst->y_buffer + row_offset_y * dst->y_stride + col_offset_y;

            if (use_hbd) {
                svt_av1_highbd_convolve_2d_sr(CONVERT_TO_SHORTPTR(src_buf),
                                              source->stride_y,
                                              CONVERT_TO_SHORTPTR(dst_buf),
                                              dst->y_stride,
                                              num_mi_w << 2,
                                              num_mi_h << 2,
                                              &filter,
                                              &filter,
                                              0,
                                              0,
                                              &conv_params,
                                              cm->bit_depth);
            } else {
                svt_av1_convolve_2d_sr(src_buf,
                                       source->stride_y,
                                       dst_buf,
                                       dst->y_stride,
                                       num_mi_w << 2,
                                       num_mi_h << 2,
                                       &filter,
                                       &filter,
                                       0,
                                       0,
                                       &conv_params);
            }
        }
    }
}


static double find_best_frame_unsharp_amount(PictureParentControlSet *pcs, SequenceControlSet *scs,
                                             EbPictureBufferDesc *const source, Yv12BufferConfig *const blurred) { 
    Av1Common *             cm     = pcs->av1_cm;
    const int               width  = source->width;
    const int               height = source->height;

    Yv12BufferConfig sharpened;
    memset(&sharpened, 0, sizeof(sharpened));
    svt_aom_realloc_frame_buffer(&sharpened,
                           width,
                           height,
                           1,
                           1,
                           cm->use_highbitdepth,
                                 AOM_BORDER_IN_PIXELS,
                                 cm->byte_alignment,
                                 NULL,
                                 NULL,
                                 NULL);

    double best_vmaf, new_vmaf, unsharp_amount = 0.0;
    
    aom_calc_vmaf_ori(scs->static_config.vmaf_model_path,source, source, &new_vmaf); 

    const double step_size      = 0.05;
    int    max_loop_count = 20;
    int    loop_count     = 0;
    int    loop_count_ssim     = 0;
    double          loop_count_psnr = 0;
    
    double best_ssim=0.0;
    double new_ssim=0.0;
    double k_ssim=0.0;
    if (true) {
        
        EbByte input_buffer;
        EbByte unsharp_buffer;
        EbByte buffer_y;
        Yv12BufferConfig blurred_ssim;
        memset(&blurred_ssim, 0, sizeof(blurred_ssim));
        svt_aom_realloc_frame_buffer(&blurred_ssim,
                                     width,
                                     height,
                                     1,
                                     1,
                                     cm->use_highbitdepth,
                                     AOM_BORDER_IN_PIXELS,
                                     cm->byte_alignment,
                                     NULL,
                                     NULL,
                                     NULL);

        
        do { 
            best_ssim = new_ssim; 
            k_ssim += step_size;
            unsharp(source, blurred, &sharpened, k_ssim);
            gaussian_blur_adapt(pcs, &sharpened, &blurred_ssim);
            
            buffer_y           = source->buffer_y;
            unsharp_buffer = (&blurred_ssim)->y_buffer;
            input_buffer = &(buffer_y[source->org_x + source->org_y * source->stride_y]);
            new_ssim     = aom_ssim2_new(input_buffer,
                                 source->stride_y,
                                 unsharp_buffer,
                                 (&blurred_ssim)->y_stride,
                                 scs->max_input_luma_width,
                                 scs->max_input_luma_height);
            
            loop_count_ssim++;
        } while (new_ssim > best_ssim && loop_count_ssim < max_loop_count);
        
        if ((&blurred_ssim)->buffer_alloc_sz) {
            (&blurred_ssim)->buffer_alloc_sz = 0;
            EB_FREE_ARRAY((&blurred_ssim)->buffer_alloc);
        }
        
        
    }
    

    uint8_t             qindex      = (uint8_t)pcs->scs->static_config.qp;
    double k_psnr=exp(0.074*qindex-5.101)+0.124;
    loop_count_psnr=round(k_psnr/0.05);
    max_loop_count = loop_count_psnr<(loop_count_ssim-1)?loop_count_psnr:(loop_count_ssim-1);
    do {
        best_vmaf = new_vmaf;
        unsharp_amount += step_size;
        unsharp(source, blurred, &sharpened, unsharp_amount);
        aom_calc_vmaf_dif(
            scs->static_config.vmaf_model_path,source, &sharpened, &new_vmaf); //new_vmaf
        loop_count++;
    } while (new_vmaf > best_vmaf && loop_count < max_loop_count);

    if ((&sharpened)->buffer_alloc_sz) {
        (&sharpened)->buffer_alloc_sz = 0;
        EB_FREE_ARRAY((&sharpened)->buffer_alloc);
    }
    return unsharp_amount - step_size;
  
}

void av1_vmaf_preprocessing(PictureParentControlSet *pcs, SequenceControlSet *scs, bool use_block_based_method) { 
    aom_clear_system_state();
    EbPictureBufferDesc *input_pic = pcs->enhanced_pic;
    
    Av1Common *          cm        = pcs->av1_cm;
    const int            width     = input_pic->width;
    const int            height    = input_pic->height;
    
    Yv12BufferConfig     source_extended, blurred, sharpened;
    memset(&source_extended, 0, sizeof(source_extended));
    memset(&blurred, 0, sizeof(blurred));
    memset(&sharpened, 0, sizeof(sharpened));
    
    svt_aom_realloc_frame_buffer(&source_extended,
                                 width,
                                 height,
                                 1,
                                 1,
                                 cm->use_highbitdepth,
                                 AOM_BORDER_IN_PIXELS,
                                 cm->byte_alignment,NULL,NULL,NULL);
    
    svt_aom_realloc_frame_buffer(&blurred,
                                 width,
                                 height,
                                 1,
                                 1,
                                 cm->use_highbitdepth,
                                 AOM_BORDER_IN_PIXELS,
                                 cm->byte_alignment,
                                 NULL,
                                 NULL,
                                 NULL);
    svt_aom_realloc_frame_buffer(&sharpened,
                                 width,
                                 height,
                                 1,
                                 1,
                                 cm->use_highbitdepth,
                                 AOM_BORDER_IN_PIXELS,
                                 cm->byte_alignment,
                                 NULL,
                                 NULL,
                                 NULL);
    
    
    
    svt_copy_and_extend_frame(input_pic, &source_extended);
    svt_copy_and_extend_frame(input_pic, &sharpened);
    
    gaussian_blur(pcs, &source_extended, &blurred);
    
    if ((&source_extended)->buffer_alloc_sz) {
        (&source_extended)->buffer_alloc_sz = 0;
        EB_FREE_ARRAY((&source_extended)->buffer_alloc);
    }
    const double best_frame_unsharp_amount = find_best_frame_unsharp_amount(pcs, scs, input_pic, &blurred);
    
    if (!use_block_based_method) {
        frame_unsharp(input_pic, &blurred, input_pic, best_frame_unsharp_amount);
        if ((&sharpened)->buffer_alloc_sz) {
            (&sharpened)->buffer_alloc_sz = 0;
            EB_FREE_ARRAY((&sharpened)->buffer_alloc);
        }
        if ((&blurred)->buffer_alloc_sz) {
            (&blurred)->buffer_alloc_sz = 0;
            EB_FREE_ARRAY((&blurred)->buffer_alloc);
        }
        aom_clear_system_state();
        return;
    }

    const int block_size           = BLOCK_128X128;
    const int num_mi_w             = mi_size_wide[block_size];
    const int num_mi_h             = mi_size_high[block_size];
    const int num_cols             = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
    const int num_rows             = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
    const int block_w              = num_mi_w << 2;
    const int block_h              = num_mi_h << 2;
    double *  best_unsharp_amounts = svt_aom_malloc(sizeof(*best_unsharp_amounts) * num_cols * num_rows);
    memset(best_unsharp_amounts, 0, sizeof(*best_unsharp_amounts) * num_cols * num_rows);

    for (int row = 0; row < num_rows; ++row) {
        for (int col = 0; col < num_cols; ++col) {
            const int mi_row = row * num_mi_h;
            const int mi_col = col * num_mi_w;

            const int row_offset_y = mi_row << 2;
            const int col_offset_y = mi_col << 2;

            const int block_width  = AOMMIN(input_pic->width - col_offset_y, block_w);
            const int block_height = AOMMIN(input_pic->height - row_offset_y, block_h);

            uint8_t *src_buf = (input_pic->buffer_y + input_pic->org_y * input_pic->stride_y + input_pic->org_x) +
                row_offset_y * input_pic->stride_y + col_offset_y;
            uint8_t *blurred_buf = blurred.y_buffer + row_offset_y * blurred.y_stride + col_offset_y;
            uint8_t *dst_buf     = sharpened.y_buffer + row_offset_y * sharpened.y_stride + col_offset_y;

            const int    index     = col + row * num_cols;
            const double step_size = 0.1;
            double       amount    = AOMMAX(best_frame_unsharp_amount - 0.2, step_size);
            unsharp_rect(src_buf,
                         input_pic->stride_y,
                         blurred_buf,
                         blurred.y_stride,
                         dst_buf,
                         sharpened.y_stride,
                         block_width,
                         block_height,
                         amount);
            double best_vmaf;
            aom_calc_vmaf_dif(scs->static_config.vmaf_model_path, input_pic, &sharpened, &best_vmaf);

            // Find the best unsharp amount.
            bool exit_loop = false;
            while (!exit_loop && amount < best_frame_unsharp_amount + 0.2) {
                amount += step_size;
                unsharp_rect(src_buf,
                             input_pic->stride_y,
                             blurred_buf,
                             blurred.y_stride,
                             dst_buf,
                             sharpened.y_stride,
                             block_width,
                             block_height,
                             amount);

                double new_vmaf;
                aom_calc_vmaf_dif(scs->static_config.vmaf_model_path, input_pic, &sharpened, &new_vmaf);
                if (new_vmaf <= best_vmaf) {
                    exit_loop = true;
                    amount -= step_size;
                } else {
                    best_vmaf = new_vmaf;
                }
            }
            best_unsharp_amounts[index] = amount;
            // Reset blurred frame
            unsharp_rect(src_buf,
                         input_pic->stride_y,
                         blurred_buf,
                         blurred.y_stride,
                         dst_buf,
                         sharpened.y_stride,
                         block_width,
                         block_height,
                         0.0);
        }
    }

    // Apply best blur amounts
    for (int row = 0; row < num_rows; ++row) {
        for (int col = 0; col < num_cols; ++col) {
            const int mi_row       = row * num_mi_h;
            const int mi_col       = col * num_mi_w;
            const int row_offset_y = mi_row << 2;
            const int col_offset_y = mi_col << 2;
            const int block_width  = AOMMIN(input_pic->width - col_offset_y, block_w);
            const int block_height = AOMMIN(input_pic->height - row_offset_y, block_h);
            const int index        = col + row * num_cols;
            uint8_t * src_buf      = (input_pic->buffer_y + input_pic->org_y * input_pic->stride_y + input_pic->org_x) +
                row_offset_y * input_pic->stride_y + col_offset_y;
            uint8_t * blurred_buf  = blurred.y_buffer + row_offset_y * blurred.y_stride + col_offset_y;
            unsharp_rect(src_buf,
                         input_pic->stride_y,
                         blurred_buf,
                         blurred.y_stride,
                         src_buf,
                         input_pic->stride_y,
                         block_width,
                         block_height,
                         best_unsharp_amounts[index]);
        }
    }

   
    if ((&sharpened)->buffer_alloc_sz) {
        (&sharpened)->buffer_alloc_sz = 0;
        EB_FREE_ARRAY((&sharpened)->buffer_alloc);
    }
    if ((&blurred)->buffer_alloc_sz) {
        (&blurred)->buffer_alloc_sz = 0;
        EB_FREE_ARRAY((&blurred)->buffer_alloc);
    }
    svt_aom_free(best_unsharp_amounts);
    aom_clear_system_state();


}


static  double image_sse_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, int w,int h) {
    double accum = 0.0;
    int    i, j;
    for (i = 0; i < h; ++i) {
        for (j = 0; j < w; ++j) {
            double img1px = src[i * src_stride + j];
            double img2px = ref[i * ref_stride + j];
            accum += (img1px - img2px) * (img1px - img2px);
        }
    }
    return accum;
}

// A callback function used to pass data to VMAF.
// Returns 0 after reading a frame.
// Returns 2 when there is no more frame to read.
static int update_frame(float *ref_data, float *main_data, float *temp_data, int stride, void *user_data) {
    FrameData_RDO *               frames   = (FrameData_RDO *)user_data;
    const int                 width    = frames->source->width;
    const int                 height   = frames->source->height;
    const int                 row      = frames->row;
    const int                 col      = frames->col;
    const int                 num_rows = frames->num_rows;
    const int                 num_cols = frames->num_cols;
    const int                 block_w  = frames->block_w;
    const int                 block_h  = frames->block_h;
    const EbPictureBufferDesc *   source   = frames->source;
    const Yv12BufferConfig *      blurred  = frames->blurred;
    (void)temp_data;
    stride /= (int)sizeof(*ref_data);

    for (int i = 0; i < height; ++i) {
        float *  ref, *main;
        uint8_t *src;
        ref  = ref_data + i * stride;
        main = main_data + i * stride;
        src  = source->buffer_y + source->org_y * source->stride_y + source->org_x + i * source->stride_y;
        for (int j = 0; j < width; ++j) { ref[j] = main[j] = (float)src[j]; }
    }
    if (row < 0 && col < 0) {
        frames->row = 0;
        frames->col = 0;
        return 0;
    } else if (row < num_rows && col < num_cols) {
        // Set current block
        const int row_offset   = row * block_h;
        const int col_offset   = col * block_w;
        const int block_width  = AOMMIN(width - col_offset, block_w);
        const int block_height = AOMMIN(height - row_offset, block_h);

        float *  main_buf    = main_data + col_offset + row_offset * stride;
        float *  ref_buf     = ref_data + col_offset + row_offset * stride;
        uint8_t *blurred_buf = blurred->y_buffer + row_offset * blurred->y_stride + col_offset;

        unsharp_rect_float(
            ref_buf, stride, blurred_buf, blurred->y_stride, main_buf, stride, block_width, block_height, -1.0f);

        frames->col++;
        if (frames->col >= num_cols) {
            frames->col = 0;
            frames->row++;
        }
        return 0;
    } else {
        return 2;
    }
}

void aom_av1_set_mb_vmaf_rdmult_scaling(PictureParentControlSet *pcs) {
    //////revise////////
    ////!!!!!!!!////////
    Av1Common *cm       = pcs->av1_cm;
    const int  y_stride = pcs->enhanced_pic->stride_y;
    uint8_t *  y_buffer = pcs->enhanced_pic->buffer_y + pcs->enhanced_pic->org_x + pcs->enhanced_pic->org_y * y_stride;
    //ThreadData *   td         = &cpi->td;
    //MACROBLOCK *   x          = &td->mb;
    //MACROBLOCKD *  xd         = &x->e_mbd;
    const int y_width    = pcs->enhanced_pic->width;
    const int y_height   = pcs->enhanced_pic->height;
    const int block_size = BLOCK_64X64;

    const int num_mi_w = mi_size_wide[block_size];
    const int num_mi_h = mi_size_high[block_size];
    const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
    const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;

    const int block_w = num_mi_w << 2;
    const int block_h = num_mi_h << 2;

    const int use_hbd = cm->use_highbitdepth;
    // TODO(sdeng): Add high bit depth support.
    if (use_hbd) {
        fprintf(stderr,"VMAF RDO for high bit depth videos is unsupported yet.\n");
        exit(0);
    }
    
    aom_clear_system_state();
    Yv12BufferConfig blurred;
    memset(&blurred, 0, sizeof(blurred));
    svt_aom_realloc_frame_buffer(&blurred,
                                 y_width,
                                 y_height,
                                 1,
                                 1,
                                 cm->use_highbitdepth,
                                 AOM_BORDER_IN_PIXELS,
                                 cm->byte_alignment,
                                 NULL,
                                 NULL,
                                 NULL);

    
    gaussian_blur_RDO(pcs, pcs->enhanced_pic, &blurred);


    double *scores = svt_aom_malloc(sizeof(*scores) * (num_rows * num_cols + 1));
    memset(scores, 0, sizeof(*scores) * (num_rows * num_cols + 1));
    FrameData_RDO frame_data;
    frame_data.source   = pcs->enhanced_pic;
    frame_data.blurred  = &blurred;
    frame_data.block_w  = block_w;
    frame_data.block_h  = block_h;
    frame_data.num_rows = num_rows;
    frame_data.num_cols = num_cols;
    frame_data.row      = -1;
    frame_data.col      = -1;

    aom_calc_vmaf_multi_frame(&frame_data, pcs->scs->static_config.vmaf_model_path, update_frame, y_width, y_height, scores);

    const double baseline_mse = 0.0, baseline_vmaf = scores[0];

    // Loop through each 'block_size' block.
    for (int row = 0; row < num_rows; ++row) {
        for (int col = 0; col < num_cols; ++col) {
            const int mi_row       = row * num_mi_h;
            const int mi_col       = col * num_mi_w;
            const int index        = row * num_cols + col;
            const int row_offset_y = mi_row << 2;
            const int col_offset_y = mi_col << 2;

            uint8_t *const orig_buf    = y_buffer + row_offset_y * y_stride + col_offset_y;
            uint8_t *const blurred_buf = blurred.y_buffer + row_offset_y * blurred.y_stride + col_offset_y;

            const int block_width  = AOMMIN(y_width - col_offset_y, block_w);
            const int block_height = AOMMIN(y_height - row_offset_y, block_h);

            const double vmaf  = scores[index + 1];
            const double dvmaf = baseline_vmaf - vmaf;

            const double mse = image_sse_c(
                                   orig_buf, y_stride, blurred_buf, blurred.y_stride, block_width, block_height) /
                (double)(y_width * y_height);
            const double dmse = mse - baseline_mse;

            double       weight = 0.0;
            const double eps    = 0.01 / (num_rows * num_cols);
            if (dvmaf < eps || dmse < eps) {
                weight = 1.0;
            } else {
                weight = dmse / dvmaf;
            }
            
            weight                                  = 6.0 * (1.0 - exp(-0.05 * weight)) + 0.8;
            pcs->pa_me_data->vmaf_rdmult_scaling_factors[index] = weight;
            
        }
    }

    if ((&blurred)->buffer_alloc_sz) {
        (&blurred)->buffer_alloc_sz = 0;
        EB_FREE_ARRAY((&blurred)->buffer_alloc);
    }
    svt_aom_free(scores);
    aom_clear_system_state();
    
}