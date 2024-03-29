/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2012, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TEncCu.cpp
    \brief    Coding Unit (CU) encoder class
*/

#include <stdio.h>
#include "TEncTop.h"
#include "TEncCu.h"
#include "TEncAnalyze.h"

#if QP_ADAPTATION
#include <cmath>
#include <algorithm>
using namespace std;
#endif

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

/**
 \param    uiTotalDepth  total number of allowable depth
 \param    uiMaxWidth    largest CU width
 \param    uiMaxHeight   largest CU height
 */
Void TEncCu::create(UChar uhTotalDepth, UInt uiMaxWidth, UInt uiMaxHeight)
{
  Int i;
  
  m_uhTotalDepth   = uhTotalDepth + 1;
  m_ppcBestCU      = new TComDataCU*[m_uhTotalDepth-1];
  m_ppcTempCU      = new TComDataCU*[m_uhTotalDepth-1];
  
  m_ppcPredYuvBest = new TComYuv*[m_uhTotalDepth-1];
  m_ppcResiYuvBest = new TComYuv*[m_uhTotalDepth-1];
  m_ppcRecoYuvBest = new TComYuv*[m_uhTotalDepth-1];
  m_ppcPredYuvTemp = new TComYuv*[m_uhTotalDepth-1];
  m_ppcResiYuvTemp = new TComYuv*[m_uhTotalDepth-1];
  m_ppcRecoYuvTemp = new TComYuv*[m_uhTotalDepth-1];
  m_ppcOrigYuv     = new TComYuv*[m_uhTotalDepth-1];
  
  UInt uiNumPartitions;
  for( i=0 ; i<m_uhTotalDepth-1 ; i++)
  {
    uiNumPartitions = 1<<( ( m_uhTotalDepth - i - 1 )<<1 );
    UInt uiWidth  = uiMaxWidth  >> i;
    UInt uiHeight = uiMaxHeight >> i;
    
    m_ppcBestCU[i] = new TComDataCU; m_ppcBestCU[i]->create( uiNumPartitions, uiWidth, uiHeight, false, uiMaxWidth >> (m_uhTotalDepth - 1) );
    m_ppcTempCU[i] = new TComDataCU; m_ppcTempCU[i]->create( uiNumPartitions, uiWidth, uiHeight, false, uiMaxWidth >> (m_uhTotalDepth - 1) );
    
    m_ppcPredYuvBest[i] = new TComYuv; m_ppcPredYuvBest[i]->create(uiWidth, uiHeight);
    m_ppcResiYuvBest[i] = new TComYuv; m_ppcResiYuvBest[i]->create(uiWidth, uiHeight);
    m_ppcRecoYuvBest[i] = new TComYuv; m_ppcRecoYuvBest[i]->create(uiWidth, uiHeight);
    
    m_ppcPredYuvTemp[i] = new TComYuv; m_ppcPredYuvTemp[i]->create(uiWidth, uiHeight);
    m_ppcResiYuvTemp[i] = new TComYuv; m_ppcResiYuvTemp[i]->create(uiWidth, uiHeight);
    m_ppcRecoYuvTemp[i] = new TComYuv; m_ppcRecoYuvTemp[i]->create(uiWidth, uiHeight);
    
    m_ppcOrigYuv    [i] = new TComYuv; m_ppcOrigYuv    [i]->create(uiWidth, uiHeight);
  }
  
  m_bEncodeDQP = false;

  // initialize partition order.
  UInt* piTmp = &g_auiZscanToRaster[0];
  initZscanToRaster( m_uhTotalDepth, 1, 0, piTmp);
  initRasterToZscan( uiMaxWidth, uiMaxHeight, m_uhTotalDepth );
  
  // initialize conversion matrix from partition index to pel
  initRasterToPelXY( uiMaxWidth, uiMaxHeight, m_uhTotalDepth );
  initMotionReferIdx ( uiMaxWidth, uiMaxHeight, m_uhTotalDepth );
}

Void TEncCu::destroy()
{
  Int i;
  
  for( i=0 ; i<m_uhTotalDepth-1 ; i++)
  {
    if(m_ppcBestCU[i])
    {
      m_ppcBestCU[i]->destroy();      delete m_ppcBestCU[i];      m_ppcBestCU[i] = NULL;
    }
    if(m_ppcTempCU[i])
    {
      m_ppcTempCU[i]->destroy();      delete m_ppcTempCU[i];      m_ppcTempCU[i] = NULL;
    }
    if(m_ppcPredYuvBest[i])
    {
      m_ppcPredYuvBest[i]->destroy(); delete m_ppcPredYuvBest[i]; m_ppcPredYuvBest[i] = NULL;
    }
    if(m_ppcResiYuvBest[i])
    {
      m_ppcResiYuvBest[i]->destroy(); delete m_ppcResiYuvBest[i]; m_ppcResiYuvBest[i] = NULL;
    }
    if(m_ppcRecoYuvBest[i])
    {
      m_ppcRecoYuvBest[i]->destroy(); delete m_ppcRecoYuvBest[i]; m_ppcRecoYuvBest[i] = NULL;
    }
    if(m_ppcPredYuvTemp[i])
    {
      m_ppcPredYuvTemp[i]->destroy(); delete m_ppcPredYuvTemp[i]; m_ppcPredYuvTemp[i] = NULL;
    }
    if(m_ppcResiYuvTemp[i])
    {
      m_ppcResiYuvTemp[i]->destroy(); delete m_ppcResiYuvTemp[i]; m_ppcResiYuvTemp[i] = NULL;
    }
    if(m_ppcRecoYuvTemp[i])
    {
      m_ppcRecoYuvTemp[i]->destroy(); delete m_ppcRecoYuvTemp[i]; m_ppcRecoYuvTemp[i] = NULL;
    }
    if(m_ppcOrigYuv[i])
    {
      m_ppcOrigYuv[i]->destroy();     delete m_ppcOrigYuv[i];     m_ppcOrigYuv[i] = NULL;
    }
  }
  if(m_ppcBestCU)
  {
    delete [] m_ppcBestCU;
    m_ppcBestCU = NULL;
  }
  if(m_ppcTempCU)
  {
    delete [] m_ppcTempCU;
    m_ppcTempCU = NULL;
  }
  
  if(m_ppcPredYuvBest)
  {
    delete [] m_ppcPredYuvBest;
    m_ppcPredYuvBest = NULL;
  }
  if(m_ppcResiYuvBest)
  {
    delete [] m_ppcResiYuvBest;
    m_ppcResiYuvBest = NULL;
  }
  if(m_ppcRecoYuvBest)
  {
    delete [] m_ppcRecoYuvBest;
    m_ppcRecoYuvBest = NULL;
  }
  if(m_ppcPredYuvTemp)
  {
    delete [] m_ppcPredYuvTemp;
    m_ppcPredYuvTemp = NULL;
  }
  if(m_ppcResiYuvTemp)
  {
    delete [] m_ppcResiYuvTemp;
    m_ppcResiYuvTemp = NULL;
  }
  if(m_ppcRecoYuvTemp)
  {
    delete [] m_ppcRecoYuvTemp;
    m_ppcRecoYuvTemp = NULL;
  }
  if(m_ppcOrigYuv)
  {
    delete [] m_ppcOrigYuv;
    m_ppcOrigYuv = NULL;
  }
}

/** \param    pcEncTop      pointer of encoder class
 */
