/*
    wm_win_ddraw.h - platform-dependent module : DirectDraw
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include "ddraw.h"

int dd_init( int fullscreen, window_manager* wm )
{
    DDSURFACEDESC ddsd;
    DDSCAPS ddscaps;
    HRESULT ddrval;

    wm->dd_primary_locked = false;

    ddrval = DirectDrawCreate( NULL, &wm->lpDD, NULL );
    if( ddrval != DD_OK ) 
    {
	MessageBox( wm->hwnd, "DirectDrawCreate error", "Error", MB_OK );
	return 1;
    }
    if( fullscreen )
	ddrval = wm->lpDD->SetCooperativeLevel( wm->hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    else
	ddrval = wm->lpDD->SetCooperativeLevel( wm->hwnd, DDSCL_NORMAL );
    if( ddrval != DD_OK ) 
    {
	MessageBox( wm->hwnd, "SetCooperativeLevel error", "Error", MB_OK );
	return 1;
    }
    if( fullscreen )
    {
	ddrval = wm->lpDD->SetDisplayMode( wm->screen_xsize, wm->screen_ysize, COLORBITS ); 
	if( ddrval != DD_OK ) 
	{ 
	    MessageBox( wm->hwnd, "SetDisplayMode error", "Error", MB_OK );
	    return 1;
	}
    }

    // Create the primary surface with 1 back buffer
    memset( &ddsd, 0, sizeof(ddsd) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS;// | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;// | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    //ddsd.dwBackBufferCount = 1;

    ddrval = wm->lpDD->CreateSurface( &ddsd, &wm->lpDDSPrimary, NULL ); 
    if( ddrval != DD_OK ) 
    { 
	switch( ddrval )
	{
	    case DDERR_UNSUPPORTEDMODE:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_UNSUPPORTEDMODE","Error",MB_OK);
	    break;
	    case DDERR_PRIMARYSURFACEALREADYEXISTS:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_PRIMARYSURFACEALREADYEXISTS ","Error",MB_OK);
	    break;
	    case DDERR_OUTOFVIDEOMEMORY:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_OUTOFVIDEOMEMORY  ","Error",MB_OK);
	    break;
	    case DDERR_OUTOFMEMORY:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_OUTOFMEMORY","Error",MB_OK);
	    break;
	    case DDERR_NOZBUFFERHW:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOZBUFFERHW","Error",MB_OK);
	    break;
	    case DDERR_NOOVERLAYHW:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOOVERLAYHW","Error",MB_OK);
	    break;
	    case DDERR_NOMIPMAPHW:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOMIPMAPHW","Error",MB_OK);
	    break;
	    case DDERR_NOEXCLUSIVEMODE:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOEXCLUSIVEMODE","Error",MB_OK);
	    break;
	    case DDERR_NOEMULATION:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOEMULATION","Error",MB_OK);
	    break;
	    case DDERR_NODIRECTDRAWHW:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NODIRECTDRAWHW","Error",MB_OK);
	    break;
	    case DDERR_NOCOOPERATIVELEVELSET:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOCOOPERATIVELEVELSET","Error",MB_OK);
	    break;
	    case DDERR_NOALPHAHW:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_NOALPHAHW","Error",MB_OK);
	    break;
	    case DDERR_INVALIDPIXELFORMAT:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_INVALIDPIXELFORMAT","Error",MB_OK);
	    break;
	    case DDERR_INVALIDPARAMS:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_INVALIDPARAMS","Error",MB_OK);
	    break;
	    case DDERR_INVALIDOBJECT:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_INVALIDOBJECT","Error",MB_OK);
	    break;
	    case DDERR_INVALIDCAPS:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_INVALIDCAPS","Error",MB_OK);
	    break;
	    case DDERR_INCOMPATIBLEPRIMARY:
	    MessageBox( wm->hwnd,"CreateSurface DDERR_INCOMPATIBLEPRIMARY","Error",MB_OK);
	    break;
	}
        return 1;
    } 

    if( fullscreen == 0 )
    {
	// Create a clipper to ensure that our drawing stays inside our window
	LPDIRECTDRAWCLIPPER lpClipper;
	ddrval = wm->lpDD->CreateClipper( 0, &lpClipper, NULL );
	if( ddrval != DD_OK )
	{
	    MessageBox( wm->hwnd, "Error in CreateClipper", "Error", MB_OK );
	    return 1;
	}

	// setting it to our hwnd gives the clipper the coordinates from our window
	ddrval = lpClipper->SetHWnd( 0, wm->hwnd );
	if( ddrval != DD_OK )
	{
	    MessageBox( wm->hwnd, "Error in SetHWnd", "Error", MB_OK );
	    return 1;
	}

	// attach the clipper to the primary surface
	ddrval = wm->lpDDSPrimary->SetClipper( lpClipper );
	if( ddrval != DD_OK )
	{
	    MessageBox( wm->hwnd, "Error in SetClipper", "Error", MB_OK );
	    return 1;
	}
    }

    /*ddscaps.dwCaps = DDSCAPS_BACKBUFFER; 
    ddrval = wm->lpDDSPrimary->GetAttachedSurface( &ddscaps, &wm->lpDDSBack );
    if( ddrval != DD_OK )
    { 
	MessageBox( wm->hwnd, "GetAttachedSurface error", "Error", MB_OK );
	return 1;
    }*/

    memset( &wm->dd_sd, 0, sizeof( DDSURFACEDESC ) );
    wm->dd_sd.dwSize = sizeof( wm->dd_sd );
    /*if( wm->lpDDSPrimary )
    {
	wm->lpDDSPrimary->Lock( 0, &wm->dd_sd, DDLOCK_SURFACEMEMORYPTR  | DDLOCK_WAIT, 0 );
	wm->fb_ypitch = wm->dd_sd.lPitch / COLORLEN;
	wm->fb = (COLORPTR)wm->dd_sd.lpSurface;
	wm->dd_primary_locked = true;
    }*/
    wm->dd_primary_locked = false;
    wm->fb = 0;

    return 0;
}

int dd_close( window_manager* wm )
{
    if( wm->lpDD != NULL ) 
    { 
        if( wm->lpDDSPrimary != NULL ) 
        { 
	    if( wm->dd_primary_locked )
	    {
		wm->lpDDSPrimary->Unlock( wm->dd_sd.lpSurface );
		wm->dd_primary_locked = false;
	    }
            wm->lpDDSPrimary->Release(); 
            wm->lpDDSPrimary = NULL; 
        } 
        wm->lpDD->Release(); 
        wm->lpDD = NULL; 
    }
    return 0;
}
