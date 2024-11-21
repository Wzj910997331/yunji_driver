/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "sample_ivps_main.h"
#include "ivps_bitmap_text.h"

static SAMPLE_REGION_INFO_T gRegions[AX_IVPS_MAX_RGN_HANDLE_NUM] = {
    {
        .handle = AX_IVPS_INVALID_REGION_HANDLE,
        .IvpsGrp = 1,
        .IvpsFilter = 0x10, /* CHN0 FILTER0 */
    },
    {
        .handle = AX_IVPS_INVALID_REGION_HANDLE,
        .IvpsGrp = 1,
        .IvpsFilter = 0x10,
    },
    {
        .handle = AX_IVPS_INVALID_REGION_HANDLE,
        .IvpsGrp = 1,
        .IvpsFilter = 0x10,
    },
    {
        .handle = AX_IVPS_INVALID_REGION_HANDLE,
        .IvpsGrp = 1,
        .IvpsFilter = 0x10,
    },
    {
        .handle = AX_IVPS_INVALID_REGION_HANDLE,
        .IvpsGrp = 1,
        .IvpsFilter = 0x10,
    },
    {
        .handle = AX_IVPS_INVALID_REGION_HANDLE,
        .IvpsGrp = 1,
        .IvpsFilter = 0x10,
    },
};

/* Line/Polygon config */
static AX_VOID IVPS_RegionCfg0(AX_IVPS_RGN_DISP_GROUP_T *ptRegion)
{

    /*
    nZindex: Required
    Indicates which layer to draw Line/Polygon/OSD on.
    If Filter engine type is TDP, nZindex is 0 ~ 4.
    If Filter engine type is VO, nZindex is 0 ~ 31.
    nAlpha: Required
            (0, 255], 0: transparent, 255: opaque.
    eFormat: Required
            TDP engine support AX_FORMAT_ARGB8888/AX_FORMAT_RGBA8888.
            VO engine support AX_FORMAT_ARGB8888.
            The sub arrDisp's format should be the same as the for format.
    */
    ptRegion->nNum = 5;
    ptRegion->tChnAttr.nZindex = 0;
    ptRegion->tChnAttr.nAlpha = 200; /* (0, 255], 0: transparent, 255: opaque*/
    ptRegion->tChnAttr.eFormat = AX_FORMAT_ARGB8888;

    ptRegion->arrDisp[0].bShow = AX_TRUE;
    ptRegion->arrDisp[0].eType = AX_IVPS_RGN_TYPE_LINE;
    ptRegion->arrDisp[0].uDisp.tLine.tPTs[0].nX = 10;
    ptRegion->arrDisp[0].uDisp.tLine.tPTs[0].nY = 10;
    ptRegion->arrDisp[0].uDisp.tLine.tPTs[1].nX = 100;
    ptRegion->arrDisp[0].uDisp.tLine.tPTs[1].nY = 10;
    ptRegion->arrDisp[0].uDisp.tLine.nLineWidth = 4;
    ptRegion->arrDisp[0].uDisp.tLine.nColor = GREEN;
    ptRegion->arrDisp[0].uDisp.tLine.nAlpha = 200 /* (0, 255], 0: transparent, 255: opaque*/;

    ptRegion->arrDisp[1].bShow = AX_TRUE;
    ptRegion->arrDisp[1].eType = AX_IVPS_RGN_TYPE_POLYGON;
    ptRegion->arrDisp[1].uDisp.tPolygon.nPointNum = 4;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[0].nX = 200;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[0].nY = 200;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[1].nX = 400;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[1].nY = 200;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[2].nX = 450;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[2].nY = 500;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[3].nX = 200;
    ptRegion->arrDisp[1].uDisp.tPolygon.tPTs[3].nY = 500;
    ptRegion->arrDisp[1].uDisp.tPolygon.bSolid = AX_TRUE;
    ptRegion->arrDisp[1].uDisp.tPolygon.nColor = YELLOW;
    ptRegion->arrDisp[1].uDisp.tPolygon.nAlpha = 200;

    ptRegion->arrDisp[2].bShow = AX_TRUE;
    ptRegion->arrDisp[2].eType = AX_IVPS_RGN_TYPE_RECT;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nX = 20;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nY = 500;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nW = 100;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nH = 200;
    ptRegion->arrDisp[2].uDisp.tPolygon.bSolid = AX_FALSE;
    ptRegion->arrDisp[2].uDisp.tPolygon.nLineWidth = 20;
    ptRegion->arrDisp[2].uDisp.tPolygon.nColor = RED;
    ptRegion->arrDisp[2].uDisp.tPolygon.nAlpha = 200;

    ptRegion->arrDisp[3].bShow = AX_TRUE;
    ptRegion->arrDisp[3].eType = AX_IVPS_RGN_TYPE_RECT;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nX = 100;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nY = 800;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nW = 100;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nH = 80;
    ptRegion->arrDisp[3].uDisp.tPolygon.bSolid = AX_FALSE;
    ptRegion->arrDisp[3].uDisp.tPolygon.tCornerRect.bEnable = AX_TRUE;
    ptRegion->arrDisp[3].uDisp.tPolygon.tCornerRect.nHorLength = 20;
    ptRegion->arrDisp[3].uDisp.tPolygon.tCornerRect.nVerLength = 10;
    ptRegion->arrDisp[3].uDisp.tPolygon.nLineWidth = 2;
    ptRegion->arrDisp[3].uDisp.tPolygon.nColor = GREEN;
    ptRegion->arrDisp[3].uDisp.tPolygon.nAlpha = 200;

    ptRegion->arrDisp[4].bShow = AX_TRUE;
    ptRegion->arrDisp[4].eType = AX_IVPS_RGN_TYPE_POLYGON;
    ptRegion->arrDisp[4].uDisp.tPolygon.nPointNum = 7;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[0].nX = 200;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[0].nY = 150;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[1].nX = 400;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[1].nY = 150;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[2].nX = 450;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[2].nY = 200;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[3].nX = 500;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[3].nY = 200;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[4].nX = 600;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[4].nY = 220;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[5].nX = 700;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[5].nY = 250;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[6].nX = 800;
    ptRegion->arrDisp[4].uDisp.tPolygon.tPTs[6].nY = 300;
    ptRegion->arrDisp[4].uDisp.tPolygon.bSolid = AX_TRUE;
    ptRegion->arrDisp[4].uDisp.tPolygon.nColor = VIOLET;
    ptRegion->arrDisp[4].uDisp.tPolygon.nAlpha = 250;

}

