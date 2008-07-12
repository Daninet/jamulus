/******************************************************************************\
 * Copyright (c) 2004-2008
 *
 * Author(s):
 *  Volker Fischer
 *
 * Description:
 * Sound card interface for Windows operating systems
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

#include "Sound.h"


/* Implementation *************************************************************/
#ifdef USE_ASIO_SND_INTERFACE
#include <qmutex.h>

// external references
extern AsioDrivers* asioDrivers;
bool   loadAsioDriver ( char *name );

// mutex
QMutex ASIOMutex;

// TODO the following variables should be in the class definition but we cannot
// do it here since we have static callback functions which cannot access the
// class members :-(((

// ASIO stuff
ASIODriverInfo   driverInfo;
ASIOBufferInfo   bufferInfos[2 * NUM_IN_OUT_CHANNELS]; // for input and output buffers -> "2 *"
ASIOChannelInfo  channelInfos[2 * NUM_IN_OUT_CHANNELS];
bool             bASIOPostOutput;
ASIOCallbacks    asioCallbacks;
int              iBufferSizeMono;
int              iBufferSizeStereo;

// event
HANDLE           m_ASIOEvent;

// wave in
int              iInCurBlockToWrite;
short*           psSoundcardBuffer[MAX_SND_BUF_IN];

// wave out
int              iOutCurBlockToWrite;
short*           psPlaybackBuffer[MAX_SND_BUF_OUT];

int              iCurNumSndBufIn;
int              iCurNumSndBufOut;

// we must implement these functions here to get access to global variables
int CSound::GetOutNumBuf() { return iCurNumSndBufOut; }
int CSound::GetInNumBuf() { return iCurNumSndBufIn; }


/******************************************************************************\
* Wave in                                                                      *
\******************************************************************************/
bool CSound::Read ( CVector<short>& psData )
{
    int  i, j;
    bool bError = false;

    // check if device must be opened or reinitialized
    if ( bChangParamIn )
    {
        // Reinit sound interface (init recording requires stereo buffer size)
        InitRecordingAndPlayback ( iBufferSizeStereo );

        // Reset flag
        bChangParamIn = false;
    }

    // wait until data is available
    if ( iInCurBlockToWrite == 0 )
    {
        if ( bBlockingRec )
		{
            WaitForSingleObject ( m_ASIOEvent, INFINITE );
		}
        else
		{
            return false;
		}
    }

    // If the number of done buffers equals the total number of buffers, it is
    // very likely that a buffer got lost -> set error flag
    bError = ( iInCurBlockToWrite == iCurNumSndBufIn );

    ASIOMutex.lock(); // get mutex lock
    {
        // copy data from sound card in output buffer
        for ( i = 0; i < iBufferSizeStereo; i++ )
	    {
            psData[i] = psSoundcardBuffer[0][i];
	    }

        // move all other data in buffer
        for ( j = 0; j < iInCurBlockToWrite - 1; j++ )
        {
            for ( i = 0; i < iBufferSizeStereo; i++ )
	        {
                psSoundcardBuffer[j][i] = psSoundcardBuffer[j + 1][i];
	        }
        }

        // adjust "current block to write" pointer
        iInCurBlockToWrite--;
    }
    ASIOMutex.unlock();

    // in case more than one buffer was ready, reset event
    ResetEvent ( m_ASIOEvent );

    return bError;
}

void CSound::SetInNumBuf ( int iNewNum )
{
    // check new parameter
    if ( ( iNewNum >= MAX_SND_BUF_IN ) || ( iNewNum < 1 ) )
	{
        iNewNum = NUM_SOUND_BUFFERS_IN;
	}

    // change only if parameter is different
    if ( iNewNum != iCurNumSndBufIn )
    {
        iCurNumSndBufIn = iNewNum;
        bChangParamIn   = true;
    }
}


/******************************************************************************\
* Wave out                                                                     *
\******************************************************************************/
bool CSound::Write ( CVector<short>& psData )
{
    // init return state
    bool bError = false;

    // check if device must be opened or reinitialized
    if ( bChangParamOut )
    {
        // reinit sound interface (init recording requires stereo buffer size)
        InitRecordingAndPlayback ( iBufferSizeStereo );

        // reset flag
        bChangParamOut = false;
    }

    ASIOMutex.lock(); // get mutex lock
    {
        // first check if buffer is available
        if ( iOutCurBlockToWrite < iCurNumSndBufOut )
        {
            // copy stereo data from input in soundcard buffer
            for ( int i = 0; i < iBufferSizeStereo; i++ )
            {
                psPlaybackBuffer[iOutCurBlockToWrite][i] = psData[i];
            }

            iOutCurBlockToWrite++;
        }
        else
        {
            // buffer overrun, return error
            bError = true;
        }
    }
    ASIOMutex.unlock();

    return bError;
}

