// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>

#include "pattern.h"

#include "xml.h"

#include <sstream>

bool CompareEventLocations(std::pair<double,int> lhs, std::pair<double,int> rhs){
    return ( lhs.first < rhs.first ) ||
            ( lhs.first <= rhs.first && lhs.second < rhs.second );
}

namespace psycle { namespace core {

Pattern::Pattern() 
    : id_(-1),
      lines_( CompareEventLocations )
{
    TimeSignature timeSig;
	timeSig.setCount(4);
	timeSignatures_.push_back(timeSig);

}

// Explicit copy constructor needed because boost::signal is noncopyable
Pattern::Pattern(Pattern const& other):   
	name_(other.name_),
	category_(other.category_),
	timeSignatures_(other.timeSignatures_),
    zeroTime(other.zeroTime),
    id_(-1)
{
	lines_ = other.lines_;
}

Pattern::~Pattern() {
	wasDeleted(this);
}

Pattern& Pattern::operator=(const Pattern& rhs)
{
	if (this == &rhs) return *this; 
	lines_ = rhs.lines_;
	name_ = rhs.name_;
	category_ = rhs.category_;
	timeSignatures_ = rhs.timeSignatures_;
	return *this;
}


void Pattern::Clear() {
	lines_.clear();
	TimeSignature timeSig;
	timeSig.setCount(4);
	timeSignatures_.push_back(timeSig);
}

void Pattern::addBar( const TimeSignature & signature ) {
	if ( timeSignatures_.size() > 0 ) {
		TimeSignature & last = timeSignatures_.back();
		if (last.numerator()   != signature.numerator() ||
				last.denominator() != signature.denominator() )
		{
			timeSignatures_.push_back(signature);
		} else
		timeSignatures_.back().incCount();
	} else
			timeSignatures_.push_back(signature);
}

void Pattern::removeBar( float /*pos*/ ) {
#if 0 ///\todo
	float searchPos = 0;
	std::vector<TimeSignature>::iterator it = timeSignatures_.begin();
	for (; it < timeSignatures_.end(); it++) {
		TimeSignature & timeSignature = *it;
		float oldPos = searchPos;
		searchPos += timeSignature.beats();
		if (searchPos > pos) {
				// found our bar
				float beginPos = searchPos;
				float endPos   = oldPos;

				Pattern::iterator startIt = lower_bound(pos);
				Pattern::iterator endIt   = upper_bound(pos);

				if (startIt != end() && endIt != end() ) {
						erase(startIt, endIt);
				}
				if (timeSignature.count() > 1) {
					timeSignature.setCount(timeSignature.count()-1);
				} else {
					timeSignatures_.erase(it);
				}
				break;
		}
	}
#endif
}

const TimeSignature & Pattern::playPosTimeSignature(double pos) const
{
		double bts = 0;
		std::vector<TimeSignature>::const_iterator it = timeSignatures_.begin();
		for (; it < timeSignatures_.end(); it++)
		{
			const TimeSignature & timeSignature = *it;
			bts += timeSignature.beats();
			if (pos < bts) return timeSignature;
		}
		return zeroTime;
}

bool Pattern::barStart( double pos , TimeSignature & signature ) const
{
	if (pos - ((int) pos) != 0) return false;
	int bts = 0;
	std::vector<TimeSignature>::const_iterator it = timeSignatures_.begin();
	for (; it < timeSignatures_.end(); it++)
	{
		const TimeSignature & timeSignature = *it;
		for (int count = 0; count < timeSignature.count(); count++) {
			if (bts == pos) {
				signature = timeSignature;
				return true;
			}
			bts += timeSignature.numerator();
		}
	}
	return false;
}

void Pattern::clearBars( )
{
	timeSignatures_.clear();
}

float Pattern::beats( ) const
{
	float bts = 0;
	std::vector<TimeSignature>::const_iterator it = timeSignatures_.begin();
	for (; it < timeSignatures_.end(); it++)
	{
		const TimeSignature & signature = *it;
		bts += signature.beats();
	}
	return bts;
}

void Pattern::setName( const std::string & name )
{
	name_ = name;
}

const std::string& Pattern::name( ) const
{
	return name_;
}

void Pattern::setCategory(const std::string& category)
{
	category_ = category;
}

void Pattern::scaleBlock(int /*left*/, int /*right*/, double /*top*/, double /*bottom*/, float /*factor*/) {
#if 0 ///\todo
	double length = bottom - top;
	
	if(factor>1) //expanding-- iterate backwards
	{
		reverse_iterator rLineIt = (reverse_iterator)(lower_bound(bottom));
	
		// use > instead of >= -- lines at exactly top don't need to be moved
		for(; rLineIt != rend() && rLineIt->first >top; ++rLineIt )
		{
			PatternLine & line = rLineIt->second;
			double newpos = top + (rLineIt->first-top) * factor;
	
			for( std::map<int, PatternEvent>::iterator entryIt = line.notes().lower_bound(left)
				; entryIt != line.notes().end() && entryIt->first < right
				; )
			{
				if( newpos < beats() )
				{
					(*this)[newpos].notes()[entryIt->first] = entryIt->second;
				} 
				line.notes().erase(entryIt++);
			}
		}
	}
	else //contracting -- iterate forwards
	{
		//use upper_bound, not lower_bound.. lines at exactly top don't need to be moved
		iterator lineIt = upper_bound(top);
		
		for(; lineIt != end() && lineIt->first < bottom; ++lineIt )
		{
			PatternLine & line = lineIt->second;
			double newpos = top + (lineIt->first-top) * factor;
			
			for( std::map<int, PatternEvent>::iterator entryIt = line.notes().lower_bound(left)
				; entryIt != line.notes().end() && entryIt->first < right
				; )
			{
				if( newpos < beats() )
				{
					(*this)[newpos].notes()[entryIt->first] = entryIt->second;
				}
				line.notes().erase(entryIt++);
			}
		}
	}
#endif
}

std::vector<TimeSignature>& Pattern::timeSignatures() {
	return timeSignatures_;
}

const std::vector<TimeSignature>& Pattern::timeSignatures() const {
	return timeSignatures_;
}

std::string Pattern::toXml( ) const {
	std::ostringstream xml;
#if 0 ///\todo
	xml << "<pattern name='" << replaceIllegalXmlChr(name()) << "' zoom='" << beatZoom() << std::hex << "' id='" << id() << std::hex << "'>" << std::endl;
	std::vector<TimeSignature>::const_iterator it = timeSignatures_.begin();
	for ( ; it < timeSignatures_.end(); it++) {
		const TimeSignature & sign = *it;
		xml << "<sign ";
		if (sign.ownerDefined()) {
			xml << "free='" << sign.beats() <<"'";
		} else {
			xml << "num='" << sign.numerator() << "' ";
			xml << "denom='" << sign.numerator() << "' ";
			xml << "count='" << sign.count() << "' ";
		}
		xml << "/>" << std::endl;
	}

	for ( const_iterator it = begin() ; it != end() ; it++ ) {
		double beatPos = it->first;
		const PatternLine & line = it->second;
		xml << line.toXml( static_cast<float>(beatPos) );
	}
	xml << "</pattern>" << std::endl;
#endif
	return xml.str();
}

Pattern Pattern::Clone(double from, double to, int start_track, int end_track) {
	Pattern clone_pattern;
	Pattern::iterator it(lower_bound(from));
	while (it != end()) {
        PatternEvent& pattern_event = it->second;
        if (it->first.first > to) break;
        if (it->first.second >= start_track &&
            it->first.second <= end_track) {
                clone_pattern.insert( it->first.first - from, it->first.second - start_track, pattern_event );
        }
		++it;
	}
	return clone_pattern;
}

void Pattern::insert(const Pattern& src_pattern, double to, int to_track) {
    for( auto it = src_pattern.begin(); it != src_pattern.end(); ++it) {
        const PatternEvent& line = it->second;
        insert(it->first.first+to, it->first.second + to_track, line );
	}
}
const psycle::core::PatternEvent& Pattern::getPatternEvent(int line, int track){

    static psycle::core::PatternEvent dummyEvent;
    auto itr = lines_.find({line,track});
    if( itr == lines_.end() ) return dummyEvent;
    else return itr->second;
}



void Pattern::erase(double from, double to, int start_track, int end_track) {
	Pattern::iterator it(lower_bound(from));
	while (it != end()) {
        if ( it->first.first >= to ) break;
        if( it->first.second <= end_track &&
            it->first.second >= start_track ) {
            it = erase(it);
        } else ++it;
	}
}

void Pattern::Transpose(int delta, double from, double to, int start_track, int end_track)
{
	Pattern::iterator it(lower_bound(from));
	while (it != end()) {
		PatternEvent& pattern_ev = it->second;
        if ( it->first.first >= to ) break;
        if(
            it->first.second <= end_track &&
            it->first.second >= start_track
        ) {
			int note = pattern_ev.note();
			if (note < 120) {
				pattern_ev.setNote(std::min(std::max(0, note + delta), 119));
            }
        }
		++it;
	}
}

void Pattern::ChangeInst(int new_inst, double from, double to, int start_track, int end_track)
{
	Pattern::iterator it(lower_bound(from));
	while (it != end()) {
		PatternEvent& pattern_ev = it->second;
        if ( it->first.first >= to ) break;
        if(
            it->first.second<= end_track &&
            it->first.second >= start_track &&
            pattern_ev.machine() != 255
        ) {
            pattern_ev.setInstrument(std::min(std::max(0, new_inst), 255));
        }
		++it;
	}
}

void Pattern::ChangeMac(int new_mac, double from, double to, int start_track, int end_track)
{
	Pattern::iterator it(lower_bound(from));
	while (it != end()) {
		PatternEvent& pattern_ev = it->second;
        if ( it->first.first >= to ) break;
        if(
            it->first.second <= end_track &&
            it->first.second >= start_track &&
            pattern_ev.machine() != 255
        ) {
            pattern_ev.setMachine(std::min(std::max(0, new_mac), 255));
        }
		++it;
	}
}

}}