/* OSD config */
static AX_VOID IVPS_RegionCfg1(AX_IVPS_RGN_DISP_GROUP_T *ptRegion)
{
    char arrPath1[] = "/opt/data/ivps/OSD_754x70.argb1555";
    char arrPath2[] = "/opt/data/ivps/OSD_634x60.argb1555";

    /*
    nZindex: Required
            Indicates which layer to draw Line/Polygon/OSD on.
            If Filter engine type is TDP, nZindex is 0 ~ 4.
            If Filter engine type is VO, nZindex is 0 ~ 31.
    nAlpha: Required
            (0, 255], 0: transparent, 255: opaque.
    eFormat: Required
            TDP engine support AX_FORMAT_RGBA8888/AX_FORMAT_ARGB8888/AX_FORMAT_ARGB1555/
            AX_FORMAT_RGBA5551/AX_FORMAT_ARGB4444/AX_FORMAT_RGBA4444/AX_FORMAT_ARGB8565.
            VO engine  can only support AX_FORMAT_ARGB8888/AX_FORMAT_ARGB1555/AX_FORMAT_ARGB4444/AX_FORMAT_ARGB8565.
            The sub arrDisp's format should be the same as the for format.
    Note:   u64PhyAddr of tOSD is not required. But pBitmap is required.
    */
    ptRegion->nNum = 2;
    ptRegion->tChnAttr.nZindex = 1;
    ptRegion->tChnAttr.nAlpha = 200; /* (0, 255], 0: transparent, 255: opaque*/
    ptRegion->tChnAttr.eFormat = AX_FORMAT_ARGB1555;
    ptRegion->arrDisp[0].bShow = AX_TRUE;
    ptRegion->arrDisp[0].eType = AX_IVPS_RGN_TYPE_OSD;
    ptRegion->arrDisp[0].uDisp.tOSD.u16Alpha = 200;
    ptRegion->arrDisp[0].uDisp.tOSD.enRgbFormat = AX_FORMAT_ARGB1555;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpWidth = 754;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpHeight = 70;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstXoffset = 100;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstYoffset = 150;
    LoadImage(arrPath1, 0, (AX_U64 *)&ptRegion->arrDisp[0].uDisp.tOSD.u64PhyAddr,
              (AX_VOID **)&ptRegion->arrDisp[0].uDisp.tOSD.pBitmap);

    ptRegion->arrDisp[1].bShow = AX_TRUE;
    ptRegion->arrDisp[1].eType = AX_IVPS_RGN_TYPE_OSD;
    ptRegion->arrDisp[1].uDisp.tOSD.u16Alpha = 200;
    ptRegion->arrDisp[1].uDisp.tOSD.enRgbFormat = AX_FORMAT_ARGB1555;
    ptRegion->arrDisp[1].uDisp.tOSD.u32BmpWidth = 634;
    ptRegion->arrDisp[1].uDisp.tOSD.u32BmpHeight = 60;
    ptRegion->arrDisp[1].uDisp.tOSD.u32DstXoffset = 800;
    ptRegion->arrDisp[1].uDisp.tOSD.u32DstYoffset = 50;
    LoadImage(arrPath2, 0, (AX_U64 *)&ptRegion->arrDisp[1].uDisp.tOSD.u64PhyAddr,
              (AX_VOID **)&ptRegion->arrDisp[1].uDisp.tOSD.pBitmap);
}