void CSound::SetOutNumBuf ( int iNewNum )
{
    // check new parameter
    if ( ( iNewNum >= MAX_SND_BUF_OUT ) || ( iNewNum < 1 ) )
	{
        iNewNum = NUM_SOUND_BUFFERS_OUT;
	}

    // change only if parameter is different
    if ( iNewNum != iCurNumSndBufOut )
    {
        iCurNumSndBufOut = iNewNum;
        bChangParamOut   = true;
    }
}


/******************************************************************************\
* Common                                                                       *
\******************************************************************************/
void CSound::InitRecordingAndPlayback ( int iNewBufferSize )
{
    int i, j;

    // first, stop audio and dispose ASIO buffers
    ASIOStop();
    ASIODisposeBuffers();

    ASIOMutex.lock(); // get mutex lock
    {
        // we need mono buffer size but get stereo buffer size
        const int iNewBufferSizeMono = iNewBufferSize / 2;

        // calculate "nearest" buffer size and set internal parameter accordingly
        // first check minimum and maximum values
        if ( iNewBufferSizeMono < HWBufferInfo.lMinSize )
        {
            iBufferSizeMono = HWBufferInfo.lMinSize;
        }
        else
        {
            if ( iNewBufferSizeMono > HWBufferInfo.lMaxSize )
            {
                iBufferSizeMono = HWBufferInfo.lMaxSize;
            }
            else
            {
                // initialization
                int  iTrialBufSize     = HWBufferInfo.lMinSize;
                int  iLastTrialBufSize = HWBufferInfo.lMinSize;
                bool bSizeFound        = false;

                // test loop
                while ( ( iTrialBufSize <= HWBufferInfo.lMaxSize ) && ( !bSizeFound ) )
                {
                    if ( iTrialBufSize > iNewBufferSizeMono )
                    {
                        // test which buffer size fits better: the old one or the
                        // current one
                        if ( ( iTrialBufSize - iNewBufferSizeMono ) < ( iNewBufferSizeMono - iLastTrialBufSize ) )
                        {
                            iBufferSizeMono = iTrialBufSize;
                        }
                        else
                        {
                            iBufferSizeMono = iLastTrialBufSize;
                        }

                        // exit while loop
                        bSizeFound = true;
                    }

                    // store old trial buffer size
                    iLastTrialBufSize = iTrialBufSize;

                    // increment trial buffer size (check for special case first)
                    if ( HWBufferInfo.lGranularity == -1 )
                    {
                        // special case: buffer sizes are a power of 2
                        iTrialBufSize *= 2;
                    }
                    else
                    {
                        iTrialBufSize += HWBufferInfo.lGranularity;
                    }
                }
            }
        }

        // calculate stereo buffer size
        iBufferSizeStereo = 2 * iBufferSizeMono;

 // TEST test if requested buffer size is supported by the audio hardware, if not, fire error
if ( iNewBufferSize != iBufferSizeStereo )
{
    throw CGenErr ( "Required sound card buffer size not allowed by the audio hardware." );
}


	    // create and activate ASIO buffers (buffer size in samples)
	    ASIOCreateBuffers ( bufferInfos, 2 * NUM_IN_OUT_CHANNELS,
		    iBufferSizeMono, &asioCallbacks );

	    // now set all the buffer details
	    for ( i = 0; i < 2 * NUM_IN_OUT_CHANNELS; i++ )
	    {
		    channelInfos[i].channel = NUM_IN_OUT_CHANNELS;
		    channelInfos[i].isInput = bufferInfos[i].isInput;
		    ASIOGetChannelInfo ( &channelInfos[i] );

            // only 16 bit is supported
            if ( channelInfos[i].type != ASIOSTInt16LSB )
            {
                throw CGenErr ( "Required audio sample format not available (16 bit LSB)." );
            }
	    }


        // our buffer management -----------------------------------------------
        // initialize write block pointer in
        iInCurBlockToWrite = 0;

        // create memory for sound card buffer
        for ( i = 0; i < iCurNumSndBufIn; i++ )
        {
            if ( psSoundcardBuffer[i] != NULL )
		    {
                delete[] psSoundcardBuffer[i];
		    }

            psSoundcardBuffer[i] = new short[iBufferSizeStereo];
        }

        // initialize write block pointer out
        iOutCurBlockToWrite = 0;

        // create memory for playback buffer
        for ( j = 0; j < iCurNumSndBufOut; j++ )
        {
            if ( psPlaybackBuffer[j] != NULL )
		    {
                delete[] psPlaybackBuffer[j];
		    }

            psPlaybackBuffer[j] = new short[iBufferSizeStereo];

            // clear new buffer
            for ( i = 0; i < iBufferSizeStereo; i++ )
		    {
                psPlaybackBuffer[j][i] = 0;
		    }
        }

        // reset event
        ResetEvent ( m_ASIOEvent );
    }
    ASIOMutex.unlock();

    // initialization is done, (re)start audio
    ASIOStart();
}

