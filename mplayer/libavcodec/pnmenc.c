/*
 * PNM image format
 * Copyright (c) 2002, 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "bytestream.h"
#include "pnm.h"


static int pnm_encode_frame(AVCodecContext *avctx, unsigned char *outbuf,
                            int buf_size, void *data)
{
    PNMContext *s     = avctx->priv_data;
    AVFrame *pict     = data;
    AVFrame * const p = (AVFrame*)&s->picture;
    int i, h, h1, c, n, linesize;
    uint8_t *ptr, *ptr1, *ptr2;

    if (buf_size < avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height) + 200) {
        av_log(avctx, AV_LOG_ERROR, "encoded frame too large\n");
        return -1;
    }

    *p           = *pict;
    p->pict_type = FF_I_TYPE;
    p->key_frame = 1;

    s->bytestream_start =
    s->bytestream       = outbuf;
    s->bytestream_end   = outbuf + buf_size;

    h  = avctx->height;
    h1 = h;
    switch (avctx->pix_fmt) {
    case PIX_FMT_MONOWHITE:
        c  = '4';
        n  = (avctx->width + 7) >> 3;
        break;
    case PIX_FMT_GRAY8:
        c  = '5';
        n  = avctx->width;
        break;
    case PIX_FMT_GRAY16BE:
        c  = '5';
        n  = avctx->width * 2;
        break;
    case PIX_FMT_RGB24:
        c  = '6';
        n  = avctx->width * 3;
        break;
    case PIX_FMT_RGB48BE:
        c  = '6';
        n  = avctx->width * 6;
        break;
    case PIX_FMT_YUV420P:
        c  = '5';
        n  = avctx->width;
        h1 = (h * 3) / 2;
        break;
    default:
        return -1;
    }
    snprintf(s->bytestream, s->bytestream_end - s->bytestream,
             "P%c\n%d %d\n", c, avctx->width, h1);
    s->bytestream += strlen(s->bytestream);
    if (avctx->pix_fmt != PIX_FMT_MONOWHITE) {
        snprintf(s->bytestream, s->bytestream_end - s->bytestream,
                 "%d\n", (avctx->pix_fmt != PIX_FMT_GRAY16BE && avctx->pix_fmt != PIX_FMT_RGB48BE) ? 255 : 65535);
        s->bytestream += strlen(s->bytestream);
    }

    ptr      = p->data[0];
    linesize = p->linesize[0];
    for (i = 0; i < h; i++) {
        memcpy(s->bytestream, ptr, n);
        s->bytestream += n;
        ptr           += linesize;
    }

    if (avctx->pix_fmt == PIX_FMT_YUV420P) {
        h >>= 1;
        n >>= 1;
        ptr1 = p->data[1];
        ptr2 = p->data[2];
        for (i = 0; i < h; i++) {
            memcpy(s->bytestream, ptr1, n);
            s->bytestream += n;
            memcpy(s->bytestream, ptr2, n);
            s->bytestream += n;
                ptr1 += p->linesize[1];
                ptr2 += p->linesize[2];
        }
    }
    return s->bytestream - s->bytestream_start;
}


<<<<<<< .working
    if(buf_size < avpicture_get_size(avctx->pix_fmt, avctx->width, avctx->height) + 200){
        av_log(avctx, AV_LOG_ERROR, "encoded frame too large\n");
        return -1;
    }

    *p = *pict;
    p->pict_type= FF_I_TYPE;
    p->key_frame= 1;

    s->bytestream_start=
    s->bytestream= outbuf;
    s->bytestream_end= outbuf+buf_size;

    h = avctx->height;
    w = avctx->width;
    switch(avctx->pix_fmt) {
    case PIX_FMT_MONOWHITE:
        n = (w + 7) >> 3;
        depth = 1;
        maxval = 1;
        tuple_type = "BLACKANDWHITE";
        break;
    case PIX_FMT_GRAY8:
        n = w;
        depth = 1;
        maxval = 255;
        tuple_type = "GRAYSCALE";
        break;
    case PIX_FMT_RGB24:
        n = w * 3;
        depth = 3;
        maxval = 255;
        tuple_type = "RGB";
        break;
    case PIX_FMT_RGB32:
        n = w * 4;
        depth = 4;
        maxval = 255;
        tuple_type = "RGB_ALPHA";
        break;
    default:
        return -1;
    }
    snprintf(s->bytestream, s->bytestream_end - s->bytestream,
             "P7\nWIDTH %d\nHEIGHT %d\nDEPTH %d\nMAXVAL %d\nTUPLETYPE %s\nENDHDR\n",
             w, h, depth, maxval, tuple_type);
    s->bytestream += strlen(s->bytestream);

    ptr = p->data[0];
    linesize = p->linesize[0];

    if (avctx->pix_fmt == PIX_FMT_RGB32) {
        int j;
        unsigned int v;

        for(i=0;i<h;i++) {
            for(j=0;j<w;j++) {
                v = ((uint32_t *)ptr)[j];
                bytestream_put_be24(&s->bytestream, v);
                *s->bytestream++ = v >> 24;
            }
            ptr += linesize;
        }
    } else {
        for(i=0;i<h;i++) {
            memcpy(s->bytestream, ptr, n);
            s->bytestream += n;
            ptr += linesize;
        }
    }
    return s->bytestream - s->bytestream_start;
}

#if 0
static int pnm_probe(AVProbeData *pd)
{
    const char *p = pd->buf;
    if (pd->buf_size >= 8 &&
        p[0] == 'P' &&
        p[1] >= '4' && p[1] <= '6' &&
        pnm_space(p[2]) )
        return AVPROBE_SCORE_MAX - 1; /* to permit pgmyuv probe */
    else
        return 0;
}

