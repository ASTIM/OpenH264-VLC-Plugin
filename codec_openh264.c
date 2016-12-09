#define __PLUGIN__
#define MODULE_STRING "OpenH264"
#define HAVE_GMTIME_R
#define HAVE_LOCALTIME_R
#define HAVE_LLDIV
#define HAVE_USELOCALE
#define HAVE_STRCASECMP
#define HAVE_STRDUP
#define HAVE_STRVERSCMP
#define HAVE_STRNLEN
#define HAVE_STRNDUP
#define HAVE_STRSEP
#define HAVE_STRTOK_R
#define HAVE_ATOF
#define HAVE_ATOLL
#define HAVE_STRTOF
#define HAVE_STRTOLL
#define HAVE_GETENV
#define HAVE_SETENV
#define HAVE_POSIX_MEMALIGN
#define HAVE_NRAND48
#define HAVE_SWAB
#define HAVE_STRCASESTR
#define LIBVLC_USE_PTHREAD_CANCEL

#ifdef __MINGW32__
#   define HAVE_GETPID
#endif
#ifndef INT64_C
#   define INT64_C(c)	c ## L
#endif

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifndef __MINGW32__
#   include "vlc_fixups.h"
#endif
#include "vlc_common.h"
#include "vlc_plugin.h"
#include "vlc_codec.h"
#include "vlc_block.h"
#include "vlc_sout.h"
#include "vlc_input.h"

#include <assert.h>
#include <limits.h>

#include "codec_api.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
static picture_t *DecodeBlock  ( decoder_t *, block_t ** );

static int  OpenEncoder( vlc_object_t *p_this );
static void CloseEncoder( vlc_object_t *p_this );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( "OpenH264 codec" )

    set_description( "OpenH264 encoder" )
    set_capability( "encoder", 0 )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_shortcut( "OpenH264" )

    add_submodule ()
    set_description( "OpenH264 decoder" )
    set_capability( "decoder", 0 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "OpenH264" )
vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : OpenH264 decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    ISVCDecoder* pDecoder;
    SDecodingParam* sDecParam;
    SBufferInfo* sDstBufInfo;
    video_format_t out_fmt;
    unsigned long long uiTimeStamp;

    bool b_withDelimiter;
    uint8_t* p_buff;
    unsigned long long i_size;
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_H264 )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = malloc(sizeof *p_sys) ) == NULL )
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;

    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_I420;
    p_dec->pf_decode_video = (picture_t *(*)( decoder_t *, block_t ** )) DecodeBlock;
    p_dec->pf_packetize  =  NULL;
    p_dec->pf_decode_audio = NULL;

    p_dec->fmt_out.video.i_chroma = VLC_CODEC_I420;
    p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width = p_dec->fmt_in.video.i_width;
    p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height = p_dec->fmt_in.video.i_height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;

    if( p_dec->fmt_in.video.i_frame_rate > 0 &&
        p_dec->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate =
            p_dec->fmt_in.video.i_frame_rate;
        p_dec->fmt_out.video.i_frame_rate_base =
            p_dec->fmt_in.video.i_frame_rate_base;
    }

    p_sys->pDecoder = NULL;
    if(WelsCreateDecoder(&p_sys->pDecoder))
    {
        msg_Err(p_dec, "WelsCreateDecoder failed!");
        return VLC_ENOMEM;
    }

    p_sys->sDecParam = malloc(sizeof *p_sys->sDecParam);
    if (p_sys->sDecParam == NULL)
    {
        msg_Err(p_dec, "Can't allocate memory for SDecodingParam");
        return VLC_ENOMEM;
    }
    p_sys->uiTimeStamp = 0;

    p_sys->b_withDelimiter = false;
    p_sys->p_buff = NULL;
    p_sys->i_size = 0;

    p_sys->sDecParam->sVideoProperty.size = sizeof (p_sys->sDecParam->sVideoProperty);
    //p_sys->sDecParam->eOutputColorFormat = videoFormatI420;
    p_sys->sDecParam->uiTargetDqLayer = (uint8_t) - 1;
    p_sys->sDecParam->eEcActiveIdc = ERROR_CON_SLICE_COPY;
    p_sys->sDecParam->sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    p_sys->sDecParam->bParseOnly = false;

    p_sys->out_fmt = p_dec->fmt_out.video;

    p_sys->sDstBufInfo = malloc(sizeof *p_sys->sDstBufInfo);
    memset (p_sys->sDstBufInfo, 0, sizeof (SBufferInfo));

    int ret = ((ISVCDecoder)*p_sys->pDecoder)->Initialize(p_sys->pDecoder, p_sys->sDecParam);
    if (cmResultSuccess != ret) {
        printf ("Decoder initialization failed with error: %d.\n", ret);
        return VLC_ENOMEM;
    }

    int32_t iErrorConMethod = (int32_t) ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
    ((ISVCDecoder)*p_sys->pDecoder)->SetOption(p_sys->pDecoder, DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);

    msg_Info(p_dec, "OpenH264 decoder opened!");
    return VLC_SUCCESS;
}

