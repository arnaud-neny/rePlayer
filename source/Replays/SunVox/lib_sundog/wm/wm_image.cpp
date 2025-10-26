/*
    wm_image.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifndef OPENGL
    #define IMG_OPENGL false
#else
    #define IMG_OPENGL !wm->fb
#endif

sdwm_image* new_image( 
    int xsize, 
    int ysize, 
    void* src,
    int src_xsize, //whole source image size
    int src_ysize,
    uint flags,
    window_manager* wm )
{
    sdwm_image* img = NULL;
    
    img = SMEM_ALLOC2( sdwm_image, 1 );
    if( img )
    {
	img->wm = wm;
	img->xsize = xsize;
	img->ysize = ysize;
	img->flags = flags;
	img->backcolor = COLORMASK;
	img->color = COLORMASK;
	img->opacity = 255;
	img->pixelsize = 1;
	if( flags & IMAGE_NATIVE_RGB ) img->pixelsize = COLORLEN;
	if( flags & IMAGE_STATIC_SOURCE )
	{
	    img->data = src;
	}
	else
	{
	    img->data = SMEM_ALLOC( img->xsize * img->ysize * img->pixelsize );
	    if( flags & IMAGE_CLEAN ) smem_zero( img->data );
	    if( src )
	    {
		if( xsize == src_xsize && ysize == src_ysize )
		{
		    smem_copy( img->data, src, xsize * ysize * img->pixelsize );
		}
		else
		{
		    int ysize2 = ysize;
		    if( src_ysize < ysize ) ysize2 = src_ysize;
		    for( int y = 0; y < ysize2; y++ )
		    {
			int8_t* line_dest = (int8_t*)img->data + y * img->xsize * img->pixelsize;
			int8_t* line_src = (int8_t*)src + y * src_xsize * img->pixelsize;
			int line_size = xsize;
			if( src_xsize < line_size ) line_size = src_xsize;
			line_size *= img->pixelsize;
			smem_copy( line_dest, line_src, line_size );
		    }
		}
	    }
	}
	if( IMG_OPENGL && img->data )
	{
#ifdef OPENGL
	    gl_lock( wm );
	    
	    unsigned int texture_id;
	    glGenTextures( 1, &texture_id );
	    img->gl_texture_id = texture_id;
	    img->gl_xsize = 0;
	    img->gl_ysize = 0;
	    update_image( img );
	    
	    gl_unlock( wm );
#endif
	}
    }
    
    return img;
}

void update_image( sdwm_image* img, int x, int y, int xsize, int ysize )
{
    if( !img ) return;
    if( !img->data ) return;
    window_manager* wm = img->wm;
    if( xsize == 0 ) xsize = img->xsize;
    if( ysize == 0 ) ysize = img->ysize;
    if( IMG_OPENGL )
    {
#ifdef OPENGL
	gl_lock( wm );

	if( ysize > 1 )
	{
	    x = 0;
	    xsize = img->xsize;
	}

        GL_BIND_TEXTURE( wm, img->gl_texture_id );
	if( img->flags & IMAGE_INTERP )
	{
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	if( img->flags & IMAGE_NO_XREPEAT ) glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	if( img->flags & IMAGE_NO_YREPEAT ) glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        int internal_format = GL_ALPHA;
        int format = GL_ALPHA;
        int type = GL_UNSIGNED_BYTE;
        if( img->flags & IMAGE_NATIVE_RGB )
        {
#ifdef COLOR32BITS
            internal_format = GL_RGB;
            format = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
#endif
#ifdef COLOR16BITS
            internal_format = GL_RGB;
            format = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
#endif
	}
	//internal_format == GL_RGB && format == GL_RGBA
	//  in OpenGL: texture alpha value will always be 1.0;
	//  in OpenGL ES: not allowed; use GL_RGBA with special RGB shader or fill the alpha value in the source data (see code below);
#ifdef OPENGLES
        if( internal_format == GL_RGB && format == GL_RGBA )
	{
    	    internal_format = GL_RGBA;
    	    /*uint32_t* p = (uint32_t*)img->data + x + y * img->xsize;
    	    for( int yy = 0; yy < ysize; yy++ )
    	    {
    		for( int xx = 0; xx < xsize; xx++ )
    		{
    		    (*p) |= 0xFF000000; 
    		    p++;
    		}
    		p += img->xsize - xsize;
    	    }*/
        }
#endif
	if( img->gl_xsize == 0 )
	{
	    //First update:
	    img->gl_xsize = round_to_power_of_two( xsize );
	    img->gl_ysize = round_to_power_of_two( ysize );
    	    glTexImage2D(
		GL_TEXTURE_2D,
    		0,
    		internal_format,
    		img->gl_xsize, img->gl_ysize,
    		0,
    		format,
    		type,
    		0 );
    	}
    	glTexSubImage2D( GL_TEXTURE_2D, 0, x, y, xsize, ysize, format, type, (int8_t*)img->data + y * img->xsize * img->pixelsize );

	gl_unlock( wm );
#endif
    }
}

void update_image( sdwm_image* img )
{
    update_image( img, 0, 0, 0, 0 );
}

sdwm_image* resize_image( sdwm_image* img, int resize_flags, int new_xsize, int new_ysize )
{
    if( !img ) return 0;
    if( !img->data ) return 0;
    window_manager* wm = img->wm;
    if( !( img->flags & IMAGE_STATIC_SOURCE ) )
    {
	sdwm_image* new_img = new_image( new_xsize, new_ysize, img->data, img->xsize, img->ysize, img->flags, wm );
	remove_image( img );
	return new_img;
    }
    return 0;
}

void remove_image( sdwm_image* img )
{
    if( !img ) return;
    window_manager* wm = img->wm;
    if( IMG_OPENGL )
    {
#ifdef OPENGL
        gl_lock( wm );
        GL_DELETE_TEXTURES( wm, 1, &img->gl_texture_id );
        gl_unlock( wm );
#endif
    }
    if( img->data && !( img->flags & IMAGE_STATIC_SOURCE ) )
    {
        smem_free( img->data );
    }
    smem_free( img );
}
