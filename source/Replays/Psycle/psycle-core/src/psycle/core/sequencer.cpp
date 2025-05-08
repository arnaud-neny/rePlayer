// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "sequencer.h"

#include "machine.h"
#include "playertimeinfo.h"
#include "patternevent.h"
#include "sequence.h"
#include "player.h"
#include "song.h"

namespace psycle { namespace core {

void Sequencer::Work(unsigned int nframes) {
	///\todo: Need to add the events coming from the MIDI device. (Of course, first we need the MIDI device)
	///\todo this will not work frame correct for BPM changes for now
	double beats = nframes / ((time_info()->sampleRate() * 60) / time_info()->bpm());
    const Sequence &seq = song_->sequence();
	unsigned int rest_frames = nframes;
	double last_pos = 0;
    for( auto seqIt = seq.begin(); seqIt != seq.end(); ++seqIt ) {
        const SequenceLine &line = **seqIt;
        double start = time_info()->playBeatPos();
        for( SequenceLine::const_reverse_iterator lineIt (line.lower_bound( start + beats ));
             lineIt != line.rend() &&
             lineIt->first + lineIt->second->patternBeats() >= start;
             ++lineIt ){
            Pattern & pat = lineIt->second->pattern();
            double entryStart = lineIt->first;
            double entryStartOffset  = lineIt->second->startPos();
            double entryEndOffset  = lineIt->second->endPos();
            double relativeStart = start - entryStart + entryStartOffset;
            for(Pattern::iterator
                patIt  = pat.lower_bound(std::min(relativeStart, entryEndOffset)),
                patEnd = pat.lower_bound(std::min(relativeStart + beats, entryEndOffset));
                patIt != patEnd; ++patIt
                ) {
                PatternEvent ev = patIt->second;
                ev.set_time_offset(entryStart + patIt->first.first - entryStartOffset);
                if(ev.IsGlobal()) {
                    double pos = ev.time_offset();
                    unsigned int num = static_cast<int>((pos - last_pos) * time_info()->samplesPerBeat());
                    if(rest_frames < num) {
                        if(loggers::warning()) {
                            std::ostringstream s;
                            s << "wrong global event time offset: " << pos << " beats, exceeds: " << beats << " beats";
                            loggers::warning()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
                        }
                        rest_frames = 0;
                    } else {
                        player_->process(num);
                        rest_frames -= num;
                    }
                    last_pos = pos;
                    process_global_event(ev);
                }
               int track = patIt->first.second;
               execute_notes(ev.time_offset() - time_info()->playBeatPos(), track, ev);

            }
        }

    }
	player_->process(rest_frames);
}

void Sequencer::process_global_event(const PatternEvent& event) {
//	Machine::id_type mac_id;
	switch(event.command()) {
		case commandtypes::BPM_CHANGE:
			loggers::warning()("psycle: core: unimplemented global event: bpm change");
			#if 0
			setBpm(event.parameter());
			#endif
		break;
		#if 0
		case commandtypes::JUMP_TO:
			loggers::warning()("psycle: core: unimplemented global event: jump to");
			#if 0
			//todo: fix this. parameter indicates the pattern, not the beat!
			timeInfo_.setPlayBeatPos(event.parameter());
			#endif
		break;
		#endif
		case commandtypes::SET_VOLUME:
			loggers::warning()("psycle: core: unimplemented global event: set volume");
			#if 0
			if(event.machine() == 255) {
				Master & master(static_cast<Master&>(*song().machine(MASTER_INDEX)));
				master._outDry = static_cast<int>(event.parameter());
			} else {
				mac_id = event.machine();
				if(mac_id < MAX_MACHINES && song().machine(mac_id)) {
					Wire::id_type wire(event.target2());
					song().machine(mac_id)->SetDestWireVolume(mac_id, wire,
						value_mapper::map_256_1(static_cast<int>(event.parameter()))
					);
				}
			}
			#endif
		break;
		case commandtypes::SET_PANNING:
			loggers::warning()("psycle: core: unimplemented global event: set panning");
			#if 0
			mac_id = event.target();
			if(mac_id < MAX_MACHINES && song().machine(mac_id))
				song().machine(mac_id)->SetPan(static_cast<int>( event.parameter()));
			#endif
		break;
		case commandtypes::EXTENDED:
			switch(event.parameter() & 0xf0) {
				case commandtypes::extended::SET_BYPASS:
					loggers::warning()("psycle: core: unimplemented global event: set bypass");
					#if 0
					mac_id = event.target();
					if(mac_id < MAX_MACHINES && song().machine(mac_id) && song().machine(mac_id)->acceptsConnections()) //i.e. Effect
						song().machine(mac_id)->_bypass = event.parameter() != 0;
					#endif
				break;
				case commandtypes::extended::SET_MUTE:
					loggers::warning()("psycle: core: unimplemented global event: set mute");
					#if 0
					mac_id = event.target();
					if(mac_id < MAX_MACHINES && song().machine(mac_id))
						song().machine(mac_id)->_mute = event.parameter() != 0;
					#endif
				break;
				default:
					if(loggers::warning()) {
						std::ostringstream s;
						s << "psycle: core: unhandled global event: extended command: "
								<< std::hex << event.command() << ' '
								<< std::hex << event.parameter();
						loggers::warning()(s.str());
					}
			}
		break;
		default: {
			if(loggers::warning()) {
				std::ostringstream s;
				s << "psycle: core: unhandled global event: "
						<< std::hex << event.command() << ' '
						<< std::hex << event.parameter();
				loggers::warning()(s.str());
			}
		}
	}
}

void Sequencer::execute_notes(double beat_offset, int track, PatternEvent& entry) {
	// WARNING!!! In this function, the events inside the patterline are assumed to be temporary! (thus, modifiable)

    int patTrack = track % MAX_TRACKS;
    int sequence_track = track-patTrack;

	int mac = entry.machine();
    if(mac != 255) prev_machines_[patTrack] = mac;
    else mac = prev_machines_[patTrack];

	// not a valid machine id?
	if(mac >= MAX_MACHINES || !song()->machine(mac))
		return;
	
	Machine& machine = *song()->machine(mac);

	// step 1: process all tweaks.
	{
		switch(entry.note()) {
			case notetypes::tweak_slide: {
				int const delay(64);
				int delaysamples(0), origin(machine.GetParamValue(entry.instrument()));
				float increment(origin);
				int previous(0);
				float rate = (((entry.command() << 16 ) | entry.parameter()) - origin) / (time_info()->samplesPerTick() / 64.0f);
				entry.setNote(notetypes::tweak);
				entry.setCommand(origin >> 8);
				entry.setParameter(origin & 0xff);
				machine.AddEvent(
					beat_offset + static_cast<double>(delaysamples) / time_info()->samplesPerBeat(),
                    sequence_track * 1024 + patTrack, entry
				);
				previous = origin;
				delaysamples += delay;
				while(delaysamples < time_info()->samplesPerTick()) {
					increment += rate;
					if(static_cast<int>(increment) != previous) {
						origin = static_cast<int>(increment);
						entry.setCommand(origin >> 8);
						entry.setParameter(origin & 0xff);
						machine.AddEvent(
							beat_offset + static_cast<double>(delaysamples) / time_info()->samplesPerBeat(),
                            sequence_track * 1024 + patTrack, entry
						);
						previous = origin;
					}
					delaysamples += delay;
				}
			} break;
			case notetypes::tweak:
                machine.AddEvent(beat_offset, sequence_track * 1024 + patTrack, entry);
			break;
			default: ;
		}
	}

	// step 2: collect note
	{
		// track muted?
        if(song()->sequence().trackMuted(patTrack)) return;

		// not a note ?
		if(entry.note() > notetypes::release && entry.note() != notetypes::empty) return;

		// machine muted?
		if(machine._mute) return;

		switch(entry.command()) {
			case commandtypes::NOTE_DELAY: {
				double delayoffset(entry.parameter() / 256.0);
				// At least Plucked String works erroneously if the command is not ommited.
				entry.setCommand(0); entry.setParameter(0);
                machine.AddEvent(beat_offset + delayoffset, sequence_track * 1024 + patTrack, entry);
			} break;
			case commandtypes::RETRIGGER: {
				///\todo: delaysamples and rate should be memorized (for RETR_CONT command ). Then set delaysamples to zero in this function.
				int delaysamples(0);
				int rate = entry.parameter() + 1;
				int delay = (rate * static_cast<int>(time_info()->samplesPerTick())) >> 8;
				entry.setCommand(0); entry.setParameter(0);
                machine.AddEvent(beat_offset, sequence_track * 1024 + patTrack, entry);
				delaysamples += delay;
				while(delaysamples < time_info()->samplesPerTick()) {
					machine.AddEvent(
					beat_offset + static_cast<double>(delaysamples) / time_info()->samplesPerBeat(),
                    sequence_track * 1024 + patTrack, entry
				);
				delaysamples += delay;
			}
		} break;
		case commandtypes::RETR_CONT: {
			///\todo: delaysamples and rate should be memorized, do not reinit delaysamples.
			///\todo: verify that using ints for rate and variation is enough, or has to be float.
			int delaysamples(0), rate(0), delay(0), variation(0);
			int parameter = entry.parameter() & 0x0f;
			variation = (parameter < 9) ? (4 * parameter) : (-2 * (16 - parameter));
			if(entry.parameter() & 0xf0) rate = entry.parameter() & 0xf0;
			delay = (rate * static_cast<int>(time_info()->samplesPerTick())) >> 8;
			entry.setCommand(0); entry.setParameter(0);
			machine.AddEvent(
				beat_offset + static_cast<double>(delaysamples) / time_info()->samplesPerBeat(),
                sequence_track * 1024 + patTrack, entry
			);
			delaysamples += delay;
			while(delaysamples < time_info()->samplesPerTick()) {
				machine.AddEvent(
					beat_offset + static_cast<double>(delaysamples) / time_info()->samplesPerBeat(),
                    sequence_track * 1024 + patTrack, entry
				);
				rate += variation;
				if(rate < 16)
					rate = 16;
				delay = (rate * static_cast<int>(time_info()->samplesPerTick())) >> 8;
				delaysamples += delay;
			}
		} break;
		case commandtypes::ARPEGGIO: {
			///\todo : Add Memory.
			///\todo : This won't work... What about sampler's NNA's?
			#if 0
				if(entry.parameter()) {
					machine.TriggerDelay[track] = entry;
					machine.ArpeggioCount[track] = 1;
				}
				machine.RetriggerRate[track] = static_cast<int>(timeInfo_.samplesPerTick() * timeInfo_.linesPerBeat() / 24);
			#endif
		} break;
		default:
            machine.TriggerDelay[patTrack].setCommand(0);
            machine.AddEvent(beat_offset, sequence_track * 1024 + patTrack, entry);
            machine.TriggerDelayCounter[patTrack] = 0;
            machine.ArpeggioCount[patTrack] = 0;
		}
	}
}

}}