Void TEncCu::init( TEncTop* pcEncTop )
{
  m_pcEncCfg           = pcEncTop;
  m_pcPredSearch       = pcEncTop->getPredSearch();
  m_pcTrQuant          = pcEncTop->getTrQuant();
  m_pcBitCounter       = pcEncTop->getBitCounter();
  m_pcRdCost           = pcEncTop->getRdCost();
  
  m_pcEntropyCoder     = pcEncTop->getEntropyCoder();
  m_pcCavlcCoder       = pcEncTop->getCavlcCoder();
  m_pcSbacCoder       = pcEncTop->getSbacCoder();
  m_pcBinCABAC         = pcEncTop->getBinCABAC();
  
  m_pppcRDSbacCoder   = pcEncTop->getRDSbacCoder();
  m_pcRDGoOnSbacCoder = pcEncTop->getRDGoOnSbacCoder();
  
  m_bUseSBACRD        = pcEncTop->getUseSBACRD();
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/** \param  rpcCU pointer of CU data class
 */
Void TEncCu::compressCU( TComDataCU*& rpcCU )
{
  // initialize CU data
  m_ppcBestCU[0]->initCU( rpcCU->getPic(), rpcCU->getAddr() );
  m_ppcTempCU[0]->initCU( rpcCU->getPic(), rpcCU->getAddr() );

  // analysis of CU
  xCompressCU( m_ppcBestCU[0], m_ppcTempCU[0], 0 );

#if ADAPTIVE_QP_SELECTION
  if(rpcCU->getSlice()->getSliceType()!=I_SLICE) //IIII
  {
    xLcuCollectARLStats( rpcCU);
  }
#endif
}
/** \param  pcCU  pointer of CU data class, bForceTerminate when set to true terminates slice (default is false).
 */
Void TEncCu::encodeCU ( TComDataCU* pcCU, Bool bForceTerminate )
{
#if G507_QP_ISSUE_FIX
  if ( pcCU->getSlice()->getPPS()->getUseDQP() )
#else
  if ( pcCU->getSlice()->getSPS()->getUseDQP() )
#endif
  {
    setdQPFlag(true);
  }
  // Encode CU data
  xEncodeCU( pcCU, 0, 0 );
  
#if !FINE_GRANULARITY_SLICES
  //--- write terminating bit ---
  Bool bTerminateSlice = bForceTerminate;
  UInt uiCUAddr = pcCU->getAddr();
#if TILES
  if (uiCUAddr == pcCU->getPic()->getPicSym()->getCUOrderMap( (pcCU->getPic()->getNumCUsInFrame()-1) ) )
    bTerminateSlice = true;

  if (uiCUAddr == pcCU->getPic()->getPicSym()->getCUOrderMap( (pcCU->getSlice()->getSliceCurEndCUAddr()-1) ) )
    bTerminateSlice = true;
#else
  if (uiCUAddr == (pcCU->getPic()->getNumCUsInFrame()-1) )
    bTerminateSlice = true;

  if (uiCUAddr == (pcCU->getSlice()->getSliceCurEndCUAddr()-1))
    bTerminateSlice = true;
#endif

#if OL_USE_WPP
  // The 1-terminating bit is added to all streams, so don't add it here when it's 1.
  if (!bTerminateSlice)
    m_pcEntropyCoder->encodeTerminatingBit( bTerminateSlice ? 1 : 0 );
#else
  m_pcEntropyCoder->encodeTerminatingBit( bTerminateSlice ? 1 : 0 );
  
  // Encode slice finish
  if ( bTerminateSlice )
  {
    m_pcEntropyCoder->encodeSliceFinish();
  }
#endif // OL_USE_WPP
#endif
#if OL_FLUSH
#if FINE_GRANULARITY_SLICES
  bool bTerminateSlice = bForceTerminate;
  UInt uiCUAddr = pcCU->getAddr();
#endif
    /* If at the end of an LCU line but not at the end of a substream, perform CABAC flush */
    if (!bTerminateSlice && pcCU->getSlice()->getPPS()->getCabacIstateReset())
    {
      Int iNumSubstreams = pcCU->getSlice()->getPPS()->getNumSubstreams();
      UInt uiWidthInLCUs = pcCU->getPic()->getPicSym()->getFrameWidthInCU();
      UInt uiCol     = uiCUAddr % uiWidthInLCUs;
      UInt uiLin     = uiCUAddr / uiWidthInLCUs;
#if TILES
      Int iBreakDep = pcCU->getPic()->getPicSym()->getTileBoundaryIndependenceIdr();
      UInt uiTileStartLCU = pcCU->getPic()->getPicSym()->getTComTile(pcCU->getPic()->getPicSym()->getTileIdxMap(uiCUAddr))->getFirstCUAddr();
      UInt uiTileLCUX = uiTileStartLCU % uiWidthInLCUs;
      UInt uiTileLCUY = uiTileStartLCU / uiWidthInLCUs;
      UInt uiTileWidth = pcCU->getPic()->getPicSym()->getTComTile(pcCU->getPic()->getPicSym()->getTileIdxMap(uiCUAddr))->getTileWidth();
      UInt uiTileHeight = pcCU->getPic()->getPicSym()->getTComTile(pcCU->getPic()->getPicSym()->getTileIdxMap(uiCUAddr))->getTileHeight();
      Int iNumSubstreamsPerTile = iNumSubstreams;
      if (iBreakDep && pcCU->getSlice()->getPPS()->getEntropyCodingSynchro())
        iNumSubstreamsPerTile /= pcCU->getPic()->getPicSym()->getNumTiles();
      if ((iBreakDep && (uiCol == uiTileLCUX+uiTileWidth-1) && (uiLin+iNumSubstreamsPerTile < uiTileLCUY+uiTileHeight))
          || (!iBreakDep && (uiCol == uiWidthInLCUs-1) && (uiLin+iNumSubstreams < pcCU->getPic()->getFrameHeightInCU())))
#else
      if (uiCol == uiWidthInLCUs-1 && uiLin+iNumSubstreams < pcCU->getPic()->getFrameHeightInCU())
#endif
      {
        m_pcEntropyCoder->encodeFlush();
      }
    }
#endif // OL_FLUSH
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================
/** Derive small set of test modes for AMP encoder speed-up
 *\param   rpcBestCU
 *\param   eParentPartSize
 *\param   bTestAMP_Hor
 *\param   bTestAMP_Ver
 *\param   bTestMergeAMP_Hor
 *\param   bTestMergeAMP_Ver
 *\returns Void 
*/
#if AMP_ENC_SPEEDUP
#if AMP_MRG
Void TEncCu::deriveTestModeAMP (TComDataCU *&rpcBestCU, PartSize eParentPartSize, Bool &bTestAMP_Hor, Bool &bTestAMP_Ver, Bool &bTestMergeAMP_Hor, Bool &bTestMergeAMP_Ver)
#else
Void TEncCu::deriveTestModeAMP (TComDataCU *&rpcBestCU, PartSize eParentPartSize, Bool &bTestAMP_Hor, Bool &bTestAMP_Ver)
#endif
{
  if ( rpcBestCU->getPartitionSize(0) == SIZE_2NxN )
  {
    bTestAMP_Hor = true;
  }
  else if ( rpcBestCU->getPartitionSize(0) == SIZE_Nx2N )
  {
    bTestAMP_Ver = true;
  }
  else if ( rpcBestCU->getPartitionSize(0) == SIZE_2Nx2N && rpcBestCU->getMergeFlag(0) == false && rpcBestCU->isSkipped(0) == false )
  {
    bTestAMP_Hor = true;          
    bTestAMP_Ver = true;          
  }

#if AMP_MRG
  //! Utilizing the partition size of parent PU    
  if ( eParentPartSize >= SIZE_2NxnU && eParentPartSize <= SIZE_nRx2N )
  { 
    bTestMergeAMP_Hor = true;
    bTestMergeAMP_Ver = true;
  }

  if ( eParentPartSize == SIZE_NONE ) //! if parent is intra
  {
    if ( rpcBestCU->getPartitionSize(0) == SIZE_2NxN )
    {
      bTestMergeAMP_Hor = true;
    }
    else if ( rpcBestCU->getPartitionSize(0) == SIZE_Nx2N )
    {
      bTestMergeAMP_Ver = true;
    }
  }

  if ( rpcBestCU->getPartitionSize(0) == SIZE_2Nx2N && rpcBestCU->isSkipped(0) == false )
  {
    bTestMergeAMP_Hor = true;          
    bTestMergeAMP_Ver = true;          
  }

  if ( rpcBestCU->getWidth(0) == 64 )
  { 
    bTestAMP_Hor = false;
    bTestAMP_Ver = false;
  }    
#else
  //! Utilizing the partition size of parent PU        
  if ( eParentPartSize >= SIZE_2NxnU && eParentPartSize <= SIZE_nRx2N )
  { 
    bTestAMP_Hor = true;
    bTestAMP_Ver = true;
  }

  if ( eParentPartSize == SIZE_2Nx2N )
  { 
    bTestAMP_Hor = false;
    bTestAMP_Ver = false;
  }      
#endif
}
#endif

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================
/** Compress a CU block recursively with enabling sub-LCU-level delta QP
 *\param   rpcBestCU
 *\param   rpcTempCU
 *\param   uiDepth
 *\returns Void
 *
 *- for loop of QP value to compress the current CU with all possible QP
*/
#if AMP_ENC_SPEEDUP
Void TEncCu::xCompressCU( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, UInt uiDepth, PartSize eParentPartSize )
#else
Void TEncCu::xCompressCU( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, UInt uiDepth )
#endif
{
  TComPic* pcPic = rpcBestCU->getPic();

  // get Original YUV data from picture
  m_ppcOrigYuv[uiDepth]->copyFromPicYuv( pcPic->getPicYuvOrg(), rpcBestCU->getAddr(), rpcBestCU->getZorderIdxInCU() );

  // variables for fast encoder decision
  Bool    bEarlySkip  = false;
  Bool    bTrySplit    = true;
  Double  fRD_Skip    = MAX_DOUBLE;

  // variable for Early CU determination
#if EARLY_CU_DETERMINATION
  Bool    bSubBranch = true;
#endif

  // variable for Cbf fast mode PU decision
#if CBF_FAST_MODE
  Bool    doNotBlockPu = true;
#endif

  Bool    bTrySplitDQP  = true;

  static  Double  afCost[ MAX_CU_DEPTH ];
  static  Int      aiNum [ MAX_CU_DEPTH ];

  if ( rpcBestCU->getAddr() == 0 )
  {
    ::memset( afCost, 0, sizeof( afCost ) );
    ::memset( aiNum,  0, sizeof( aiNum  ) );
  }

  Bool bBoundary = false;
  UInt uiLPelX   = rpcBestCU->getCUPelX();
  UInt uiRPelX   = uiLPelX + rpcBestCU->getWidth(0)  - 1;
  UInt uiTPelY   = rpcBestCU->getCUPelY();
  UInt uiBPelY   = uiTPelY + rpcBestCU->getHeight(0) - 1;

#if QP_ADAPTATION
  Int iBaseQP = xComputeQP( rpcBestCU, uiDepth );
#else
  Int iBaseQP = rpcBestCU->getSlice()->getSliceQp();
#endif
  Int iMinQP;
  Int iMaxQP;

  if( (g_uiMaxCUWidth>>uiDepth) >= rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    Int idQP = m_pcEncCfg->getMaxDeltaQP();
    iMinQP = Clip3( MIN_QP, MAX_QP, iBaseQP-idQP );
    iMaxQP = Clip3( MIN_QP, MAX_QP, iBaseQP+idQP );
  }
  else
  {
    iMinQP = rpcTempCU->getQP(0);
    iMaxQP = rpcTempCU->getQP(0);
  }

#if FINE_GRANULARITY_SLICES
  // If slice start or slice end is within this cu...
  TComSlice * pcSlice = rpcTempCU->getPic()->getSlice(rpcTempCU->getPic()->getCurrSliceIdx());
  Bool bSliceStart = pcSlice->getEntropySliceCurStartCUAddr()>rpcTempCU->getSCUAddr()&&pcSlice->getEntropySliceCurStartCUAddr()<rpcTempCU->getSCUAddr()+rpcTempCU->getTotalNumPart();
  Bool bSliceEnd = (pcSlice->getEntropySliceCurEndCUAddr()>rpcTempCU->getSCUAddr()&&pcSlice->getEntropySliceCurEndCUAddr()<rpcTempCU->getSCUAddr()+rpcTempCU->getTotalNumPart());
  Bool bInsidePicture = ( uiRPelX < rpcBestCU->getSlice()->getSPS()->getWidth() ) && ( uiBPelY < rpcBestCU->getSlice()->getSPS()->getHeight() );
  // We need to split, so don't try these modes.
  if(!bSliceEnd && !bSliceStart && bInsidePicture )
#else
  if( ( uiRPelX < rpcBestCU->getSlice()->getSPS()->getWidth() ) && ( uiBPelY < rpcBestCU->getSlice()->getSPS()->getHeight() ) )
#endif
  {
    for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++)
    {
      // variables for fast encoder decision
      bEarlySkip  = false;
      bTrySplit    = true;
      fRD_Skip    = MAX_DOUBLE;

      rpcTempCU->initEstData( uiDepth, iQP );

      // do inter modes, SKIP and 2Nx2N
      if( rpcBestCU->getSlice()->getSliceType() != I_SLICE )
      {
        // SKIP

        if( pcPic->getSlice(0)->getSPS()->getUseMRG() )
        {
          xCheckRDCostMerge2Nx2N( rpcBestCU, rpcTempCU );
          rpcTempCU->initEstData( uiDepth, iQP );
        }
        else
        {
          xCheckRDCostAMVPSkip ( rpcBestCU, rpcTempCU );
          rpcTempCU->initEstData( uiDepth, iQP );
        }

        // fast encoder decision for early skip
        if ( m_pcEncCfg->getUseFastEnc() )
        {
          Int iIdx = g_aucConvertToBit[ rpcBestCU->getWidth(0) ];
          if ( aiNum [ iIdx ] > 5 && fRD_Skip < EARLY_SKIP_THRES*afCost[ iIdx ]/aiNum[ iIdx ] )
          {
            bEarlySkip = true;
            bTrySplit  = false;
          }
        }

        // 2Nx2N, NxN
        if ( !bEarlySkip )
        {
          xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2Nx2N );  rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
          if(m_pcEncCfg->getUseCbfFastMode())
          {
            doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
          }
#endif
        }
      }

      if( (g_uiMaxCUWidth>>uiDepth) >= rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
      {
        if(iQP == iBaseQP)
        {
          bTrySplitDQP = bTrySplit;
        }
      }
      else
      {
        bTrySplitDQP = bTrySplit;
      }
    }


    for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++)
    {
      rpcTempCU->initEstData( uiDepth, iQP );

      // do inter modes, NxN, 2NxN, and Nx2N
      if( rpcBestCU->getSlice()->getSliceType() != I_SLICE )
      {
        // 2Nx2N, NxN
        if ( !bEarlySkip )
        {

#if DISABLE_4x4_INTER
        if(!( rpcBestCU->getSlice()->getSPS()->getDisInter4x4()  && (rpcBestCU->getWidth(0)==8) && (rpcBestCU->getHeight(0)==8) ))
        {
#endif
#if CBF_FAST_MODE
          if( uiDepth == g_uiMaxCUDepth - g_uiAddCUDepth && doNotBlockPu)
#else
          if( uiDepth == g_uiMaxCUDepth - g_uiAddCUDepth )
#endif
          {
            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_NxN   );
            rpcTempCU->initEstData( uiDepth, iQP );
          }
#if DISABLE_4x4_INTER
        }
#endif
        }

        { // 2NxN, Nx2N
#if CBF_FAST_MODE
          if(doNotBlockPu)
#endif
          {
            xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_Nx2N  );
            rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
            if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_Nx2N )
            {
              doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
            }
#endif
          }
#if CBF_FAST_MODE
          if(doNotBlockPu)
#endif
          {
            xCheckRDCostInter      ( rpcBestCU, rpcTempCU, SIZE_2NxN  );
            rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
            if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxN)
            {
              doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
            }
#endif
          }
        }