void CSound::Close()
{
    // set event to ensure that thread leaves the waiting function
    if ( m_ASIOEvent != NULL )
	{
        SetEvent ( m_ASIOEvent );
	}

    // wait for the thread to terminate
    Sleep ( 500 );

    // stop audio and dispose ASIO buffers
    ASIOStop();
    ASIODisposeBuffers();

    // set flag to open devices the next time it is initialized
    bChangParamIn  = true;
    bChangParamOut = true;
}

CSound::CSound()
{
    int i;

    // init number of sound buffers
    iCurNumSndBufIn  = NUM_SOUND_BUFFERS_IN;
    iCurNumSndBufOut = NUM_SOUND_BUFFERS_OUT;

    // should be initialized because an error can occur during init
    m_ASIOEvent = NULL;

    // get available ASIO driver names in system
    char* cDriverNames[MAX_NUMBER_SOUND_CARDS];
	for ( i = 0; i < MAX_NUMBER_SOUND_CARDS; i++ )
    {
        cDriverNames[i] = new char[32];
    }

    loadAsioDriver ( "dummy" ); // to initialize external object
    const long lNumDetDriv = asioDrivers->getDriverNames ( cDriverNames, MAX_NUMBER_SOUND_CARDS );


	// load and initialize first valid ASIO driver
    bool bValidDriverDetected = false;
    int  iCurDriverIdx = 0;

    while ( !bValidDriverDetected && iCurDriverIdx < lNumDetDriv )
    {
	    if ( loadAsioDriver ( cDriverNames[iCurDriverIdx] ) )
        {
		    if ( ASIOInit ( &driverInfo ) == ASE_OK )
		    {
                bValidDriverDetected = true;
            }
            else
            {
                // driver could not be loaded, free memory
                asioDrivers->removeCurrentDriver();
            }
        }

        // try next driver
        iCurDriverIdx++;
    }

    // in case we do not have a driver available, throw error
    if ( !bValidDriverDetected )
    {
        throw CGenErr ( "No suitable ASIO audio device found." );
    }


// TEST we only use one driver for a first try
iNumDevs = 1;
pstrDevices[0] = driverInfo.name;



	// check the number of available channels
	long lNumInChan;
	long lNumOutChan;
	ASIOGetChannels ( &lNumInChan, &lNumOutChan );
    if ( ( lNumInChan < NUM_IN_OUT_CHANNELS ) || ( lNumOutChan < NUM_IN_OUT_CHANNELS ) )
    {
        throw CGenErr ( "The audio device does not support required number of channels." );
    }

    // query the usable buffer sizes
	ASIOGetBufferSize ( &HWBufferInfo.lMinSize,
                        &HWBufferInfo.lMaxSize,
                        &HWBufferInfo.lPreferredSize,
                        &HWBufferInfo.lGranularity );

	// set the sample rate and check if sample rate is supported
	ASIOSetSampleRate ( SND_CRD_SAMPLE_RATE );

    ASIOSampleRate sampleRate;
    ASIOGetSampleRate ( &sampleRate );
    if ( sampleRate != SND_CRD_SAMPLE_RATE )
    {
        throw CGenErr ( "The audio device does not support required sample rate." );
    }

	// check wether the driver requires the ASIOOutputReady() optimization
	// (can be used by the driver to reduce output latency by one block)
	bASIOPostOutput = ( ASIOOutputReady() == ASE_OK );

	// set up the asioCallback structure and create the ASIO data buffer
	asioCallbacks.bufferSwitch         = &bufferSwitch;
	asioCallbacks.sampleRateDidChange  = &sampleRateChanged;
	asioCallbacks.asioMessage          = &asioMessages;
	asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;

    // prepare input channels
    for ( i = 0; i < NUM_IN_OUT_CHANNELS; i++ )
	{
		bufferInfos[i].isInput    = ASIOTrue;
		bufferInfos[i].channelNum = i;
		bufferInfos[i].buffers[0] = 0;
        bufferInfos[i].buffers[1] = 0;
	}

    // prepare output channels
    for ( i = 0; i < NUM_IN_OUT_CHANNELS; i++ )
	{
		bufferInfos[NUM_IN_OUT_CHANNELS + i].isInput    = ASIOFalse;
		bufferInfos[NUM_IN_OUT_CHANNELS + i].channelNum = i;
		bufferInfos[NUM_IN_OUT_CHANNELS + i].buffers[0] = 0;
        bufferInfos[NUM_IN_OUT_CHANNELS + i].buffers[1] = 0;
	}

    // init buffer pointer to zero
    for ( i = 0; i < MAX_SND_BUF_IN; i++ )
    {
        psSoundcardBuffer[i] = NULL;
    }

    for ( i = 0; i < MAX_SND_BUF_OUT; i++ )
    {
        psPlaybackBuffer[i] = NULL;
    }

    // we use an event controlled structure
    // create event
    m_ASIOEvent = CreateEvent ( NULL, FALSE, FALSE, NULL );

    // init flag to open devices
    bChangParamIn  = false;
    bChangParamOut = false;
}

