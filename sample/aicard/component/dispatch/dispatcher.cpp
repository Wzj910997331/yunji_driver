/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include <string.h>

#include "dispatcher.hpp"
#include "AppLog.hpp"
#include "ElapsedTimer.hpp"
#include "ax_ivps_api.h"

#define DISPATCHER "dispatcher"
using namespace std;

AX_VOID CDispatcher::DispatchThread(AX_VOID* pArg) {
    LOG_M_D(DISPATCHER, "%s: +++", __func__);

    CAXFrame axFrame;
    DETECT_RESULT_T fhvp;
    while (m_DispatchThread.IsRunning()) {
        if (m_qFrame.Pop(axFrame, -1)) {
#ifndef __HOST_DEBUG__
            if (CDetectResult::GetInstance()->Get(axFrame.nGrp, axFrame.stFrame.stVFrame.stVFrame.u64SeqNum, fhvp) || fhvp.nCount > 0) {
                /* CPU draw rectange, needs virtual address */
                if (0 == axFrame.stFrame.stVFrame.stVFrame.u64VirAddr[0]) {
                    axFrame.stFrame.stVFrame.stVFrame.u64VirAddr[0] = (AX_U64)AX_POOL_GetBlockVirAddr(axFrame.stFrame.stVFrame.stVFrame.u32BlkId[0]);
                }
                DrawBox(axFrame, fhvp);
            }
#else
            GenSimulateResult(fhvp);
            /* CPU draw rectange, needs virtual address */
            if (0 == axFrame.stFrame.stVFrame.stVFrame.u64VirAddr[0]) {
                axFrame.stFrame.stVFrame.stVFrame.u64VirAddr[0] = (AX_U64)AX_POOL_GetBlockVirAddr(axFrame.stFrame.stVFrame.stVFrame.u32BlkId[0]);
            }
            DrawBox(axFrame, fhvp);
#endif
            for (auto& m : m_lstObs) {
                m->OnRecvData(E_OBS_TARGET_TYPE_VDEC, axFrame.nGrp, axFrame.nChn, (AX_VOID*)&axFrame);
            }

            axFrame.DecRef();
        }
    }

    LOG_M_D(DISPATCHER, "%s: ---", __func__);
}

AX_BOOL CDispatcher::Init(const DISPATCH_ATTR_T& stAttr) {
    LOG_M_D(DISPATCHER, "%s: +++", __func__);

#ifdef DRAW_FHVP_LABEL
    AX_U16 nW = 0;
    AX_U16 nH = 0;
    AX_U32 nSize = 0;
    if (!m_font.LoadBmp(stAttr.strBmpFontPath.c_str(), nW, nH, nSize)) {
        LOG_M_E(DISPATCHER, "%s: load %s fail", __func__, stAttr.strBmpFontPath.c_str());
        return AX_FALSE;
    }
#endif

    m_vdGrp = stAttr.vdGrp;
    m_qFrame.SetCapacity(stAttr.nDepth);
    LOG_M_I(DISPATCHER, "depth: %d", stAttr.nDepth);

    LOG_M_D(DISPATCHER, "%s: ---", __func__);
    return AX_TRUE;
}

AX_BOOL CDispatcher::DeInit(AX_VOID) {
    LOG_M_D(DISPATCHER, "%s: +++", __func__);

    if (m_DispatchThread.IsRunning()) {
        LOG_M_E(DISPATCHER, "%s: dispatch thread is running", __func__);
        return AX_FALSE;
    }

    /* TODO: */
    LOG_M_D(DISPATCHER, "%s: ---", __func__);
    return AX_TRUE;
}

AX_BOOL CDispatcher::Start(AX_VOID) {
    LOG_M_D(DISPATCHER, "%s: +++", __func__);

    AX_CHAR szName[32];
    sprintf(szName, "AppDispatch%d", m_vdGrp);
    if (!m_DispatchThread.Start([this](AX_VOID* pArg) -> AX_VOID { DispatchThread(pArg); }, nullptr, szName)) {
        LOG_M_E(DISPATCHER, "%s: create dispatch thread fail", __func__);
        return AX_FALSE;
    }

    LOG_M_D(DISPATCHER, "%s: ---", __func__);
    return AX_TRUE;
}

AX_BOOL CDispatcher::Stop(AX_VOID) {
    LOG_M_D(DISPATCHER, "%s: +++", __func__);

    m_DispatchThread.Stop();
    m_qFrame.Wakeup();
    m_DispatchThread.Join();

    ClearQueue();

    LOG_M_D(DISPATCHER, "%s: ---", __func__);
    return AX_TRUE;
}

AX_BOOL CDispatcher::Clear(AX_VOID) {
    ClearQueue();
    return AX_TRUE;
}

AX_BOOL CDispatcher::RegObserver(IObserver* pObs) {
    if (!pObs) {
        LOG_M_E(DISPATCHER, "%s observer is nil", __func__);
        return AX_FALSE;
    }

    for (auto& m : m_lstObs) {
        if (m == pObs) {
            LOG_M_W(DISPATCHER, "%s: observer %p is already registed", __func__, pObs);
            return AX_TRUE;
        }
    }

    m_lstObs.push_back(pObs);
    LOG_M_I(DISPATCHER, "regist observer %p ok", __func__, pObs);

    return AX_TRUE;
}

