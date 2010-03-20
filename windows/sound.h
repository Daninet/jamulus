/******************************************************************************\
 * Copyright (c) 2004-2010
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined ( _SOUNDIN_H__9518A621_7F78_11D3_8C0D_EEBF182CF549__INCLUDED_ )
#define _SOUNDIN_H__9518A621_7F78_11D3_8C0D_EEBF182CF549__INCLUDED_

#include <windows.h>
#include <qmutex.h>
#include <qmessagebox.h>
#include "../src/util.h"
#include "../src/global.h"
#include "../src/soundbase.h"

// copy the ASIO SDK in the llcon/windows directory: "llcon/windows/ASIOSDK2" to
// get it work
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"


/* Definitions ****************************************************************/
// stereo for input and output
#define NUM_IN_OUT_CHANNELS         2

// define the maximum number of audio channel for input/output we can store
// channel infos for (and therefore this is the maximum number of entries in
// the channel selection combo box regardless of the actual available number
// of channels by the audio device)
#define MAX_NUM_IN_OUT_CHANNELS     32


/* Classes ********************************************************************/
class CSound : public CSoundBase
{
public:
    CSound ( void (*fpNewCallback) ( CVector<int16_t>& psData, void* arg ), void* arg );
    virtual ~CSound();

    virtual int  Init ( const int iNewPrefMonoBufferSize );
    virtual void Start();
    virtual void Stop();

    virtual void OpenDriverSetup() { ASIOControlPanel(); }

    // device selection
    int          GetNumDev() { return lNumDevs; }
    QString      GetDeviceName ( const int iDiD ) { return cDriverNames[iDiD]; }
    QString      SetDev ( const int iNewDev );
    int          GetDev() { return lCurDev; }

    // channel selection
    int          GetNumInputChannels() { return static_cast<int> ( lNumInChan ); }
    QString      GetInputChannelName ( const int iDiD ) { return channelInfosInput[iDiD].name; }
    void         SetLeftInputChannel  ( const int iNewChan );
    void         SetRightInputChannel ( const int iNewChan );
    int          GetLeftInputChannel()  { return vSelectedInputChannels[0]; }
    int          GetRightInputChannel() { return vSelectedInputChannels[1]; }

    int          GetNumOutputChannels() { return static_cast<int> ( lNumOutChan ); }
    QString      GetOutputChannelName ( const int iDiD ) { return channelInfosOutput[iDiD].name; }
    void         SetLeftOutputChannel  ( const int iNewChan );
    void         SetRightOutputChannel ( const int iNewChan );
    int          GetLeftOutputChannel()  { return vSelectedOutputChannels[0]; }
    int          GetRightOutputChannel() { return vSelectedOutputChannels[1]; }

protected:
    QVector<QString> LoadAndInitializeFirstValidDriver();
    QString          LoadAndInitializeDriver ( int iIdx );
    int              GetActualBufferSize ( const int iDesiredBufferSizeMono );
    QString          CheckDeviceCapabilities();
    bool             CheckSampleTypeSupported ( const ASIOSampleType SamType );
    void             ResetChannelMapping();

    int              iASIOBufferSizeMono;
    int              iASIOBufferSizeStereo;

    long             lNumInChan;
    long             lNumOutChan;
    CVector<int>     vSelectedInputChannels;
    CVector<int>     vSelectedOutputChannels;

    CVector<int16_t> vecsTmpAudioSndCrdStereo;

    QMutex           ASIOMutex;

    // utility functions
    static int16_t   Flip16Bits ( const int16_t iIn );
    static int32_t   Flip32Bits ( const int32_t iIn );
    static int64_t   Flip64Bits ( const int64_t iIn );

    // audio hardware buffer info
    struct sHWBufferInfo
    {
        long lMinSize;
        long lMaxSize;
        long lPreferredSize;
        long lGranularity;
    } HWBufferInfo;

    // ASIO stuff
    ASIODriverInfo   driverInfo;
    ASIOBufferInfo   bufferInfos[2 * NUM_IN_OUT_CHANNELS]; // for input and output buffers -> "2 *"
    ASIOChannelInfo  channelInfosInput[MAX_NUM_IN_OUT_CHANNELS];
    ASIOChannelInfo  channelInfosOutput[MAX_NUM_IN_OUT_CHANNELS];
    bool             bASIOPostOutput;
    ASIOCallbacks    asioCallbacks;

    // callbacks
    static void      bufferSwitch ( long index, ASIOBool processNow );
    static ASIOTime* bufferSwitchTimeInfo ( ASIOTime* timeInfo, long index, ASIOBool processNow );
    static void      sampleRateChanged ( ASIOSampleRate sRate ) {}
    static long      asioMessages ( long selector, long value, void* message, double* opt );

    long             lNumDevs;
    long             lCurDev;
    char*            cDriverNames[MAX_NUMBER_SOUND_CARDS];
};

#endif // !defined ( _SOUNDIN_H__9518A621_7F78_11D3_8C0D_EEBF182CF549__INCLUDED_ )
