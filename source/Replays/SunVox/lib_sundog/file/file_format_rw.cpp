/*
    file_format_rw.cpp - helper functions for reading and writing various file formats
    This file is part of the SunDog engine.
    Copyright (C) 2022 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

const int g_sfs_sample_format_sizes[ SFMT_MAX ] = 
{
    1, //SFMT_INT8
    2, //SFMT_INT16
    3, //SFMT_INT24
    4, //SFMT_INT32
    4, //SFMT_FLOAT32
    8 //SFMT_FLOAT64
};

#ifndef NOIMAGEFORMATS

//
// JPEG
//

#ifndef NOJPEG

#include "jpgd.h"
#include "jpge.h"

struct sfs_exif : public smem_base
{
    uint8_t* data; //EXIF data
    uint32_t offset; //current offset
    bool little_endian;
    const char* manufacturer;
    const char* model;
    const char* datetime;
    int orientation;	//the orientation of the camera relative to the scene:
		        //1 = Horizontal (normal)
		        //2 = Mirror horizontal
			//3 = Rotate 180
			//4 = Mirror vertical
			//5 = Mirror horizontal and rotate 270 clockwise
			//6 = Rotate 90 clockwise
			//7 = Mirror horizontal and rotate 90 clockwise
			//8 = Rotate 270 clockwise

    sfs_exif()
    {
	data = nullptr;
	offset = 0;
	little_endian = true;
	manufacturer = nullptr;
	model = nullptr;
	datetime = nullptr;
	orientation = 0;
    }
    ~sfs_exif()
    {
	smem_free( data );
    }

    uint8_t read_int8()
    {
	uint8_t v;
	if( offset+1 > smem_get_size( data ) ) return 0;
	v = data[ offset++ ];
	return v;
    }
    uint16_t read_int16()
    {
	uint16_t v;
	if( offset+2 > smem_get_size( data ) ) return 0;
	v = data[ offset++ ];
	v |= (int16_t)data[ offset++ ] << 8;
	if( !little_endian ) v = INT16_SWAP( v );
	return v;
    }
    uint32_t read_int32()
    {
	uint32_t v;
	if( offset+4 > smem_get_size( data ) ) return 0;
	v = data[ offset++ ];
	v |= (int32_t)data[ offset++ ] << 8;
	v |= (int32_t)data[ offset++ ] << 16;
	v |= (int32_t)data[ offset++ ] << 24;
	if( !little_endian ) v = INT32_SWAP( v );
	return v;
    }
    void seek( uint32_t new_offset )
    {
	if( new_offset >= smem_get_size( data ) ) return;
	offset = new_offset;
    }

    void parse( sfs_file f, int len ) //len - without EXIF signature
    {
	//slog( "EXIF %d bytes\n", len );
	data = SMEM_ALLOC2( uint8_t, len );
	if( (int)sfs_read( data, 1, len, f ) != len )
	{
	    smem_free( data );
	    return;
	}

	//TIFF header:

	uint16_t v = read_int16(); //byte align (endianness)
	if( v == 0x4d4d ) little_endian = false;
	//slog( "EXIF little endian = %d\n", little_endian );
	v = read_int16(); //0x002A
	if( v != 0x002A )
	{
	    //slog( "EXIF incorrect endianness\n" );
	    return;
	}
	uint32_t ifd0 = read_int32(); //IFD0 (Image File Directory) offset
	//slog( "EXIF IFD0 offset = %d\n", ifd0 );
	if( ifd0 > offset ) seek( ifd0 );

	//IFD0 (Image File Directory):

	while( 1 )
	{
	    uint16_t entry_cnt = read_int16(); //number of directory entry contains in this IFD
	    //slog( "EXIF IFD entry count = %d\n", entry_cnt );
	    for( uint16_t i = 0; i < entry_cnt; i++ )
	    {
		uint16_t tag = read_int16(); //tag ID
		uint16_t format = read_int16(); //data format
		uint32_t num_components = read_int32(); //number of components
		int component_size = 1; //in bytes
		switch( format )
		{
		    case 3: component_size = 2; break; //unsigned short
		    case 4: component_size = 4; break; //unsigned long
		    case 5: component_size = 8; break; //unsigned rational
		    case 8: component_size = 2; break; //signed short
		    case 9: component_size = 4; break; //signed long
		    case 10: component_size = 8; break; //signed rational
		    case 11: component_size = 4; break; //float32
		    case 12: component_size = 8; break; //float64
		    default: break;
		}
		uint8_t* val_ptr = &data[ offset ];
		uint32_t val = 0; //data value or offset to data value
		if( num_components * component_size > 4 )
		{
		    val = read_int32();
		    val_ptr = &data[ val ];
		}
		else
		{
		    switch( component_size )
		    {
			case 1: val = read_int8(); offset += 3; break;
			case 2: val = read_int16(); offset += 2; break;
			default: val = read_int32(); break;
		    }
		}
	        //slog( "EXIF IFD entry: tag=%x; format=%d; components=%d; data=%x;\n", tag, format, num_components, val );
	        switch( tag )
	        {
	    	    case 0x010f:
	    		//manufacturer:
	    		//slog( "EXIF IFD manufacturer = %s\n", &data[ val ] );
	    		manufacturer = (const char*)val_ptr;
	    		break;
	    	    case 0x0110:
	    		//model:
	    		//slog( "EXIF IFD model = %s\n", &data[ val ] );
	    		model = (const char*)val_ptr;
	    		break;
	    	    case 0x0132:
	    		//date/time:
	    		//slog( "EXIF IFD date/time = %s\n", &data[ val ] );
	    		datetime = (const char*)val_ptr;
	    		break;
	    	    case 0x0112:
	    		//orientation:
	    		//slog( "EXIF IFD orientation = %x\n", val );
	    		orientation = val;
	    		break;
	        }
	    }
	    uint32_t next_ifd = read_int32(); //offset to the next IFD
	    //slog( "NEXT IFD offset %d\n", next_ifd );
	    if( next_ifd == 0 ) break;
	    seek( next_ifd ); //go to the next IFD
	}
    }

    bool load_from_jpeg( sfs_file f )
    {
	bool rv = 0;
	uint16_t v;
	sfs_read( &v, 1, 2, f ); //skip SOI marker (0xFFD8)
	while( 1 )
	{
	    if( sfs_read( &v, 1, 2, f ) != 2 ) break;
	    if( ( v & 0xFF ) != 0xFF ) break;
	    uint8_t marker = ( v >> 8 ) & 0xFF;
	    uint16_t len;
	    if( sfs_read( &len, 1, 2, f ) != 2 ) break;
	    len = INT16_SWAP( len );
	    if( len < 2 ) break;
	    if( marker == 0xE1 ) //APP1 marker = EXIF
	    {
		char sign[ 6 ];
		if( sfs_read( &sign, 1, 6, f ) != 6 ) break;
		if( sign[ 0 ] == 'E' && sign[ 1 ] == 'x' && sign[ 2 ] == 'i' && sign[ 3 ] == 'f' && sign[ 4 ] == 0 && sign[ 5 ] == 0 )
		{
		    //EXIF signature found:
		    parse( f, len - 2 - 6 );
		}
		else break;
		rv = 1;
		break;
	    }
    	    sfs_seek( f, len - 2, SFS_SEEK_CUR );
	}
	return rv;
    }
};

class jd_file_stream : public jpgd::jpeg_decoder_stream
{
    sfs_file f;
    bool eof_flag, error_flag;

public:
    jd_file_stream()
    {
	f = 0;
        eof_flag = false;
        error_flag = false;
    }

    void attach( sfs_file new_f ) { f = new_f; }

    int read( uint8_t* pBuf, int max_bytes_to_read, bool* pEOF_flag ) CPP_OVERRIDE
    {
    	if( !f )
	    return -1;

	if( eof_flag )
	{
	    *pEOF_flag = true;
	    return 0;
	}

	if( error_flag )
	    return -1;

	int bytes_read = static_cast<int>(sfs_read(pBuf, 1, max_bytes_to_read, f));
	if( bytes_read < max_bytes_to_read )
	{
	    if( sfs_eof( f ) )
	    {
		eof_flag = true;
		*pEOF_flag = true;
		return bytes_read;
	    }
	    error_flag = true;
	    return -1;
	}
	return bytes_read;
    }
};

int sfs_load_jpeg( sundog_engine* sd, const char* filename, sfs_file f, simage_desc* img, uint32_t flags )
{
    int rv = SD_RES_ERR;

    if( filename && f == 0 )
	f = sfs_open( sd, filename, "rb" );
    if( f == 0 ) return SD_RES_ERR_FOPEN;

    int exif_orientation = 0;
    if( flags & LOAD_JPEG_FLAG_AUTOROTATE )
    {
	int64_t off0 = sfs_tell( f );
	sfs_exif exif = sfs_exif();
	exif.load_from_jpeg( f );
	sfs_seek( f, off0, SFS_SEEK_SET );
	exif_orientation = exif.orientation;
    }

    jd_file_stream stream;
    stream.attach( f );

    jpgd::jpeg_decoder* jd = nullptr;
    uint8_t* dest = nullptr;

    while( 1 )
    {
	uint32_t flags = 0;
	jd = SMEM_NEW jpgd::jpeg_decoder( &stream, flags );
	if( jd->get_error_code() != jpgd::JPGD_SUCCESS )
	{
	    slog( "JPEG loading: jpeg_decoder() error %d\n", jd->get_error_code() );
	    break;
	}

	int width = jd->get_width();
	int height = jd->get_height();
	int channels = jd->get_num_components();
	int pixel_format = img->format;
	img->width = width;
	img->height = height;

	if( jd->begin_decoding() != jpgd::JPGD_SUCCESS )
	{
	    slog( "JPEG loading: begin_decoding() error %d\n", jd->get_error_code() );
	    break;
	}

	int dest_bpp = g_simage_pixel_format_size[ pixel_format ];
	const int dest_bpl = width * dest_bpp;

        dest = SMEM_ALLOC2( uint8_t, dest_bpl * height );
	if( !dest ) break;

	for( int y = 0; y < height; y++ )
	{
	    const uint8_t* scan_line;
	    uint scan_line_len;
	    if( jd->decode( (const void**)&scan_line, &scan_line_len ) != jpgd::JPGD_SUCCESS )
	    {
		slog( "JPEG loading: decode() error %d\n", jd->get_error_code() );
    		break;
	    }

	    uint8_t* dest_line = dest + y * dest_bpl;

	    switch( pixel_format )
	    {
    	        case PFMT_GRAYSCALE8_SRGB:
    	    	    if( channels == 3 )
    		    {
    		    	for( int x = 0; x < width * 4; x += 4 )
    		    	{
        	    	    int r = scan_line[ x + 0 ];
        		    int g = scan_line[ x + 1 ];
        		    int b = scan_line[ x + 2 ];
        		    *dest_line++ = ( r + g + b ) / 3;
    			}
    		    }
    		    if( channels == 1 )
    		    {
		        memcpy( dest_line, scan_line, dest_bpl );
    		    }
    		    break;
    		case PFMT_R8G8B8A8_SRGB:
    		    if( channels == 3 )
    		    {
    		        for( int x = 0; x < width * 4; x += 4 )
    		        {
        	    	    *dest_line++ = scan_line[ x + 0 ];
        		    *dest_line++ = scan_line[ x + 1 ];
        		    *dest_line++ = scan_line[ x + 2 ];
        		    *dest_line++ = 255;
    			}
    		    }
    		    if( channels == 1 )
    		    {
    		        for( int x = 0; x < width; x++ )
    		        {
        	    	    uint8_t luma = scan_line[ x ];
        		    *dest_line++ = luma;
        		    *dest_line++ = luma;
        		    *dest_line++ = luma;
        		    *dest_line++ = 255;
    			}
    		    }
    		    break;
#ifndef SUNDOG_VER
    		case PFMT_SUNDOG_COLOR:
    		    if( channels == 3 )
    		    {
    		        COLORPTR p = (COLORPTR)dest_line;
    			for( int x = 0; x < width * 4; x += 4 )
    			{
        		    int r = scan_line[ x + 0 ];
        		    int g = scan_line[ x + 1 ];
        		    int b = scan_line[ x + 2 ];
        		    *p++ = get_color( r, g, b );
    			}
    		    }
    		    if( channels == 1 )
    		    {
    		        COLORPTR p = (COLORPTR)dest_line;
    		        for( int x = 0; x < width; x++ )
    		        {
        	    	    uint8_t luma = scan_line[ x ];
        		    *p++ = get_color( luma, luma, luma );
    			}
    		    }
    		    break;
#endif
	    }
	}

	img->data = dest;
	dest = nullptr;
	rv = SD_RES_SUCCESS;
	break;
    }

    if( f && filename ) sfs_close( f );
    SMEM_DELETE( jd );
    smem_free( dest );

    if( img->data )
    {
	bool hflip = 0;
	bool vflip = 0;
	int rotate = 0;
	switch( exif_orientation )
	{
	    case 2: //Mirror horizontal
		hflip = 1;
		break;
	    case 3: //Rotate 180
		rotate = 2;
		break;
	    case 4: //Mirror vertical
		vflip = 1;
		break;
	    case 5: //Mirror horizontal and rotate 270 clockwise
		hflip = 1;
		rotate = 3;
		break;
	    case 6: //Rotate 90 clockwise
		rotate = 1;
		break;
	    case 7: //Mirror horizontal and rotate 90 clockwise
		hflip = 1;
		rotate = 1;
		break;
	    case 8: //Rotate 270 clockwise
		rotate = 3;
		break;
	    default:
		break;
	}
	if( hflip )
	    hflip_2d_array( img->data, img->width, img->height, g_simage_pixel_format_size[img->format] );
	if( vflip )
	    vflip_2d_array( img->data, img->width, img->height, g_simage_pixel_format_size[img->format] );
	if( rotate )
	    rotate_2d_array( &img->data, img->width, img->height, g_simage_pixel_format_size[img->format], rotate, nullptr );
    }

    return rv;
}

class je_file_stream : public jpge::output_stream
{
    sfs_file f;

public:

    je_file_stream()
    {
	f = 0;
    }

    void attach( sfs_file new_f ) { f = new_f; }

    bool put_buf( const void* pBuf, int len ) CPP_OVERRIDE
    {
	if( sfs_write( pBuf, len, 1, f ) == 1 ) return true;
	return false;
    }
};

int sfs_save_jpeg( sundog_engine* sd, const char* filename, sfs_file f, simage_desc* img, sfs_save_jpeg_pars* pars )
{
    int rv = SD_RES_ERR;

    jpge::jpeg_encoder* je = nullptr;

    while( 1 )
    {
	jpge::params encoder_pars;
	encoder_pars.m_quality = pars->quality;
	encoder_pars.m_two_pass_flag = pars->two_pass_flag;
	switch( pars->subsampling )
        {
	    case JE_Y_ONLY: encoder_pars.m_subsampling = jpge::Y_ONLY; break;
	    case JE_H1V1: encoder_pars.m_subsampling = jpge::H1V1; break;
	    case JE_H2V1: encoder_pars.m_subsampling = jpge::H2V1; break;
	    case JE_H2V2: encoder_pars.m_subsampling = jpge::H2V2; break;
	}
#ifndef SUNDOG_VER
    	encoder_pars.m_sundog_color = false;
#endif

	int num_channels = 0;
	int pixel_size = 0;
	switch( img->format )
	{
    	    case PFMT_GRAYSCALE8_SRGB:
    		num_channels = 1;
    		pixel_size = 1;
		break;
	    case PFMT_R8G8B8A8_SRGB:
    		num_channels = 3;
    		pixel_size = 4;
		break;
#ifndef SUNDOG_VER
	    case PFMT_SUNDOG_COLOR:
    		num_channels = 3;
    		pixel_size = COLORLEN;
    		encoder_pars.m_sundog_color = true;
		break;
#endif
	    default:
		break;
	}
	if( num_channels == 0 || pixel_size == 0 )
	{
	    slog( "JPEG saving: unsupported img format %d\n", img->format );
	    break;
	}
	//num_channels == je->m_image_bpp
	//je->m_num_components = 1 for Y_ONLY; and 3 for other values;

	if( filename && f == 0 ) f = sfs_open( sd, filename, "wb" );
	if( !f ) { rv = SD_RES_ERR_FOPEN; break; }
	je_file_stream stream;
	stream.attach( f );

        je = SMEM_NEW jpge::jpeg_encoder();
        if( !je->init( &stream, img->width, img->height, num_channels, encoder_pars ) )
        {
	    slog( "JPEG saving: init() error\n" );
    	    break;
	}

	const int width = img->width;
	const int height = img->height;
        const int line_size = width * pixel_size;
        for( uint pass_index = 0; pass_index < je->get_total_passes(); pass_index++ )
        {
	    const uint8_t* buf = (const uint8_t*)img->data;
            for( int i = 0; i < height; i++ )
            {
                if( !je->process_scanline( buf ) )
                {
            	    break;
            	}
                buf += line_size;
            }
            if( !je->process_scanline( NULL ) ) 
            {
        	break;
    	    }
        }

	rv = SD_RES_SUCCESS;
	break;
    }

    if( filename && f ) sfs_close( f );
    if( rv ) slog( "JPEG saving error: %d\n", rv );
    SMEM_DELETE( je );

    return rv;
}

#endif //!NOJPEG

//
// PNG
//

#ifndef NOPNG

#include "png.h"

static void sfs_png_read( png_structp png_ptr, png_bytep data, png_size_t length )
{
    sfs_file f = *(sfs_file*)png_get_io_ptr( png_ptr );
    sfs_read( data, 1, length, f );
}

static void sfs_png_write( png_structp png_ptr, png_bytep data, png_size_t length )
{
    sfs_file f = *(sfs_file*)png_get_io_ptr( png_ptr );
    sfs_write( data, 1, length, f );
}

static void sfs_png_flush( png_structp png_ptr )
{
    sfs_file f = *(sfs_file*)png_get_io_ptr( png_ptr );
    sfs_flush( f );
}

int sfs_load_png( sundog_engine* sd, const char* filename, sfs_file f, simage_desc* img )
{
    int rv = SD_RES_ERR;
    
    if( filename && f == 0 )
	f = sfs_open( sd, filename, "rb" );
    if( f == 0 ) return -1;
    
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep* row_pointers = NULL;
    int8_t* pixels = NULL;
    int width = 0;
    int height = 0;
    void* palette = NULL;

    while( 1 )
    {
	png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( !png_ptr ) break;
        info_ptr = png_create_info_struct( png_ptr );
        if( !info_ptr ) break;
        if( setjmp( png_jmpbuf( png_ptr ) ) ) break;
        
        png_set_read_fn( png_ptr, &f, sfs_png_read );
        png_read_info( png_ptr, info_ptr );

        img->width = width = png_get_image_width( png_ptr, info_ptr );
        img->height = height = png_get_image_height( png_ptr, info_ptr );
        int color_type = png_get_color_type( png_ptr, info_ptr );
        int bits_per_channel = png_get_bit_depth( png_ptr, info_ptr );
        int channels = png_get_channels( png_ptr, info_ptr );

	if( bits_per_channel < 8 ) png_set_packing( png_ptr ); //Extract pixels with bit depths of 1/2/4 from a single byte into separate bytes (useful for paletted and grayscale images)

	/*if( bits_per_channel != 8 )
	{
	    slog( "PNG load: unsupported number of bits per channel %d\n", bits_per_channel );
	    break;
	}*/

	int number_of_passes = png_set_interlace_handling( png_ptr );
        png_read_update_info( png_ptr, info_ptr );
	
	if( setjmp( png_jmpbuf( png_ptr ) ) ) break;
        row_pointers = SMEM_ALLOC2( png_bytep, height );
        uint32_t row_bytes = png_get_rowbytes( png_ptr, info_ptr );
        pixels = SMEM_ALLOC2( int8_t, row_bytes * height );
        for( int y = 0; y < height; y++ )
            row_pointers[ y ] = (png_byte*)&pixels[ y * row_bytes ];
        png_read_image( png_ptr, row_pointers );

	int dest_bpp = g_simage_pixel_format_size[ img->format ];
	int dest_format = img->format;
	
	if( color_type == PNG_COLOR_TYPE_PALETTE )
	{
	    png_colorp pp = NULL;
	    int items = 0;
	    if( png_get_PLTE( png_ptr, info_ptr, &pp, &items ) )
	    {
		palette = SMEM_ALLOC( items * dest_bpp );
		switch( dest_format )
		{
		    case PFMT_GRAYSCALE8_SRGB:
			for( int i = 0; i < items; i++ )
			{
			    int r = pp[ i ].red;
			    int g = pp[ i ].green;
			    int b = pp[ i ].blue;
			    ((uint8_t*)palette)[ i ] = ( r + g + b ) / 3;
			}
			break;
		    case PFMT_R8G8B8A8_SRGB:
			for( int i = 0; i < items; i++ )
			{
			    int r = pp[ i ].red;
			    int g = pp[ i ].green;
			    int b = pp[ i ].blue;
			    ((uint8_t*)palette)[ i * 4 + 0 ] = r;
			    ((uint8_t*)palette)[ i * 4 + 1 ] = g;
			    ((uint8_t*)palette)[ i * 4 + 2 ] = b;
			    ((uint8_t*)palette)[ i * 4 + 3 ] = 255;
			}
			break;
#ifndef SUNDOG_VER
		    case PFMT_SUNDOG_COLOR:
			for( int i = 0; i < items; i++ )
			{
			    int r = pp[ i ].red;
			    int g = pp[ i ].green;
			    int b = pp[ i ].blue;
			    ((COLORPTR)palette)[ i ] = get_color( r, g, b );
			}
			break;
#endif
		}
	    }
	}

	const int dest_bpl = width * dest_bpp;
        uint8_t* dest = SMEM_ALLOC2( uint8_t, dest_bpl * height );
	if( !dest ) break;

	if( bits_per_channel <= 8 )
	{
	    for( int y = 0; y < height; y++ )
	    {
		uint8_t* src = row_pointers[ y ];
		uint8_t* dest_line = dest + y * dest_bpl;
		switch( channels )
		{
		    case 1:
			if( palette )
			{
			    switch( dest_format )
			    {
				case PFMT_GRAYSCALE8_SRGB:
				    for( int x = 0; x < width; x++ )
				    {
					int i = *src; src++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
				    }
				    break;
				case PFMT_R8G8B8A8_SRGB:
				    for( int x = 0; x < width; x++ )
				    {
					int i = *src; src++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
					*dest_line = ((uint8_t*)palette)[ i ]; dest_line++;
				    }
				    break;
#ifndef SUNDOG_VER
				case PFMT_SUNDOG_COLOR:
				    for( int x = 0; x < width; x++ )
				    {
					int i = *src; src++;
					*((COLORPTR)dest_line) = ((COLORPTR)palette)[ i ]; dest_line += COLORLEN;
				    }
				    break;
#endif
			    }
			}
			else
			{
			    switch( dest_format )
			    {
				case PFMT_GRAYSCALE8_SRGB:
				    for( int x = 0; x < width; x++ )
				    {
					int v = *src; src++;
					*dest_line = v; dest_line++;
				    }
				    break;
				case PFMT_R8G8B8A8_SRGB:
				    for( int x = 0; x < width; x++ )
				    {
					int v = *src; src++;
					*dest_line = v; dest_line++;
					*dest_line = v; dest_line++;
					*dest_line = v; dest_line++;
					*dest_line = 255; dest_line++;
				    }
				    break;
#ifndef SUNDOG_VER
				case PFMT_SUNDOG_COLOR:
				    for( int x = 0; x < width; x++ )
				    {
					int v = *src; src++;
					*((COLORPTR)dest_line) = get_color( v, v, v ); dest_line += COLORLEN;
				    }
				    break;
#endif
			    }
			}
			break;
		    case 3:
			switch( dest_format )
			{
			    case PFMT_GRAYSCALE8_SRGB:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    *dest_line = ( r + g + b ) / 3;
				    dest_line++;
				}
				break;
			    case PFMT_R8G8B8A8_SRGB:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    *dest_line = r; dest_line++;
				    *dest_line = g; dest_line++;
				    *dest_line = b; dest_line++;
				    *dest_line = 255; dest_line++;
				}
				break;
#ifndef SUNDOG_VER
			    case PFMT_SUNDOG_COLOR:
			        for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    *((COLORPTR)dest_line) = get_color( r, g, b ); dest_line += COLORLEN;
				}
				break;
#endif
			}
			break;
		    case 4:
			switch( dest_format )
			{
			    case PFMT_GRAYSCALE8_SRGB:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    /*int a = *src;*/ src++;
				    *dest_line = ( r + g + b ) / 3;
				    dest_line++;
				}
				break;
			    case PFMT_R8G8B8A8_SRGB:
				for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    int a = *src; src++;
				    *dest_line = r; dest_line++;
				    *dest_line = g; dest_line++;
				    *dest_line = b; dest_line++;
				    *dest_line = a; dest_line++;
				}
				break;
