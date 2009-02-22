/******************************************************************************\
 * Copyright (c) 2004-2009
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

#if !defined ( SOUNDBASE_HOIHGEH8_3_4344456456345634565KJIUHF1912__INCLUDED_ )
#define SOUNDBASE_HOIHGEH8_3_4344456456345634565KJIUHF1912__INCLUDED_

#include <qthread.h>
#include "global.h"
#include "util.h"


/* Classes ********************************************************************/
class CSoundBase : public QThread
{
public:
    CSoundBase ( const int iNewStereoBufferSize,
        void (*fpNewCallback) ( CVector<short>& psData, void* arg ),
        void* arg ) : iStereoBufferSize ( iNewStereoBufferSize ),
        fpCallback ( fpNewCallback ), pCallbackArg ( arg ), bRun ( false ) {}
    virtual ~CSoundBase() {}

    virtual void Init();
    virtual void Start();
    virtual void Stop();
    bool         IsRunning() const { return bRun; }

protected:
    // function pointer to callback function
    void (*fpCallback) ( CVector<short>& psData, void* arg );
    void* pCallbackArg;

    // these functions should be overwritten by derived class for
    // non callback based audio interfaces
    virtual bool Read  ( CVector<short>& psData ) { printf ( "no sound!" ); return false; }
    virtual bool Write ( CVector<short>& psData ) { printf ( "no sound!" ); return false; }
    virtual void InitRecording ( const bool bNewBlocking = true ) = 0;
    virtual void InitPlayback  ( const bool bNewBlocking = false ) = 0;

    void run();
    bool bRun;

    int iStereoBufferSize;
};

#endif /* !defined ( SOUNDBASE_HOIHGEH8_3_4344456456345634565KJIUHF1912__INCLUDED_ ) */