CSound::~CSound()
{
    int i;

    // cleanup ASIO stuff
    ASIOStop();
    ASIODisposeBuffers();
    ASIOExit();
    asioDrivers->removeCurrentDriver();

    // delete allocated memory
    for ( i = 0; i < iCurNumSndBufIn; i++ )
    {
        if ( psSoundcardBuffer[i] != NULL )
		{
            delete[] psSoundcardBuffer[i];
		}
    }

    for ( i = 0; i < iCurNumSndBufOut; i++ )
    {
        if ( psPlaybackBuffer[i] != NULL )
		{
            delete[] psPlaybackBuffer[i];
		}
    }

    // close the handle for the event
    if ( m_ASIOEvent != NULL )
	{
        CloseHandle ( m_ASIOEvent );
	}
}

// ASIO callbacks -------------------------------------------------------------
ASIOTime* CSound::bufferSwitchTimeInfo ( ASIOTime *timeInfo, long index, ASIOBool processNow )
{
	bufferSwitch ( index, processNow );
	return 0L;
}

void CSound::bufferSwitch ( long index, ASIOBool processNow )
{
    int iCurSample;

    ASIOMutex.lock(); // get mutex lock
    {
	    // perform the processing for input and output
	    for ( int i = 0; i < 2 * NUM_IN_OUT_CHANNELS; i++ )
	    {
            if ( bufferInfos[i].isInput == false )
		    {
                // PLAYBACK ----------------------------------------------------
                if ( iOutCurBlockToWrite > 0 )
                {
                    // copy data from sound card in output buffer
                    for ( iCurSample = 0; iCurSample < iBufferSizeMono; iCurSample++ )
	                {
                        // copy interleaved stereo data in mono sound card buffer
                        ((short*) bufferInfos[i].buffers[index])[iCurSample] =
                            psSoundcardBuffer[0][2 * iCurSample + bufferInfos[i].channelNum];
	                }
                }
		    }
            else
            {
                // CAPTURE -----------------------------------------------------
                // first check if buffer is available
                if ( iInCurBlockToWrite < iCurNumSndBufIn )
                {
                    // copy new captured block in thread transfer buffer
                    for ( iCurSample = 0; iCurSample < iBufferSizeMono; iCurSample++ )
                    {
                        // copy mono data interleaved in stereo buffer
                        psSoundcardBuffer[iInCurBlockToWrite][2 * iCurSample + bufferInfos[i].channelNum] =
                            ((short*) bufferInfos[i].buffers[index])[iCurSample];
                    }
                }
            }
	    }


        // Manage thread interface buffers for input and output ----------------
        // capture
        if ( iInCurBlockToWrite < iCurNumSndBufIn )
        {
            iInCurBlockToWrite++;
        }
        else
        {
            // TODO: buffer overrun, inform user somehow...?
        }

        // playback
        if ( iOutCurBlockToWrite > 0 )
        {
            // move all other data in playback buffer
            for ( int j = 0; j < iOutCurBlockToWrite - 1; j++ )
            {
                for ( iCurSample = 0; iCurSample < iBufferSizeStereo; iCurSample++ )
                {
                    psPlaybackBuffer[j][iCurSample] =
                        psPlaybackBuffer[j + 1][iCurSample];
                }
            }

            // adjust "current block to write" pointer
            iOutCurBlockToWrite--;
        }
        else
        {
            // TODO: buffer underrun, inform user somehow...?
        }
    }
    ASIOMutex.unlock();

    // finally if the driver supports the ASIOOutputReady() optimization,
    // do it here, all data are in place
    if ( bASIOPostOutput )
    {
	    ASIOOutputReady();
    }

    // set event
    SetEvent ( m_ASIOEvent );
}

long CSound::asioMessages ( long selector, long value, void* message, double* opt )
{
	long ret = 0;
	switch(selector)
	{
		case kAsioEngineVersion:
			// return the supported ASIO version of the host application
			ret = 2L;
			break;
	}
	return ret;
}

#else // USE_ASIO_SND_INTERFACE

