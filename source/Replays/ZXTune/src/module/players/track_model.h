/**
 *
 * @file
 *
 * @brief  Track model definition
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <iterator.h>
#include <types.h>
// library includes
#include <module/track_state.h>
// std includes
#include <vector>

namespace Module
{
  struct Command
  {
    Command()
      : Type()
      , Param1()
      , Param2()
      , Param3()
    {}

    Command(uint_t type, int_t p1, int_t p2, int_t p3)
      : Type(type)
      , Param1(p1)
      , Param2(p2)
      , Param3(p3)
    {}

    bool operator==(uint_t type) const
    {
      return Type == type;
    }

    uint_t Type;
    int_t Param1;
    int_t Param2;
    int_t Param3;
  };

  typedef std::vector<Command> CommandsArray;
  typedef RangeIterator<CommandsArray::const_iterator> CommandsIterator;

  class Cell
  {
  public:
    Cell()
      : Mask()
      , Enabled()
      , Note()
      , SampleNum()
      , OrnamentNum()
      , Volume()
      , Commands()
    {}

    bool HasData() const
    {
      return 0 != Mask || !Commands.empty();
    }

    const bool* GetEnabled() const
    {
      return 0 != (Mask & ENABLED) ? &Enabled : nullptr;
    }

    const uint_t* GetNote() const
    {
      return 0 != (Mask & NOTE) ? &Note : nullptr;
    }

    const uint_t* GetSample() const
    {
      return 0 != (Mask & SAMPLENUM) ? &SampleNum : nullptr;
    }

    const uint_t* GetOrnament() const
    {
      return 0 != (Mask & ORNAMENTNUM) ? &OrnamentNum : nullptr;
    }

    const uint_t* GetVolume() const
    {
      return 0 != (Mask & VOLUME) ? &Volume : nullptr;
    }

    CommandsIterator GetCommands() const
    {
      return CommandsIterator(Commands.begin(), Commands.end());
    }

  protected:
    enum Flags
    {
      ENABLED = 1,
      NOTE = 2,
      SAMPLENUM = 4,
      ORNAMENTNUM = 8,
      VOLUME = 16
    };

    uint_t Mask;
    bool Enabled;
    uint_t Note;
    uint_t SampleNum;
    uint_t OrnamentNum;
    uint_t Volume;
    CommandsArray Commands;
  };

  class Line
  {
  public:
    virtual ~Line() = default;

    virtual const Cell* GetChannel(uint_t idx) const = 0;
    virtual uint_t CountActiveChannels() const = 0;
    virtual uint_t GetTempo() const = 0;
  };

  class Pattern
  {
  public:
    virtual ~Pattern() = default;

    virtual const class Line* GetLine(uint_t row) const = 0;
    virtual uint_t GetSize() const = 0;
  };

  class PatternsSet
  {
  public:
    typedef std::unique_ptr<const PatternsSet> Ptr;
    virtual ~PatternsSet() = default;

    virtual const class Pattern* Get(uint_t idx) const = 0;
    virtual uint_t GetSize() const = 0;
  };

  class OrderList
  {
  public:
    typedef std::unique_ptr<const OrderList> Ptr;
    virtual ~OrderList() = default;

    virtual uint_t GetSize() const = 0;
    virtual uint_t GetPatternIndex(uint_t pos) const = 0;
    virtual uint_t GetLoopPosition() const = 0;
  };

  class TrackModel
  {
  public:
    typedef std::shared_ptr<const TrackModel> Ptr;
    virtual ~TrackModel() = default;

    virtual uint_t GetChannelsCount() const = 0;
    virtual uint_t GetInitialTempo() const = 0;
    virtual const OrderList& GetOrder() const = 0;
    virtual const PatternsSet& GetPatterns() const = 0;
  };

  class TrackModelState : public TrackState
  {
  public:
    typedef std::shared_ptr<const TrackModelState> Ptr;

    virtual const class Pattern* PatternObject() const = 0;
    virtual const class Line* LineObject() const = 0;
  };
}  // namespace Module
