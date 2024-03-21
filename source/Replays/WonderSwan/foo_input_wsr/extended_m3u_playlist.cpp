// Game_Music_Emu $vers.http://www.slack.net/~ant/

/* Copyright (C) 2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */


//WARNING
//This source has been modified by the author of "foo_npnez", so please do NOT contact the original author for this source.
//Also, please do NOT replace this source with M3u_Playlist in the Game_Music_Emu library.

#pragma warning(disable : 4996)

#include "extended_m3u_playlist.h"
#include <algorithm>

extended_m3u_playlist::exm3u_err_t extended_m3u_playlist::load(const void* p_file_data, size_t file_size)
{
	data_size = file_size;
	
	data.resize(data_size + 1);
	if (data.size() <= data_size)
	{
		return exm3u_error;
	}

	std::copy_n(static_cast<const char*>(p_file_data), file_size, data.data());

	int err = parse();

	if (err != 0)
	{
		clear();
		return exm3u_error;
	}
	return exm3u_success;
}

static char* skip_white(char* in)
{
	// skip white space etc. exclude null. 
	while (unsigned (*in - 1) <= ' ' - 1)
		in++;
	return in;
}

inline unsigned from_dec(unsigned n)
{
	return n - '0';
}

static char* parse_filename(char* in, extended_m3u_playlist::entry_t& entry)
{
	entry.file = in;
	entry.type = "";
	char* out = in;
	while (1)
	{
		int c = *in;
		if (!c) break;
		in++;

		if (c == ',') // commas in filename
		{
			char* p = skip_white(in);
			if (*p == '$' || from_dec(*p) <= 9)
			{
				in = p;
				break;
			}
		}

		if (c == ':' && in[0] == ':' && in[1] && in[2] != ',') // ::type suffix
		{
			entry.type = ++in;
			while ((c = *in) != 0 && c != ',')
				in++;
			if (c == ',')
			{
				*in++ = '\0'; // terminate type
				in = skip_white(in);
			}
			break;
		}

		if (c == '\\') // \ prefix for special characters
		{
			c = *in;
			if (!c) break;
			in++;
		}
		*out++ = (char)c;
	}
	*out = '\0'; // terminate string
	return in;
}

static char* next_field(char* in, int* result)
{
	while (1)
	{
		in = skip_white(in);

		if (*in == '\0')
			break;

		if (*in == ',')
		{
			in++;
			break;
		}

		*result = 1;
		in++;
	}
	return skip_white(in);
}

static char* parse_int_(char* in, int* out)
{
	int n = 0;
	while (1)
	{
		unsigned d = from_dec(*in);
		if (d > 9)
			break;
		in++;
		n = n * 10 + d;
		*out = n;
	}
	return in;
}

static char* parse_int(char* in, int* out, int* result)
{
	return next_field(parse_int_(in, out), result);
}

// Returns 16 or greater if not hex
inline int from_hex_char(int h)
{
	//in ascii, '0' is 0x30.
	h -= 0x30;
	if ((unsigned) h > 9)
		h = ((h - 0x11) & 0xDF) + 10;
	return h;
}

static char* parse_track(char* in, extended_m3u_playlist::entry_t& entry, int* result)
{
	if (*in == '$')
	{
		in++;
		int n = 0;
		while (1)
		{
			int h = from_hex_char(*in);
			if (h > 15)
				break;
			in++;
			n = n * 16 + h;
			entry.track = n;
		}
	}
	else
	{
		in = parse_int_(in, &entry.track);
		if (entry.track >= 0)
			entry.decimal_track = 1;
	}
	return next_field(in, result);
}