#if AMP
        //! Try AMP (SIZE_2NxnU, SIZE_2NxnD, SIZE_nLx2N, SIZE_nRx2N)
        if( pcPic->getSlice(0)->getSPS()->getAMPAcc(uiDepth) )
        {
#if AMP_ENC_SPEEDUP        
          Bool bTestAMP_Hor = false, bTestAMP_Ver = false;

#if AMP_MRG
          Bool bTestMergeAMP_Hor = false, bTestMergeAMP_Ver = false;

          deriveTestModeAMP (rpcBestCU, eParentPartSize, bTestAMP_Hor, bTestAMP_Ver, bTestMergeAMP_Hor, bTestMergeAMP_Ver);
#else
          deriveTestModeAMP (rpcBestCU, eParentPartSize, bTestAMP_Hor, bTestAMP_Ver);
#endif

          //! Do horizontal AMP
          if ( bTestAMP_Hor )
          {
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU );
              rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
              if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnU )
              {
                doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
              }
#endif
            }
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD );
              rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
              if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnD )
              {
                doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
              }
#endif
            }
          }
#if AMP_MRG
          else if ( bTestMergeAMP_Hor ) 
          {
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU, true );
              rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
              if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnU )
              {
                doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
              }
#endif
            }
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD, true );
              rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
              if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_2NxnD )
              {
                doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
              }
#endif
            }
          }
#endif

          //! Do horizontal AMP
          if ( bTestAMP_Ver )
          {
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N );
              rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
              if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_nLx2N )
              {
                doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
              }
#endif
            }
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N );
              rpcTempCU->initEstData( uiDepth, iQP );
            }
          }
#if AMP_MRG
          else if ( bTestMergeAMP_Ver )
          {
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N, true );
              rpcTempCU->initEstData( uiDepth, iQP );
#if CBF_FAST_MODE
              if(m_pcEncCfg->getUseCbfFastMode() && rpcBestCU->getPartitionSize(0) == SIZE_nLx2N )
              {
                doNotBlockPu = rpcBestCU->getQtRootCbf( 0 ) != 0;
              }
#endif
            }
#if CBF_FAST_MODE
            if(doNotBlockPu)
#endif
            {
              xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N, true );
              rpcTempCU->initEstData( uiDepth, iQP );
            }
          }
#endif

#else
          xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnU );
          rpcTempCU->initEstData( uiDepth, iQP );
          xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_2NxnD );
          rpcTempCU->initEstData( uiDepth, iQP );
          xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nLx2N );
          rpcTempCU->initEstData( uiDepth, iQP );

          xCheckRDCostInter( rpcBestCU, rpcTempCU, SIZE_nRx2N );
          rpcTempCU->initEstData( uiDepth, iQP );

#endif
        }    
#endif
      }

      // initialize PCM flag
      rpcTempCU->setIPCMFlag( 0, false);
      rpcTempCU->setIPCMFlagSubParts ( false, 0, uiDepth); //SUB_LCU_DQP

      // do normal intra modes
      if ( !bEarlySkip )
      {
        // speedup for inter frames
        if( rpcBestCU->getSlice()->getSliceType() == I_SLICE || 
          rpcBestCU->getCbf( 0, TEXT_LUMA     ) != 0   ||
          rpcBestCU->getCbf( 0, TEXT_CHROMA_U ) != 0   ||
          rpcBestCU->getCbf( 0, TEXT_CHROMA_V ) != 0     ) // avoid very complex intra if it is unlikely
        {
          xCheckRDCostIntra( rpcBestCU, rpcTempCU, SIZE_2Nx2N );
          rpcTempCU->initEstData( uiDepth, iQP );
          if( uiDepth == g_uiMaxCUDepth - g_uiAddCUDepth )
          {
            if( rpcTempCU->getWidth(0) > ( 1 << rpcTempCU->getSlice()->getSPS()->getQuadtreeTULog2MinSize() ) )
            {
              xCheckRDCostIntra( rpcBestCU, rpcTempCU, SIZE_NxN   );
              rpcTempCU->initEstData( uiDepth, iQP );
            }
          }
        }
      }

      // test PCM
#if MAX_PCM_SIZE
      if(pcPic->getSlice(0)->getSPS()->getUsePCM()
        && rpcTempCU->getWidth(0) <= (1<<pcPic->getSlice(0)->getSPS()->getPCMLog2MaxSize())
        && rpcTempCU->getWidth(0) >= (1<<pcPic->getSlice(0)->getSPS()->getPCMLog2MinSize()) )
#else
      if(rpcTempCU->getWidth(0) >= (1<<pcPic->getSlice(0)->getSPS()->getPCMLog2MinSize()))
#endif
      {
        UInt uiRawBits = (g_uiBitDepth * rpcBestCU->getWidth(0) * rpcBestCU->getHeight(0) * 3 / 2);
        UInt uiBestBits = rpcBestCU->getTotalBits();
        if((uiBestBits > uiRawBits) || (rpcBestCU->getTotalCost() > m_pcRdCost->calcRdCost(uiRawBits, 0)))
        {
          xCheckIntraPCM (rpcBestCU, rpcTempCU);
          rpcTempCU->initEstData( uiDepth, iQP );
        }
      }
    }

    m_pcEntropyCoder->resetBits();
    m_pcEntropyCoder->encodeSplitFlag( rpcBestCU, 0, uiDepth, true );
    rpcBestCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // split bits
#if FINE_GRANULARITY_SLICES
    if(m_pcEncCfg->getUseSBACRD())
    {
      rpcBestCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
    }
#endif
    rpcBestCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcBestCU->getTotalBits(), rpcBestCU->getTotalDistortion() );

    // accumulate statistics for early skip
    if ( m_pcEncCfg->getUseFastEnc() )
    {
      if ( rpcBestCU->isSkipped(0) )
      {
        Int iIdx = g_aucConvertToBit[ rpcBestCU->getWidth(0) ];
        afCost[ iIdx ] += rpcBestCU->getTotalCost();
        aiNum [ iIdx ] ++;
      }
    }

    // Early CU determination
#if EARLY_CU_DETERMINATION
    if( m_pcEncCfg->getUseEarlyCU() && ((*rpcBestCU->getPredictionMode()) == 0) )
    {
      bSubBranch = false;
    }
    else
    {
      bSubBranch = true;
    }
#endif
  }
#if FINE_GRANULARITY_SLICES
  else if(!(bSliceEnd && bInsidePicture))
#else
  else
