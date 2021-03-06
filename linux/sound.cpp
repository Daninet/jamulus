/******************************************************************************\
 * Copyright (c) 2004-2013
 *
 * Author(s):
 *  Volker Fischer
 *
 * This code is based on the simple_client example of the Jack audio interface.
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

#include "sound.h"

#ifdef WITH_SOUND
void CSound::OpenJack()
{
    jack_status_t JackStatus;

    // try to become a client of the JACK server
    pJackClient = jack_client_open ( APP_NAME, JackNullOption, &JackStatus );

    if ( pJackClient == NULL )
    {
        throw CGenErr ( tr ( "The Jack server is not running. This software "
            "requires a Jack server to run. Normally if the Jack server is "
            "not running this software will automatically start the Jack server. "
            "It seems that this auto start has not worked. Try to start the Jack "
            "server manually." ) );
    }

    // tell the JACK server to call "process()" whenever
    // there is work to be done
    jack_set_process_callback ( pJackClient, process, this );

    // register a "buffer size changed" callback function
    jack_set_buffer_size_callback ( pJackClient, bufferSizeCallback, this );

    // register shutdown callback function
    jack_on_shutdown ( pJackClient, shutdownCallback, this );

    // check sample rate, if not correct, just fire error
    if ( jack_get_sample_rate ( pJackClient ) != SYSTEM_SAMPLE_RATE_HZ )
    {
        throw CGenErr ( tr ( "The Jack server sample rate is different from "
            "the required one. The required sample rate is: <b>" ) +
            QString().setNum ( SYSTEM_SAMPLE_RATE_HZ ) + tr ( " Hz</b>. You can "
            "use a tool like <i><a href=""http://qjackctl.sourceforge.net"">QJackCtl</a></i> "
            "to adjust the Jack server sample rate.<br>Make sure to set the "
            "<b>Frames/Period</b> to a low value like <b>" ) +
            QString().setNum ( SYSTEM_FRAME_SIZE_SAMPLES * FRAME_SIZE_FACTOR_PREFERRED ) +
            tr ( "</b> to achieve a low delay." ) );
    }

    // create four ports (two for input, two for output -> stereo)
    input_port_left = jack_port_register ( pJackClient, "input left",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );

    input_port_right = jack_port_register ( pJackClient, "input right",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );

    output_port_left = jack_port_register ( pJackClient, "output left",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );

    output_port_right = jack_port_register ( pJackClient, "output right",
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );

    if ( ( input_port_left   == NULL ) ||
         ( input_port_right  == NULL ) ||
         ( output_port_left  == NULL ) ||
         ( output_port_right == NULL ) )
    {
        throw CGenErr ( tr ( "The Jack port registering failed." ) );
    }

    const char** ports;

    // tell the JACK server that we are ready to roll
    if ( jack_activate ( pJackClient ) )
    {
        throw CGenErr ( tr ( "Cannot activate the Jack client." ) );
    }

    // connect the ports, note: you cannot do this before
    // the client is activated, because we cannot allow
    // connections to be made to clients that are not
    // running
    if ( ( ports = jack_get_ports ( pJackClient, NULL, NULL,
           JackPortIsPhysical | JackPortIsOutput ) ) == NULL )
    {
        throw CGenErr ( tr ( "There are no physical capture ports available. "
            "This software requires at least one stereo input channel available. "
            "Maybe you have selected the wrong sound card in the Jack server "
            "configuration.<br>"
            "You can use a tool like <i><a href=""http://qjackctl.sourceforge.net"">QJackCtl</a></i> "
            "to adjust the Jack server settings." ) );
    }

    if ( !ports[1] )
    {
        throw CGenErr ( tr ( "There are no physical capture ports available. "
            "This software requires at least one stereo input channel available. "
            "Maybe you have selected the wrong sound card in the Jack server "
            "configuration.<br>"
            "You can use a tool like <i><a href=""http://qjackctl.sourceforge.net"">QJackCtl</a></i> "
            "to adjust the Jack server settings." ) );
    }

    if ( jack_connect ( pJackClient, ports[0], jack_port_name ( input_port_left ) ) )
    {
        throw CGenErr ( tr ( "Cannot connect the Jack input ports" ) );
    }
    if ( jack_connect ( pJackClient, ports[1], jack_port_name ( input_port_right ) ) )
    {
        throw CGenErr ( tr ( "Cannot connect the Jack input ports" ) );
    }

    free ( ports );

    if ( ( ports = jack_get_ports ( pJackClient, NULL, NULL,
           JackPortIsPhysical | JackPortIsInput ) ) == NULL )
    {
        throw CGenErr ( tr ( "There are no physical playback ports available. "
            "This software requires at least one stereo output channel available. "
            "Maybe you have selected the wrong sound card in the Jack server "
            "configuration.<br>"
            "You can use a tool like <i><a href=""http://qjackctl.sourceforge.net"">QJackCtl</a></i> "
            "to adjust the Jack server settings." ) );
    }

    if ( !ports[1] )
    {
        throw CGenErr ( tr ( "There are no physical playback ports available. "
            "This software requires at least one stereo output channel available. "
            "Maybe you have selected the wrong sound card in the Jack server "
            "configuration.<br>"
            "You can use a tool like <i><a href=""http://qjackctl.sourceforge.net"">QJackCtl</a></i> "
            "to adjust the Jack server settings." ) );
    }

    if ( jack_connect ( pJackClient, jack_port_name ( output_port_left ), ports[0] ) )
    {
        throw CGenErr ( tr ( "Cannot connect the Jack output ports." ) );
    }
    if ( jack_connect ( pJackClient, jack_port_name ( output_port_right ), ports[1] ) )
    {
        throw CGenErr ( tr ( "Cannot connect the Jack output ports." ) );
    }

    free ( ports );
}

void CSound::CloseJack()
{
    // deactivate client
    jack_deactivate ( pJackClient );

    // unregister ports
    jack_port_unregister ( pJackClient, input_port_left );
    jack_port_unregister ( pJackClient, input_port_right );
    jack_port_unregister ( pJackClient, output_port_left );
    jack_port_unregister ( pJackClient, output_port_right );

    // close client connection to jack server
    jack_client_close ( pJackClient );
}

void CSound::Start()
{
    // call base class
    CSoundBase::Start();
}

void CSound::Stop()
{
    // call base class
    CSoundBase::Stop();
}

int CSound::Init ( const int /* iNewPrefMonoBufferSize */ )
{
// try setting buffer size
// TODO seems not to work! -> no audio after this operation!
//jack_set_buffer_size ( pJackClient, iNewPrefMonoBufferSize );

    // get actual buffer size
    iJACKBufferSizeMono = jack_get_buffer_size ( pJackClient );  	

    // init base class
    CSoundBase::Init ( iJACKBufferSizeMono );

    // set internal buffer size value and calculate stereo buffer size
    iJACKBufferSizeStero = 2 * iJACKBufferSizeMono;

    // create memory for intermediate audio buffer
    vecsTmpAudioSndCrdStereo.Init ( iJACKBufferSizeStero );

    return iJACKBufferSizeMono;
}