static char* parse_time_(char* in, int* out)
{
	*out = -1;
	int n = -1;
	in = parse_int_(in, &n);
	if (n >= 0)
	{
		*out = n;
		while (*in == ':')
		{
			n = -1;
			in = parse_int_(in + 1, &n);
			if (n >= 0)
				*out = *out * 60 + n;
		}
		*out *= 1000;
		if (*in == '.')
		{
			n = -1;
			in = parse_int_(in + 1, &n);
			if (n >= 0)
				*out = *out + n;
		}
		else if (*in == '\'')
		{
			n = -1;
			in = parse_int_(in + 1, &n);
			if (n >= 0)
				*out = *out + n * 10;
		}
	}
	return in;
}

static char* parse_time(char* in, int* out, int* result)
{
	return next_field(parse_time_(in, out), result);
}

static char* parse_name(char* in)
{
	char* out = in;
	while (1)
	{
		int c = *in;
		if (!c) break;
		in++;

		if (c == ',') // commas in string
		{
			char* p = skip_white(in);
			if (*p == ',' || *p == '-' || from_dec(*p) <= 9)
			{
				in = p;
				break;
			}
		}

		if (c == '\\') // \ prefix for special characters
		{
			c = *in;
			if (!c) break;
			in++;
		}
		*out++ = (char)c;
	}
	*out = '\0'; // terminate string
	return in;
}

static int parse_line(char* in, extended_m3u_playlist::entry_t& entry)
{
	int result = 0;

	//file
	entry.file = in;
	entry.type = "";
	in = parse_filename(in, entry);

	//track
	entry.track = -1;
	entry.decimal_track = 0;
	in = parse_track(in, entry, &result);

	//name
	entry.name = in;
	in = parse_name(in);

	// time
	entry.length = -1;
	in = parse_time(in, &entry.length, &result);

	// loop
	entry.intro = -1;
	entry.loop = -1;
	if (*in == '-')
	{
		entry.loop = entry.length;
		entry.intro = 0;
		in++;
	}
	else
	{
		in = parse_time_(in, &entry.loop);
		if (entry.loop >= 0)
		{
			if (entry.length != -1) entry.intro = entry.length - entry.loop;
			if (*in == '-') // trailing '-' means that intro length was specified 
			{
				in++;
				entry.intro = entry.loop;
				if (entry.length != -1) entry.loop = entry.length - entry.intro;
				else entry.loop = -1;
			}
		}
	}
	in = next_field(in, &result);

	// fade
	entry.fade = -1;
	in = parse_time(in, &entry.fade, &result);

	// repeat
	entry.repeat = -1;
	in = parse_int(in, &entry.repeat, &result);

	return result;
}

