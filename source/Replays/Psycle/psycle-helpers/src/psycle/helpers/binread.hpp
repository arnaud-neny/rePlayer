// This program is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net
// based on sondar binread, copryright 2007 Stefan Nattkemper

#pragma once

#include <universalis.hpp>
#include <istream>

namespace psycle { namespace helpers {

using namespace universalis::stdlib;

class BinRead {
	public:

		enum BinPlatform { byte4LE, byte4BE, byte8LE, byte8BE };

		BinRead( std::istream & in );
		~BinRead();

		int16_t readInt2LE();
		uint16_t readUInt2LE();
	
		int16_t readInt2BE();
		uint16_t readUInt2BE();
		
		int32_t readInt4LE(); 
		uint32_t readUInt4LE();

		int32_t readInt4BE();
		uint32_t readUInt4BE();
			
		void readUIntArray4LE( uint32_t data[], int count );
		void readIntArray4LE( int32_t data[], int count );

		void read( char * data, std::streamsize const & bytes );

		bool eof() const;
		bool bad() const;

		//BinPlatform platform() const;

	private:

		std::istream & in_;

		#if 0
			BinPlatform platform_;
			BinPlatform testPlatform();
			uint32_t swap4( uint32_t value );
		#endif
};

}}