// JACK callbacks --------------------------------------------------------------
int CSound::process ( jack_nframes_t nframes, void* arg )
{
    CSound* pSound = reinterpret_cast<CSound*> ( arg );
    int     i;

    if ( pSound->IsRunning() )
    {
        // get input data pointer
        jack_default_audio_sample_t* in_left =
            (jack_default_audio_sample_t*) jack_port_get_buffer (
            pSound->input_port_left, nframes );

        jack_default_audio_sample_t* in_right =
            (jack_default_audio_sample_t*) jack_port_get_buffer (
            pSound->input_port_right, nframes );

        // copy input data
        for ( i = 0; i < pSound->iJACKBufferSizeMono; i++ )
        {
            pSound->vecsTmpAudioSndCrdStereo[2 * i] =
                (short) ( in_left[i] * _MAXSHORT );

            pSound->vecsTmpAudioSndCrdStereo[2 * i + 1] =
                (short) ( in_right[i] * _MAXSHORT );
        }

        // call processing callback function
        pSound->ProcessCallback ( pSound->vecsTmpAudioSndCrdStereo );

        // get output data pointer
        jack_default_audio_sample_t* out_left =
            (jack_default_audio_sample_t*) jack_port_get_buffer (
            pSound->output_port_left, nframes );

        jack_default_audio_sample_t* out_right =
            (jack_default_audio_sample_t*) jack_port_get_buffer (
            pSound->output_port_right, nframes );

        // copy output data
        for ( i = 0; i < pSound->iJACKBufferSizeMono; i++ )
        {
            out_left[i] = (jack_default_audio_sample_t)
                pSound->vecsTmpAudioSndCrdStereo[2 * i] / _MAXSHORT;

            out_right[i] = (jack_default_audio_sample_t)
                pSound->vecsTmpAudioSndCrdStereo[2 * i + 1] / _MAXSHORT;
        }
    }
    else
    {
        // get output data pointer
        jack_default_audio_sample_t* out_left =
            (jack_default_audio_sample_t*) jack_port_get_buffer (
            pSound->output_port_left, nframes );

        jack_default_audio_sample_t* out_right =
            (jack_default_audio_sample_t*) jack_port_get_buffer (
            pSound->output_port_right, nframes );

        // clear output data
        memset ( out_left,
            0, sizeof ( jack_default_audio_sample_t ) * nframes );

        memset ( out_right,
            0, sizeof ( jack_default_audio_sample_t ) * nframes );
    }

    return 0; // zero on success, non-zero on error 
}

int CSound::bufferSizeCallback ( jack_nframes_t, void *arg )
{
    CSound* pSound = reinterpret_cast<CSound*> ( arg );

    pSound->EmitReinitRequestSignal ( RS_ONLY_RESTART_AND_INIT );

    return 0; // zero on success, non-zero on error
}

void CSound::shutdownCallback ( void* )
{
    // without a Jack server, our software makes no sense to run, throw
    // error message
    throw CGenErr ( tr ( "The Jack server was shut down. This software "
        "requires a Jack server to run. Try to restart the software to "
        "solve the issue." ) );
}
#endif // WITH_SOUND