/* Mosaic and Polygon config */
static AX_VOID IVPS_RegionCfg2(AX_IVPS_RGN_DISP_GROUP_T *ptRegion)
{
    /*
    nZindex:
            For Line/Polygon/OSD nZindex is required, It indicates which layer to draw Line/Polygon/OSD on.
            If Filter engine type is TDP, nZindex is 0 ~ 4.
            If Filter engine type is VO, nZindex is 0 ~ 31.
            But for mosaic nZindex is not required.
    nAlpha: Required
            (0, 255], 0: transparent, 254: opaque.
*/
    ptRegion->nNum = 5;
    ptRegion->tChnAttr.nZindex = 2;
    ptRegion->tChnAttr.nAlpha = 200;
    ptRegion->tChnAttr.eFormat = AX_FORMAT_ARGB8888;
    ptRegion->arrDisp[0].bShow = AX_TRUE;
    ptRegion->arrDisp[0].eType = AX_IVPS_RGN_TYPE_MOSAIC;
    ptRegion->arrDisp[0].uDisp.tMosaic.tRect.nX = 1500;
    ptRegion->arrDisp[0].uDisp.tMosaic.tRect.nY = 780;
    ptRegion->arrDisp[0].uDisp.tMosaic.tRect.nW = 200;
    ptRegion->arrDisp[0].uDisp.tMosaic.tRect.nH = 300;
    ptRegion->arrDisp[0].uDisp.tMosaic.eBklSize = AX_IVPS_MOSAIC_BLK_SIZE_64;

    ptRegion->arrDisp[1].bShow = AX_TRUE;
    ptRegion->arrDisp[1].eType = AX_IVPS_RGN_TYPE_MOSAIC;
    ptRegion->arrDisp[1].uDisp.tMosaic.tRect.nX = 1000;
    ptRegion->arrDisp[1].uDisp.tMosaic.tRect.nY = 1000;
    ptRegion->arrDisp[1].uDisp.tMosaic.tRect.nW = 200;
    ptRegion->arrDisp[1].uDisp.tMosaic.tRect.nH = 100;
    ptRegion->arrDisp[1].uDisp.tMosaic.eBklSize = AX_IVPS_MOSAIC_BLK_SIZE_64;

    ptRegion->arrDisp[2].bShow = AX_TRUE;
    ptRegion->arrDisp[2].eType = AX_IVPS_RGN_TYPE_RECT;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nX = 400;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nY = 600;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nW = 100;
    ptRegion->arrDisp[2].uDisp.tPolygon.tRect.nH = 150;
    ptRegion->arrDisp[2].uDisp.tPolygon.bSolid = AX_FALSE;
    ptRegion->arrDisp[2].uDisp.tPolygon.nLineWidth = 4;
    ptRegion->arrDisp[2].uDisp.tPolygon.nColor = RED;
    ptRegion->arrDisp[2].uDisp.tPolygon.nAlpha = 200;

    ptRegion->arrDisp[3].bShow = AX_TRUE;
    ptRegion->arrDisp[3].eType = AX_IVPS_RGN_TYPE_RECT;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nX = 400;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nY = 800;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nW = 100;
    ptRegion->arrDisp[3].uDisp.tPolygon.tRect.nH = 100;
    ptRegion->arrDisp[3].uDisp.tPolygon.bSolid = AX_FALSE;
    ptRegion->arrDisp[3].uDisp.tPolygon.nLineWidth = 2;
    ptRegion->arrDisp[3].uDisp.tPolygon.nColor = RED;
    ptRegion->arrDisp[3].uDisp.tPolygon.nAlpha = 200;
    ptRegion->arrDisp[4].bShow = AX_TRUE;
    ptRegion->arrDisp[4].eType = AX_IVPS_RGN_TYPE_RECT;
    ptRegion->arrDisp[4].uDisp.tPolygon.tRect.nX = 100;
    ptRegion->arrDisp[4].uDisp.tPolygon.tRect.nY = 800;
    ptRegion->arrDisp[4].uDisp.tPolygon.tRect.nW = 100;
    ptRegion->arrDisp[4].uDisp.tPolygon.tRect.nH = 100;
    ptRegion->arrDisp[4].uDisp.tPolygon.bSolid = AX_TRUE;
    ptRegion->arrDisp[4].uDisp.tPolygon.nColor = PINK;
    ptRegion->arrDisp[4].uDisp.tPolygon.nAlpha = 200;
}