#endif
  {
    bBoundary = true;
  }

  if( (g_uiMaxCUWidth>>uiDepth) == rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    Int idQP = m_pcEncCfg->getMaxDeltaQP();
    iMinQP = Clip3( MIN_QP, MAX_QP, iBaseQP-idQP );
    iMaxQP = Clip3( MIN_QP, MAX_QP, iBaseQP+idQP );
  }
  else if( (g_uiMaxCUWidth>>uiDepth) > rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() )
  {
    iMinQP = iBaseQP;
    iMaxQP = iBaseQP;
  }
  else
  {
    Int iStartQP;
#if FINE_GRANULARITY_SLICES
    if( pcPic->getCU( rpcTempCU->getAddr() )->getEntropySliceStartCU(rpcTempCU->getZorderIdxInCU()) == pcSlice->getEntropySliceCurStartCUAddr())
    {
      iStartQP = rpcTempCU->getQP(0);
    }
    else
    {
      UInt uiCurSliceStartPartIdx = pcSlice->getEntropySliceCurStartCUAddr() % pcPic->getNumPartInCU() - rpcTempCU->getZorderIdxInCU();
      iStartQP = rpcTempCU->getQP(uiCurSliceStartPartIdx);
    }
#else
    iStartQP = rpcTempCU->getQP(0);
#endif
    iMinQP = iStartQP;
    iMaxQP = iStartQP;
  }

  for (Int iQP=iMinQP; iQP<=iMaxQP; iQP++)
  {
    rpcTempCU->initEstData( uiDepth, iQP );

    // further split
#if EARLY_CU_DETERMINATION
    if( bSubBranch && bTrySplitDQP && uiDepth < g_uiMaxCUDepth - g_uiAddCUDepth )
#else
    if( bTrySplitDQP && uiDepth < g_uiMaxCUDepth - g_uiAddCUDepth )
#endif
    {
      UChar       uhNextDepth         = uiDepth+1;
      TComDataCU* pcSubBestPartCU     = m_ppcBestCU[uhNextDepth];
      TComDataCU* pcSubTempPartCU     = m_ppcTempCU[uhNextDepth];

      for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++ )
      {
        pcSubBestPartCU->initSubCU( rpcTempCU, uiPartUnitIdx, uhNextDepth, iQP );           // clear sub partition datas or init.
        pcSubTempPartCU->initSubCU( rpcTempCU, uiPartUnitIdx, uhNextDepth, iQP );           // clear sub partition datas or init.

#if FINE_GRANULARITY_SLICES
        Bool bInSlice = pcSubBestPartCU->getSCUAddr()+pcSubBestPartCU->getTotalNumPart()>pcSlice->getEntropySliceCurStartCUAddr()&&pcSubBestPartCU->getSCUAddr()<pcSlice->getEntropySliceCurEndCUAddr();
        if(bInSlice && ( pcSubBestPartCU->getCUPelX() < pcSlice->getSPS()->getWidth() ) && ( pcSubBestPartCU->getCUPelY() < pcSlice->getSPS()->getHeight() ) )
#else
        if( ( pcSubBestPartCU->getCUPelX() < pcSubBestPartCU->getSlice()->getSPS()->getWidth() ) && ( pcSubBestPartCU->getCUPelY() < pcSubBestPartCU->getSlice()->getSPS()->getHeight() ) )
#endif
        {
          if( m_bUseSBACRD )
          {
            if ( 0 == uiPartUnitIdx) //initialize RD with previous depth buffer
            {
              m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]);
            }
            else
            {
              m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]);
            }
          }

#if AMP_ENC_SPEEDUP
          if ( rpcBestCU->isIntra(0) )
          {
            xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth, SIZE_NONE );
          }
          else
          {
            xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth, rpcBestCU->getPartitionSize(0) );
          }
#else
          xCompressCU( pcSubBestPartCU, pcSubTempPartCU, uhNextDepth );
#endif

          rpcTempCU->copyPartFrom( pcSubBestPartCU, uiPartUnitIdx, uhNextDepth );         // Keep best part data to current temporary data.
          xCopyYuv2Tmp( pcSubBestPartCU->getTotalNumPart()*uiPartUnitIdx, uhNextDepth );
        }
#if FINE_GRANULARITY_SLICES
        else if (bInSlice)
#else
        else
#endif
        {
          pcSubBestPartCU->copyToPic( uhNextDepth );
          rpcTempCU->copyPartFrom( pcSubBestPartCU, uiPartUnitIdx, uhNextDepth );
        }
      }

      if( !bBoundary )
      {
        m_pcEntropyCoder->resetBits();
        m_pcEntropyCoder->encodeSplitFlag( rpcTempCU, 0, uiDepth, true );

        rpcTempCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // split bits
#if FINE_GRANULARITY_SLICES
        if(m_pcEncCfg->getUseSBACRD())
        {
          rpcTempCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
        }
#endif
      }
      rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

#if G507_QP_ISSUE_FIX
      if( (g_uiMaxCUWidth>>uiDepth) == rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() && rpcTempCU->getSlice()->getPPS()->getUseDQP())
#else
      if( (g_uiMaxCUWidth>>uiDepth) == rpcTempCU->getSlice()->getPPS()->getMinCuDQPSize() && rpcTempCU->getSlice()->getSPS()->getUseDQP())
#endif
      {
        Bool bHasRedisual = false;
        for( UInt uiBlkIdx = 0; uiBlkIdx < rpcTempCU->getTotalNumPart(); uiBlkIdx ++)
        {
#if FINE_GRANULARITY_SLICES
          if( ( pcPic->getCU( rpcTempCU->getAddr() )->getEntropySliceStartCU(uiBlkIdx+rpcTempCU->getZorderIdxInCU()) == rpcTempCU->getSlice()->getEntropySliceCurStartCUAddr() ) && 
              ( rpcTempCU->getCbf( uiBlkIdx, TEXT_LUMA ) || rpcTempCU->getCbf( uiBlkIdx, TEXT_CHROMA_U ) || rpcTempCU->getCbf( uiBlkIdx, TEXT_CHROMA_V ) ) )
#else
          if ( rpcTempCU->getCbf( uiBlkIdx, TEXT_LUMA ) || rpcTempCU->getCbf( uiBlkIdx, TEXT_CHROMA_U ) || rpcTempCU->getCbf( uiBlkIdx, TEXT_CHROMA_V ) )
#endif
          {
            bHasRedisual = true;
            break;
          }
        }

        UInt uiTargetPartIdx;
#if FINE_GRANULARITY_SLICES
        if ( pcPic->getCU( rpcTempCU->getAddr() )->getEntropySliceStartCU(rpcTempCU->getZorderIdxInCU()) != pcSlice->getEntropySliceCurStartCUAddr() )
        {
          uiTargetPartIdx = pcSlice->getEntropySliceCurStartCUAddr() % pcPic->getNumPartInCU() - rpcTempCU->getZorderIdxInCU();
        }
        else
#endif
        {
          uiTargetPartIdx = 0;
        }
        if ( bHasRedisual )
        {
#if !RDO_WITHOUT_DQP_BITS
          m_pcEntropyCoder->resetBits();
          m_pcEntropyCoder->encodeQP( rpcTempCU, uiTargetPartIdx, false );
          rpcTempCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // dQP bits
#if FINE_GRANULARITY_SLICES
          if(m_pcEncCfg->getUseSBACRD())
          {
            rpcTempCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
          }
#endif
          rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );
#endif
        }
        else
        {
          rpcTempCU->setQPSubParts( rpcTempCU->getRefQP( uiTargetPartIdx ), 0, uiDepth ); // set QP to default QP
        }
      }

      if( m_bUseSBACRD )
      {
        m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);
      }
#if FINE_GRANULARITY_SLICES
      Bool bEntropyLimit=false;
      Bool bSliceLimit=false;
      bSliceLimit=rpcBestCU->getSlice()->getSliceMode()==AD_HOC_SLICES_FIXED_NUMBER_OF_BYTES_IN_SLICE&&(rpcBestCU->getTotalBits()>rpcBestCU->getSlice()->getSliceArgument()<<3);
      if(rpcBestCU->getSlice()->getEntropySliceMode()==SHARP_MULTIPLE_CONSTRAINT_BASED_ENTROPY_SLICE&&m_pcEncCfg->getUseSBACRD())
      {
        if(rpcBestCU->getTotalBins()>rpcBestCU->getSlice()->getEntropySliceArgument())
        {
          bEntropyLimit=true;
        }
      }
      else if(rpcBestCU->getSlice()->getEntropySliceMode()==SHARP_MULTIPLE_CONSTRAINT_BASED_ENTROPY_SLICE)
      {
        if(rpcBestCU->getTotalBits()>rpcBestCU->getSlice()->getEntropySliceArgument())
        {
          bEntropyLimit=true;
        }
      }
      if(rpcBestCU->getDepth(0)>=rpcBestCU->getSlice()->getPPS()->getSliceGranularity())
      {
        bSliceLimit=false;
        bEntropyLimit=false;
      }
      if(bSliceLimit||bEntropyLimit)
      {
        rpcBestCU->getTotalCost()=rpcTempCU->getTotalCost()+1;
      }
#endif
      xCheckBestMode( rpcBestCU, rpcTempCU, uiDepth);                                  // RD compare current larger prediction
    }                                                                                  // with sub partitioned prediction.

  }

  rpcBestCU->copyToPic(uiDepth);                                                     // Copy Best data to Picture for next partition prediction.

  xCopyYuv2Pic( rpcBestCU->getPic(), rpcBestCU->getAddr(), rpcBestCU->getZorderIdxInCU(), uiDepth, uiDepth, rpcBestCU, uiLPelX, uiTPelY );   // Copy Yuv data to picture Yuv
#if FINE_GRANULARITY_SLICES
  if( bBoundary ||(bSliceEnd && bInsidePicture))
#else
  if( bBoundary )
#endif
  {
    return;
  }

  // Assert if Best prediction mode is NONE
  // Selected mode's RD-cost must be not MAX_DOUBLE.
  assert( rpcBestCU->getPartitionSize ( 0 ) != SIZE_NONE  );
  assert( rpcBestCU->getPredictionMode( 0 ) != MODE_NONE  );
  assert( rpcBestCU->getTotalCost     (   ) != MAX_DOUBLE );
}

#if FINE_GRANULARITY_SLICES
/** finish encoding a cu and handle end-of-slice conditions
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth 
 * \returns Void
 */