#ifndef SUNDOG_VER
			    case PFMT_SUNDOG_COLOR:
			        for( int x = 0; x < width; x++ )
				{
				    int r = *src; src++;
				    int g = *src; src++;
				    int b = *src; src++;
				    /*int a = *src;*/ src++;
				    *((COLORPTR)dest_line) = get_color( r, g, b ); dest_line += COLORLEN;
				}
				break;
#endif
			}
			break;
		}
	    }
	}

	img->data = dest;
	rv = SD_RES_SUCCESS;
	break;
    }

    png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
    if( row_pointers )
    {
        smem_free( row_pointers );
        smem_free( pixels );
    }
    
    smem_free( palette );

    if( f && filename )
    {
        sfs_close( f );
    }

    return rv;
}

#endif //!NOPNG

#endif //!NOIMAGEFORMATS

#ifndef NOAUDIOFORMATS

//
// Audio formats
//

#ifndef NOOGG

#include "tremor/ivorbiscodec.h"
#include "tremor/ivorbisfile.h"

struct vorbis_decoder
{
    OggVorbis_File vf;
    ov_callbacks vc;
    bool vf_initialized;
};

static size_t ov_read( void* ptr, size_t s, size_t nmemb, void* datasource )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)datasource;
    return sfs_read( ptr, 1, s * nmemb, d->f );
}