static void parse_comment(char* in, extended_m3u_playlist::info_t& info, bool first, const char* & first_text)
{
	in = skip_white(in + 1);
	const char* field = in;
	if (*field != '@')
	while (*in && *in!= ':')
	{
		in++;
	}

	if (*in == ':')
	{
		const char* text = skip_white(in + 1);
		if (*text)
		{
			*in = '\0';
			     if (!strcmp("Composer"   , field)) info.composer    = text;
			else if (!strcmp("Date"       , field)) info.date        = text;
			else if (!strcmp("AlbumArtist", field)) info.albumartist = text;
			else if (!strcmp("Genre"      , field)) info.genre       = text;
			else if (!strcmp("Engineer"   , field)) info.engineer    = text;
			else if (!strcmp("Ripping"    , field)) info.ripping     = text;
			else if (!strcmp("Tagging"    , field)) info.tagging     = text;
			else if (!strcmp("Game"       , field)) info.title       = text;
			else if (!strcmp("Artist"     , field)) info.artist      = text;
			else if (!strcmp("Copyright"  , field)) info.copyright   = text;
			else if (!strcmp("Comment"    , field)) info.comment     = text;
			else if (!strcmp("System"     , field)) info.system      = text;
			else
				text = 0;
			if (text)
				return;
			*in = ':';
		}
	}
	else if (*field == '@')
	{
		++field;
		in = (char*)field;
		while ( *in != '\0' && *in > ' ')
			in++;
		const char* text = skip_white(in);
		if (*text)
		{
			char saved = *in;
			*in = '\0';
			     if (!strcmp("TITLE"      , field)) info.title       = text;
			else if (!strcmp("ARTIST"     , field)) info.artist      = text;
			else if (!strcmp("ALBUMARTIST", field)) info.albumartist = text;
			else if (!strcmp("DATE"       , field)) info.date        = text;
			else if (!strcmp("GENRE"      , field)) info.genre       = text;
			else if (!strcmp("COMPOSER"   , field)) info.composer    = text;
			else if (!strcmp("SEQUENCER"  , field)) info.sequencer   = text;
			else if (!strcmp("ENGINEER"   , field)) info.engineer    = text;
			else if (!strcmp("RIPPER"     , field)) info.ripping     = text;
			else if (!strcmp("TAGGER"     , field)) info.tagging     = text;
			else if (!strcmp("COPYRIGHT"  , field)) info.copyright   = text;
			else if (!strcmp("COMMENT"    , field)) info.comment     = text;
			else if (!strcmp("SYSTEM"     , field)) info.system      = text;
			else
				text = 0;
			if (text)
			{
				//last_comment_value = (char*)text;
				return;
			}
			*in = saved;
		}
	}
	/*else if (last_comment_value)
	{
		size_t len = strlen(last_comment_value);
		last_comment_value[len] = ',';
		last_comment_value[len + 1] = ' ';
		size_t field_len = strlen(field);
		memmove(last_comment_value + len + 2, field, field_len);
		last_comment_value[len + 2 + field_len] = 0;
		return;
	}*/

	if (first)
		first_text = field;

}

int extended_m3u_playlist::parse()
{
	info_.title = "";
	info_.artist = "";
	info_.albumartist = "";
	info_.date = "";
	info_.genre = "";
	info_.composer = "";
	info_.sequencer = "";
	info_.engineer = "";
	info_.ripping = "";
	info_.tagging = "";
	info_.copyright = "";
	info_.comment = "";
	info_.system = "";

	int const CR = 13;
	int const LF = 10;

	data[data_size] = LF;		// terminate input

	first_error_ = 0;
	bool first_comment = true;
	const char* first_comment_text = "";
	int line = 0;
	int count = 0;
	if (data.size() <= 1)
		return -1;
	char* in = static_cast<char*>(&data[0]);
	char* dataend = static_cast<char*>(&data[0] + data.size());
	//char* last_comment_value = 0;

	//Check BOM for UTF-8
	if (data_size > 3 && in[0] == -17 && in[1] == -69 && in[2] == -65)
		in += 3;
	
	while ( in < dataend )
	{
		
		// find end of line and terminate it
		line++;
		char* begin = in;

		while (*in != CR && *in != LF)
		{
			if ( *in == '\0')
				return -1;
			in++;
		}
		if (in[0] == CR && in[1] == LF) // treat CR,LF as a single line
			*in++ = '\0';
		*in++ = '\0';

		//parse line
		if (*begin == '#')
		{
			parse_comment(begin, info_, first_comment, first_comment_text);
			first_comment = false;
		}
		else if (*begin != '\0')
		{
			
			if ((int)entries.size() <= count)
			{
				entries.resize(count * 2 + 64);
				if ((int)entries.size() < count)
					return -1;
			}
			
			if (!parse_line(begin, entries[count]))
				count++;
			else if (!first_error_)
				first_error_ = line;
			first_comment = false;
		}
		//else last_comment_value = 0;
	}
	if (count <= 0)
		return -1;

	// Treat first comment as title only if another field is also specified
	if (!info_.title[0] && (info_.artist[0] | info_.composer[0] | info_.date[0] | info_.engineer[0] | info_.ripping[0] | info_.sequencer[0] | info_.tagging[0] | info_.copyright[0] | info_.system[0]))
		info_.title = first_comment_text;

	entries.resize(count);
	if ((int)entries.size() < count)
		return -1;
	else 
		return 0;
}