/******************************************************************************\
* Wave in                                                                      *
\******************************************************************************/
bool CSound::Read ( CVector<short>& psData )
{
    int  i;
    bool bError;

    // check if device must be opened or reinitialized
    if ( bChangParamIn )
    {
        OpenInDevice();

        // Reinit sound interface
        InitRecording ( iBufferSizeIn, bBlockingRec );

        // Reset flag
        bChangParamIn = false;
    }

    // wait until data is available
    if ( ! ( m_WaveInHeader[iWhichBufferIn].dwFlags & WHDR_DONE ) )
    {
        if ( bBlockingRec )
		{
            WaitForSingleObject ( m_WaveInEvent, INFINITE );
		}
        else
		{
            return false;
		}
    }

    // check if buffers got lost
    int iNumInBufDone = 0;
    for ( i = 0; i < iCurNumSndBufIn; i++ )
    {
        if ( m_WaveInHeader[i].dwFlags & WHDR_DONE )
		{
            iNumInBufDone++;
		}
    }

    /* If the number of done buffers equals the total number of buffers, it is
       very likely that a buffer got lost -> set error flag */
    if ( iNumInBufDone == iCurNumSndBufIn )
	{
        bError = true;
	}
    else
	{
        bError = false;
	}

    // copy data from sound card in output buffer
    for ( i = 0; i < iBufferSizeIn; i++ )
	{
        psData[i] = psSoundcardBuffer[iWhichBufferIn][i];
	}

    // add the buffer so that it can be filled with new samples
    AddInBuffer();

    // in case more than one buffer was ready, reset event
    ResetEvent ( m_WaveInEvent );

    return bError;
}

void CSound::AddInBuffer()
{
    // unprepare old wave-header
    waveInUnprepareHeader (
        m_WaveIn, &m_WaveInHeader[iWhichBufferIn], sizeof ( WAVEHDR ) );

    // prepare buffers for sending to sound interface
    PrepareInBuffer ( iWhichBufferIn );

    // send buffer to driver for filling with new data
    waveInAddBuffer ( m_WaveIn, &m_WaveInHeader[iWhichBufferIn], sizeof ( WAVEHDR ) );

    // toggle buffers
    iWhichBufferIn++;
    if ( iWhichBufferIn == iCurNumSndBufIn )
	{
        iWhichBufferIn = 0;
	}
}

void CSound::PrepareInBuffer ( int iBufNum )
{
    // set struct entries
    m_WaveInHeader[iBufNum].lpData         = (LPSTR) &psSoundcardBuffer[iBufNum][0];
    m_WaveInHeader[iBufNum].dwBufferLength = iBufferSizeIn * BYTES_PER_SAMPLE;
    m_WaveInHeader[iBufNum].dwFlags        = 0;

    // prepare wave-header
    waveInPrepareHeader ( m_WaveIn, &m_WaveInHeader[iBufNum], sizeof ( WAVEHDR ) );
}

void CSound::InitRecording ( int iNewBufferSize, bool bNewBlocking )
{
    // check if device must be opened or reinitialized
    if ( bChangParamIn )
    {
        OpenInDevice();

        // reset flag
        bChangParamIn = false;
    }

    // set internal parameter
    iBufferSizeIn = iNewBufferSize;
    bBlockingRec  = bNewBlocking;

    // reset interface so that all buffers are returned from the interface
    waveInReset ( m_WaveIn );
    waveInStop ( m_WaveIn );
    
    /* reset current buffer ID (it is important to do this BEFORE calling
       "AddInBuffer()" */
    iWhichBufferIn = 0;

    // create memory for sound card buffer
    for ( int i = 0; i < iCurNumSndBufIn; i++ )
    {
        /* Unprepare old wave-header in case that we "re-initialized" this
           module. Calling "waveInUnprepareHeader()" with an unprepared
           buffer (when the module is initialized for the first time) has
           simply no effect */
        waveInUnprepareHeader ( m_WaveIn, &m_WaveInHeader[i], sizeof ( WAVEHDR ) );

        if ( psSoundcardBuffer[i] != NULL )
		{
            delete[] psSoundcardBuffer[i];
		}

        psSoundcardBuffer[i] = new short[iBufferSizeIn];


        // Send all buffers to driver for filling the queue --------------------
        // prepare buffers before sending them to the sound interface
        PrepareInBuffer ( i );

        AddInBuffer();
    }

    // notify that sound capturing can start now
    waveInStart ( m_WaveIn );

    /* This reset event is very important for initialization, otherwise we will
       get errors! */
    ResetEvent ( m_WaveInEvent );
}