static int ov_seek( void* datasource, ogg_int64_t offset, int whence )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)datasource;
    return sfs_seek( d->f, offset, whence );
}

static int ov_close( void* datasource )
{
    return 0;
}

static long ov_tell( void* datasource )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)datasource;
    return sfs_tell( d->f );
}

static int vorbis_decoder_init( sfs_sound_decoder_data* d )
{
    int rv = -1;
    vorbis_decoder* data = nullptr;
    while( 1 )
    {
	d->format_decoder_data = SMEM_ZALLOC( sizeof( vorbis_decoder ) );
	data = (vorbis_decoder*)d->format_decoder_data;

	data->vc.read_func = &ov_read;
        data->vc.seek_func = &ov_seek;
        data->vc.close_func = &ov_close;
        data->vc.tell_func = &ov_tell;

        rv = tremor_ov_open_callbacks( d, &data->vf, 0, 0, data->vc );
        if( rv != 0 ) { rv = -2; break; }
        data->vf_initialized = true;

        tremor_vorbis_info* vi = tremor_ov_info( &data->vf, -1 );
        d->rate = vi->rate;
        d->channels = vi->channels;
        d->len = tremor_ov_pcm_total( &data->vf, -1 );
        d->sample_format = SFMT_INT16;
        d->sample_size = 2;
	d->frame_size = d->sample_size * d->channels;

	rv = 0;
	break;
    }
    if( rv && data )
    {
        if( data->vf_initialized )
	    tremor_ov_clear( &data->vf );
    }
    return rv;
}

static void vorbis_decoder_deinit( sfs_sound_decoder_data* d )
{
    vorbis_decoder* data = (vorbis_decoder*)d->format_decoder_data;
    tremor_ov_clear( &data->vf );
    smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
}

static size_t vorbis_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    vorbis_decoder* data = (vorbis_decoder*)d->format_decoder_data;
    size_t bytes_to_read = len * d->frame_size;
    int logical_bitstream_number;
    int64_t read_rv = tremor_ov_read( &data->vf, dest_buf, bytes_to_read, &logical_bitstream_number );
    if( read_rv <= 0 )
    {
	//error/hole in data (OV_HOLE), partial open (OV_EINVAL)
	read_rv = 0;
    }
    if( read_rv == 0 )
    {
	//EOF
    }
    if( read_rv > 0 )
    {
	read_rv /= d->frame_size; //bytes -> frames
    }
    return read_rv;
}

static int vorbis_decoder_seek( sfs_sound_decoder_data* d, size_t frame )
{
    vorbis_decoder* data = (vorbis_decoder*)d->format_decoder_data;
    return tremor_ov_pcm_seek( &data->vf, frame );
}

static int64_t vorbis_decoder_tell( sfs_sound_decoder_data* d )
{
    vorbis_decoder* data = (vorbis_decoder*)d->format_decoder_data;
    return tremor_ov_pcm_tell( &data->vf );
}

#endif //!NOOGG

#ifndef NOMP3

#ifdef NOSIMD
    #define DR_MP3_NO_SIMD
#endif
#define DR_MP3_NO_STDIO
#define DR_MP3_ONLY_MP3
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

struct mp3_decoder
{
    drmp3 decoder;
    bool decoder_initialized;
    uint32_t enc_delay; //Delay at start of file; produced by the encoder; (in PCM frames)
    uint32_t enc_padding; //Extra padding at the end of a file; produced by the encoder; (in PCM frames)
    uint32_t dec_delay; //Decoder delay (in PCM frames)
    uint8_t tmp_buf[ 2500 ];
    //http://en.wikipedia.org/wiki/Gapless_playback
    //https://lame.sourceforge.io/tech-FAQ.txt
};

static size_t mp3_read( void* pUserData, void* pBufferOut, size_t bytesToRead )
{
    /*
    pUserData   [in]  The user data that was passed to drmp3_init(), drmp3_open() and family.
    pBufferOut  [out] The output buffer.
    bytesToRead [in]  The number of bytes to read.
    Returns the number of bytes actually read.
    A return value of less than bytesToRead indicates the end of the stream.
    Do _not_ return from this callback until either the entire bytesToRead is filled or you have reached the end of the stream.
    */
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)pUserData;
    return sfs_read( pBufferOut, 1, bytesToRead, d->f );
}

drmp3_bool32 mp3_seek( void* pUserData, int offset, drmp3_seek_origin origin )
{
    /*
    pUserData [in] The user data that was passed to drmp3_init(), drmp3_open() and family.
    offset    [in] The number of bytes to move, relative to the origin. Will never be negative.
    origin    [in] The origin of the seek - the current position or the start of the stream.
    Returns whether or not the seek was successful.
    Whether or not it is relative to the beginning or current position is determined by the "origin" parameter which
    will be either drmp3_seek_origin_start or drmp3_seek_origin_current.
    */
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)pUserData;
    return sfs_seek( d->f, offset, origin ) ? 0 : 1;
}

static int mp3_decoder_init( sfs_sound_decoder_data* d )
{
    int rv = -1;
    mp3_decoder* data = nullptr;
    while( 1 )
    {
	d->format_decoder_data = SMEM_ZALLOC( sizeof( mp3_decoder ) );
	data = (mp3_decoder*)d->format_decoder_data;

	//http://en.wikipedia.org/wiki/Gapless_playback
	//https://lame.sourceforge.io/tech-FAQ.txt
	data->enc_delay = 0;
	data->dec_delay = 528;

	//drmp3 can't parse MP3 Info Tag, so i will try to parse it here:
	int f_ptr = 0;
	uint8_t* v = data->tmp_buf;
	for( int c = 0; c < 2; c++ )
	{
	    size_t r = sfs_read( v, 1, 10, d->f );
	    if( r != 10 ) break;
	    if( v[0] == 'I' && v[1] == 'D' && v[2] == '3' )
	    {
		//ID3 (TAG v2):
		uint32_t tag_size = v[9] | (v[8]<<7) | (v[7]<<14) | (v[6]<<21);
		sfs_seek( d->f, tag_size, SFS_SEEK_CUR );
		//printf( "ID3 TAG found: size %d\n", tag_size );
		f_ptr += tag_size + 10;
	    }
	    if( v[0] == 0xFF && v[1] == 0xFB )
	    {
		//MP3 frame:
		uint32_t frame_size = drmp3_hdr_frame_bytes( v, 0 ) + drmp3_hdr_padding( v );
		uint32_t frame_samples = drmp3_hdr_frame_samples( v );
		size_t r = sfs_read( v + 10, 1, frame_size - 10, d->f );
		//printf( "MP3 frame found: size %d\n", frame_size );
		uint8_t* info = (uint8_t*)smem_memmem( v, frame_size, "Xing", 4 );
		if( !info ) info = (uint8_t*)smem_memmem( v, frame_size, "Info", 4 );
		if( info )
		{
		    int ptr = 4;
		    uint8_t flags = info[ ptr + 3 ];
		    ptr += 4;
		    //printf( "Flags: %x\n", flags );
		    if( flags & 1 ) ptr += 4; //total number of frames
		    if( flags & 2 ) ptr += 4; //num of bytes of MPEG Audio in the file
		    if( flags & 4 ) ptr += 100; //TOC Data
		    if( flags & 8 ) ptr += 4; //VBR Scale
		    ptr += 9; //LAME version
		    ptr += 1; //revision + VBR Type
		    ptr += 1; //lowpass freq
		    ptr += 4; //peak
		    ptr += 2; //radio replay
		    ptr += 2; //audiophile replay gain
		    ptr += 1; //more flags...
		    ptr += 1; //average bitrate (kbit/s)
		    
		    data->enc_delay = ( (uint32_t)info[ ptr ] << 4 ) | ( info[ ptr + 1 ] >> 4 );
		    data->enc_delay += frame_samples;
		    data->enc_padding = ( (uint32_t)( info[ ptr + 1 ] & 15 ) << 8 ) | info[ ptr + 2 ];
		    ptr += 3;
		    //printf( "ENCODER DELAY = %d; PADDING = %d\n", data->enc_delay, data->enc_padding );
		    
		    ptr += 1; //misc
		    ptr += 1; //unused
		    ptr += 2; //preset
		    uint32_t mp3_len = ( (uint32_t)info[ ptr + 0 ] << 24 ) | ( (uint32_t)info[ ptr + 1 ] << 16 ) | ( (uint32_t)info[ ptr + 2 ] << 8 ) |	( (uint32_t)info[ ptr + 3 ] );
		    ptr += 4; //MP3 file length in bytes: from the first LAME tag byte ... to the last byte containing music
		}
		f_ptr += frame_size;
	    }
	}
	sfs_rewind( d->f );

	if( drmp3_init( &data->decoder, mp3_read, mp3_seek, d, nullptr ) == 0 )
	{
	    rv = -2;
	    break;
	}
	data->decoder_initialized = true;

	d->rate = data->decoder.sampleRate;
	d->channels = data->decoder.channels;
	d->len = drmp3_get_pcm_frame_count( &data->decoder ) - data->enc_delay - data->enc_padding;
        d->sample_format = SFMT_INT16;
        d->sample_size = 2;
	d->frame_size = d->sample_size * d->channels;

	if( data->enc_delay ) drmp3_seek_to_pcm_frame( &data->decoder, data->enc_delay );
	if( data->dec_delay ) drmp3_read_pcm_frames_s16( &data->decoder, data->dec_delay, (drmp3_int16*)data->tmp_buf );

	rv = 0;
	break;
    }
    if( rv && data )
    {
	if( data->decoder_initialized )
	    drmp3_uninit( &data->decoder );
    }
    return rv;
}