AX_BOOL CDispatcher::UnRegObserver(IObserver* pObs) {
    if (!pObs) {
        LOG_M_E(DISPATCHER, "%s observer is nil", __func__);
        return AX_FALSE;
    }

    for (auto& m : m_lstObs) {
        if (m == pObs) {
            m_lstObs.remove(m);
            LOG_M_I(DISPATCHER, "%s: unregist observer %p ok", __func__, pObs);
            return AX_TRUE;
        }
    }

    LOG_M_E(DISPATCHER, "%s: observer %p is not registed", __func__, pObs);
    return AX_FALSE;
}

AX_BOOL CDispatcher::SendFrame(const CAXFrame& axFrame) {
    axFrame.IncRef();
    if (!m_qFrame.Push(axFrame)) {
        LOG_M_E(DISPATCHER, "%s: push frame %lld, pts %lld, phy 0x%llx to queue fail", __func__, axFrame.stFrame.stVFrame.stVFrame.u64SeqNum,
                axFrame.stFrame.stVFrame.stVFrame.u64PTS, axFrame.stFrame.stVFrame.stVFrame.u64PhyAddr[0]);
        axFrame.DecRef();
    } else {
        LOG_M_D(DISPATCHER, "recv frame %lld, pts %lld, phy 0x%llx from vdGrp %d vdChn %d", axFrame.stFrame.stVFrame.stVFrame.u64SeqNum,
                axFrame.stFrame.stVFrame.stVFrame.u64PTS, axFrame.stFrame.stVFrame.stVFrame.u64PhyAddr[0], axFrame.nGrp, axFrame.nChn);
    }

    return AX_TRUE;
}

AX_VOID CDispatcher::ClearQueue(AX_VOID) {
    AX_U32 nCount = m_qFrame.GetCount();
    if (nCount > 0) {
        CAXFrame axFrame;
        for (AX_U32 i = 0; i < nCount; ++i) {
            if (m_qFrame.Pop(axFrame, 0)) {
                axFrame.DecRef();
            }
        }
    }
}