AX_U64 bitmap_phy_addr = 0;
AX_VOID *bitmap_vir_addr = NULL;
static AX_VOID IVPS_RegionCfg3(AX_IVPS_RGN_DISP_GROUP_T *ptRegion)
{
    /*
    nZindex: Required
            Indicates which layer to draw Line/Polygon/OSD on.
            If Filter engine type is TDP, nZindex is 0 ~ 4.
            If Filter engine type is VO, nZindex is 0 ~ 31.
    nAlpha: Required
            (0, 255], 0: transparent, 255: opaque.
    eFormat: Required
            TDP engine support AX_FORMAT_RGBA8888/AX_FORMAT_ARGB8888/AX_FORMAT_ARGB1555/
            AX_FORMAT_RGBA5551/AX_FORMAT_ARGB4444/AX_FORMAT_RGBA4444/AX_FORMAT_ARGB8565.
            VO engine  can only support AX_FORMAT_ARGB8888/AX_FORMAT_ARGB1555/AX_FORMAT_ARGB4444/AX_FORMAT_ARGB8565.
            The sub arrDisp's format should be the same as the for format.
    Note:   u64PhyAddr of tOSD is not required. But pBitmap is required.
    */
    ByteReverse(&bitmap_cursor[0], sizeof(bitmap_cursor));

    AXCL_SYS_MemAlloc((AX_U64 *)&bitmap_phy_addr, bitmap_vir_addr, sizeof(bitmap_cursor), SAMPLE_PHY_MEM_ALIGN_SIZE, NULL);
    axclrtMemcpy((void *)bitmap_phy_addr, &bitmap_cursor[0], sizeof(bitmap_cursor), AXCL_MEMCPY_HOST_TO_DEVICE);

    ptRegion->nNum = 2;
    ptRegion->tChnAttr.nZindex = 3;
    ptRegion->tChnAttr.nAlpha = 200; /* (0, 255], 0: transparent, 255: opaque*/
    ptRegion->tChnAttr.eFormat = AX_FORMAT_BITMAP;
    ptRegion->tChnAttr.nBitColor.nColor = 0xFF0000;
    ptRegion->tChnAttr.nBitColor.bColorInvEn = AX_TRUE;
    ptRegion->tChnAttr.nBitColor.nColorInv = 0xFF;
    ptRegion->tChnAttr.nBitColor.nColorInvThr = 0xA0A0A0;
    ptRegion->arrDisp[0].bShow = AX_TRUE;
    ptRegion->arrDisp[0].eType = AX_IVPS_RGN_TYPE_OSD;
    ptRegion->arrDisp[0].uDisp.tOSD.u16Alpha = 512;
    ptRegion->arrDisp[0].uDisp.tOSD.enRgbFormat = AX_FORMAT_BITMAP;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpWidth = 240;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpHeight = 227;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstXoffset = 512;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstYoffset = 400;
    ptRegion->arrDisp[0].uDisp.tOSD.pBitmap = NULL;
    ptRegion->arrDisp[0].uDisp.tOSD.u64PhyAddr = bitmap_phy_addr;

    ptRegion->arrDisp[1].bShow = AX_TRUE;
    ptRegion->arrDisp[1].eType = AX_IVPS_RGN_TYPE_OSD;
    ptRegion->arrDisp[1].uDisp.tOSD.u16Alpha = 512;
    ptRegion->arrDisp[1].uDisp.tOSD.enRgbFormat = AX_FORMAT_BITMAP;
    ptRegion->arrDisp[1].uDisp.tOSD.u32BmpWidth = 240;
    ptRegion->arrDisp[1].uDisp.tOSD.u32BmpHeight = 227;
    ptRegion->arrDisp[1].uDisp.tOSD.u32DstXoffset = 653;
    ptRegion->arrDisp[1].uDisp.tOSD.u32DstYoffset = 303;
    ptRegion->arrDisp[1].uDisp.tOSD.pBitmap = NULL;
    ptRegion->arrDisp[1].uDisp.tOSD.u64PhyAddr = bitmap_phy_addr;
}