static void mp3_decoder_deinit( sfs_sound_decoder_data* d )
{
    mp3_decoder* data = (mp3_decoder*)d->format_decoder_data;
    drmp3_uninit( &data->decoder );
    smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
}

static size_t mp3_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    mp3_decoder* data = (mp3_decoder*)d->format_decoder_data;
    return drmp3_read_pcm_frames_s16( &data->decoder, len, (drmp3_int16*)dest_buf );
}

static int mp3_decoder_seek( sfs_sound_decoder_data* d, size_t frame )
{
    mp3_decoder* data = (mp3_decoder*)d->format_decoder_data;
    int rv = drmp3_seek_to_pcm_frame( &data->decoder, frame + data->enc_delay ) ? 0 : 1;
    if( rv ) return rv; //error
    if( data->dec_delay ) drmp3_read_pcm_frames_s16( &data->decoder, data->dec_delay, (drmp3_int16*)data->tmp_buf );
    return rv;
}

static int64_t mp3_decoder_tell( sfs_sound_decoder_data* d )
{
    mp3_decoder* data = (mp3_decoder*)d->format_decoder_data;
    return data->decoder.currentPCMFrame - data->enc_delay;
}

#endif //!NOMP3

#ifndef NOFLAC

#include "flac_config.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_decoder.h"
#include "FLAC/stream_encoder.h"

struct flac_decoder
{
    FLAC__StreamDecoder* decoder;
    const FLAC__Frame* frame;
    const FLAC__int32* const *buffer;
    size_t read_ptr; //global PCM frame number
};