Void TEncCu::finishCU( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  TComPic* pcPic = pcCU->getPic();
  TComSlice * pcSlice = pcCU->getPic()->getSlice(pcCU->getPic()->getCurrSliceIdx());

  //Calculate end address
  UInt uiCUAddr = pcCU->getSCUAddr()+uiAbsPartIdx;

#if TILES
  UInt uiInternalAddress = pcPic->getPicSym()->getPicSCUAddr(pcSlice->getEntropySliceCurEndCUAddr()-1) % pcPic->getNumPartInCU();
  UInt uiExternalAddress = pcPic->getPicSym()->getPicSCUAddr(pcSlice->getEntropySliceCurEndCUAddr()-1) / pcPic->getNumPartInCU();
#else
  UInt uiInternalAddress = (pcSlice->getEntropySliceCurEndCUAddr()-1) % pcCU->getPic()->getNumPartInCU();
  UInt uiExternalAddress = (pcSlice->getEntropySliceCurEndCUAddr()-1) / pcCU->getPic()->getNumPartInCU();
#endif
  UInt uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
  UInt uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
  UInt uiWidth = pcSlice->getSPS()->getWidth();
  UInt uiHeight = pcSlice->getSPS()->getHeight();
  while(uiPosX>=uiWidth||uiPosY>=uiHeight)
  {
    uiInternalAddress--;
    uiPosX = ( uiExternalAddress % pcPic->getFrameWidthInCU() ) * g_uiMaxCUWidth+ g_auiRasterToPelX[ g_auiZscanToRaster[uiInternalAddress] ];
    uiPosY = ( uiExternalAddress / pcPic->getFrameWidthInCU() ) * g_uiMaxCUHeight+ g_auiRasterToPelY[ g_auiZscanToRaster[uiInternalAddress] ];
  }
  uiInternalAddress++;
  if(uiInternalAddress==pcCU->getPic()->getNumPartInCU())
  {
    uiInternalAddress = 0;
#if TILES
    uiExternalAddress = pcPic->getPicSym()->getCUOrderMap(pcPic->getPicSym()->getInverseCUOrderMap(uiExternalAddress)+1);
#else
    uiExternalAddress++;
#endif
  }
#if TILES
  UInt uiRealEndAddress = pcPic->getPicSym()->getPicSCUEncOrder(uiExternalAddress*pcPic->getNumPartInCU()+uiInternalAddress);
#else
  UInt uiRealEndAddress = uiExternalAddress*pcCU->getPic()->getNumPartInCU()+uiInternalAddress;
#endif

  // Encode slice finish
  Bool bTerminateSlice = false;
  if (uiCUAddr+(pcCU->getPic()->getNumPartInCU()>>(uiDepth<<1)) == uiRealEndAddress)
  {
    bTerminateSlice = true;
  }
  UInt uiGranularityWidth = g_uiMaxCUWidth>>(pcSlice->getPPS()->getSliceGranularity());
  uiPosX = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsPartIdx] ];
  uiPosY = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsPartIdx] ];
  Bool granularityBoundary=((uiPosX+pcCU->getWidth(uiAbsPartIdx))%uiGranularityWidth==0||(uiPosX+pcCU->getWidth(uiAbsPartIdx)==uiWidth))
    &&((uiPosY+pcCU->getHeight(uiAbsPartIdx))%uiGranularityWidth==0||(uiPosY+pcCU->getHeight(uiAbsPartIdx)==uiHeight));
  if(granularityBoundary)
  {
#if OL_USE_WPP
    // The 1-terminating bit is added to all streams, so don't add it here when it's 1.
    if (!bTerminateSlice)
      m_pcEntropyCoder->encodeTerminatingBit( bTerminateSlice ? 1 : 0 );
#else
    m_pcEntropyCoder->encodeTerminatingBit( bTerminateSlice ? 1 : 0 );
#endif
  }
#if !OL_USE_WPP
  if ( bTerminateSlice )
  {
    m_pcEntropyCoder->encodeSliceFinish();
  }
#endif
  
  Int numberOfWrittenBits = 0;
  if (m_pcBitCounter)
  {
    numberOfWrittenBits = m_pcEntropyCoder->getNumberOfWrittenBits();
  }
  
  // Calculate slice end IF this CU puts us over slice bit size.
  unsigned iGranularitySize = pcCU->getPic()->getNumPartInCU()>>(pcSlice->getPPS()->getSliceGranularity()<<1);
  int iGranularityEnd = ((pcCU->getSCUAddr()+uiAbsPartIdx)/iGranularitySize)*iGranularitySize;
  if(iGranularityEnd<=pcSlice->getEntropySliceCurStartCUAddr()) 
  {
    iGranularityEnd+=max(iGranularitySize,(pcCU->getPic()->getNumPartInCU()>>(uiDepth<<1)));
  }
  // Set slice end parameter
  if(pcSlice->getSliceMode()==AD_HOC_SLICES_FIXED_NUMBER_OF_BYTES_IN_SLICE&&!pcSlice->getFinalized()&&pcSlice->getSliceBits()+numberOfWrittenBits>pcSlice->getSliceArgument()<<3) 
  {
    pcSlice->setEntropySliceCurEndCUAddr(iGranularityEnd);
    pcSlice->setSliceCurEndCUAddr(iGranularityEnd);
    return;
  }
  // Set entropy slice end parameter
  if(m_pcEncCfg->getUseSBACRD()) 
  {
    TEncBinCABAC *pppcRDSbacCoder = (TEncBinCABAC *) m_pppcRDSbacCoder[0][CI_CURR_BEST]->getEncBinIf();
    UInt uiBinsCoded = pppcRDSbacCoder->getBinsCoded();
    if(pcSlice->getEntropySliceMode()==SHARP_MULTIPLE_CONSTRAINT_BASED_ENTROPY_SLICE&&!pcSlice->getFinalized()&&pcSlice->getEntropySliceCounter()+uiBinsCoded>pcSlice->getEntropySliceArgument())
    {
      pcSlice->setEntropySliceCurEndCUAddr(iGranularityEnd);
      return;
    }
  }
  else
  {
    if(pcSlice->getEntropySliceMode()==SHARP_MULTIPLE_CONSTRAINT_BASED_ENTROPY_SLICE&&!pcSlice->getFinalized()&&pcSlice->getEntropySliceCounter()+numberOfWrittenBits>pcSlice->getEntropySliceArgument()) 
    {
      pcSlice->setEntropySliceCurEndCUAddr(iGranularityEnd);
      return;
    }
  }
  if(granularityBoundary)
  {
    pcSlice->setSliceBits( (UInt)(pcSlice->getSliceBits() + numberOfWrittenBits) );
    if(m_pcEncCfg->getUseSBACRD()) 
    {
      TEncBinCABAC *pppcRDSbacCoder = (TEncBinCABAC *) m_pppcRDSbacCoder[0][CI_CURR_BEST]->getEncBinIf();
      pcSlice->setEntropySliceCounter(pcSlice->getEntropySliceCounter()+pppcRDSbacCoder->getBinsCoded());
      pppcRDSbacCoder->setBinsCoded( 0 );
    }
    else 
    {
      pcSlice->setEntropySliceCounter(pcSlice->getEntropySliceCounter()+numberOfWrittenBits);
    }
    if (m_pcBitCounter)
    {
      m_pcEntropyCoder->resetBits();      
    }
  }
}
#endif

#if QP_ADAPTATION
/** Compute QP for each CU
 * \param pcCU Target CU
 * \param uiDepth CU depth
 * \returns quantization parameter
 */
Int TEncCu::xComputeQP( TComDataCU* pcCU, UInt uiDepth )
{
  Int iBaseQp = pcCU->getSlice()->getSliceQp();
  Int iQpOffset = 0;
  if ( m_pcEncCfg->getUseAdaptiveQP() )
  {
    TEncPic* pcEPic = dynamic_cast<TEncPic*>( pcCU->getPic() );
    UInt uiAQDepth = min( uiDepth, pcEPic->getMaxAQDepth()-1 );
    TEncPicQPAdaptationLayer* pcAQLayer = pcEPic->getAQLayer( uiAQDepth );
    UInt uiAQUPosX = pcCU->getCUPelX() / pcAQLayer->getAQPartWidth();
    UInt uiAQUPosY = pcCU->getCUPelY() / pcAQLayer->getAQPartHeight();
    UInt uiAQUStride = pcAQLayer->getAQPartStride();
    TEncQPAdaptationUnit* acAQU = pcAQLayer->getQPAdaptationUnit();

    Double dMaxQScale = pow(2.0, m_pcEncCfg->getQPAdaptationRange()/6.0);
    Double dAvgAct = pcAQLayer->getAvgActivity();
    Double dCUAct = acAQU[uiAQUPosY * uiAQUStride + uiAQUPosX].getActivity();
    Double dNormAct = (dMaxQScale*dCUAct + dAvgAct) / (dCUAct + dMaxQScale*dAvgAct);
    Double dQpOffset = log(dNormAct) / log(2.0) * 6.0;
    iQpOffset = Int(floor( dQpOffset + 0.49999 ));
  }
  return Clip3( MIN_QP, MAX_QP, iBaseQp+iQpOffset );
}
#endif

/** encode a CU block recursively
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth 
 * \returns Void
 */
Void TEncCu::xEncodeCU( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  TComPic* pcPic = pcCU->getPic();
  
  Bool bBoundary = false;
  UInt uiLPelX   = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsPartIdx] ];
  UInt uiRPelX   = uiLPelX + (g_uiMaxCUWidth>>uiDepth)  - 1;
  UInt uiTPelY   = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsPartIdx] ];
  UInt uiBPelY   = uiTPelY + (g_uiMaxCUHeight>>uiDepth) - 1;
  
#if FINE_GRANULARITY_SLICES
  TComSlice * pcSlice = pcCU->getPic()->getSlice(pcCU->getPic()->getCurrSliceIdx());
  // If slice start is within this cu...
#if TILES
  Bool bSliceStart = pcSlice->getEntropySliceCurStartCUAddr() > pcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx && 
    pcSlice->getEntropySliceCurStartCUAddr() < pcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcPic->getNumPartInCU() >> (uiDepth<<1) );
#else
  Bool bSliceStart = pcSlice->getEntropySliceCurStartCUAddr()>pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx&&pcSlice->getEntropySliceCurStartCUAddr()<pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcPic->getNumPartInCU() >> (uiDepth<<1) );
#endif
  // We need to split, so don't try these modes.
  if(!bSliceStart&&( uiRPelX < pcSlice->getSPS()->getWidth() ) && ( uiBPelY < pcSlice->getSPS()->getHeight() ) )
#else
  if( ( uiRPelX < pcCU->getSlice()->getSPS()->getWidth() ) && ( uiBPelY < pcCU->getSlice()->getSPS()->getHeight() ) )
#endif
  {
    m_pcEntropyCoder->encodeSplitFlag( pcCU, uiAbsPartIdx, uiDepth );
  }
  else
  {
    bBoundary = true;
  }
  
  if( ( ( uiDepth < pcCU->getDepth( uiAbsPartIdx ) ) && ( uiDepth < (g_uiMaxCUDepth-g_uiAddCUDepth) ) ) || bBoundary )
  {
    UInt uiQNumParts = ( pcPic->getNumPartInCU() >> (uiDepth<<1) )>>2;
#if G507_QP_ISSUE_FIX
    if( (g_uiMaxCUWidth>>uiDepth) == pcCU->getSlice()->getPPS()->getMinCuDQPSize() && pcCU->getSlice()->getPPS()->getUseDQP())
#else
    if( (g_uiMaxCUWidth>>uiDepth) == pcCU->getSlice()->getPPS()->getMinCuDQPSize() && pcCU->getSlice()->getSPS()->getUseDQP())
#endif
    {
      setdQPFlag(true);
    }
    for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++, uiAbsPartIdx+=uiQNumParts )
    {
      uiLPelX   = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsPartIdx] ];
      uiTPelY   = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsPartIdx] ];