/* ColorKey config */
static AX_VOID IVPS_RegionCfg4(AX_IVPS_RGN_DISP_GROUP_T *ptRegion)
{
    char arrPath1[] = "/opt/data/ivps/800x480logo.rgb24";

    ptRegion->nNum = 1;
    ptRegion->tChnAttr.nZindex = 4;
    ptRegion->tChnAttr.nAlpha = 255; /* (0, 255], 0: transparent, 255: opaque*/
    ptRegion->tChnAttr.eFormat = AX_FORMAT_RGB888;
    ptRegion->tChnAttr.nColorKey.u16Enable = AX_TRUE;
    ptRegion->tChnAttr.nColorKey.u16Inv = AX_FALSE;
    ptRegion->tChnAttr.nColorKey.u32KeyHigh = 0x3551A2; /* RRGGGBB */
    ptRegion->tChnAttr.nColorKey.u32KeyLow = 0x3551A2;  /* RRGGGBB */
    ptRegion->arrDisp[0].bShow = AX_TRUE;
    ptRegion->arrDisp[0].eType = AX_IVPS_RGN_TYPE_OSD;
    ptRegion->arrDisp[0].uDisp.tOSD.u16Alpha = 255;
    ptRegion->arrDisp[0].uDisp.tOSD.enRgbFormat = AX_FORMAT_RGB888;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpWidth = 800;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpHeight = 480;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstXoffset = 1076;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstYoffset = 324;
    LoadImage(arrPath1, 0, (AX_U64 *)&ptRegion->arrDisp[0].uDisp.tOSD.u64PhyAddr,
              (AX_VOID **)&ptRegion->arrDisp[0].uDisp.tOSD.pBitmap);
}