static FLAC__StreamDecoderReadStatus flac_read( const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    if( *bytes > 0 )
    {
	*bytes = sfs_read( buffer, sizeof(FLAC__byte), *bytes, d->f );
	if( *bytes == 0 )
	    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	else
	    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    else
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

static FLAC__StreamDecoderSeekStatus flac_seek( const FLAC__StreamDecoder* decoder, FLAC__uint64 absolute_byte_offset, void* client_data )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    if( SFS_IS_STD_STREAM( d->f ) )
	return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
    if( sfs_seek( d->f, absolute_byte_offset, SEEK_SET ) )
	return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus flac_tell( const FLAC__StreamDecoder* decoder, FLAC__uint64* absolute_byte_offset, void* client_data )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    if( SFS_IS_STD_STREAM( d->f ) )
	return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
    long pos;
    pos = sfs_tell( d->f );
    if( pos < 0 )
	return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = (FLAC__uint64)pos;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus flac_length( const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    *stream_length = sfs_get_data_size( d->f );
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flac_eof( const FLAC__StreamDecoder* decoder, void* client_data )
{
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    return sfs_eof( d->f ) ? true : false;
}

static FLAC__StreamDecoderWriteStatus flac_on_receive( const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data )
{
    /*
    The supplied function will be called when the decoder has decoded a single audio frame. 
    The decoder will pass the frame metadata as well as an array of pointers (one for each channel) pointing to the decoded audio.
    */
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    flac_decoder* data = (flac_decoder*)d->format_decoder_data;
    data->frame = frame;
    data->buffer = buffer;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    //FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
}

static void flac_on_metadata( const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data )
{
    //The supplied function will be called when the decoder has decoded a metadata block.
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
    if( metadata->type == FLAC__METADATA_TYPE_STREAMINFO )
    {
        d->len = metadata->data.stream_info.total_samples;
        d->rate = metadata->data.stream_info.sample_rate;
        d->channels = metadata->data.stream_info.channels;
	if( metadata->data.stream_info.bits_per_sample == 8 )
	{
    	    d->sample_format = SFMT_INT8;
    	    d->sample_size = 1;
    	}
	if( metadata->data.stream_info.bits_per_sample == 16 )
	{
    	    d->sample_format = SFMT_INT16;
	    d->sample_size = 2;
    	}
	if( metadata->data.stream_info.bits_per_sample == 24 )
	{
    	    d->sample_format = SFMT_INT24;
            d->sample_size = 3;
	}
	if( metadata->data.stream_info.bits_per_sample == 32 )
	{
    	    d->sample_format = SFMT_INT32;
    	    d->sample_size = 4;
    	}
	d->frame_size = d->sample_size * d->channels;
    }
}

static void flac_on_error( const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data )
{
    //The supplied function will be called whenever an error occurs during decoding.
    sfs_sound_decoder_data* d = (sfs_sound_decoder_data*)client_data;
}

static int flac_decoder_init( sfs_sound_decoder_data* d )
{
    int rv = -1;
    flac_decoder* data = nullptr;
    while( 1 )
    {
	d->format_decoder_data = SMEM_ZALLOC( sizeof( flac_decoder ) );
	data = (flac_decoder*)d->format_decoder_data;

	data->decoder = FLAC__stream_decoder_new();
	if( data->decoder == nullptr )
	{
            rv = -2;
            break;
        }

        FLAC__StreamDecoderInitStatus rv2 = FLAC__stream_decoder_init_stream(
    	    data->decoder,
	    flac_read,
	    flac_seek,
	    flac_tell,
	    flac_length,
    	    flac_eof,
	    flac_on_receive,
	    flac_on_metadata,
	    flac_on_error,
	    d
	);
	if( rv2 )
	{
	    rv = -3;
	    break;
	}

	if( FLAC__stream_decoder_process_until_end_of_metadata( data->decoder ) == 0 )
	{
	    rv = -4;
	    break;
	}
	if( d->channels == 0 || d->sample_size == 0 || d->len == 0 )
	{
	    rv = -5;
	    break;
	}
	//printf( "%dHz, %dch, %d, %d frames\n", d->rate, d->channels, d->sample_size, (int)d->len );

	rv = 0;
	break;
    }
    if( rv && data )
    {
	if( data->decoder )
	    FLAC__stream_decoder_delete( data->decoder );
    }
    return rv;
}

static void flac_decoder_deinit( sfs_sound_decoder_data* d )
{
    flac_decoder* data = (flac_decoder*)d->format_decoder_data;
    FLAC__stream_decoder_delete( data->decoder );
    smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
}

static size_t flac_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    flac_decoder* data = (flac_decoder*)d->format_decoder_data;
    if( data->read_ptr >= d->len ) return 0; //eof
    while( 1 )
    {
	bool need_more_packets = false;
	if( data->buffer && data->frame )
	{
	    size_t block_len = data->frame->header.blocksize; //in frames
	    size_t block_frame = data->frame->header.number.sample_number;
	    if( data->read_ptr >= block_frame + block_len )
	    {
		need_more_packets = true;
	    }
	    else
	    {
		if( data->read_ptr < block_frame ) data->read_ptr = block_frame; //packets lost
		size_t can_read = block_frame + block_len - data->read_ptr;
		if( len > can_read ) len = can_read;
		for( int c = 0; c < d->channels; c++ )
		{
		    switch( d->sample_format )
		    {
			case SFMT_INT8:
			    for( size_t i = 0, i2 = c; i < len; i++, i2 += d->channels )
			    {
				((int8_t*)dest_buf)[ i2 ] = data->buffer[c][i];
			    }
			    break;
			case SFMT_INT16:
			    for( size_t i = 0, i2 = c; i < len; i++, i2 += d->channels )
			    {
				((int16_t*)dest_buf)[ i2 ] = data->buffer[c][i];
			    }
			    break;
			case SFMT_INT24:
			    for( size_t i = 0, i2 = c*3; i < len; i++, i2 += d->channels*3 )
			    {
				int v = data->buffer[c][i];
				((uint8_t*)dest_buf)[ i2 ] = v & 255;
				((uint8_t*)dest_buf)[ i2 + 1 ] = ( v >> 8 ) & 255;
				((uint8_t*)dest_buf)[ i2 + 2 ] = ( v >> 16 ) & 255;
			    }
			    break;
			case SFMT_INT32:
			    for( size_t i = 0, i2 = c; i < len; i++, i2 += d->channels )
			    {
				((int32_t*)dest_buf)[ i2 ] = data->buffer[c][i];
			    }
			    break;
			default: break;
		    }
		}
		data->read_ptr += len;
		return len;
	    }
	} else need_more_packets = true;
	if( need_more_packets )
	{
	    if( FLAC__stream_decoder_process_single( data->decoder ) == 0 )
		break;
	}
    }
    return 0; //eof
}

static int flac_decoder_seek( sfs_sound_decoder_data* d, size_t frame )
{
    flac_decoder* data = (flac_decoder*)d->format_decoder_data;
    int rv = FLAC__stream_decoder_seek_absolute( data->decoder, frame ) ? 0 : 1;
    if( rv == 0 )
	data->read_ptr = frame;
    return rv;
}

static int64_t flac_decoder_tell( sfs_sound_decoder_data* d )
{
    flac_decoder* data = (flac_decoder*)d->format_decoder_data;
    return data->read_ptr;
}

#endif //!NOFLAC

struct wave_decoder
{
    size_t data_ptr;
};

static int wave_decoder_init( sfs_sound_decoder_data* d )
{
    int rv = -1;
    wave_decoder* data = nullptr;
    while( 1 )
    {
	d->format_decoder_data = SMEM_ZALLOC( sizeof( wave_decoder ) );
	data = (wave_decoder*)d->format_decoder_data;

	size_t file_size = sfs_get_data_size( d->f );

	sfs_seek( d->f, 12, SFS_SEEK_SET );

	uint32_t chunk[ 2 ]; //Chunk type and size
	uint16_t compression = 1;
	uint16_t channels = 1;
	uint32_t rate = 44100;
	uint32_t bytes_per_second = 0;
	uint16_t block_align = 0;
	uint16_t bits = 16;
	while( 1 )
	{
	    sfs_read( &chunk, 8, 1, d->f );
    	    if( sfs_eof( d->f ) != 0 ) break;
    	    //printf( "%08x %c%c%c%c 8+%d\n", chunk[ 0 ], chunk[ 0 ] & 255, ( chunk[ 0 ] >> 8 ) & 255, ( chunk[ 0 ] >> 16 ) & 255, ( chunk[ 0 ] >> 24 ) & 255, chunk[ 1 ] );
    	    uint32_t received = 0;
    	    if( chunk[ 0 ] == 0x20746D66 ) //'fmt ':
    	    {
        	sfs_read( &compression, 2, 1, d->f );
        	sfs_read( &channels, 2, 1, d->f );
        	sfs_read( &rate, 4, 1, d->f );
        	sfs_read( &bytes_per_second, 4, 1, d->f );
        	sfs_read( &block_align, 2, 1, d->f );
        	sfs_read( &bits, 2, 1, d->f );
        	received = 16;
        	if( bits != 8 && bits != 16 && bits != 24 && bits != 32 && bits != 64 )
        	{
            	    slog( "WAV decoder: %d-bit sample width is not supported\n", bits );
            	    rv = -2;
            	    break;
        	}
        	if( compression == 0xFFFE )
        	{
            	    //Compression format determined by SubFormat:
            	    if( chunk[ 1 ] == 40 )
            	    {
                	uint16_t cbSize = 0;
                	uint16_t wValidBitsPerSample = 0;
                	uint32_t dwChannelMask = 0;
                	uint16_t format2 = 0;
                	sfs_read( &cbSize, 2, 1, d->f );
                	sfs_read( &wValidBitsPerSample, 2, 1, d->f );
                	sfs_read( &dwChannelMask, 4, 1, d->f );
                	sfs_read( &format2, 2, 1, d->f );
                	sfs_seek( d->f, 14, 1 );
                	received = 40;
                	compression = format2;
            	    }
            	    else
            	    {
            	        slog( "WAV decoder: unsupported SubFormat size %d\n", chunk[ 1 ] );
            	    }
        	}
        	if( compression != 1 && compression != 3 )
        	{
            	    slog( "WAV decoder: wrong compression format %d. Only uncompressed PCM files supported\n", compression );
            	    rv = -3;
            	    break;
        	}
    	    }
    	    if( chunk[ 0 ] == 0x6C706D73 ) //'smpl':
    	    {
        	uint32_t dwManufacturer;
        	uint32_t dwProduct;
        	uint32_t dwSamplePeriod;
        	uint32_t dwMIDIUnityNote;
        	uint32_t dwMIDIPitchFraction;
        	uint32_t dwSMPTEFormat;
        	uint32_t dwSMPTEOffset;
        	uint32_t cSampleLoops;
        	uint32_t cbSamplerData;
        	sfs_read( &dwManufacturer, 4, 1, d->f );
        	sfs_read( &dwProduct, 4, 1, d->f );
        	sfs_read( &dwSamplePeriod, 4, 1, d->f );
        	sfs_read( &dwMIDIUnityNote, 4, 1, d->f );
        	sfs_read( &dwMIDIPitchFraction, 4, 1, d->f );
        	sfs_read( &dwSMPTEFormat, 4, 1, d->f );
        	sfs_read( &dwSMPTEOffset, 4, 1, d->f );
        	sfs_read( &cSampleLoops, 4, 1, d->f );
        	sfs_read( &cbSamplerData, 4, 1, d->f );
        	received = 9 * 4;
        	for( uint32_t i = 0; i < cSampleLoops; i++ )
        	{
            	    uint32_t dwIdentifier;
            	    uint32_t dwType;
            	    uint32_t dwStart;
            	    uint32_t dwEnd;
            	    uint32_t dwFraction;
            	    uint32_t dwPlayCount;
            	    sfs_read( &dwIdentifier, 4, 1, d->f );
            	    sfs_read( &dwType, 4, 1, d->f );
            	    sfs_read( &dwStart, 4, 1, d->f );
            	    sfs_read( &dwEnd, 4, 1, d->f );
            	    sfs_read( &dwFraction, 4, 1, d->f );
        	    sfs_read( &dwPlayCount, 4, 1, d->f );
            	    received += 4 * 6;
            	    if( i == 0 )
            	    {
                	switch( dwType )
                	{
                    	    case 0: d->loop_type = 1; break;
                    	    case 1: d->loop_type = 2; break;
                    	    default: d->loop_type = 1; break;
                	}
                	d->loop_start = dwStart;
                	d->loop_len = dwEnd - dwStart + 1;
            	    }
        	}
    	    }
    	    if( chunk[ 0 ] == 0x61746164 ) //'data':
    	    {
    		if( file_size && chunk[ 1 ] >= file_size )
                {
                    slog( "WAV decoder: incorrect 'data' chunk (%u bytes)\n", chunk[ 1 ] );
                    rv = -4;
                    break;
                }
                if( data->data_ptr == 0 )
                {
            	    data->data_ptr = sfs_tell( d->f );
            	    uint32_t samples_num = chunk[ 1 ] / ( bits / 8 );
		    d->rate = rate;
		    d->channels = channels;
		    d->len = samples_num / channels;
		    if( bits == 8 )
		    {
    			d->sample_format = SFMT_INT8;
    		    }
		    if( bits == 16 )
		    {
    			d->sample_format = SFMT_INT16;
    		    }
		    if( bits == 24 )
		    {
    			d->sample_format = SFMT_INT24;
    		    }
		    if( bits == 32 )
		    {
			if( compression == 1 )
    			    d->sample_format = SFMT_INT32;
    			else
    			    d->sample_format = SFMT_FLOAT32;
    		    }
		    if( bits == 64 )
		    {
			if( compression == 1 )
			{
			    //64bit int
			    rv = -5;
			    break;
    			}
    			else
    			    d->sample_format = SFMT_FLOAT64;
    		    }
    		    d->sample_size = g_sfs_sample_format_sizes[ d->sample_format ];
		    d->frame_size = d->sample_size * d->channels;
		    rv = 0;
            	}
	    }
	    if( chunk[ 1 ] & 1 ) chunk[ 1 ]++;
    	    if( received < chunk[ 1 ] ) sfs_seek( d->f, chunk[ 1 ] - received, 1 );
	}

	break;
    }
    if( rv == 0 )
    {
	if( data->data_ptr )
	{
	    sfs_seek( d->f, data->data_ptr, SFS_SEEK_SET );
	}
	else
	{
	    rv = -10;
	}
    }
    return rv;
}

static void wave_decoder_deinit( sfs_sound_decoder_data* d )
{
    wave_decoder* data = (wave_decoder*)d->format_decoder_data;
    smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
}

static size_t wave_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    wave_decoder* data = (wave_decoder*)d->format_decoder_data;
    size_t r = sfs_read( dest_buf, d->frame_size, len, d->f );
    if( r > 0 )
    {
	if( d->sample_format == SFMT_INT8 )
	{
	    //unsigned -> signed:
	    for( size_t i = 0; i < r * d->channels; i++ )
	    {
		((int8_t*)dest_buf)[ i ] -= 128;
	    }
	}
    }
    return r;
}

static int wave_decoder_seek( sfs_sound_decoder_data* d, size_t frame )
{
    wave_decoder* data = (wave_decoder*)d->format_decoder_data;
    return sfs_seek( d->f, data->data_ptr + frame * d->frame_size, SFS_SEEK_SET );
}

static int64_t wave_decoder_tell( sfs_sound_decoder_data* d )
{
    wave_decoder* data = (wave_decoder*)d->format_decoder_data;
    return ( sfs_tell( d->f ) - data->data_ptr ) / d->frame_size;
}

struct aiff_decoder
{
    size_t data_ptr;
    bool pcm_little_endian;
};

static int aiff_decoder_init( sfs_sound_decoder_data* d )
{
    int rv = -1;
    aiff_decoder* data = nullptr;
    while( 1 )
    {
	d->format_decoder_data = SMEM_ZALLOC( sizeof( aiff_decoder ) );
	data = (aiff_decoder*)d->format_decoder_data;

	sfs_seek( d->f, 4, SFS_SEEK_SET );
	uint32_t size;
	sfs_read( &size, 1, 4, d->f );

	char form_type[ 5 ];
	form_type[ 4 ] = 0;
	sfs_read( &form_type, 1, 4, d->f );
	int format_id = -1;
	if( smem_strcmp( form_type, "AIFF" ) == 0 ) format_id = 0; //Original AIFF-1.3 specification, 1989-01-04
	if( smem_strcmp( form_type, "AIFC" ) == 0 ) format_id = 1; //AIFF-C specifications, 1991-08-26
	if( format_id == -1 )
	{
    	    slog( "AIFF decoder: unknown format id: %s\n", format_id );
    	    rv = -2;
    	    break;
	}
	swap_bytes( &size, 4 );
	size -= 4;

	uint16_t channels = 1;
	uint32_t frames = 0;
	uint16_t bits = 8;
	bool smp_float = false;
	int rate = 44100;
	char compression_type[ 5 ];
	compression_type[ 4 ] = 0;

	uint32_t chunk_ids[ 2 ];
	smem_copy( &chunk_ids[ 0 ], "COMM", 4 );
	smem_copy( &chunk_ids[ 1 ], "SSND", 4 );
	uint32_t ptr = 0;
	uint32_t chunk[ 2 ]; //Chunk type and size
	while( ptr < size )
	{
    	    sfs_read( &chunk, 8, 1, d->f ); //name + data size
    	    if( sfs_eof( d->f ) ) break;
    	    uint32_t received = 0;
    	    swap_bytes( &chunk[ 1 ], 4 );
    	    //printf( "%08x %c%c%c%c %d\n", chunk[ 0 ], chunk[ 0 ] & 255, ( chunk[ 0 ] >> 8 ) & 255, ( chunk[ 0 ] >> 16 ) & 255, ( chunk[ 0 ] >> 24 ) & 255, chunk[ 1 ] );
    	    uint32_t chunk_type = chunk[ 0 ];
    	    if( chunk_type == chunk_ids[ 0 ] )
    	    {
        	//Common:
        	sfs_read( &channels, 1, 2, d->f );
        	sfs_read( &frames, 1, 4, d->f );
        	sfs_read( &bits, 1, 2, d->f );
        	uint8_t freq80[ 10 ];
        	sfs_read( freq80, 1, 10, d->f );
        	received = 18;
        	if( format_id == 1 )
        	{
            	    sfs_read( compression_type, 1, 4, d->f );
            	    uint8_t string_len = sfs_getc( d->f ) & 255;
            	    if( ( string_len & 1 ) == 0 ) string_len++; //extra pad byte
            	    sfs_seek( d->f, string_len, 1 );
            	    bool comp_error = 1;
            	    if( smem_strcmp( compression_type, "NONE" ) == 0 ) comp_error = 0;
            	    if( smem_strcmp( compression_type, "sowt" ) == 0 ) { comp_error = 0; data->pcm_little_endian = true; }
        	    if( smem_strcmp( compression_type, "fl32" ) == 0 ) { comp_error = 0; smp_float = true; }
        	    if( smem_strcmp( compression_type, "FL32" ) == 0 ) { comp_error = 0; smp_float = true; }
        	    if( smem_strcmp( compression_type, "fl64" ) == 0 ) { comp_error = 0; smp_float = true; }
            	    if( comp_error )
            	    {
                	slog( "AIFF decoder: wrong compression format %s. Only uncompressed PCM files supported\n", compression_type );
                	rv = -3;
                	break;
            	    }
        	    received += 4 + 1+string_len;
        	}
            	swap_bytes( &channels, 2 );
            	swap_bytes( &frames, 4 );
            	swap_bytes( &bits, 2 );
            	swap_bytes( freq80, 10 );
        	if( bits != 8 && bits != 16 && bits != 24 && bits != 32 && bits != 64 )
        	{
            	    slog( "AIFF decoder: %d-bit sample width is not supported\n", bits );
            	    rv = -4;
            	    break;
        	}
        	bool sign;
        	if( freq80[ 9 ] & 0x80 )
            	    sign = 1;
        	else
            	    sign = 0;
        	uint16_t exponent = freq80[ 8 ] + ( ( freq80[ 9 ] & 0x7F ) << 8 );
        	exponent = exponent - 16383 + 127;
        	exponent &= 255;
        	uint32_t fraction;
        	smem_copy( &fraction, &freq80[ 4 ], 4 );
        	volatile float freq32;
        	volatile uint32_t* p = (uint*)&freq32;
        	*p = ( ( fraction >> 8 ) & 0x7FFFFF ) | ( (uint)exponent << 23 ) | ( (uint)sign << 31 );
        	rate = freq32;
    	    }
    	    if( chunk_type == chunk_ids[ 1 ] )
    	    {
        	//Sound data:
        	uint32_t offset;
        	uint32_t block_size;
        	sfs_read( &offset, 1, 4, d->f );
        	sfs_read( &block_size, 1, 4, d->f );
        	received += 8;
            	swap_bytes( &offset, 4 );
            	swap_bytes( &block_size, 4 );
            	if( data->data_ptr == 0 )
        	{
            	    data->data_ptr = sfs_tell( d->f );
            	    uint32_t samples_num = ( chunk[ 1 ] - 8 ) / ( bits / 8 );
		    d->rate = rate;
		    d->channels = channels;
		    d->len = samples_num / channels;
		    if( bits == 8 )
		    {
    			d->sample_format = SFMT_INT8;
    		    }
		    if( bits == 16 )
		    {
    			d->sample_format = SFMT_INT16;
    		    }
		    if( bits == 24 )
		    {
    			d->sample_format = SFMT_INT24;
    		    }
		    if( bits == 32 )
		    {
			if( smp_float )
    			    d->sample_format = SFMT_FLOAT32;
    			else
        		    d->sample_format = SFMT_INT32;
    		    }
		    if( bits == 64 )
		    {
			if( smp_float )
    			    d->sample_format = SFMT_FLOAT64;
    			else
    			{
			    //64bit int
			    rv = -5;
			    break;
        		}
    		    }
    		    d->sample_size = g_sfs_sample_format_sizes[ d->sample_format ];
		    d->frame_size = d->sample_size * d->channels;
            	    rv = 0;
                }
    	    }
    	    if( chunk[ 1 ] & 1 ) chunk[ 1 ]++;
            if( received < chunk[ 1 ] ) sfs_seek( d->f, chunk[ 1 ] - received, 1 );
    	    ptr += chunk[ 1 ];
    	}
	break;
    }
    if( rv == 0 )
    {
	if( data->data_ptr )
	{
	    sfs_seek( d->f, data->data_ptr, SFS_SEEK_SET );
	}
	else
	{
	    rv = -10;
	}
    }
    return rv;
}

static void aiff_decoder_deinit( sfs_sound_decoder_data* d )
{
    aiff_decoder* data = (aiff_decoder*)d->format_decoder_data;
    smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
}

static size_t aiff_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    aiff_decoder* data = (aiff_decoder*)d->format_decoder_data;
    size_t r = sfs_read( dest_buf, d->frame_size, len, d->f );
    if( r > 0 )
    {
	if( !data->pcm_little_endian )
	{
            int8_t* ptr = (int8_t*)dest_buf;
	    for( size_t i = 0; i < r * d->frame_size; i += d->sample_size )
	    {
                swap_bytes( &ptr[ i ], d->sample_size );
            }
	}
    }
    return r;
}

