// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "sequence.h"

#include "commands.h"
#include <universalis/os/loggers.hpp>
#include <sstream>
#include <limits>
#include <algorithm>

namespace psycle { namespace core {

namespace loggers = universalis::os::loggers;

/**************************************************************************/
// SequenceEntry
// pattern Entry contains one ptr to a Pattern and the tickPosition for the absolute Sequencer pos

SequenceEntry::SequenceEntry(SequenceLine & line, Pattern & pattern)
:
	line_(&line),
	pattern_(&pattern),
	startPos_(0),
	endPos_(pattern.beats()),
	transpose_(0)
{}

SequenceEntry::~SequenceEntry() {
	//wasDeleted(*this);
}

void SequenceEntry::setPattern(Pattern & pattern) {
	pattern_ = &pattern;
	startPos_ = 0;
	endPos_ = pattern.beats();
}

double SequenceEntry::tickPosition() const {
	SequenceLine::iterator iter = line_->begin();
	for(; iter!= line_->end(); ++iter) {
		if(iter->second == this)
		break;
	}
	if(iter != line_->end()) {
		return iter->first;
	}
	return 0;
}

double SequenceEntry::tickEndPosition() const {
	return tickPosition() + patternBeats();
}

std::string SequenceEntry::toXml(double pos) const {
	std::ostringstream xml;
	xml << "<seqentry pos='" << pos << "' patid='" << pattern().id() << std::hex << "' "  << "start='" << startPos() << "' end='" << endPos() << "' " << "transpose='" << transpose() << std::hex << "' />" << std::endl;
	return xml.str();
}

void SequenceEntry::setSequenceLine(SequenceLine & newLine) {
	line_->moveEntryToNewLine(*this, newLine);
	line_ = &newLine;
}

/**************************************************************************/
// SequenceLine
// represents one track/line in the sequencer

SequenceLine::~SequenceLine() {
	for(iterator it = begin(); it != end(); ++it) delete it->second;
	//wasDeleted(*this);
}

SequenceEntry & SequenceLine::createEntry(Pattern & pattern, double position) {
	SequenceEntry & entry = *new SequenceEntry(*this, pattern);
	line_.insert(std::pair<double, SequenceEntry*>(position, &entry));
	return entry;
}

void SequenceLine::insertEntry(SequenceEntry & entry) {
	line_.insert(std::pair<double, SequenceEntry*>(entry.startPos(), &entry));
}

void SequenceLine::insertEntryAndMoveRest(SequenceEntry & entry, double pos) {
	std::multimap<double, SequenceEntry*> old_line_ = line_;
	line_.clear();
	std::multimap<double, SequenceEntry*>::iterator it = old_line_.begin();
	bool inserted = false;
	for ( ; it != old_line_.end(); ++it ) {
		if ( it->first < pos ) {
			line_.insert(std::pair<double, SequenceEntry*>(it->first, it->second));
		} else  {
			if ( !inserted ) {
				line_.insert(std::pair<double, SequenceEntry*>(pos, &entry));
				inserted = true;
			}
			double move = entry.patternBeats();
			line_.insert(std::pair<double, SequenceEntry*>(it->first + move, it->second));
		}
	}
	if ( !inserted ) {
		line_.insert(std::pair<double, SequenceEntry*>(pos, &entry));
	}
}

void SequenceLine::moveEntries(SequenceEntry & start_entry, double delta) {
	std::multimap<double, SequenceEntry*> old_line_ = line_;
	line_.clear();
	bool inserted = false;
	iterator it = old_line_.begin();
	for ( ; it != old_line_.end(); ++it ) {
		if (!inserted && it->second != &start_entry) {
			line_.insert(std::pair<double, SequenceEntry*>(it->first, it->second));
		} else {
			line_.insert(std::pair<double, SequenceEntry*>(it->first + delta, it->second));
			inserted = true;
		}
	}
}

SequenceLine::iterator SequenceLine::find(SequenceEntry & entry) {
	iterator it = line_.begin();
	for ( ; it != line_.end(); ++it ) {
		if (it->second == &entry)
			return it;
	}
	return line_.end();
}

void SequenceLine::removeSpaces()
{
	iterator it = begin();
	iterator next_it;
	double pos = 0.0;
	while ( it != end() ) {
		double diff = it->first - pos;
		pos = it->second->tickEndPosition() - diff;
		if (diff > 0) {
			next_it = it;
			++next_it;
			MoveEntry(*it->second, it->first - diff);
			it = next_it;
		} else {
			++it;
		}
	}
}

void SequenceLine::moveEntryToNewLine(SequenceEntry & entry, SequenceLine & newLine) {
	newLine.insertEntry(entry);
	iterator it = begin();
	for(; it!= end(); ++it) {
		if (it->second == &entry)
		break;
	}
	line_.erase(it); // Removes entry from this SequenceLine, but doesn't delete it.
}

void SequenceLine::removePatternEntries(Pattern & pattern ) {
	iterator it = begin();
	while(it != end()) {
		SequenceEntry & entry = *it->second;
		if(&entry.pattern() == &pattern) {
			delete &entry;
			line_.erase(it);
		}
		++it;
	}
}

double SequenceLine::tickLength() const {
	if (size() > 0) {
		return rbegin()->first + rbegin()->second->patternBeats();
	} else {
		return 0;
	}
}

void SequenceLine::MoveEntry(SequenceEntry & entry, double newpos) {
	iterator iter = begin();
	for(; iter!= end(); ++iter) {
		if(iter->second == &entry)
		break;
	}
	assert("entry to move found" && iter != end());
	erase(iter);
	line_.insert(std::pair<double, SequenceEntry*>(newpos, &entry));
}

void SequenceLine::removeEntry(SequenceEntry & entry) {
	iterator iter = begin();
	for(; iter!= end(); ++iter) {
		if(iter->second == &entry)
			break;
	}
	assert("entry to remove found" && iter != end());
	line_.erase(iter);
	delete &entry;
}

void SequenceLine::clear() {
	iterator it = begin();
	for ( ; it != end(); ++it)
		delete it->second;
	line_.clear();
}

bool SequenceLine::isPatternUsed(Pattern & pattern) const {
	const_iterator it = begin();
	for ( ; it != end(); ++it)
		if (&it->second->pattern() == &pattern)
			return true;
	return false;
}

std::string SequenceLine::toXml() const {
	std::ostringstream xml;
	xml << "<seqline>" << std::endl;
	for(const_iterator it = begin(); it != end(); ++it) {
		SequenceEntry* entry = it->second;
		xml << entry->toXml( it-> first);
	}
	xml << "</seqline>" << std::endl;
	return xml.str();
}

/**************************************************************************/
// Sequence

Sequence::Sequence() {
	setNumTracks(16);
	create_master_pattern();
}

Sequence::~Sequence() {
	for(iterator it = begin(); it != end(); ++it) delete *it;
	for(patterns_type::iterator it = patterns_.begin(); it != patterns_.end(); ++it) delete *it;
}

void Sequence::create_master_pattern() {
	// make pattern length endless
	Pattern & master_pattern = *new Pattern();
	master_pattern.timeSignatures().clear();
	master_pattern.timeSignatures().push_back(TimeSignature(std::numeric_limits<float>::max()));
	master_pattern.setName("master");
	master_pattern.setID(-1);
	Add(master_pattern);
	// create global master line with an entry that keeps the master pattern
	SequenceLine & master_line = createNewLine();
	master_line.createEntry(master_pattern, 0);
	master_pattern_ = &master_pattern;
}

SequenceLine & Sequence::createNewLine() {
	SequenceLine & line = *new SequenceLine(*this);
	lines_.push_back(&line);
	newLineCreated(line);
	return line;
}

SequenceLine & Sequence::insertNewLine(SequenceLine & selectedLine) {
	iterator it = std::find(begin(), end(), &selectedLine);
	assert("selected line found" && it != end());
	SequenceLine & line = *new SequenceLine(*this);
	lines_.insert(it, &line);
	newLineInserted(line, selectedLine);
	return line;
}

Pattern* Sequence::FindPattern(int id) {
	patterns_type::iterator it = patterns_.begin();
	for ( ; it != patterns_.end(); ++it) {
		if ((*it)->id() == id)
			return *it;
	}
	return 0;
}

void Sequence::removeLine(SequenceLine & line) {
	iterator it = std::find(begin(), end(), &line);
	assert("line to remove found" && it != end());
	lines_.erase(it);
	lineRemoved(line);
	delete &line;
}

/// returns the PatternEvents that are active in the range [start, start+length).
///\param start start time in beats since playback begin.
///\param length length of the range. start+length is the last position (non inclusive)
///\return events : A vector of sorted events
void Sequence::GetEventsInRange(double start, double length, std::vector<PatternEvent*>& events)  {
	events_.clear();
	int seqlineidx = 1; // index zero reserved for live events (midi in, or pc keyb)
	// Iterate over each timeline of the sequence
	for(iterator seqIt = begin(); seqIt != end(); ++seqIt) {
		SequenceLine & line = **seqIt;
		// locate the "sequenceEntry"s which starts nearer to "start + length"
		SequenceLine::reverse_iterator lineIt(line.lower_bound(start + length));
		// and iterate backwards to include any other that is inside the range [start,start+length)
		// (The UI won't allow more than one pattern for the same range in the same timeline, but 
		// this was left open in the player code)
//		bool worked = false;
		for(; lineIt != line.rend() && lineIt->first + lineIt->second->patternBeats() >= start; ++lineIt) {
			// take the pattern,
			Pattern & pat = lineIt->second->pattern();
//			worked = true;
			double entryStart = lineIt->first;
			double entryStartOffset  = lineIt->second->startPos();
			double entryEndOffset  = lineIt->second->endPos();
			double relativeStart = start - entryStart + entryStartOffset;
	
			// and iterate through the lines that are inside the range
			for(Pattern::iterator
				patIt  = pat.lower_bound(std::min(relativeStart, entryEndOffset)),
				patEnd = pat.lower_bound(std::min(relativeStart + length, entryEndOffset));
				patIt != patEnd; ++patIt
			) {
                PatternEvent & ev = patIt->second;
//				ev.set_sequence(seqlineidx);
                ev.set_time_offset(entryStart + patIt->first.first - entryStartOffset);
				CollectEvent(ev);
				#if 0 ///\todo
					// Since the player needs to differentiate between tracks of different SequenceEntrys,
					// we generate a temporary PatternLine with a special column value.
					PatternLine tmpline;
					for(std::map<int, PatternEvent>::iterator lineIt = line.notes().begin();
						lineIt != line.notes().end(); ++lineIt)
					{
						tmpline.notes()[lineIt->first] = lineIt->second;
						tmpline.notes()[lineIt->first].setNote(
							tmpline.notes()[lineIt->first].note() +
							lineIt->second->transpose()
						);
					}

					// finally add the PatternLine to the event map. The beat position is in absolute values from the playback start.
					tmpline.setSequenceTrack(seqlineidx);
					tmpline.tweaks() = line.tweaks();
					events.insert(Pattern::value_type(entryStart + patIt->first - entryStartOffset, tmpline));
				#endif
			}
		}
		++seqlineidx;
	}
	GetOrderedEvents(events);
	// assert test if sorted correctly
	std::vector<PatternEvent*>::iterator it = events.begin();
	double old_pos = 0;
	bool has_note = false;
	for ( ; it != events.end(); ++it ) {
		PatternEvent & cmd = **it;
		double pos = cmd.time_offset();
		if(old_pos != pos) has_note = true;
		old_pos = pos;
		switch(cmd.note()) {
			case notetypes::tweak:
			case notetypes::tweak_slide:
				//assert(!has_note);
				if(loggers::warning() && has_note) loggers::warning()("has note", UNIVERSALIS__COMPILER__LOCATION);
				break;
			default:
				has_note = true;
		}
	}
}

SequenceEntry* Sequence::GetEntryOnPosition(SequenceLine & line, double pos) {
	for (SequenceLine::iterator it = line.begin(); it != line.end(); ++it) {
		if(
			pos >= it->second->tickPosition() &&
			pos < it->second->tickEndPosition()
		) return it->second;
	}
	return 0;
}

int Sequence::priority(const PatternEvent& cmd, int /*count*/) const {
	int p = 8;
	if(cmd.IsGlobal()) p = 0;
	else if(cmd.note() == notetypes::tweak_slide) p = 1;
	else if(cmd.note() == notetypes::tweak) p = 2;
	else p = 3;
	return p;
}

void Sequence::CollectEvent(const PatternEvent & command) {
	assert(command.time_offset() >= 0);
	double delta_frames = command.time_offset();
	std::multimap<double, std::multimap<int, PatternEvent > >::iterator it = events_.find(delta_frames);
	if(it == events_.end()) {
		std::multimap<int, PatternEvent> map;
		map.insert(std::pair<int, PatternEvent>(priority(command, 0), command))->second.set_time_offset(delta_frames);
		events_.insert(std::pair<double, std::multimap<int, PatternEvent> >(delta_frames, map));
	} else {
		std::multimap<int, PatternEvent> & map = it->second;
		map.insert(std::pair<int, PatternEvent>(priority(command, map.size()), command))->second.set_time_offset(delta_frames);
	}
}

void Sequence::GetOrderedEvents(std::vector<PatternEvent*> & ordered_events) {
	for(std::multimap<double, std::multimap<int, PatternEvent> >::iterator event_it = events_.begin();
		event_it != events_.end(); ++event_it
	) {
		std::multimap<int, PatternEvent> & map = event_it->second;
		for(std::multimap<int, PatternEvent>::iterator map_it = map.begin();
			map_it != map.end(); ++map_it
		) {
			PatternEvent & event = map_it->second;
			ordered_events.push_back(&event);
		}
	}
}

bool Sequence::getPlayInfo(Pattern & /*pattern*/, double /*start*/, double /*length*/, double& /*entryStart*/) const {
	#if 0 ///\todo
		entryStart = 0;
		PatternLine* searchLine = 0;

		// Iterate over each timeline of the sequence,
		for( const_iterator seqIt = begin(); seqIt != end(); ++seqIt )
		{
			SequenceLine *pSLine = *seqIt;
			// locate the "sequenceEntry"s which starts at "start+length"
			SequenceLine::reverse_iterator sLineIt( pSLine->upper_bound(start + length) );
			// and iterate backwards to include any other that is inside the range [start,start+length)
			// (The UI won't allow more than one pattern for the same range in the same timeline, but 
			// this was left open in the player code)
			for(; sLineIt != pSLine->rend() && sLineIt->first + sLineIt->second->patternBeats() >= start; ++sLineIt )
			{
				// take the pattern,
				Pattern* pPat = sLineIt->second->pattern();
				if ( pPat == pattern ) {
					entryStart = sLineIt->first;
					return true;
				}
			}
		}
	#endif
	return false;
}

void Sequence::removePattern(Pattern & pattern) {
	for(iterator it = begin(); it != end(); ++it) {
		(*it)->removePatternEntries(pattern);
	}
	Remove(pattern);
}

void Sequence::removeAll( ) {
	for(iterator it = begin(); it != end(); ++it) {
		lineRemoved(**it);
		delete *it;
	}
	lines_.clear();
	for(patterns_type::iterator it = patterns_.begin(); it != patterns_.end(); ++it) delete *it;
	patterns_.clear();
	create_master_pattern();
}

double Sequence::tickLength() const {
	double max = 0;
	for(const_iterator it = begin(); it != end(); ++it) {
		SequenceLine* line = *it;
		max = std::max(line->tickLength(), max);
	}
	return max;
}

void Sequence::moveUpLine(SequenceLine & line) {
	iterator it = find( begin(), end(), &line);
	if(it != begin()) {
		iterator prev = it;
		--prev;
		std::swap(*prev, *it);
		linesSwapped(**it, **prev);
	}
}

void Sequence::moveDownLine(SequenceLine & line) {
	iterator it = find( begin(), end(), &line);
	if(it != end()) {
		iterator next = it;
		++next;
		if( next != end()) {
			std::swap(*it, *next);
			linesSwapped(**next, **it);
		}
	}
}

std::string Sequence::toXml() const {
	std::ostringstream xml;
	xml << "<sequence>" << std::endl;
	for(const_iterator it = begin(); it != end(); ++it) {
		SequenceLine* line = *it;
		xml << line->toXml();
	}
	xml << "</sequence>" << std::endl;
	return xml.str();
}

void Sequence::Add(Pattern & pattern) {
    patterns_.push_back(&pattern);
}

void Sequence::Remove(Pattern & pattern) {
	patterns_type::iterator i = std::find(patterns_.begin(), patterns_.end(), &pattern);
	if(i != patterns_.end()) patterns_.erase(i);
}

double Sequence::max_beats() const {
	double max_ = 0;
	const_iterator seq_track_it = begin()+1;
	// iterate over the sequenceitems inside a sequencer track
	for ( seq_track_it = begin()+1; seq_track_it != end(); ++seq_track_it ) {
		SequenceLine::reverse_iterator rev_item_it = (*seq_track_it)->rbegin();
		if ( rev_item_it != (*seq_track_it)->rend() ) {
			max_ = std::max(max_, rev_item_it->first + rev_item_it->second->patternBeats() );
		}
	}
	return max_;
}

}}