/* ColorKey config */
static AX_VOID IVPS_RegionCfg5(AX_IVPS_RGN_DISP_GROUP_T *ptRegion)
{
    char arrPath1[] = "/opt/data/ivps/800x480.rgb565";

    ptRegion->nNum = 1;
    ptRegion->tChnAttr.nZindex = 5;
    ptRegion->tChnAttr.nAlpha = 255; /* (0, 255], 0: transparent, 255: opaque*/
    ptRegion->tChnAttr.eFormat = AX_FORMAT_RGB565;
    ptRegion->tChnAttr.nColorKey.u16Enable = AX_TRUE;
    ptRegion->tChnAttr.nColorKey.u16Inv = AX_FALSE;
    ptRegion->tChnAttr.nColorKey.u32KeyHigh = 0xffffff; /* RRGGGBB */
    ptRegion->tChnAttr.nColorKey.u32KeyLow = 0x000000;  /* RRGGGBB */
    ptRegion->arrDisp[0].bShow = AX_TRUE;
    ptRegion->arrDisp[0].eType = AX_IVPS_RGN_TYPE_OSD;
    ptRegion->arrDisp[0].uDisp.tOSD.u16Alpha = 255;
    ptRegion->arrDisp[0].uDisp.tOSD.enRgbFormat = AX_FORMAT_RGB565;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpWidth = 800;
    ptRegion->arrDisp[0].uDisp.tOSD.u32BmpHeight = 480;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstXoffset = 1922;
    ptRegion->arrDisp[0].uDisp.tOSD.u32DstYoffset = 592;
    LoadImage(arrPath1, 0, (AX_U64 *)&ptRegion->arrDisp[0].uDisp.tOSD.u64PhyAddr,
              (AX_VOID **)&ptRegion->arrDisp[0].uDisp.tOSD.pBitmap);
}

static AX_IVPS_RGN_DISP_GROUP_T tRegionGrp[8];

static AX_VOID IVPS_RegionConfigAll(AX_S32 nNum)
{
    IVPS_RegionCfg0(&tRegionGrp[0]);
    IVPS_RegionCfg1(&tRegionGrp[1]);
    IVPS_RegionCfg2(&tRegionGrp[2]);
    IVPS_RegionCfg3(&tRegionGrp[3]);
    IVPS_RegionCfg4(&tRegionGrp[4]);
    IVPS_RegionCfg5(&tRegionGrp[5]);
}

static AX_S32 IVPS_RegionUpdate(AX_U32 nRgnNum)
{
    AX_U32 ret, index;
    for (index = 0; index < nRgnNum; index++)
    {
        ret = AXCL_IVPS_RGN_Update(gRegions[index].handle, &tRegionGrp[index]);
        if (IVPS_SUCC != ret)
        {
            ALOGE("AXCL_IVPS_RGN_UPDATE fail, ret=0x%x", ret);
        }
    }

    return ret;
}

static AX_VOID *IVPS_UpdateCanvasThread(AX_VOID *pData)
{
    SAMPLE_IVPS_GRP_T *pGrp = NULL;
    axclrtContext context;
    ALOGI("IVPS_UpdateCanvasThread, ret=0x%x", 1);

    pGrp = (SAMPLE_IVPS_GRP_T *)pData;
    if (!pGrp->nRegionNum)
    {
        return (AX_VOID *)1;
    }

    /* step01: create thread context */
    axclrtCreateContext(&context, pGrp->nDeviceId);

    while (!ThreadLoopStateGet() && pGrp->nIvpsRepeatNum)
    {
        sleep(1);
        IVPS_RegionUpdate(pGrp->nRegionNum);
    }

    /* step04: destory thread context */
    axclrtDestroyContext(context);
    return (AX_VOID *)0;
}