static int aiff_decoder_seek( sfs_sound_decoder_data* d, size_t frame )
{
    aiff_decoder* data = (aiff_decoder*)d->format_decoder_data;
    return sfs_seek( d->f, data->data_ptr + frame * d->frame_size, SFS_SEEK_SET );
}

static int64_t aiff_decoder_tell( sfs_sound_decoder_data* d )
{
    aiff_decoder* data = (aiff_decoder*)d->format_decoder_data;
    return ( sfs_tell( d->f ) - data->data_ptr ) / d->frame_size;
}

int sfs_sound_decoder_init( sundog_engine* sd, const char* filename, sfs_file f, sfs_file_fmt file_format, uint32_t flags, sfs_sound_decoder_data* d )
{
    int rv = -1;
    if( d->initialized ) return 0;

    d->sd = sd;
    d->f_close_req = 0;
    if( filename && f == 0 )
    {
	f = sfs_open( sd, filename, "rb" );
	d->f_close_req = 1;
    }
    if( f == 0 ) return -1;
    d->filename = SMEM_STRDUP( filename );
    d->f = f;
    d->file_format = file_format;
    d->flags = flags;

    while( 1 )
    {
	d->format_decoder_data = nullptr;
	d->init = nullptr;
	switch( file_format )
	{
#ifndef NOOGG
	    case SFS_FILE_FMT_OGG:
		d->init = vorbis_decoder_init;
		d->deinit = vorbis_decoder_deinit;
		d->read = vorbis_decoder_read;
		d->seek = vorbis_decoder_seek;
		d->tell = vorbis_decoder_tell;
		break;
#endif //!NOOGG
#ifndef NOMP3
	    case SFS_FILE_FMT_MP3:
		d->init = mp3_decoder_init;
		d->deinit = mp3_decoder_deinit;
		d->read = mp3_decoder_read;
		d->seek = mp3_decoder_seek;
		d->tell = mp3_decoder_tell;
		break;
#endif //!NOMP3
#ifndef NOFLAC
	    case SFS_FILE_FMT_FLAC:
		d->init = flac_decoder_init;
		d->deinit = flac_decoder_deinit;
		d->read = flac_decoder_read;
		d->seek = flac_decoder_seek;
		d->tell = flac_decoder_tell;
		break;
#endif //!NOFLAC
	    case SFS_FILE_FMT_WAVE:
		d->init = wave_decoder_init;
		d->deinit = wave_decoder_deinit;
		d->read = wave_decoder_read;
		d->seek = wave_decoder_seek;
		d->tell = wave_decoder_tell;
		break;
	    case SFS_FILE_FMT_AIFF:
		d->init = aiff_decoder_init;
		d->deinit = aiff_decoder_deinit;
		d->read = aiff_decoder_read;
		d->seek = aiff_decoder_seek;
		d->tell = aiff_decoder_tell;
		break;
	    default:
		break;
	}
	if( !d->init ) { rv = -100; break; }
	rv = d->init( d );
	if( rv ) break;
	d->sample_format2 = d->sample_format;
	d->sample_size2 = d->sample_size2;
	d->frame_size2 = d->frame_size2;
	if( d->sample_format == SFMT_INT24 && ( d->flags & SFS_SDEC_CONVERT_INT24_TO_FLOAT32 ) ) d->sample_format2 = SFMT_FLOAT32;
	if( d->sample_format == SFMT_INT32 && ( d->flags & SFS_SDEC_CONVERT_INT32_TO_FLOAT32 ) ) d->sample_format2 = SFMT_FLOAT32;
	if( d->sample_format == SFMT_FLOAT64 && ( d->flags & SFS_SDEC_CONVERT_FLOAT64_TO_FLOAT32 ) ) d->sample_format2 = SFMT_FLOAT32;
	d->sample_size2 = g_sfs_sample_format_sizes[ d->sample_format2 ];
	d->frame_size2 = d->sample_size2 * d->channels;
	d->initialized = true;
    	rv = 0;
	break;
    }

    if( rv )
    {
	if( d->f_close_req ) sfs_close( d->f );
	smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
    }

    return rv;
}

void sfs_sound_decoder_deinit( sfs_sound_decoder_data* d )
{
    if( !d->initialized ) return;

    d->deinit( d );

    if( d->f_close_req ) sfs_close( d->f );
    smem_free( d->format_decoder_data ); d->format_decoder_data = nullptr;
    smem_free( d->tmp_buf ); d->tmp_buf = nullptr;
    smem_free( d->filename ); d->filename = nullptr;

    d->initialized = false;
}

//len = number of frames we want to read;
//return value:
//  =0: EOF;
//  >0: actually read (may be < len);
size_t sfs_sound_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    if( !d->initialized ) return 0;
    size_t r;
    if( d->sample_format != d->sample_format2 )
    {
	void* dest_buf2 = dest_buf;
	if( d->sample_format == SFMT_FLOAT64 && d->sample_format2 == SFMT_FLOAT32 )
	{
	    size_t prev_size = smem_get_size( d->tmp_buf );
	    size_t new_size = len * d->frame_size;
	    if( new_size > prev_size )
		d->tmp_buf = SMEM_RESIZE( d->tmp_buf, new_size );
	    dest_buf2 = d->tmp_buf;
	}
	r = d->read( d, dest_buf2, len );
	if( r > 0 )
	{
	    //Convert:
	    size_t r2 = r * d->channels; //number of samples
	    if( d->sample_format2 == SFMT_FLOAT32 )
	    {
                float* dest32f = (float*)dest_buf2;
		uint8_t* dest8 = (uint8_t*)dest_buf2;
		switch( d->sample_format )
		{
		    case SFMT_INT24:
			//int24 -> float32:
                        for( size_t i = r2 - 1, i24 = ( r2 - 1 ) * 3; (signed)i >= 0; i--, i24 -= 3 )
                        {
                            int v = dest8[ i24 + 0 ] | ( (int)dest8[ i24 + 1 ] << 8 ) | ( (int)dest8[ i24 + 2 ] << 16 );
                            if( v & 0x800000 ) v |= 0xFF000000; //sign
                            SMP_INT24_TO_FLOAT32( dest32f[ i ], v )
                        }
			break;
		    case SFMT_INT32:
			//int32 -> float32:
                        for( size_t i = 0; i < r2; i++ )
                        {
                            int32_t v = ((int32_t*)dest_buf2)[ i ];
                            SMP_INT32_TO_FLOAT32( dest32f[ i ], v )
                        }
			break;
		    case SFMT_FLOAT64:
			//float64 -> float32:
                        for( size_t i = 0; i < r2; i++ )
                        {
                            ((float*)dest_buf)[ i ] = ((double*)d->tmp_buf)[ i ];
                        }
			break;
		    default: break;
		}
	    }
	}
    }
    else
    {
	r = d->read( d, dest_buf, len );
    }
    return r;
}

//Read until the file/buffer ends
size_t sfs_sound_decoder_read2( sfs_sound_decoder_data* d, void* dest_buf, size_t len )
{
    size_t ptr = 0;
    while( ptr < len )
    {
        size_t to_read = len - ptr;
        uint8_t* dest = (uint8_t*)dest_buf + ptr * d->frame_size2;
        int64_t r = sfs_sound_decoder_read( d, dest, to_read );
        if( r <= 0 ) break;
        ptr += r;
    }
    return ptr;
}

int sfs_sound_decoder_seek( sfs_sound_decoder_data* d, size_t frame )
{
    if( !d->initialized ) return -1;
    return d->seek( d, frame );
}