/*****************************************************************************
 * getNal: Search for NAL units and returns it position in stream
 *****************************************************************************/
static size_t getNal(uint8_t* p_buf, uint8_t* p_bufEnd, uint8_t** p_nalStart, uint8_t** p_nalEnd)
{
    *p_nalStart = p_buf;
    while(*p_nalStart + 2 < p_bufEnd)
    {
        if ((*p_nalStart)[0] == 0 && (*p_nalStart)[1] == 0 && (*p_nalStart)[2] == 1)
        {
            break;
        }
        (*p_nalStart)++;
    }
    if (*p_nalStart + 2 == p_bufEnd)
    {
        *p_nalStart = p_bufEnd;
        *p_nalEnd = p_bufEnd;
        return 0;
    }

    *p_nalEnd = *p_nalStart + 3;
    while(*p_nalEnd + 2 < p_bufEnd)
    {
        if ((*p_nalEnd)[0] == 0 && (*p_nalEnd)[1] == 0 && (*p_nalEnd)[2] == 1)
        {
            break;
        }
        (*p_nalEnd)++;
    }
    if (*p_nalEnd + 2 == p_bufEnd)
        *p_nalEnd = p_bufEnd;
    return *p_nalEnd - *p_nalStart;
}

static void getNalType(uint8_t* p_nal, int* i_type)
{
    *i_type = (int) p_nal[3] & 0x1F; // & 00011111
}