/*
 * IVPS_StartRegion()
 * Create a region handle, attach the handle to filter.
 */
AX_S32 IVPS_StartRegion(AX_U32 nRegionNum)
{
    for (AX_S32 j = 0; j < sizeof(gRegions) / sizeof(gRegions[0]); j++)
    {
        gRegions[j].handle = AX_IVPS_INVALID_REGION_HANDLE;
    }
    for (AX_S32 i = 0; i < nRegionNum; i++)
    {

        gRegions[i].handle = AXCL_IVPS_RGN_Create();
        ALOGI("gRegions[%d].handle:%d", i, gRegions[i].handle);
        if (AX_IVPS_INVALID_REGION_HANDLE == gRegions[i].handle)
        {
            ALOGE("AXCL_IVPS_CreateRegion(%d) fail", i);
            return -1;
        }

        AX_S32 ret = AXCL_IVPS_RGN_AttachToFilter(gRegions[i].handle,
                                                gRegions[i].IvpsGrp, gRegions[i].IvpsFilter);
        if (IVPS_SUCC != ret)
        {
            ALOGE("AXCL_IVPS_RGN_DetachFromFilter(handle %d => Grp %d Filter %x) fail, ret=0x%x",
                  gRegions[i].handle, gRegions[i].IvpsGrp, gRegions[i].IvpsFilter, ret);

            /* detach and destroy overlay */
            for (AX_S32 j = 0; j < AX_IVPS_MAX_RGN_HANDLE_NUM; j++)
            {
                if (AX_IVPS_INVALID_REGION_HANDLE != gRegions[j].handle)
                {
                    AXCL_IVPS_RGN_DetachFromFilter(gRegions[j].handle,
                                                 gRegions[j].IvpsGrp, gRegions[i].IvpsFilter);
                    AXCL_IVPS_RGN_Destroy(gRegions[j].handle);
                    gRegions[j].handle = AX_IVPS_INVALID_REGION_HANDLE;
                }
            }
            return -1;
        }
    }

    return 0;
}

/*
 * IVPS_StopRegion()
 * Detach the handle from filter, then destroy the handle.
 */
AX_S32 IVPS_StopRegion(AX_VOID)
{
    ALOGI("IVPS_StopRegion num:%ld", sizeof(gRegions) / sizeof(gRegions[0]));

    for (AX_U32 j = 0; j < sizeof(gRegions) / sizeof(gRegions[0]); ++j)
    {
        if (AX_IVPS_INVALID_REGION_HANDLE != gRegions[j].handle)
        {
            AXCL_IVPS_RGN_DetachFromFilter(gRegions[j].handle,
                                         gRegions[j].IvpsGrp, gRegions[j].IvpsFilter);
            AXCL_IVPS_RGN_Destroy(gRegions[j].handle);
            gRegions[j].handle = AX_IVPS_INVALID_REGION_HANDLE;
        }
    }
    if (bitmap_phy_addr) {
        AXCL_SYS_MemFree(bitmap_phy_addr, bitmap_vir_addr);
    }

    return 0;
}

/*
 * IVPS_UpdateThreadStart()
 * Create a thread to update the region canvas every time.
 */
AX_S32 IVPS_UpdateThreadStart(AX_S32 nRegionNum, SAMPLE_IVPS_GRP_T *pGrp)
{
    IVPS_RegionConfigAll(nRegionNum);
    ALOGI("UPDATE nRegionNum:%d", nRegionNum);

    if (0 != pthread_create(&pGrp->region_tid, NULL, IVPS_UpdateCanvasThread, (AX_VOID *)(AX_LONG)pGrp))
    {
        return -1;
    }

    return 0;
}

AX_S32 IVPS_UpdateThreadStop(SAMPLE_IVPS_GRP_T *pGrp)
{
    if (pGrp->region_tid)
    {
        pthread_join(pGrp->region_tid, NULL);
    }
    return 0;
}