#if FINE_GRANULARITY_SLICES
      Bool bInSlice = pcCU->getSCUAddr()+uiAbsPartIdx+uiQNumParts>pcSlice->getEntropySliceCurStartCUAddr()&&pcCU->getSCUAddr()+uiAbsPartIdx<pcSlice->getEntropySliceCurEndCUAddr();
      if(bInSlice&&( uiLPelX < pcSlice->getSPS()->getWidth() ) && ( uiTPelY < pcSlice->getSPS()->getHeight() ) )
#else
      if( ( uiLPelX < pcCU->getSlice()->getSPS()->getWidth() ) && ( uiTPelY < pcCU->getSlice()->getSPS()->getHeight() ) )
#endif
      {
        xEncodeCU( pcCU, uiAbsPartIdx, uiDepth+1 );
      }
    }
    return;
  }
  
#if G507_QP_ISSUE_FIX
  if( (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuDQPSize() && pcCU->getSlice()->getPPS()->getUseDQP())
#else
  if( (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuDQPSize() && pcCU->getSlice()->getSPS()->getUseDQP())
#endif
  {
    setdQPFlag(true);
  }
  if( !pcCU->getSlice()->isIntra() )
  {
    m_pcEntropyCoder->encodeSkipFlag( pcCU, uiAbsPartIdx );
  }
  
  if( pcCU->isSkipped( uiAbsPartIdx ) )
  {
    if ( pcCU->getSlice()->getSPS()->getUseMRG() )
    {
      m_pcEntropyCoder->encodeMergeIndex( pcCU, uiAbsPartIdx, 0 );
    } 
    else
    {
      for ( UInt uiRefListIdx = 0; uiRefListIdx < 2; uiRefListIdx++ )
      {        
        if ( pcCU->getSlice()->getNumRefIdx( RefPicList( uiRefListIdx ) ) > 0 )
        {
          m_pcEntropyCoder->encodeMVPIdxPU( pcCU, uiAbsPartIdx, RefPicList( uiRefListIdx ));
        }
      }
    }
#if FINE_GRANULARITY_SLICES
    finishCU(pcCU,uiAbsPartIdx,uiDepth);
#endif
    return;
  }
  m_pcEntropyCoder->encodePredMode( pcCU, uiAbsPartIdx );
  
  m_pcEntropyCoder->encodePartSize( pcCU, uiAbsPartIdx, uiDepth );
  
  if (pcCU->isIntra( uiAbsPartIdx ) && pcCU->getPartitionSize( uiAbsPartIdx ) == SIZE_2Nx2N )
  {
    m_pcEntropyCoder->encodeIPCMInfo( pcCU, uiAbsPartIdx );

    if(pcCU->getIPCMFlag(uiAbsPartIdx))
    {
#if FINE_GRANULARITY_SLICES
      // Encode slice finish
      finishCU(pcCU,uiAbsPartIdx,uiDepth);
#endif
      return;
    }
  }

  // prediction Info ( Intra : direction mode, Inter : Mv, reference idx )
  m_pcEntropyCoder->encodePredInfo( pcCU, uiAbsPartIdx );
  
  // Encode Coefficients
  Bool bCodeDQP = getdQPFlag();
  m_pcEntropyCoder->encodeCoeff( pcCU, uiAbsPartIdx, uiDepth, pcCU->getWidth (uiAbsPartIdx), pcCU->getHeight(uiAbsPartIdx), bCodeDQP );
  setdQPFlag( bCodeDQP );

  // --- write terminating bit ---
#if FINE_GRANULARITY_SLICES
  finishCU(pcCU,uiAbsPartIdx,uiDepth);
#endif
}

Void TEncCu::xCheckRDCostSkip( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, Bool bSkipRes )
{
  UChar uhDepth = rpcTempCU->getDepth( 0 );
  
  rpcTempCU->setPredModeSubParts( MODE_SKIP,   0, uhDepth );
  rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N,  0, uhDepth );
  
  m_pcPredSearch->predInterSkipSearch       ( rpcTempCU,
                                             m_ppcOrigYuv    [uhDepth],
                                             m_ppcPredYuvTemp[uhDepth],
                                             m_ppcResiYuvTemp[uhDepth],
                                             m_ppcRecoYuvTemp[uhDepth] );
  
  m_pcPredSearch->encodeResAndCalcRdInterCU ( rpcTempCU,
                                             m_ppcOrigYuv    [uhDepth],
                                             m_ppcPredYuvTemp[uhDepth],
                                             m_ppcResiYuvTemp[uhDepth],
                                             m_ppcResiYuvBest[uhDepth],
                                             m_ppcRecoYuvTemp[uhDepth],
                                             bSkipRes );
  
  xCheckDQP( rpcTempCU );
  xCheckBestMode(rpcBestCU, rpcTempCU, uhDepth);
}

/** check RD costs for a CU block encoded with merge
 * \param rpcBestCU
 * \param rpcTempCU
 * \returns Void
 */
Void TEncCu::xCheckRDCostMerge2Nx2N( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU )
{
  assert( rpcTempCU->getSlice()->getSliceType() != I_SLICE );
  TComMvField  cMvFieldNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
  UChar uhInterDirNeighbours[MRG_MAX_NUM_CANDS];
  Int numValidMergeCand = 0;

  for( UInt ui = 0; ui < MRG_MAX_NUM_CANDS; ++ui )
  {
    uhInterDirNeighbours[ui] = 0;
  }
  UChar uhDepth = rpcTempCU->getDepth( 0 );
  rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N, 0, uhDepth ); // interprets depth relative to LCU level
  rpcTempCU->getInterMergeCandidates( 0, 0, uhDepth, cMvFieldNeighbours,uhInterDirNeighbours, numValidMergeCand );
  
  for( UInt uiMergeCand = 0; uiMergeCand < numValidMergeCand; ++uiMergeCand )
  {
    {
      TComYuv* pcPredYuvTemp = NULL;
      for( UInt uiNoResidual = 0; uiNoResidual < 2; ++uiNoResidual )
      {
      // set MC parameters
      rpcTempCU->setPredModeSubParts( MODE_SKIP, 0, uhDepth ); // interprets depth relative to LCU level
      rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N, 0, uhDepth ); // interprets depth relative to LCU level
      rpcTempCU->setMergeFlagSubParts( true, 0, 0, uhDepth ); // interprets depth relative to LCU level
      rpcTempCU->setMergeIndexSubParts( uiMergeCand, 0, 0, uhDepth ); // interprets depth relative to LCU level
      rpcTempCU->setInterDirSubParts( uhInterDirNeighbours[uiMergeCand], 0, 0, uhDepth ); // interprets depth relative to LCU level
      rpcTempCU->getCUMvField( REF_PIC_LIST_0 )->setAllMvField( cMvFieldNeighbours[0 + 2*uiMergeCand], SIZE_2Nx2N, 0, 0 ); // interprets depth relative to rpcTempCU level
      rpcTempCU->getCUMvField( REF_PIC_LIST_1 )->setAllMvField( cMvFieldNeighbours[1 + 2*uiMergeCand], SIZE_2Nx2N, 0, 0 ); // interprets depth relative to rpcTempCU level

      // do MC
      if ( uiNoResidual == 0 )
      {
        m_pcPredSearch->motionCompensation ( rpcTempCU, m_ppcPredYuvTemp[uhDepth] );
        // save pred adress
        pcPredYuvTemp = m_ppcPredYuvTemp[uhDepth];
        
      }
      else
      {
        if ( pcPredYuvTemp != m_ppcPredYuvTemp[uhDepth])
        {
          //adress changes take best (old temp)
          pcPredYuvTemp = m_ppcPredYuvBest[uhDepth];
        }
      }
      // estimate residual and encode everything
      m_pcPredSearch->encodeResAndCalcRdInterCU( rpcTempCU,
                                                m_ppcOrigYuv    [uhDepth],
                                                pcPredYuvTemp,
                                                m_ppcResiYuvTemp[uhDepth],
                                                m_ppcResiYuvBest[uhDepth],
                                                m_ppcRecoYuvTemp[uhDepth],
                                                (uiNoResidual? true:false) );     
      Bool bQtRootCbf = rpcTempCU->getQtRootCbf(0) == 1;

      UInt uiOrgQP = rpcTempCU->getQP( 0 );
      xCheckDQP( rpcTempCU );
      xCheckBestMode(rpcBestCU, rpcTempCU, uhDepth);
      rpcTempCU->initEstData( uhDepth, uiOrgQP );

      if (!bQtRootCbf)
        break;
      }
    }
  }
}

#if AMP_MRG
Void TEncCu::xCheckRDCostInter( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, PartSize ePartSize, Bool bUseMRG)
#else
Void TEncCu::xCheckRDCostInter( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, PartSize ePartSize )
#endif
{
  UChar uhDepth = rpcTempCU->getDepth( 0 );
  
  rpcTempCU->setDepthSubParts( uhDepth, 0 );
  
  rpcTempCU->setPartSizeSubParts  ( ePartSize,  0, uhDepth );
  rpcTempCU->setPredModeSubParts  ( MODE_INTER, 0, uhDepth );
  
#if AMP_MRG
  rpcTempCU->setMergeAMP (true);
  m_pcPredSearch->predInterSearch ( rpcTempCU, m_ppcOrigYuv[uhDepth], m_ppcPredYuvTemp[uhDepth], m_ppcResiYuvTemp[uhDepth], m_ppcRecoYuvTemp[uhDepth], false, bUseMRG );
#else  
  m_pcPredSearch->predInterSearch ( rpcTempCU, m_ppcOrigYuv[uhDepth], m_ppcPredYuvTemp[uhDepth], m_ppcResiYuvTemp[uhDepth], m_ppcRecoYuvTemp[uhDepth] );
#endif

#if AMP_MRG
  if ( !rpcTempCU->getMergeAMP() )
  {
    return;
  }
#endif

  m_pcPredSearch->encodeResAndCalcRdInterCU( rpcTempCU, m_ppcOrigYuv[uhDepth], m_ppcPredYuvTemp[uhDepth], m_ppcResiYuvTemp[uhDepth], m_ppcResiYuvBest[uhDepth], m_ppcRecoYuvTemp[uhDepth], false );
  rpcTempCU->getTotalCost()  = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

  xCheckDQP( rpcTempCU );
  xCheckBestMode(rpcBestCU, rpcTempCU, uhDepth);
}