/****************************************************************************
 * DecodeBlock: Decodes block and returnes decoded ref
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;
    p_block = *pp_block;

    uint8_t* pData[3] = {NULL};

    picture_t *p_picture = NULL;

    uint8_t* nalStart = p_block->p_buffer;
    uint8_t* nalEnd = NULL;
    size_t nalLength = 0;
    int nalType = -1;
    int ret = -1;

    uint8_t*    sliceStart = NULL;
    uint8_t*    sliceEnd = NULL;
    size_t      sliceLength = 0;
    int     sliceType = -1;

    p_sys->uiTimeStamp = p_block->i_pts;
    p_sys->sDstBufInfo->uiInBsTimeStamp = p_sys->uiTimeStamp;
    while (nalStart < &(p_block->p_buffer[p_block->i_buffer]))     // look through all buffer
    {
        if (!p_sys->b_withDelimiter)
        {
            nalLength = getNal(nalStart, &(p_block->p_buffer[p_block->i_buffer]), &nalStart, &nalEnd);
            if (nalLength > 0)
            {
                getNalType(nalStart, &nalType);
                switch(nalType)
                {
                case 9:     // NAL_DELIMETER
                    p_sys->b_withDelimiter = true;      //working with delimeters
                    break;
                case NAL_SPS:
                case NAL_PPS:
                case NAL_SLICE_IDR:
                case NAL_SLICE:
                    ret = ((ISVCDecoder)*p_sys->pDecoder)->DecodeFrameNoDelay(p_sys->pDecoder, nalStart, nalLength, pData, p_sys->sDstBufInfo);
                    if (dsErrorFree != ret)
                    {
                        printf("some error in decoding :( %d\n", ret);
                    }
                    break;
                default:
                    printf("Unknown NAL unit type: %d !\n", nalType);
                    break;
                }
            }
        }
        else        //(!p_sys->b_withDelimiter)
        {
            uint8_t* tmp_nalStart = p_block->p_buffer;
            nalLength = getNal(nalStart, &(p_block->p_buffer[p_block->i_buffer]), &tmp_nalStart, &nalEnd);
            if (nalLength > 0)
            {
                getNalType(tmp_nalStart, &nalType);
                switch (nalType)
                {
                case 9: // NAL_DELIMETER
                    // must copy everything from buffer before delimiter!
                    p_sys->p_buff = (uint8_t*)realloc(p_sys->p_buff, p_sys->i_size + (tmp_nalStart - nalStart));
                    memcpy(&(p_sys->p_buff[p_sys->i_size]), nalStart, tmp_nalStart - nalStart);
                    p_sys->i_size += tmp_nalStart - nalStart;

                    // PROCESS SAVED BUFFER!
                    sliceStart = p_sys->p_buff;
                    while (sliceStart < &(p_sys->p_buff[p_sys->i_size]))     // look through all saved buffer
                    {
                        sliceLength = getNal(sliceStart, &(p_sys->p_buff[p_sys->i_size]), &sliceStart, &sliceEnd);
                        if (sliceLength > 0)
                        {
                            getNalType(sliceStart, &sliceType);
                            switch(sliceType)
                            {
                            case 9:     // NAL_DELIMETER
                                printf("SHOULD NEVER HAPPEN! :(\n");
                                break;
                            case NAL_SPS:
                            case NAL_PPS:
                            case NAL_SLICE_IDR:
                            case NAL_SLICE:
                                ret = ((ISVCDecoder)*p_sys->pDecoder)->DecodeFrameNoDelay(p_sys->pDecoder, sliceStart, sliceLength, pData, p_sys->sDstBufInfo);
                                if (dsErrorFree != ret)
                                {
                                    printf("some error in decoding :( %d\n", ret);
                                }
                                break;
                            default:
                                printf("Unknown NAL unit type: %d !\n", nalType);
                                break;
                            }
                        }
                        sliceStart = sliceEnd;
                    }
                    free(p_sys->p_buff);
                    p_sys->p_buff = NULL;
                    p_sys->i_size = 0;
                    nalStart = nalEnd;
                    break;
                default:
                    break;
                }
            }
            p_sys->p_buff = (uint8_t*)realloc(p_sys->p_buff, p_sys->i_size + (nalEnd - nalStart));
            memcpy(&(p_sys->p_buff[p_sys->i_size]), nalStart, nalEnd - nalStart);
            p_sys->i_size += nalEnd - nalStart;
        }
        nalStart = nalEnd;
    }

    // walkaround for RTSP stream when picture size is availble only in PPS
    // also it is good idea to update resolution with new PPS "just in case"
    if (!p_sys->out_fmt.i_width || !p_sys->out_fmt.i_height)
    {
        p_sys->out_fmt.i_visible_width = p_sys->out_fmt.i_width = p_sys->sDstBufInfo->UsrData.sSystemBuffer.iWidth;
        p_sys->out_fmt.i_visible_height = p_sys->out_fmt.i_height = p_sys->sDstBufInfo->UsrData.sSystemBuffer.iHeight;
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width = p_sys->sDstBufInfo->UsrData.sSystemBuffer.iWidth;
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height = p_sys->sDstBufInfo->UsrData.sSystemBuffer.iHeight;
    }
    if (p_sys->sDstBufInfo->iBufferStatus == 1) {
        int iWidth  = p_sys->sDstBufInfo->UsrData.sSystemBuffer.iWidth;
        int iHeight = p_sys->sDstBufInfo->UsrData.sSystemBuffer.iHeight;

        p_picture = decoder_NewPicture(p_dec);
        if( !p_picture ) return NULL;

        unsigned long long offset = 0;
        uint8_t*  pPtr = NULL;

        // LUMA copy
        pPtr = pData[0];
        for (int i = 0; i < iHeight; i++){
            memset(p_picture->p[0].p_pixels + offset, 0, iWidth);
            memcpy(p_picture->p[0].p_pixels + offset, pPtr, iWidth);
            pPtr += p_sys->sDstBufInfo->UsrData.sSystemBuffer.iStride[0];
            offset += p_picture->p[0].i_pitch;
        }

        // CHROMA copy
        iHeight /= 2;
        iWidth /= 2;
        offset = 0;
        pPtr = pData[1];
        for (int i = 0; i < iHeight; i++){
            memset(p_picture->p[1].p_pixels + offset, 0, iWidth);
            memcpy(p_picture->p[1].p_pixels + offset, pPtr, iWidth);
            pPtr += p_sys->sDstBufInfo->UsrData.sSystemBuffer.iStride[1];
            offset += p_picture->p[1].i_pitch;
        }
        pPtr = pData[2];
        offset = 0;
        for (int i = 0; i < iHeight; i++){
            memset(p_picture->p[2].p_pixels + offset, 0, iWidth);
            memcpy(p_picture->p[2].p_pixels + offset, pPtr, iWidth);
            pPtr += p_sys->sDstBufInfo->UsrData.sSystemBuffer.iStride[1];
            offset += p_picture->p[2].i_pitch;
        }
        p_picture->date = p_block->i_dts;
        p_sys->sDstBufInfo->iBufferStatus = 0;
    }

    *pp_block = NULL;

    if( p_block )
        block_Release( p_block );
    return p_picture;
}

/*****************************************************************************
 * CloseDecoder: OpenH264 decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_enc = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_enc->p_sys;

    if (p_sys->pDecoder) {
      WelsDestroyDecoder (p_sys->pDecoder);
    }
}

/*****************************************************************************
 * encoder_sys_t : OpenH264 encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    ISVCEncoder* pSVCEncoder;
    SEncParamExt* sSvcParam;
    uint32_t i_frame_inx;
};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_H264 )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the encoder's structure */
    if( ( p_sys = malloc(sizeof *p_sys) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->fmt_out.i_cat = VIDEO_ES;
    p_enc->pf_encode_video = (block_t *(*)( encoder_t *p_enc, picture_t *p_pict )) Encode;
    p_enc->pf_encode_audio = NULL;
    p_enc->fmt_in.i_codec = VLC_CODEC_I420;

    p_sys->pSVCEncoder = NULL;
    if(WelsCreateSVCEncoder (&p_sys->pSVCEncoder))
    {
        msg_Err(p_enc, "WelsCreateSVCEncoder failed!");
        return VLC_ENOMEM;
    }

    p_sys->sSvcParam = malloc (sizeof *p_sys->sSvcParam);
    if (p_sys->sSvcParam == NULL)
    {
        msg_Err(p_enc, "Can't allocate memory for SEncParamExt");
        return VLC_ENOMEM;
    }

    // TODO: set this as encoder options via VLC
    ((ISVCEncoder)*p_sys->pSVCEncoder)->GetDefaultParams(p_sys->pSVCEncoder, p_sys->sSvcParam);
    p_sys->sSvcParam->iUsageType = SCREEN_CONTENT_REAL_TIME;
    p_sys->sSvcParam->fMaxFrameRate  = 25;                // input frame rate
    p_sys->sSvcParam->iPicWidth      = p_enc->fmt_in.video.i_width; // width of picture in samples
    p_sys->sSvcParam->iPicHeight     = p_enc->fmt_in.video.i_height;// height of picture in samples
    p_sys->sSvcParam->iTargetBitrate = 2500000;              // target bitrate desired
    p_sys->sSvcParam->iMaxBitrate    = UNSPECIFIED_BIT_RATE;
    p_sys->sSvcParam->iRCMode        = RC_QUALITY_MODE;      //  rc mode control
    p_sys->sSvcParam->iTemporalLayerNum = 1;    // layer number at temporal level
    p_sys->sSvcParam->iSpatialLayerNum  = 1;    // layer number at spatial level
    p_sys->sSvcParam->bEnableDenoise    = 0;    // denoise control
    p_sys->sSvcParam->bEnableBackgroundDetection = 0; // background detection control
    p_sys->sSvcParam->bEnableAdaptiveQuant       = 0; // adaptive quantization control
    p_sys->sSvcParam->bEnableFrameSkip           = 1; // frame skipping
    p_sys->sSvcParam->bEnableLongTermReference   = 0; // long term reference control
    p_sys->sSvcParam->uiIntraPeriod  = 25;           // period of Intra frame
    p_sys->sSvcParam->eSpsPpsIdStrategy = CONSTANT_ID;
    p_sys->sSvcParam->bPrefixNalAddingCtrl = 0;
    p_sys->sSvcParam->iComplexityMode = LOW_COMPLEXITY;
    p_sys->sSvcParam->iEntropyCodingModeFlag = 0;

    p_sys->sSvcParam->sSpatialLayers[0].uiProfileIdc       = PRO_BASELINE;
    p_sys->sSvcParam->sSpatialLayers[0].iVideoWidth        = p_enc->fmt_in.video.i_width;
    p_sys->sSvcParam->sSpatialLayers[0].iVideoHeight       = p_enc->fmt_in.video.i_height;
    p_sys->sSvcParam->sSpatialLayers[0].fFrameRate         = 25;
    p_sys->sSvcParam->sSpatialLayers[0].iSpatialBitrate    = 2500000;
    p_sys->sSvcParam->sSpatialLayers[0].iMaxSpatialBitrate    = UNSPECIFIED_BIT_RATE;
    //p_sys->sSvcParam->sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_SINGLE_SLICE;

    if (cmResultSuccess != ((ISVCEncoder)*p_sys->pSVCEncoder)->InitializeExt(p_sys->pSVCEncoder, p_sys->sSvcParam)) { // SVC encoder initialization
        printf ("SVC encoder Initialize failed\n");
        return VLC_ENOMEM;
    }

    p_sys->i_frame_inx = 0;

    msg_Info(p_enc, "OpenH264 encoder opened!");
    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: Encode frame id returnes block ref
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    SSourcePicture* pSrcPic = NULL;
    SFrameBSInfo* sFbi = NULL;

    pSrcPic = malloc(sizeof *pSrcPic);
    if (pSrcPic == NULL) {
        msg_Err(p_enc, "Can't allocate memory for pSrcPic");
        return NULL;
    }

    sFbi = malloc(sizeof *sFbi);
    if (sFbi == NULL) {
        msg_Err(p_enc, "Can't allocate memory for sFbi");
        return NULL;
    }

    //fill default pSrcPic
    pSrcPic->iColorFormat = videoFormatI420;
    pSrcPic->uiTimeStamp = 0;

    pSrcPic->iPicWidth = p_pict->format.i_width;
    pSrcPic->iPicHeight = p_pict->format.i_height;

    //update pSrcPic
    pSrcPic->iStride[0] = p_pict->p[0].i_pitch;
    pSrcPic->iStride[1] = p_pict->p[1].i_pitch;
    pSrcPic->iStride[2] = p_pict->p[2].i_pitch;

    pSrcPic->pData[0] = p_pict->p[0].p_pixels;
    pSrcPic->pData[1] = p_pict->p[1].p_pixels;
    pSrcPic->pData[2] = p_pict->p[2].p_pixels;

    int iFrameSize = 0;

    pSrcPic->uiTimeStamp = p_pict->date;
    int ret = ((ISVCEncoder)*p_sys->pSVCEncoder)->EncodeFrame(p_sys->pSVCEncoder, pSrcPic, sFbi);
    if (cmResultSuccess != ret)
    {
        printf("some error in encoding :( %d\n", ret);
        return NULL;
    }
    int iLayer = 0;
    for (iLayer = 0; iLayer < sFbi->iLayerNum; iLayer++)
    {
        int iLayerSize = 0;
        SLayerBSInfo* pLayerBsInfo = &sFbi->sLayerInfo[iLayer];
        if (pLayerBsInfo != NULL) {
            int iNalIdx = pLayerBsInfo->iNalCount - 1;
            for (iNalIdx = pLayerBsInfo->iNalCount - 1; iNalIdx >= 0; iNalIdx--)
            {
                iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
            }
            iFrameSize += iLayerSize;
        }
    }

    block_t *p_block;
    p_block = block_Alloc( iFrameSize );
    if( !p_block ) return NULL;

    unsigned int i_offset = 0;

    for (iLayer = 0; iLayer < sFbi->iLayerNum; iLayer++)
    {
        SLayerBSInfo* pLayerBsInfo = &sFbi->sLayerInfo[iLayer];
        if (pLayerBsInfo != NULL) {
            int iLayerSize = 0;
            int iNalIdx = pLayerBsInfo->iNalCount - 1;
            for (iNalIdx = pLayerBsInfo->iNalCount - 1; iNalIdx >= 0; iNalIdx--)
            {
                iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
            }
            memcpy( p_block->p_buffer + i_offset, pLayerBsInfo->pBsBuf, iLayerSize );
            i_offset += iLayerSize;
        }
    }

    switch (sFbi->eFrameType)
    {
    case videoFrameTypeIDR:
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
        break;
    case videoFrameTypeI:
    case videoFrameTypeP:
        p_block->i_flags |= BLOCK_FLAG_TYPE_P;
        break;
    case videoFrameTypeSkip:
        return NULL;
        break;
    default:
        printf("DONNO HOW TO MARK THE BLOCK!\n");
        break;
    }

    /* This isn't really valid for streams with B-frames */
    p_block->i_length = INT64_C(1000000) *
        p_enc->fmt_in.video.i_frame_rate_base /
            p_enc->fmt_in.video.i_frame_rate;

    p_block->i_pts = p_pict->date;
    p_block->i_dts = p_pict->date;

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: OpenH264 encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    if (p_sys->pSVCEncoder) {
      WelsDestroySVCEncoder (p_sys->pSVCEncoder);
    }
}