void CSound::OpenInDevice()
{
    // open wave-input and set call-back mechanism to event handle
    if ( m_WaveIn != NULL )
    {
        waveInReset ( m_WaveIn );
        waveInClose ( m_WaveIn );
    }

    MMRESULT result = waveInOpen ( &m_WaveIn, iCurInDev, &sWaveFormatEx,
        (DWORD) m_WaveInEvent, NULL, CALLBACK_EVENT );

    if ( result != MMSYSERR_NOERROR )
    {
        throw CGenErr ( "Sound Interface Start, waveInOpen() failed. This error "
            "usually occurs if another application blocks the sound in." );
    }
}

void CSound::SetInDev ( int iNewDev )
{
    // set device to wave mapper if iNewDev is invalid
    if ( ( iNewDev >= iNumDevs ) || ( iNewDev < 0 ) )
	{
        iNewDev = WAVE_MAPPER;
	}

    // change only in case new device id is not already active
    if ( iNewDev != iCurInDev )
    {
        iCurInDev     = iNewDev;
        bChangParamIn = true;
    }
}

void CSound::SetInNumBuf ( int iNewNum )
{
    // check new parameter
    if ( ( iNewNum >= MAX_SND_BUF_IN ) || ( iNewNum < 1 ) )
	{
        iNewNum = NUM_SOUND_BUFFERS_IN;
	}

    // change only if parameter is different
    if ( iNewNum != iCurNumSndBufIn )
    {
        iCurNumSndBufIn = iNewNum;
        bChangParamIn   = true;
    }
}


/******************************************************************************\
* Wave out                                                                     *
\******************************************************************************/
bool CSound::Write ( CVector<short>& psData )
{
    int     i, j;
    int     iCntPrepBuf;
    int     iIndexDoneBuf;
    bool    bError;

    // check if device must be opened or reinitialized
    if ( bChangParamOut )
    {
        OpenOutDevice();

        // reinit sound interface
        InitPlayback ( iBufferSizeOut, bBlockingPlay );

        // reset flag
        bChangParamOut = false;
    }

    // get number of "done"-buffers and position of one of them
    GetDoneBuffer ( iCntPrepBuf, iIndexDoneBuf );

    // now check special cases (Buffer is full or empty)
    if ( iCntPrepBuf == 0 )
    {
        if ( bBlockingPlay )
        {
            /* Blocking wave out routine. Always
               ensure that the buffer is completely filled to avoid buffer
               underruns */
            while ( iCntPrepBuf == 0 )
            {
                WaitForSingleObject ( m_WaveOutEvent, INFINITE );

                GetDoneBuffer ( iCntPrepBuf, iIndexDoneBuf );
            }
        }
        else
        {
            // All buffers are filled, dump new block --------------------------
// It would be better to kill half of the buffer blocks to set the start
// back to the middle: TODO
            return true; // an error occurred
        }
    }
    else
	{
		if ( iCntPrepBuf == iCurNumSndBufOut )
		{
			/* -----------------------------------------------------------------
			   Buffer is empty -> send as many cleared blocks to the sound-
			   interface until half of the buffer size is reached */
			// send half of the buffer size blocks to the sound-interface
			for ( j = 0; j < iCurNumSndBufOut / 2; j++ )
			{
				// first, clear these buffers
				for ( i = 0; i < iBufferSizeOut; i++ )
				{
					psPlaybackBuffer[j][i] = 0;
				}

				// then send them to the interface
				AddOutBuffer ( j );
			}

			// set index for done buffer
			iIndexDoneBuf = iCurNumSndBufOut / 2;

			bError = true;
		}
		else
		{
			bError = false;
		}
	}

    // copy stereo data from input in soundcard buffer
    for ( i = 0; i < iBufferSizeOut; i++ )
	{
        psPlaybackBuffer[iIndexDoneBuf][i] = psData[i];
	}

    // now, send the current block
    AddOutBuffer ( iIndexDoneBuf );

    return bError;
}

void CSound::GetDoneBuffer ( int& iCntPrepBuf, int& iIndexDoneBuf )
{
    // get number of "done"-buffers and position of one of them
    iCntPrepBuf = 0;
    for ( int i = 0; i < iCurNumSndBufOut; i++ )
    {
        if ( m_WaveOutHeader[i].dwFlags & WHDR_DONE )
        {
            iCntPrepBuf++;
            iIndexDoneBuf = i;
        }
    }
}

void CSound::AddOutBuffer ( int iBufNum )
{
    // unprepare old wave-header
    waveOutUnprepareHeader (
        m_WaveOut, &m_WaveOutHeader[iBufNum], sizeof ( WAVEHDR ) );

    // prepare buffers for sending to sound interface
    PrepareOutBuffer ( iBufNum );

    // send buffer to driver for filling with new data
    waveOutWrite ( m_WaveOut, &m_WaveOutHeader[iBufNum], sizeof ( WAVEHDR ) );
}