int64_t sfs_sound_decoder_tell( sfs_sound_decoder_data* d )
{
    if( !d->initialized ) return -1;
    return d->tell( d );
}

//

struct rawaudio_encoder
{
    size_t ptr;
};

static int rawaudio_encoder_init( sfs_sound_encoder_data* e )
{
    int rv = -1;
    rawaudio_encoder* data = nullptr;
    while( 1 )
    {
	e->format_encoder_data = SMEM_ZALLOC( sizeof( rawaudio_encoder ) );
	data = (rawaudio_encoder*)e->format_encoder_data;

	rv = 0;
	break;
    }
    return rv;
}

static void rawaudio_encoder_deinit( sfs_sound_encoder_data* e )
{
    rawaudio_encoder* data = (rawaudio_encoder*)e->format_encoder_data;
    smem_free( e->format_encoder_data ); e->format_encoder_data = nullptr;
}

static size_t rawaudio_encoder_write( sfs_sound_encoder_data* e, void* src_buf, size_t len )
{
    rawaudio_encoder* data = (rawaudio_encoder*)e->format_encoder_data;
    size_t w = sfs_write( src_buf, e->frame_size, len, e->f );
    return w;
}

struct wave_encoder
{
    size_t ptr1;
    uint32_t ptr1_val; //WAVE + Fmt + smpl
    size_t ptr2;
};

static int wave_encoder_init( sfs_sound_encoder_data* e )
{
    int rv = -1;
    wave_encoder* data = nullptr;
    while( 1 )
    {
	e->format_encoder_data = SMEM_ZALLOC( sizeof( wave_encoder ) );
	data = (wave_encoder*)e->format_encoder_data;

        int temp;
	bool with_loop_info = e->loop_type && e->loop_len;

	//Save header:
        sfs_write( (void*)"RIFF", 1, 4, e->f );
        temp = 4 + 24 + 8; //WAVE + Fmt
        if( with_loop_info ) temp += 4 + 4 + ( 9 + 6 ) * 4; // + smpl
        data->ptr1_val = temp;
        temp += e->len * e->channels * e->sample_size; // + Data
        data->ptr1 = sfs_tell( e->f );
        sfs_write( &temp, 1, 4, e->f );
        sfs_write( (void*)"WAVE", 1, 4, e->f );

        //Save Fmt:
        sfs_write( (void*)"fmt ", 1, 4, e->f );
        temp = 16;
        sfs_write( &temp, 1, 4, e->f );
        uint16_t ff = 1;
        if( e->sample_format == SFMT_FLOAT32 ) ff = 3; //Float 32-bit
        sfs_write( &ff, 1, 2, e->f ); //Format
        ff = e->channels; //Channels
        sfs_write( &ff, 1, 2, e->f );
        temp = e->rate; //Frames per second
        sfs_write( &temp, 1, 4, e->f );
        temp = e->rate * e->channels * e->sample_size; //Bytes per second
        sfs_write( &temp, 1, 4, e->f );
        ff = e->channels * e->sample_size;
        sfs_write( &ff, 1, 2, e->f );
        ff = e->sample_size * 8; // bits
        sfs_write( &ff, 1, 2, e->f );

	//LOOP info:
        if( with_loop_info )
        {
            sfs_write( (void*)"smpl", 1, 4, e->f );
            temp = ( 9 + 6 ) * 4; sfs_write( &temp, 4, 1, e->f );
            uint32_t smpl[ 9 + 6 ];
            smpl[ 0 ] = 0; //dwManufacturer
            smpl[ 1 ] = 0; //dwProduct
            smpl[ 2 ] = (uint)( (uint64_t)1000000000 / (uint64_t)e->rate ); //dwSamplePeriod
            smpl[ 3 ] = 60; //dwMIDIUnityNote
            smpl[ 4 ] = 0; //dwMIDIPitchFraction
            smpl[ 5 ] = 0; //dwSMPTEFormat
            smpl[ 6 ] = 0; //dwSMPTEOffset
            smpl[ 7 ] = 1; //cSampleLoops
            smpl[ 8 ] = 0; //cbSamplerData
            smpl[ 9 ] = 0; //SampleLoop.dwIdentifier
            uint32_t type = 0;
            if( e->loop_type == 2 ) type = 1;
            smpl[ 10 ] = type; //SampleLoop.dwType
            smpl[ 11 ] = e->loop_start; //SampleLoop.dwStart
            smpl[ 12 ] = e->loop_start + e->loop_len - 1; //SampleLoop.dwEnd
            smpl[ 13 ] = 0; //SampleLoop.dwFraction
            smpl[ 14 ] = 0; //SampleLoop.dwPlayCount
            sfs_write( smpl, 1, sizeof( smpl ), e->f );
        }

        //Save data header:
        sfs_write( (void*)"data", 1, 4, e->f );
        temp = e->len * e->channels * e->sample_size;
        data->ptr2 = sfs_tell( e->f );
        sfs_write( &temp, 1, 4, e->f );

	rv = 0;
	break;
    }
    return rv;
}

static void wave_encoder_deinit( sfs_sound_encoder_data* e )
{
    wave_encoder* data = (wave_encoder*)e->format_encoder_data;

    if( ( e->len * e->frame_size ) & 1 ) sfs_putc( 0, e->f );

    //Fix wave size (if changed):
    size_t data_size = e->len * e->channels * e->sample_size;
    int temp;
    sfs_rewind( e->f );
    sfs_seek( e->f, data->ptr1, SFS_SEEK_SET );
    temp = data->ptr1_val; //WAVE + Fmt + smpl
    temp += data_size;
    sfs_write( &temp, 1, 4, e->f );
    sfs_seek( e->f, data->ptr2, SFS_SEEK_SET );
    temp = data_size;
    sfs_write( &temp, 1, 4, e->f );

    smem_free( e->format_encoder_data ); e->format_encoder_data = nullptr;
}

static size_t wave_encoder_write( sfs_sound_encoder_data* e, void* src_buf, size_t len )
{
    wave_encoder* data = (wave_encoder*)e->format_encoder_data;
    if( e->sample_format == SFMT_INT8 )
    {
        //signed -> unsigned:
        for( size_t i = 0; i < len * e->channels; i++ )
        {
    	    ((uint8_t*)src_buf)[ i ] += 128;
	}
    }
    size_t w = sfs_write( src_buf, e->frame_size, len, e->f );
    if( e->sample_format == SFMT_INT8 )
    {
        //unsigned -> signed:
        for( size_t i = 0; i < len * e->channels; i++ )
        {
    	    ((uint8_t*)src_buf)[ i ] -= 128;
	}
    }
    return w;
}

#ifndef NOFLAC
#ifndef NOFLACENC

#define FLAC_ENC_BUF_SIZE	256 /* number of samples */
struct flac_encoder
{
    FLAC__StreamEncoder* encoder;
    FLAC__int32 pcm[ FLAC_ENC_BUF_SIZE ];
};