Void TEncCu::xCheckRDCostIntra( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, PartSize eSize )
{
  UInt uiDepth = rpcTempCU->getDepth( 0 );
  
  rpcTempCU->setPartSizeSubParts( eSize, 0, uiDepth );
  rpcTempCU->setPredModeSubParts( MODE_INTRA, 0, uiDepth );
  
  Bool bSeparateLumaChroma = true; // choose estimation mode
  UInt uiPreCalcDistC      = 0;
  if( !bSeparateLumaChroma )
  {
    m_pcPredSearch->preestChromaPredMode( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth] );
  }
  m_pcPredSearch  ->estIntraPredQT      ( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth], m_ppcResiYuvTemp[uiDepth], m_ppcRecoYuvTemp[uiDepth], uiPreCalcDistC, bSeparateLumaChroma );

  m_ppcRecoYuvTemp[uiDepth]->copyToPicLuma(rpcTempCU->getPic()->getPicYuvRec(), rpcTempCU->getAddr(), rpcTempCU->getZorderIdxInCU() );
  
  m_pcPredSearch  ->estIntraPredChromaQT( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth], m_ppcResiYuvTemp[uiDepth], m_ppcRecoYuvTemp[uiDepth], uiPreCalcDistC );
  
  m_pcEntropyCoder->resetBits();
  m_pcEntropyCoder->encodeSkipFlag ( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePredMode( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePartSize( rpcTempCU, 0, uiDepth, true );
  m_pcEntropyCoder->encodePredInfo( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodeIPCMInfo(rpcTempCU, 0, true );

  // Encode Coefficients
  Bool bCodeDQP = getdQPFlag();
  m_pcEntropyCoder->encodeCoeff( rpcTempCU, 0, uiDepth, rpcTempCU->getWidth (0), rpcTempCU->getHeight(0), bCodeDQP );
  setdQPFlag( bCodeDQP );
  
  if( m_bUseSBACRD ) m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);
  
  rpcTempCU->getTotalBits() = m_pcEntropyCoder->getNumberOfWrittenBits();
#if FINE_GRANULARITY_SLICES
  if(m_pcEncCfg->getUseSBACRD())
  {
    rpcTempCU->getTotalBins() = ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
  }
#endif
  rpcTempCU->getTotalCost() = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

  char * test1 = rpcTempCU->getPartitionSize();
  char * test2 = rpcTempCU->getPredictionMode();
  cout << "Partition Size: " << test1 << "Prediction Mode: " << test2 << "Partition size:" << eSize << endl;

  xCheckDQP( rpcTempCU );
  xCheckBestMode(rpcBestCU, rpcTempCU, uiDepth);
}

/** Check R-D costs for a CU with PCM mode. 
 * \param rpcBestCU pointer to best mode CU data structure
 * \param rpcTempCU pointer to testing mode CU data structure
 * \returns Void
 * 
 * \note Current PCM implementation encodes sample values in a lossless way. The distortion of PCM mode CUs are zero. PCM mode is selected if the best mode yields bits greater than that of PCM mode.
 */
Void TEncCu::xCheckIntraPCM( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU )
{
  UInt uiDepth = rpcTempCU->getDepth( 0 );

  rpcTempCU->setIPCMFlag(0, true);
  rpcTempCU->setIPCMFlagSubParts (true, 0, rpcTempCU->getDepth(0));
  rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N, 0, uiDepth );
  rpcTempCU->setPredModeSubParts( MODE_INTRA, 0, uiDepth );

  m_pcPredSearch->IPCMSearch( rpcTempCU, m_ppcOrigYuv[uiDepth], m_ppcPredYuvTemp[uiDepth], m_ppcResiYuvTemp[uiDepth], m_ppcRecoYuvTemp[uiDepth]);

  if( m_bUseSBACRD ) m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]);

  m_pcEntropyCoder->resetBits();
  m_pcEntropyCoder->encodeSkipFlag ( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePredMode ( rpcTempCU, 0,          true );
  m_pcEntropyCoder->encodePartSize ( rpcTempCU, 0, uiDepth, true );
  m_pcEntropyCoder->encodeIPCMInfo ( rpcTempCU, 0, true );

  if( m_bUseSBACRD ) m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);

  rpcTempCU->getTotalBits() = m_pcEntropyCoder->getNumberOfWrittenBits();
#if FINE_GRANULARITY_SLICES
  if(m_pcEncCfg->getUseSBACRD())
  {
    rpcTempCU->getTotalBins() = ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
  }
#endif
  rpcTempCU->getTotalCost() = m_pcRdCost->calcRdCost( rpcTempCU->getTotalBits(), rpcTempCU->getTotalDistortion() );

  xCheckDQP( rpcTempCU );
  xCheckBestMode( rpcBestCU, rpcTempCU, uiDepth );
}

// check whether current try is the best
Void TEncCu::xCheckBestMode( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU )
{
  if( rpcTempCU->getTotalCost() < rpcBestCU->getTotalCost() )
  {
    TComYuv* pcYuv;
    UChar uhDepth = rpcBestCU->getDepth(0);

    // Change Information data
    TComDataCU* pcCU = rpcBestCU;
    rpcBestCU = rpcTempCU;
    rpcTempCU = pcCU;
    
    // Change Prediction data
    pcYuv = m_ppcPredYuvBest[uhDepth];
    m_ppcPredYuvBest[uhDepth] = m_ppcPredYuvTemp[uhDepth];
    m_ppcPredYuvTemp[uhDepth] = pcYuv;
    
    // Change Reconstruction data
    pcYuv = m_ppcRecoYuvBest[uhDepth];
    m_ppcRecoYuvBest[uhDepth] = m_ppcRecoYuvTemp[uhDepth];
    m_ppcRecoYuvTemp[uhDepth] = pcYuv;
    
    pcYuv = NULL;
    pcCU  = NULL;
    
    if( m_bUseSBACRD )  // store temp best CI for next CU coding
      m_pppcRDSbacCoder[uhDepth][CI_TEMP_BEST]->store(m_pppcRDSbacCoder[uhDepth][CI_NEXT_BEST]);
  }
}

/** check whether current try is the best with identifying the depth of current try
 * \param rpcBestCU
 * \param rpcTempCU
 * \returns Void
 */
Void TEncCu::xCheckBestMode( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU, UInt uiDepth )
{
  if( rpcTempCU->getTotalCost() < rpcBestCU->getTotalCost() )
  {
    TComYuv* pcYuv;
    // Change Information data
    TComDataCU* pcCU = rpcBestCU;
    rpcBestCU = rpcTempCU;
    rpcTempCU = pcCU;

//    cout << "Best CU: " << rpcBestCU << "TempCU: " << rpcTempCU << endl;
    // Change Prediction data
    pcYuv = m_ppcPredYuvBest[uiDepth];
    m_ppcPredYuvBest[uiDepth] = m_ppcPredYuvTemp[uiDepth];
    m_ppcPredYuvTemp[uiDepth] = pcYuv;

    // Change Reconstruction data
    pcYuv = m_ppcRecoYuvBest[uiDepth];
    m_ppcRecoYuvBest[uiDepth] = m_ppcRecoYuvTemp[uiDepth];
    m_ppcRecoYuvTemp[uiDepth] = pcYuv;

    pcYuv = NULL;
    pcCU  = NULL;

    if( m_bUseSBACRD )  // store temp best CI for next CU coding
      m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]->store(m_pppcRDSbacCoder[uiDepth][CI_NEXT_BEST]);
  }
}

Void TEncCu::xCheckDQP( TComDataCU* pcCU )
{
  UInt uiDepth = pcCU->getDepth( 0 );

#if G507_QP_ISSUE_FIX
  if( pcCU->getSlice()->getPPS()->getUseDQP() && (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuDQPSize() )
#else
  if( pcCU->getSlice()->getSPS()->getUseDQP() && (g_uiMaxCUWidth>>uiDepth) >= pcCU->getSlice()->getPPS()->getMinCuDQPSize() )
#endif
  {
    if ( pcCU->getCbf( 0, TEXT_LUMA, 0 ) || pcCU->getCbf( 0, TEXT_CHROMA_U, 0 ) || pcCU->getCbf( 0, TEXT_CHROMA_V, 0 ) )
    {
#if !RDO_WITHOUT_DQP_BITS
      m_pcEntropyCoder->resetBits();
      m_pcEntropyCoder->encodeQP( pcCU, 0, false );
      pcCU->getTotalBits() += m_pcEntropyCoder->getNumberOfWrittenBits(); // dQP bits
#if FINE_GRANULARITY_SLICES
      if(m_pcEncCfg->getUseSBACRD())
      {
        pcCU->getTotalBins() += ((TEncBinCABAC *)((TEncSbac*)m_pcEntropyCoder->m_pcEntropyCoderIf)->getEncBinIf())->getBinsCoded();
      }
#endif
      pcCU->getTotalCost() = m_pcRdCost->calcRdCost( pcCU->getTotalBits(), pcCU->getTotalDistortion() );
#endif
    }
    else
    {
      pcCU->setQPSubParts( pcCU->getRefQP( 0 ), 0, uiDepth ); // set QP to default QP
    }
  }
}

Void TEncCu::xCheckRDCostAMVPSkip           ( TComDataCU*& rpcBestCU, TComDataCU*& rpcTempCU )
{
  UChar uhDepth = rpcTempCU->getDepth(0);
  
  AMVPInfo cAMVPInfo0;
  cAMVPInfo0.iN = 0;
  
  AMVPInfo cAMVPInfo1;
  cAMVPInfo1.iN = 0;
  
  if (rpcTempCU->getAMVPMode(0) == AM_EXPL)
  {
    rpcTempCU->setPredModeSubParts( MODE_SKIP, 0, uhDepth );
    rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N,  0, uhDepth );
    
    if ( rpcTempCU->getSlice()->isInterP() && rpcTempCU->getSlice()->getNumRefIdx( REF_PIC_LIST_0 ) > 0 )
    {
      rpcTempCU->fillMvpCand(0, 0, REF_PIC_LIST_0, 0, &cAMVPInfo0);
    }
    else if ( rpcTempCU->getSlice()->isInterB() &&
             rpcTempCU->getSlice()->getNumRefIdx( REF_PIC_LIST_0 ) > 0 &&
             rpcTempCU->getSlice()->getNumRefIdx( REF_PIC_LIST_1 ) > 0  )
    {
      rpcTempCU->fillMvpCand(0, 0, REF_PIC_LIST_0, 0, &cAMVPInfo0);
      rpcTempCU->fillMvpCand(0, 0, REF_PIC_LIST_1, 0, &cAMVPInfo1);
    }
    else
    {
      assert( 0 );
    }
  }
  
  Int iMVP0, iMVP1;
  
  UInt uiOrgQP = rpcTempCU->getQP( 0 );

  for (iMVP0 = (cAMVPInfo0.iN > 0? 0:-1); iMVP0 < cAMVPInfo0.iN; iMVP0++)
  {
    for (iMVP1 = (cAMVPInfo1.iN > 0? 0:-1); iMVP1 < cAMVPInfo1.iN; iMVP1++)
    {
      rpcTempCU->setPredModeSubParts( MODE_SKIP, 0, uhDepth );
      rpcTempCU->setPartSizeSubParts( SIZE_2Nx2N,  0, uhDepth );

      if (rpcTempCU->getSlice()->isInterB())
        rpcTempCU->setInterDirSubParts( 3, 0, 0, uhDepth );

      rpcTempCU->setMVPIdxSubParts( iMVP0, REF_PIC_LIST_0, 0, 0, uhDepth );
      rpcTempCU->setMVPIdxSubParts( iMVP1, REF_PIC_LIST_1, 0, 0, uhDepth );

      rpcTempCU->setMVPNumSubParts( cAMVPInfo0.iN, REF_PIC_LIST_0, 0, 0, uhDepth );
      rpcTempCU->setMVPNumSubParts( cAMVPInfo1.iN, REF_PIC_LIST_1, 0, 0, uhDepth );

      xCopyAMVPInfo(&cAMVPInfo0, rpcTempCU->getCUMvField(REF_PIC_LIST_0)->getAMVPInfo());
      xCopyAMVPInfo(&cAMVPInfo1, rpcTempCU->getCUMvField(REF_PIC_LIST_1)->getAMVPInfo());
      xCheckRDCostSkip ( rpcBestCU, rpcTempCU, true );      
      rpcTempCU->initEstData( uhDepth, uiOrgQP );
    }
  }
}