void CSound::PrepareOutBuffer ( int iBufNum )
{
    // set Header data
    m_WaveOutHeader[iBufNum].lpData         = (LPSTR) &psPlaybackBuffer[iBufNum][0];
    m_WaveOutHeader[iBufNum].dwBufferLength = iBufferSizeOut * BYTES_PER_SAMPLE;
    m_WaveOutHeader[iBufNum].dwFlags        = 0;

    // prepare wave-header
    waveOutPrepareHeader ( m_WaveOut, &m_WaveOutHeader[iBufNum], sizeof ( WAVEHDR ) );
}

void CSound::InitPlayback ( int iNewBufferSize, bool bNewBlocking )
{
    int i, j;

    // check if device must be opened or reinitialized
    if ( bChangParamOut )
    {
        OpenOutDevice();

        // reset flag
        bChangParamOut = false;
    }

    // set internal parameters
    iBufferSizeOut = iNewBufferSize;
    bBlockingPlay  = bNewBlocking;

    // reset interface
    waveOutReset ( m_WaveOut );

    for ( j = 0; j < iCurNumSndBufOut; j++ )
    {
        /* Unprepare old wave-header (in case header was not prepared before,
           simply nothing happens with this function call */
        waveOutUnprepareHeader ( m_WaveOut, &m_WaveOutHeader[j], sizeof ( WAVEHDR ) );

        // create memory for playback buffer
        if ( psPlaybackBuffer[j] != NULL )
		{
            delete[] psPlaybackBuffer[j];
		}

        psPlaybackBuffer[j] = new short[iBufferSizeOut];

        // clear new buffer
        for ( i = 0; i < iBufferSizeOut; i++ )
		{
            psPlaybackBuffer[j][i] = 0;
		}

        // prepare buffer for sending to the sound interface
        PrepareOutBuffer ( j );

        // initially, send all buffers to the interface
        AddOutBuffer ( j );
    }
}

void CSound::OpenOutDevice()
{
    if ( m_WaveOut != NULL )
    {
        waveOutReset ( m_WaveOut );
        waveOutClose ( m_WaveOut );
    }

    MMRESULT result = waveOutOpen ( &m_WaveOut, iCurOutDev, &sWaveFormatEx,
        (DWORD) m_WaveOutEvent, NULL, CALLBACK_EVENT );

    if ( result != MMSYSERR_NOERROR )
	{
        throw CGenErr ( "Sound Interface Start, waveOutOpen() failed." );
	}
}

void CSound::SetOutDev ( int iNewDev )
{
    // set device to wave mapper if iNewDev is invalid
    if ( ( iNewDev >= iNumDevs ) || ( iNewDev < 0 ) )
	{
        iNewDev = WAVE_MAPPER;
	}

    // change only in case new device id is not already active
    if ( iNewDev != iCurOutDev )
    {
        iCurOutDev     = iNewDev;
        bChangParamOut = true;
    }
}

void CSound::SetOutNumBuf ( int iNewNum )
{
    // check new parameter
    if ( ( iNewNum >= MAX_SND_BUF_OUT ) || ( iNewNum < 1 ) )
	{
        iNewNum = NUM_SOUND_BUFFERS_OUT;
	}

    // change only if parameter is different
    if ( iNewNum != iCurNumSndBufOut )
    {
        iCurNumSndBufOut = iNewNum;
        bChangParamOut   = true;
    }
}


/******************************************************************************\
* Common                                                                       *
\******************************************************************************/
void CSound::Close()
{
    int      i;
    MMRESULT result;

    // reset audio driver
    if ( m_WaveOut != NULL )
    {
        result = waveOutReset ( m_WaveOut );

        if ( result != MMSYSERR_NOERROR )
		{
            throw CGenErr ( "Sound Interface, waveOutReset() failed." );
		}
    }

    if ( m_WaveIn != NULL )
    {
        result = waveInReset ( m_WaveIn );

        if ( result != MMSYSERR_NOERROR )
		{
            throw CGenErr ( "Sound Interface, waveInReset() failed." );
		}
    }

    // set event to ensure that thread leaves the waiting function
    if ( m_WaveInEvent != NULL )
	{
        SetEvent(m_WaveInEvent);
	}

    // wait for the thread to terminate
    Sleep ( 500 );

    // unprepare wave-headers
    if ( m_WaveIn != NULL )
    {
        for ( i = 0; i < iCurNumSndBufIn; i++ )
        {
            result = waveInUnprepareHeader (
                m_WaveIn, &m_WaveInHeader[i], sizeof ( WAVEHDR ) );

            if ( result != MMSYSERR_NOERROR )
            {
                throw CGenErr ( "Sound Interface, waveInUnprepareHeader()"
                    " failed." );
            }
        }

        // close the sound in device
        result = waveInClose ( m_WaveIn );
        if ( result != MMSYSERR_NOERROR )
		{
            throw CGenErr ( "Sound Interface, waveInClose() failed." );
		}
    }

    if ( m_WaveOut != NULL )
    {
        for ( i = 0; i < iCurNumSndBufOut; i++ )
        {
            result = waveOutUnprepareHeader (
                m_WaveOut, &m_WaveOutHeader[i], sizeof ( WAVEHDR ) );

            if ( result != MMSYSERR_NOERROR )
            {
                throw CGenErr ( "Sound Interface, waveOutUnprepareHeader()"
                    " failed." );
            }
        }

        // close the sound out device
        result = waveOutClose ( m_WaveOut );
        if ( result != MMSYSERR_NOERROR )
		{
            throw CGenErr ( "Sound Interface, waveOutClose() failed." );
		}
    }

    // set flag to open devices the next time it is initialized
    bChangParamIn  = true;
    bChangParamOut = true;
}