static FLAC__StreamEncoderWriteStatus flac_enc_write(
    const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data )
{
    sfs_sound_encoder_data* e = (sfs_sound_encoder_data*)client_data;
    if( sfs_write( buffer, 1, bytes, e->f ) != bytes )
	return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus flac_enc_seek( const FLAC__StreamEncoder* encoder, FLAC__uint64 absolute_byte_offset, void* client_data )
{
    sfs_sound_encoder_data* e = (sfs_sound_encoder_data*)client_data;
    if( SFS_IS_STD_STREAM( e->f ) )
	return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
    if( sfs_seek( e->f, absolute_byte_offset, SEEK_SET ) != 0 )
	return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus flac_enc_tell( const FLAC__StreamEncoder* encoder, FLAC__uint64* absolute_byte_offset, void* client_data )
{
    sfs_sound_encoder_data* e = (sfs_sound_encoder_data*)client_data;
    if( SFS_IS_STD_STREAM( e->f ) )
	return FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED;
    long pos;
    pos = sfs_tell( e->f );
    if( pos < 0 )
	return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = (FLAC__uint64)pos;
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static void flac_enc_metadata( const FLAC__StreamEncoder* encoder, const FLAC__StreamMetadata* metadata, void* client_data )
{
}

static int flac_encoder_init( sfs_sound_encoder_data* e )
{
    int rv = -1;
    flac_encoder* data = nullptr;
    while( 1 )
    {
	e->format_encoder_data = SMEM_ZALLOC( sizeof( flac_encoder ) );
	data = (flac_encoder*)e->format_encoder_data;

	data->encoder = FLAC__stream_encoder_new();
	if( !data->encoder )
	{
	    rv = -2;
	    break;
	}

	int compression_level = 8;
	if( e->compression_level )
	{
	    compression_level = e->compression_level - 1;
	    if( compression_level > 8 ) compression_level = 8;
	}

	int bits_per_sample = e->sample_size * 8;
	if( e->sample_format == SFMT_FLOAT32 )
	    bits_per_sample = 24; //float32 -> int24 (due to some problems with 32-bit FLAC playback)

	FLAC__bool ok = true;
	ok &= FLAC__stream_encoder_set_verify( data->encoder, true );
        ok &= FLAC__stream_encoder_set_compression_level( data->encoder, compression_level );
        ok &= FLAC__stream_encoder_set_channels( data->encoder, e->channels );
        ok &= FLAC__stream_encoder_set_bits_per_sample( data->encoder, bits_per_sample );
        ok &= FLAC__stream_encoder_set_sample_rate( data->encoder, e->rate );
        ok &= FLAC__stream_encoder_set_total_samples_estimate( data->encoder, e->len );
        if( !ok )
        {
    	    rv = -3;
    	    break;
        }

        FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(
    	    data->encoder,
	    flac_enc_write,
	    flac_enc_seek,
	    flac_enc_tell,
	    flac_enc_metadata,
	    e );

	rv = 0;
	break;
    }
    if( rv && data )
    {
	if( data->encoder )
	    FLAC__stream_encoder_delete( data->encoder );
    }
    return rv;
}

static void flac_encoder_deinit( sfs_sound_encoder_data* e )
{
    flac_encoder* data = (flac_encoder*)e->format_encoder_data;
    FLAC__stream_encoder_finish( data->encoder );
    FLAC__stream_encoder_delete( data->encoder );
    smem_free( e->format_encoder_data ); e->format_encoder_data = nullptr;
}

static size_t flac_encoder_write( sfs_sound_encoder_data* e, void* src_buf, size_t len )
{
    flac_encoder* data = (flac_encoder*)e->format_encoder_data;
    size_t ptr = 0;
    while( ptr < len )
    {
	size_t can_write = len - ptr;
	if( can_write > (size_t)FLAC_ENC_BUF_SIZE / e->channels )
	    can_write = FLAC_ENC_BUF_SIZE / e->channels;
	switch( e->sample_format )
	{
	    case SFMT_INT8:
		for( size_t i = 0; i < can_write * e->channels; i++ )
		    data->pcm[ i ] = ((int8_t*)src_buf)[ ptr * e->channels + i ];
		break;
	    case SFMT_INT16:
		for( size_t i = 0; i < can_write * e->channels; i++ )
		    data->pcm[ i ] = ((int16_t*)src_buf)[ ptr * e->channels + i ];
		break;
	    case SFMT_INT24:
	    {
		uint8_t* src_buf8 = (uint8_t*)src_buf;
		src_buf8 += ptr * e->channels * 3;
		for( size_t i = 0, i2 = 0; i < can_write * e->channels; i++, i2 += 3 )
		{
		    int v = src_buf8[ i2 + 0 ] | ( (int)src_buf8[ i2 + 1 ] << 8 ) | ( (int)src_buf8[ i2 + 2 ] << 16 );
                    if( v & 0x800000 ) v |= 0xFF000000; //sign
                    data->pcm[ i ] = v;
		}
	    }
		break;
	    case SFMT_INT32:
		for( size_t i = 0; i < can_write * e->channels; i++ )
		    data->pcm[ i ] = ((int32_t*)src_buf)[ ptr * e->channels + i ];
		break;
	    case SFMT_FLOAT32: //float32 -> int24 (due to some problems with 32-bit FLAC playback)
		for( size_t i = 0; i < can_write * e->channels; i++ )
		{
		    float v = ((float*)src_buf)[ ptr * e->channels + i ];
		    int iv;
		    SMP_FLOAT32_TO_INT24( iv, v );
		    data->pcm[ i ] = iv;
		}
		break;
	    default: break;
	}
	FLAC__bool ok = FLAC__stream_encoder_process_interleaved( data->encoder, data->pcm, can_write );
	if( !ok ) break;
	ptr += can_write;
    }
    return ptr;
}

#endif //!NOFLACENC
#endif //!NOFLAC

#ifndef NOOGG
#ifndef NOOGGENC

#include <vorbis/vorbisenc.h>

struct vorbis_encoder
{
    ogg_stream_state os; //take physical pages, weld into a logical stream of packets
    ogg_page         og; //one Ogg bitstream page. Vorbis packets are inside
    ogg_packet       op; //one raw packet of data for decode
    vorbis_info vi; //struct that stores all the static vorbis bitstream settings
    vorbis_comment vc; //struct that stores all the user comments
    vorbis_dsp_state vd; //central working state for the packet->PCM decoder
    vorbis_block vb; //local working space for packet->PCM decode
};

static int vorbis_encoder_init( sfs_sound_encoder_data* e )
{
    int rv = -1;
    vorbis_encoder* data = nullptr;
    while( 1 )
    {
	e->format_encoder_data = SMEM_ZALLOC( sizeof( vorbis_encoder ) );
	data = (vorbis_encoder*)e->format_encoder_data;

	vorbis_info_init( &data->vi );
	float q = 0.4f; //~128kbps VBR
	if( e->compression_level )
	{
	    q = e->compression_level / 100.0f;
	    if( q < 0.1f ) q = 0.1f; //lowest quality, smallest file
	    if( q > 1.0f ) q = 1.0f; //highest quality, largest file
	}
	int rv2 = vorbis_encode_init_vbr( &data->vi, e->channels, e->rate, q );
	if( rv2 )
	{
	    rv = -2;
	    break;
	}

	//add a comment:
	vorbis_comment_init( &data->vc );
	vorbis_comment_add_tag( &data->vc, "ENCODER", "SunDog" );

	//set up the analysis state and auxiliary encoding storage:
	vorbis_analysis_init( &data->vd, &data->vi );
        vorbis_block_init( &data->vd, &data->vb );

        //set up packet->stream encoder:
        set_pseudo_random_seed( stime_ms() );
	ogg_stream_init( &data->os, pseudo_random() );

	{
	    ogg_packet header; //initial header with most of the codec setup parameters
	    ogg_packet header_comm; //comments
	    ogg_packet header_code; //bitstream codebook

	    vorbis_analysis_headerout( &data->vd, &data->vc, &header, &header_comm, &header_code );
	    ogg_stream_packetin( &data->os, &header );
	    ogg_stream_packetin( &data->os, &header_comm );
	    ogg_stream_packetin( &data->os, &header_code );

	    while( 1 )
	    {
    		int result = ogg_stream_flush( &data->os, &data->og );
    		if( result == 0 ) break;
    		sfs_write( data->og.header, 1, data->og.header_len, e->f );
    		sfs_write( data->og.body, 1, data->og.body_len, e->f );
	    }
	}

	rv = 0;
	break;
    }
    if( rv && data )
    {
    }
    return rv;
}

static void vorbis_encoder_blockout( sfs_sound_encoder_data* e )
{
    vorbis_encoder* data = (vorbis_encoder*)e->format_encoder_data;

    while( vorbis_analysis_blockout( &data->vd, &data->vb ) == 1 )
    {
	//analysis, assume we want to use bitrate management:
        vorbis_analysis( &data->vb, nullptr );
        vorbis_bitrate_addblock( &data->vb );

	while( vorbis_bitrate_flushpacket( &data->vd, &data->op ) )
	{
    	    //weld the packet into the bitstream:
    	    ogg_stream_packetin( &data->os, &data->op );

    	    //write out pages:
    	    bool eos = 0;
    	    while( !eos )
    	    {
        	int result = ogg_stream_pageout( &data->os, &data->og );
        	if( result == 0 ) break;
        	sfs_write( data->og.header, 1, data->og.header_len, e->f );
        	sfs_write( data->og.body, 1, data->og.body_len, e->f );

        	if( ogg_page_eos( &data->og ) ) eos=1;
    	    }
        }
    }
}

static void vorbis_encoder_deinit( sfs_sound_encoder_data* e )
{
    vorbis_encoder* data = (vorbis_encoder*)e->format_encoder_data;

    //Tell the library we're at end of stream so that it can handle the last frame and mark end of stream in the output properly:
    vorbis_analysis_wrote( &data->vd, 0 );

    vorbis_encoder_blockout( e );

    ogg_stream_clear( &data->os );
    vorbis_block_clear( &data->vb );
    vorbis_dsp_clear( &data->vd );
    vorbis_comment_clear( &data->vc );
    vorbis_info_clear( &data->vi );

    smem_free( e->format_encoder_data ); e->format_encoder_data = nullptr;
}

static size_t vorbis_encoder_write( sfs_sound_encoder_data* e, void* src_buf, size_t len )
{
    vorbis_encoder* data = (vorbis_encoder*)e->format_encoder_data;

    const size_t max_buf_size = 1024; //number of frames
    size_t src_ptr = 0; //frame counter
    while( src_ptr < len )
    {
	size_t len2 = len - src_ptr;
	if( len2 > max_buf_size ) len2 = max_buf_size;
	void* src_buf2 = (uint8_t*)src_buf + src_ptr * e->frame_size;
	float** buffer = vorbis_analysis_buffer( &data->vd, len2 );
	for( int c = 0; c < e->channels; c++ )
	{
	    float* dest = buffer[ c ];
	    switch( e->sample_format )
	    {
		case SFMT_INT8:
	    	    for( size_t i = 0, i2 = c; i < len2; i++, i2 += e->channels )
	    	        SMP_INT8_TO_FLOAT32( dest[ i ], ((int8_t*)src_buf2)[ i2 ] );
	    	    break;
		case SFMT_INT16:
	    	    for( size_t i = 0, i2 = c; i < len2; i++, i2 += e->channels )
	    		SMP_INT16_TO_FLOAT32( dest[ i ], ((int16_t*)src_buf2)[ i2 ] );
	    	    break;
		case SFMT_INT24:
		{
		    uint8_t* src_buf8 = (uint8_t*)src_buf2;
	    	    for( size_t i = 0, i2 = c * 3; i < len2; i++, i2 += e->channels * 3 )
	    	    {
			int v = src_buf8[ i2 + 0 ] | ( (int)src_buf8[ i2 + 1 ] << 8 ) | ( (int)src_buf8[ i2 + 2 ] << 16 );
                	if( v & 0x800000 ) v |= 0xFF000000; //sign
	    		SMP_INT24_TO_FLOAT32( dest[ i ], v );
	    	    }
	    	    break;
		}
		case SFMT_INT32:
	    	    for( size_t i = 0, i2 = c; i < len2; i++, i2 += e->channels )
	    		SMP_INT32_TO_FLOAT32( dest[ i ], ((int32_t*)src_buf2)[ i2 ] );
	    	    break;
		case SFMT_FLOAT32:
	    	    for( size_t i = 0, i2 = c; i < len2; i++, i2 += e->channels )
	    		dest[ i ] = ((float*)src_buf2)[ i2 ];
	    	    break;
		case SFMT_FLOAT64:
	    	    for( size_t i = 0, i2 = c; i < len2; i++, i2 += e->channels )
	    		dest[ i ] = ((double*)src_buf2)[ i2 ];
	    	    break;
		default: break;
	    }
	}
	vorbis_analysis_wrote( &data->vd, len2 );
	vorbis_encoder_blockout( e );
	src_ptr += len2;
    }

    return len;
}

#endif //!NOOGGENC
#endif //!NOOGG

int sfs_sound_encoder_init(
    sundog_engine* sd,
    const char* filename, sfs_file f,
    sfs_file_fmt file_format,
    sfs_sample_format sample_format,
    int rate, //Hz
    int channels,
    size_t len, //number of frames
    uint32_t flags,
    sfs_sound_encoder_data* e )
{
    int rv = -1;
    if( e->initialized ) return 0;

    e->sd = sd;
    e->f_close_req = 0;
    if( filename && f == 0 )
    {
	f = sfs_open( sd, filename, "wb" );
	e->f_close_req = 1;
    }
    if( f == 0 ) return -1;
    e->filename = SMEM_STRDUP( filename );
    e->f = f;
    e->file_format = file_format;
    e->sample_format = sample_format;
    e->rate = rate;
    e->channels = channels;
    e->len = len;
    e->sample_size = g_sfs_sample_format_sizes[ sample_format ];
    e->frame_size = e->sample_size * e->channels;

    while( 1 )
    {
	e->format_encoder_data = nullptr;
	e->init = nullptr;
	switch( file_format )
	{
	    case SFS_FILE_FMT_UNKNOWN:
		e->init = rawaudio_encoder_init;
		e->deinit = rawaudio_encoder_deinit;
		e->write = rawaudio_encoder_write;
		break;
	    case SFS_FILE_FMT_WAVE:
		e->init = wave_encoder_init;
		e->deinit = wave_encoder_deinit;
		e->write = wave_encoder_write;
		break;
#if !defined(NOFLAC) && !defined(NOFLACENC)
	    case SFS_FILE_FMT_FLAC:
		e->init = flac_encoder_init;
		e->deinit = flac_encoder_deinit;
		e->write = flac_encoder_write;
		break;
#endif
#if !defined(NOOGG) && !defined(NOOGGENC)
	    case SFS_FILE_FMT_OGG:
		e->init = vorbis_encoder_init;
		e->deinit = vorbis_encoder_deinit;
		e->write = vorbis_encoder_write;
		break;
#endif
	    default:
		break;
	}
	if( !e->init ) { rv = -100; break; }
	rv = e->init( e );
	if( rv ) break;
	e->initialized = true;
    	rv = 0;
	break;
    }

    if( rv )
    {
	if( e->f_close_req ) sfs_close( e->f );
    	smem_free( e->format_encoder_data ); e->format_encoder_data = nullptr;
    }

    return rv;
}

void sfs_sound_encoder_deinit( sfs_sound_encoder_data* e )
{
    if( !e->initialized ) return;

    e->deinit( e );

    if( e->f_close_req ) sfs_close( e->f );
    smem_free( e->format_encoder_data ); e->format_encoder_data = nullptr;
    smem_free( e->filename ); e->filename = nullptr;

    e->initialized = false;
}

size_t sfs_sound_encoder_write( sfs_sound_encoder_data* e, void* src_buf, size_t len ) //retval: =len if success;
{
    if( !e->initialized ) return 0;
    return e->write( e, src_buf, len );
}

#endif //NOAUDIOFORMATS
