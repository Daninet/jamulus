rem/******************************************************************************\
rem * Copyright (c) 2004-2006
rem *
rem * Author(s):
rem *	Volker Fischer
rem *
rem * Description:
rem *	Script for compiling the QT resources under Windows (MOCing and UICing)
rem ******************************************************************************
rem *
rem * This program is free software; you can redistribute it and/or modify it under
rem * the terms of the GNU General Public License as published by the Free Software
rem * Foundation; either version 2 of the License, or (at your option) any later 
rem * version.
rem *
rem * This program is distributed in the hope that it will be useful, but WITHOUT 
rem * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
rem * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 1111
rem * details.
rem *
rem * You should have received a copy of the GNU General Public License along with
rem * this program; if not, write to the Free Software Foundation, Inc., 
rem * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
rem *
rem\******************************************************************************/


rem .h --------------
%qtdir%\bin\moc.exe ..\src\util.h -o moc\moc_util.cpp
%qtdir%\bin\moc.exe ..\src\multicolorled.h -o moc\moc_multicolorled.cpp
%qtdir%\bin\moc.exe ..\src\llconclientdlg.h -o moc\moc_llconclientdlg.cpp
%qtdir%\bin\moc.exe ..\src\llconserverdlg.h -o moc\moc_llconserverdlg.cpp
%qtdir%\bin\moc.exe ..\src\server.h -o moc\moc_server.cpp
%qtdir%\bin\moc.exe ..\src\client.h -o moc\moc_client.cpp
%qtdir%\bin\moc.exe ..\src\socket.h -o moc\moc_socket.cpp
%qtdir%\bin\moc.exe ..\src\protocol.h -o moc\moc_protocol.cpp
%qtdir%\bin\moc.exe ..\src\channel.h -o moc\moc_channel.cpp


rem .ui -------------
%qtdir%\bin\uic.exe ..\src\aboutdlgbase.ui -o moc\aboutdlgbase.h  
%qtdir%\bin\uic.exe ..\src\aboutdlgbase.ui -i aboutdlgbase.h -o moc\aboutdlgbase.cpp  
%qtdir%\bin\moc.exe moc\aboutdlgbase.h -o moc\moc_aboutdlgbase.cpp

%qtdir%\bin\uic.exe ..\src\llconclientdlgbase.ui -o moc\llconclientdlgbase.h  
%qtdir%\bin\uic.exe ..\src\llconclientdlgbase.ui -i llconclientdlgbase.h -o moc\llconclientdlgbase.cpp  
%qtdir%\bin\moc.exe moc\llconclientdlgbase.h -o moc\moc_llconclientdlgbase.cpp

%qtdir%\bin\uic.exe ..\src\llconserverdlgbase.ui -o moc\llconserverdlgbase.h  
%qtdir%\bin\uic.exe ..\src\llconserverdlgbase.ui -i llconserverdlgbase.h -o moc\llconserverdlgbase.cpp  
%qtdir%\bin\moc.exe moc\llconserverdlgbase.h -o moc\moc_llconserverdlgbase.cpp