AX_VOID CDispatcher::DrawBox(const CAXFrame& axFrame, const DETECT_RESULT_T& fhvp) {
    AX_IVPS_RGN_CANVAS_INFO_T canvas;
    memset(&canvas, 0, sizeof(canvas));
    canvas.nPhyAddr = axFrame.stFrame.stVFrame.stVFrame.u64PhyAddr[0];
    if (0 == axFrame.stFrame.stVFrame.stVFrame.u64PhyAddr[0]) {
        canvas.pVirAddr = AX_POOL_GetBlockVirAddr(axFrame.stFrame.stVFrame.stVFrame.u32BlkId[0]);
    } else {
        canvas.pVirAddr = (AX_VOID*)axFrame.stFrame.stVFrame.stVFrame.u64VirAddr[0];
    }
    canvas.nW = axFrame.stFrame.stVFrame.stVFrame.u32Width;
    canvas.nH = axFrame.stFrame.stVFrame.stVFrame.u32Height;
    canvas.nStride = axFrame.stFrame.stVFrame.stVFrame.u32PicStride[0];
    canvas.eFormat = axFrame.stFrame.stVFrame.stVFrame.enImgFormat;
    canvas.nUVOffset = axFrame.stFrame.stVFrame.stVFrame.u32PicStride[0] * axFrame.stFrame.stVFrame.stVFrame.u32Height;

    AX_IVPS_GDI_ATTR_T gdi;
    memset(&gdi, 0, sizeof(gdi));
    gdi.nThick = 1;
    gdi.nAlpha = 0;
    gdi.nColor = 0xFF8080;
    gdi.bSolid = AX_FALSE;
    gdi.bAbsCoo = AX_TRUE;

#ifdef DRAW_FHVP_LABEL
    CYuvHandler yuv((const AX_U8*)canvas.pVirAddr, canvas.nW, canvas.nH, canvas.eFormat, canvas.nStride, AX_FALSE);
    AX_U32 arrCount[DETECT_TYPE_BUTT] = {0};
#endif

    for (AX_U32 i = 0; i < fhvp.nCount; ++i) {
        AX_IVPS_RECT_T rc;
        rc.nX = (AX_U16)((AX_F32)canvas.nW * (fhvp.item[i].tBox.fX / (AX_F32)fhvp.nW));
        rc.nY = (AX_U16)((AX_F32)canvas.nH * (fhvp.item[i].tBox.fY / (AX_F32)fhvp.nH));
        rc.nW = (AX_U16)((AX_F32)canvas.nW * (fhvp.item[i].tBox.fW / (AX_F32)fhvp.nW));
        rc.nH = (AX_U16)((AX_F32)canvas.nH * (fhvp.item[i].tBox.fH / (AX_F32)fhvp.nH));

        if ((rc.nX + rc.nW) > canvas.nW || (rc.nY + rc.nH) > canvas.nH) {
            LOG_M_W(DISPATCHER, "fhvp [%d, %d, %d, %d] [%f, %f, %f, %f] out of bound %dx%d ", rc.nX, rc.nY, rc.nW, rc.nH,
                    fhvp.item[i].tBox.fX, fhvp.item[i].tBox.fY, fhvp.item[i].tBox.fW, fhvp.item[i].tBox.fH, canvas.nW, canvas.nH);

            rc.nX = (AX_U16)((AX_F32)canvas.nW * (fhvp.item[i].tBox.fX / (AX_F32)fhvp.nW));
            rc.nY = (AX_U16)((AX_F32)canvas.nH * (fhvp.item[i].tBox.fY / (AX_F32)fhvp.nH));
            rc.nW = (AX_U16)((AX_F32)canvas.nW * (fhvp.item[i].tBox.fW / (AX_F32)fhvp.nW));
            rc.nH = (AX_U16)((AX_F32)canvas.nH * (fhvp.item[i].tBox.fH / (AX_F32)fhvp.nH));
            LOG_M_W(DISPATCHER, "fhvp [%d, %d, %d, %d] [%f, %f, %f, %f] out of bound %dx%d ", rc.nX, rc.nY, rc.nW, rc.nH,
                    fhvp.item[i].tBox.fX, fhvp.item[i].tBox.fY, fhvp.item[i].tBox.fW, fhvp.item[i].tBox.fH, canvas.nW, canvas.nH);
            continue;
        }

        switch (fhvp.item[i].eType) {
            case DETECT_TYPE_FACE:
            case DETECT_TYPE_BODY:
            case DETECT_TYPE_VEHICLE:
            case DETECT_TYPE_CYCLE:
                if (fhvp.item[i].nTrackId == 0) {
                    gdi.nColor = 0xFF8080; /* YYUUVV white */
                } else {
                    const AX_U32 colorList[16] = {0xff8080, 0xb7ba6b, 0xb2d235, 0x5c7a29, 0x7fb80e, 0xfaa755, 0xfab27b, 0xf58220,
                                                  0xf8aba6, 0x1d953f, 0x45b97c, 0x94d6da, 0x009ad6, 0x145b7d, 0xdecb00, 0xf36c21};
                    gdi.nColor = colorList[(fhvp.item[i].nTrackId - 1) % 16];
                }
#ifdef DRAW_FHVP_LABEL
                ++arrCount[fhvp.item[i].eType];
#endif
                break;
            case DETECT_TYPE_PLATE:
                gdi.nColor = 0x00FFFF; /* YYUUVV purple */
#ifdef DRAW_FHVP_LABEL
                ++arrCount[fhvp.item[i].eType];
#endif
                break;
            default:
                break;
        }

        if (DETECT_TYPE_FACE == fhvp.item[i].eType) {
            /* only draw body */
            continue;
        }

        // START_ELAPSED_TIME;
#if 1
        AX_S32 ret = AX_IVPS_DrawRect(&canvas, gdi, rc);
        if (0 != ret) {
            LOG_M_E(DISPATCHER, "draw box [%d, %d, %d, %d] in canvas %dx%d fail, ret = 0x%X", rc.nX, rc.nY, rc.nW, rc.nH, canvas.nW,
                    canvas.nH, ret);
            continue;
        }
#else
        yuv.DrawRect(rc.nX, rc.nY, rc.nW, rc.nH, CYuvHandler::YUV_WHITE);
#endif
        // PRINT_ELAPSED_USEC("cpu draw rect");
    }

#ifdef DRAW_FHVP_LABEL
    AX_CHAR szLabel[64]{0};
    snprintf(szLabel, sizeof(szLabel), "H:%02d V:%02d C:%02d P:%02d", arrCount[DETECT_TYPE_BODY], arrCount[DETECT_TYPE_VEHICLE],
             arrCount[DETECT_TYPE_CYCLE], arrCount[DETECT_TYPE_PLATE]);
    m_font.FillString(szLabel, 8, canvas.nH - 12, &yuv, canvas.nW, canvas.nH);
#endif
}

AX_VOID CDispatcher::GenSimulateResult(DETECT_RESULT_T& tResult) {
    static AX_U32 s_nSeq = 0;
    static AX_U32 s_nResultCount = 1;

    tResult.nCount = s_nResultCount++ % 5 + 1;
    for (AX_U32 i = 0; i < tResult.nCount; i++) {
        tResult.item[i].eType = (DETECT_TYPE_E)(rand() % DETECT_TYPE_BUTT);
        tResult.item[i].tBox.fX = rand() % (1920 - 200);
        tResult.item[i].tBox.fY = rand() % (1080 - 200);
        tResult.item[i].tBox.fW = 100;
        tResult.item[i].tBox.fH = 100;
    }
    tResult.nGrpId = rand() % 32;
    tResult.nW = 1920;
    tResult.nH = 1080;
    tResult.nSeqNum = s_nSeq++;
}