Void TEncCu::xCopyAMVPInfo (AMVPInfo* pSrc, AMVPInfo* pDst)
{
  pDst->iN = pSrc->iN;
  for (Int i = 0; i < pSrc->iN; i++)
  {
    pDst->m_acMvCand[i] = pSrc->m_acMvCand[i];
  }
}
Void TEncCu::xCopyYuv2Pic(TComPic* rpcPic, UInt uiCUAddr, UInt uiAbsPartIdx, UInt uiDepth, UInt uiSrcDepth, TComDataCU* pcCU, UInt uiLPelX, UInt uiTPelY )
{
  UInt uiRPelX   = uiLPelX + (g_uiMaxCUWidth>>uiDepth)  - 1;
  UInt uiBPelY   = uiTPelY + (g_uiMaxCUHeight>>uiDepth) - 1;
#if FINE_GRANULARITY_SLICES
  TComSlice * pcSlice = pcCU->getPic()->getSlice(pcCU->getPic()->getCurrSliceIdx());
#if TILES
  Bool bSliceStart = pcSlice->getEntropySliceCurStartCUAddr() > rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx && 
    pcSlice->getEntropySliceCurStartCUAddr() < rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) );
  Bool bSliceEnd   = pcSlice->getEntropySliceCurEndCUAddr() > rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx && 
    pcSlice->getEntropySliceCurEndCUAddr() < rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) );
#else
  Bool bSliceStart = pcSlice->getEntropySliceCurStartCUAddr() > pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx&&pcSlice->getEntropySliceCurStartCUAddr()<pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) );
  Bool bSliceEnd   = pcSlice->getEntropySliceCurEndCUAddr()>pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx&&pcSlice->getEntropySliceCurEndCUAddr()<pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) );
#endif
  if(!bSliceEnd && !bSliceStart && ( uiRPelX < pcSlice->getSPS()->getWidth() ) && ( uiBPelY < pcSlice->getSPS()->getHeight() ) )
#else
  if( ( uiRPelX < pcCU->getSlice()->getSPS()->getWidth() ) && ( uiBPelY < pcCU->getSlice()->getSPS()->getHeight() ) )
#endif
  {
    UInt uiAbsPartIdxInRaster = g_auiZscanToRaster[uiAbsPartIdx];
    UInt uiSrcBlkWidth = rpcPic->getNumPartInWidth() >> (uiSrcDepth);
    UInt uiBlkWidth    = rpcPic->getNumPartInWidth() >> (uiDepth);
    UInt uiPartIdxX = ( ( uiAbsPartIdxInRaster % rpcPic->getNumPartInWidth() ) % uiSrcBlkWidth) / uiBlkWidth;
    UInt uiPartIdxY = ( ( uiAbsPartIdxInRaster / rpcPic->getNumPartInWidth() ) % uiSrcBlkWidth) / uiBlkWidth;
    UInt uiPartIdx = uiPartIdxY * ( uiSrcBlkWidth / uiBlkWidth ) + uiPartIdxX;
    m_ppcRecoYuvBest[uiSrcDepth]->copyToPicYuv( rpcPic->getPicYuvRec (), uiCUAddr, uiAbsPartIdx, uiDepth - uiSrcDepth, uiPartIdx);
  }
  else
  {
    UInt uiQNumParts = ( pcCU->getPic()->getNumPartInCU() >> (uiDepth<<1) )>>2;

    for ( UInt uiPartUnitIdx = 0; uiPartUnitIdx < 4; uiPartUnitIdx++, uiAbsPartIdx+=uiQNumParts )
    {
      UInt uiSubCULPelX   = uiLPelX + ( g_uiMaxCUWidth >>(uiDepth+1) )*( uiPartUnitIdx &  1 );
      UInt uiSubCUTPelY   = uiTPelY + ( g_uiMaxCUHeight>>(uiDepth+1) )*( uiPartUnitIdx >> 1 );

#if FINE_GRANULARITY_SLICES
#if TILES
      Bool bInSlice = rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+uiQNumParts > pcSlice->getEntropySliceCurStartCUAddr() && 
        rpcPic->getPicSym()->getInverseCUOrderMap(pcCU->getAddr())*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx < pcSlice->getEntropySliceCurEndCUAddr();
#else
      Bool bInSlice = pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx+uiQNumParts > pcSlice->getEntropySliceCurStartCUAddr()&&pcCU->getAddr()*pcCU->getPic()->getNumPartInCU()+uiAbsPartIdx<pcSlice->getEntropySliceCurEndCUAddr();
#endif
      if(bInSlice&&( uiSubCULPelX < pcSlice->getSPS()->getWidth() ) && ( uiSubCUTPelY < pcSlice->getSPS()->getHeight() ) )
#else
      if( ( uiSubCULPelX < pcCU->getSlice()->getSPS()->getWidth() ) && ( uiSubCUTPelY < pcCU->getSlice()->getSPS()->getHeight() ) )
#endif
      {
        xCopyYuv2Pic( rpcPic, uiCUAddr, uiAbsPartIdx, uiDepth+1, uiSrcDepth, pcCU, uiSubCULPelX, uiSubCUTPelY );   // Copy Yuv data to picture Yuv
      }
    }
  }
}

Void TEncCu::xCopyYuv2Tmp( UInt uiPartUnitIdx, UInt uiNextDepth )
{
  UInt uiCurrDepth = uiNextDepth - 1;
  m_ppcRecoYuvBest[uiNextDepth]->copyToPartYuv( m_ppcRecoYuvTemp[uiCurrDepth], uiPartUnitIdx );
}

#if ADAPTIVE_QP_SELECTION
/** Collect ARL statistics from one block
  */
Int TEncCu::xTuCollectARLStats(TCoeff* rpcCoeff, Int* rpcArlCoeff, Int NumCoeffInCU, Double* cSum, UInt* numSamples )
{
  for( Int n = 0; n < NumCoeffInCU; n++ )
  {
    Int u = abs( rpcCoeff[ n ] );
    Int absc = rpcArlCoeff[ n ];

    if( u != 0 )
    {
      if( u < LEVEL_RANGE )
      {
        cSum[ u ] += ( Double )absc;
        numSamples[ u ]++;
      }
      else 
      {
        cSum[ LEVEL_RANGE ] += ( Double )absc - ( Double )( u << ARL_C_PRECISION );
        numSamples[ LEVEL_RANGE ]++;
      }
    }
  }

  return 0;
}

/** Collect ARL statistics from one LCU
 * \param pcCU
 */
Void TEncCu::xLcuCollectARLStats(TComDataCU* rpcCU )
{
  Double cSum[ LEVEL_RANGE + 1 ];     //: the sum of DCT coefficients corresponding to datatype and quantization output
  UInt numSamples[ LEVEL_RANGE + 1 ]; //: the number of coefficients corresponding to datatype and quantization output

  TCoeff* pCoeffY = rpcCU->getCoeffY();
  Int* pArlCoeffY = rpcCU->getArlCoeffY();

  UInt uiMinCUWidth = g_uiMaxCUWidth >> g_uiMaxCUDepth;
  UInt uiMinNumCoeffInCU = 1 << uiMinCUWidth;

  memset( cSum, 0, sizeof( Double )*(LEVEL_RANGE+1) );
  memset( numSamples, 0, sizeof( UInt )*(LEVEL_RANGE+1) );

  // Collect stats to cSum[][] and numSamples[][]
  for(Int i = 0; i < rpcCU->getTotalNumPart(); i ++ )
  {
    UInt uiTrIdx = rpcCU->getTransformIdx(i);

    if(rpcCU->getPredictionMode(i) == MODE_INTER)
    if( rpcCU->getCbf( i, TEXT_LUMA, uiTrIdx ) )
    {
      xTuCollectARLStats(pCoeffY, pArlCoeffY, uiMinNumCoeffInCU, cSum, numSamples);
    }//Note that only InterY is processed. QP rounding is based on InterY data only.
   
    pCoeffY  += uiMinNumCoeffInCU;
    pArlCoeffY  += uiMinNumCoeffInCU;
  }

  for(Int u=1; u<LEVEL_RANGE;u++)
  {
    m_pcTrQuant->getSliceSumC()[u] += cSum[ u ] ;
    m_pcTrQuant->getSliceNSamples()[u] += numSamples[ u ] ;
  }
  m_pcTrQuant->getSliceSumC()[LEVEL_RANGE] += cSum[ LEVEL_RANGE ] ;
  m_pcTrQuant->getSliceNSamples()[LEVEL_RANGE] += numSamples[ LEVEL_RANGE ] ;
}
#endif
//! \}
