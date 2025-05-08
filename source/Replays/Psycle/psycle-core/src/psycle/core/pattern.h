// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PATTERN__INCLUDED
#define PSYCLE__CORE__PATTERN__INCLUDED
#pragma once

#include "timesignature.h"
#include <boost/signals2.hpp>
#include "patternevent.h"


namespace psycle { namespace core {

class PatternEvent;

class PSYCLE__CORE__DECL Pattern {
public:

    typedef std::multimap<std::pair<double, int>, PatternEvent>::iterator iterator;
    typedef std::multimap<std::pair<double, int>, PatternEvent>::const_iterator const_iterator;
    typedef std::multimap<std::pair<double, int>, PatternEvent>::const_reverse_iterator const_reverse_iterator;
    typedef std::multimap<std::pair<double, int>, PatternEvent>::reverse_iterator reverse_iterator;

    Pattern();
    Pattern(const Pattern& other);

    virtual ~Pattern();

    Pattern& operator=(const Pattern& rhs);
    boost::signals2::signal<void (Pattern*)> wasDeleted;

    void setID(int id) { id_ = id; }
    int id() const { return id_; }

    void addBar(const TimeSignature& signature);
    void removeBar(float pos);

    float beats() const;

    bool barStart(double pos, TimeSignature & signature) const;
    void clearBars();

    const TimeSignature & playPosTimeSignature(double pos) const;

    void setName(const std::string & name);
    const std::string& name() const;

    void setCategory(const std::string& category);
    const std::string& category() const { return category_; }

    void Clear();

    Pattern Clone(double from, double to, int start_track=0, int end_track=32000);
    void erase(double from, double to, int start_track=0, int end_track=32000);

    void Transpose(int delta, double from, double to, int start_track=0, int end_track=32000);
    void ChangeInst(int new_inst, double from, double to, int start_track=0, int end_track=32000);
    void ChangeMac(int new_inst, double from, double to, int start_track=0, int end_track=32000);

    void scaleBlock(int left, int right, double top, double bottom, float factor);

    std::vector<TimeSignature>& timeSignatures();
    const std::vector<TimeSignature>&  timeSignatures() const;

    std::string toXml() const;

    iterator begin() { return lines_.begin(); }
    const_iterator begin() const { return lines_.begin(); }
    iterator end() { return lines_.end(); }
    const_iterator end() const { return lines_.end(); }

    reverse_iterator rbegin() { return lines_.rbegin(); }
    const_reverse_iterator rbegin() const { return lines_.rbegin();}
    reverse_iterator rend() { return lines_.rend(); }
    const_reverse_iterator rend() const { return lines_.rend(); }

    iterator lower_bound(double pos) { return lines_.lower_bound( {pos, 0} ); }
    iterator upper_bound(double pos) { return lines_.upper_bound( {pos, INT_MAX} ); }

    iterator insert(double pos, int track, const PatternEvent& ev) {
        return lines_.insert({ { pos, track }, ev} );
    }
    void insert(const Pattern& srcPattern, double to, int to_track=0);

    const PatternEvent &getPatternEvent(int line, int track);

    iterator erase(iterator pos) {
        iterator temp = pos;
        ++temp;
        lines_.erase(pos);
        return temp;
    }

    std::map<double, PatternEvent>::size_type size() const { return lines_.size(); }

private:
    std::string name_;
    std::string category_;
    std::vector<TimeSignature> timeSignatures_;
    TimeSignature zeroTime;
    int id_;
    std::multimap<std::pair<double, int>, PatternEvent, bool (*)(std::pair<double,int> ,std::pair<double,int>)> lines_;
};

}}



#endif