static int pgmyuv_probe(AVProbeData *pd)
{
    if (match_ext(pd->filename, "pgmyuv"))
        return AVPROBE_SCORE_MAX;
    else
        return 0;
}

static int pam_probe(AVProbeData *pd)
{
    const char *p = pd->buf;
    if (pd->buf_size >= 8 &&
        p[0] == 'P' &&
        p[1] == '7' &&
        p[2] == '\n')
        return AVPROBE_SCORE_MAX;
    else
        return 0;
}
#endif


#if CONFIG_PGM_DECODER
AVCodec pgm_decoder = {
    "pgm",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PGM,
    sizeof(PNMContext),
    common_init,
    NULL,
    NULL,
    pnm_decode_frame,
    CODEC_CAP_DR1,
    .pix_fmts= (enum PixelFormat[]){PIX_FMT_GRAY8, PIX_FMT_GRAY16BE, PIX_FMT_NONE},
    .long_name= NULL_IF_CONFIG_SMALL("PGM (Portable GrayMap) image"),
};
#endif

=======
>>>>>>> .merge-right.r523
#if CONFIG_PGM_ENCODER
AVCodec pgm_encoder = {
    "pgm",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PGM,
    sizeof(PNMContext),
    ff_pnm_init,
    pnm_encode_frame,
    .pix_fmts  = (const enum PixelFormat[]){PIX_FMT_GRAY8, PIX_FMT_GRAY16BE, PIX_FMT_NONE},
    .long_name = NULL_IF_CONFIG_SMALL("PGM (Portable GrayMap) image"),
};
<<<<<<< .working
#endif // CONFIG_PGM_ENCODER

#if CONFIG_PGMYUV_DECODER
AVCodec pgmyuv_decoder = {
    "pgmyuv",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PGMYUV,
    sizeof(PNMContext),
    common_init,
    NULL,
    NULL,
    pnm_decode_frame,
    CODEC_CAP_DR1,
    .pix_fmts= (enum PixelFormat[]){PIX_FMT_YUV420P, PIX_FMT_NONE},
    .long_name= NULL_IF_CONFIG_SMALL("PGMYUV (Portable GrayMap YUV) image"),
};
=======
>>>>>>> .merge-right.r523
#endif

#if CONFIG_PGMYUV_ENCODER
AVCodec pgmyuv_encoder = {
    "pgmyuv",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PGMYUV,
    sizeof(PNMContext),
    ff_pnm_init,
    pnm_encode_frame,
    .pix_fmts  = (const enum PixelFormat[]){PIX_FMT_YUV420P, PIX_FMT_NONE},
    .long_name = NULL_IF_CONFIG_SMALL("PGMYUV (Portable GrayMap YUV) image"),
};
<<<<<<< .working
#endif // CONFIG_PGMYUV_ENCODER

#if CONFIG_PPM_DECODER
AVCodec ppm_decoder = {
    "ppm",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PPM,
    sizeof(PNMContext),
    common_init,
    NULL,
    NULL,
    pnm_decode_frame,
    CODEC_CAP_DR1,
    .pix_fmts= (enum PixelFormat[]){PIX_FMT_RGB24, PIX_FMT_RGB48BE, PIX_FMT_NONE},
    .long_name= NULL_IF_CONFIG_SMALL("PPM (Portable PixelMap) image"),
};
=======
>>>>>>> .merge-right.r523
#endif

#if CONFIG_PPM_ENCODER
AVCodec ppm_encoder = {
    "ppm",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PPM,
    sizeof(PNMContext),
    ff_pnm_init,
    pnm_encode_frame,
    .pix_fmts  = (const enum PixelFormat[]){PIX_FMT_RGB24, PIX_FMT_RGB48BE, PIX_FMT_NONE},
    .long_name = NULL_IF_CONFIG_SMALL("PPM (Portable PixelMap) image"),
};
<<<<<<< .working
#endif // CONFIG_PPM_ENCODER

#if CONFIG_PBM_DECODER
AVCodec pbm_decoder = {
    "pbm",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PBM,
    sizeof(PNMContext),
    common_init,
    NULL,
    NULL,
    pnm_decode_frame,
    CODEC_CAP_DR1,
    .pix_fmts= (enum PixelFormat[]){PIX_FMT_MONOWHITE, PIX_FMT_NONE},
    .long_name= NULL_IF_CONFIG_SMALL("PBM (Portable BitMap) image"),
};
=======
>>>>>>> .merge-right.r523
#endif

#if CONFIG_PBM_ENCODER
AVCodec pbm_encoder = {
    "pbm",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PBM,
    sizeof(PNMContext),
    ff_pnm_init,
    pnm_encode_frame,
    .pix_fmts  = (const enum PixelFormat[]){PIX_FMT_MONOWHITE, PIX_FMT_NONE},
    .long_name = NULL_IF_CONFIG_SMALL("PBM (Portable BitMap) image"),
};
<<<<<<< .working
#endif // CONFIG_PBM_ENCODER

#if CONFIG_PAM_DECODER
AVCodec pam_decoder = {
    "pam",
    CODEC_TYPE_VIDEO,
    CODEC_ID_PAM,
    sizeof(PNMContext),
    common_init,
    NULL,
    NULL,
    pnm_decode_frame,
    CODEC_CAP_DR1,
    .pix_fmts= (enum PixelFormat[]){PIX_FMT_RGB24, PIX_FMT_RGB32, PIX_FMT_GRAY8, PIX_FMT_MONOWHITE, PIX_FMT_NONE},
    .long_name= NULL_IF_CONFIG_SMALL("PAM (Portable AnyMap) image"),
};
=======
>>>>>>> .merge-right.r523
#endif