CSound::CSound()
{
    int i;

    // init number of sound buffers
    iCurNumSndBufIn  = NUM_SOUND_BUFFERS_IN;
    iCurNumSndBufOut = NUM_SOUND_BUFFERS_OUT;

    // should be initialized because an error can occur during init
    m_WaveInEvent  = NULL;
    m_WaveOutEvent = NULL;
    m_WaveIn       = NULL;
    m_WaveOut      = NULL;

    // init buffer pointer to zero
    for ( i = 0; i < MAX_SND_BUF_IN; i++ )
    {
        memset ( &m_WaveInHeader[i], 0, sizeof ( WAVEHDR ) );
        psSoundcardBuffer[i] = NULL;
    }

    for ( i = 0; i < MAX_SND_BUF_OUT; i++ )
    {
        memset ( &m_WaveOutHeader[i], 0, sizeof ( WAVEHDR ) );
        psPlaybackBuffer[i] = NULL;
    }

    // init wave-format structure
    sWaveFormatEx.wFormatTag      = WAVE_FORMAT_PCM;
    sWaveFormatEx.nChannels       = NUM_IN_OUT_CHANNELS;
    sWaveFormatEx.wBitsPerSample  = BITS_PER_SAMPLE;
    sWaveFormatEx.nSamplesPerSec  = SND_CRD_SAMPLE_RATE;
    sWaveFormatEx.nBlockAlign     = sWaveFormatEx.nChannels *
        sWaveFormatEx.wBitsPerSample / 8;
    sWaveFormatEx.nAvgBytesPerSec = sWaveFormatEx.nBlockAlign *
        sWaveFormatEx.nSamplesPerSec;
    sWaveFormatEx.cbSize          = 0;

    // get the number of digital audio devices in this computer, check range
    iNumDevs = waveInGetNumDevs();

    if ( iNumDevs > MAX_NUMBER_SOUND_CARDS )
	{
        iNumDevs = MAX_NUMBER_SOUND_CARDS;
	}

    // at least one device must exist in the system
    if ( iNumDevs == 0 )
	{
        throw CGenErr ( "No audio device found." );
	}

    // get info about the devices and store the names
    for ( i = 0; i < iNumDevs; i++ )
    {
        if ( !waveInGetDevCaps ( i, &m_WaveInDevCaps, sizeof ( WAVEINCAPS ) ) )
		{
            pstrDevices[i] = m_WaveInDevCaps.szPname;
		}
    }

    // we use an event controlled wave-in (wave-out) structure
    // create events
    m_WaveInEvent  = CreateEvent ( NULL, FALSE, FALSE, NULL );
    m_WaveOutEvent = CreateEvent ( NULL, FALSE, FALSE, NULL );

    // set flag to open devices
    bChangParamIn  = true;
    bChangParamOut = true;

    // default device number, "wave mapper"
    iCurInDev  = WAVE_MAPPER;
    iCurOutDev = WAVE_MAPPER;

    // non-blocking wave out is default
    bBlockingPlay = false;

    // blocking wave in is default
    bBlockingRec = true;
}

CSound::~CSound()
{
    int i;

    // delete allocated memory
    for ( i = 0; i < iCurNumSndBufIn; i++ )
    {
        if ( psSoundcardBuffer[i] != NULL )
		{
            delete[] psSoundcardBuffer[i];
		}
    }

    for ( i = 0; i < iCurNumSndBufOut; i++ )
    {
        if ( psPlaybackBuffer[i] != NULL )
		{
            delete[] psPlaybackBuffer[i];
		}
    }

    // close the handle for the events
    if ( m_WaveInEvent != NULL )
	{
        CloseHandle ( m_WaveInEvent );
	}

    if ( m_WaveOutEvent != NULL )
	{
        CloseHandle ( m_WaveOutEvent );
	}
}

#endif // USE_ASIO_SND_INTERFACE
