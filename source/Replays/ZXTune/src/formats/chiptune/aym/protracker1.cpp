/**
 *
 * @file
 *
 * @brief  ProTracker v1.x support implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#include "formats/chiptune/aym/protracker1.h"

#include "formats/chiptune/container.h"

#include "binary/format_factories.h"
#include "debug/log.h"
#include "math/numeric.h"
#include "strings/optimize.h"
#include "tools/indices.h"
#include "tools/range_checker.h"

#include "byteorder.h"
#include "contract.h"
#include "make_ptr.h"
#include "pointers.h"

#include <array>

namespace Formats::Chiptune
{
  namespace ProTracker1
  {
    const Debug::Stream Dbg("Formats::Chiptune::ProTracker1");

    const auto PROGRAM = "Pro Tracker v1.x"sv;

    const std::size_t MIN_SIZE = 256;
    const std::size_t MAX_SIZE = 0x2800;
    const std::size_t MAX_POSITIONS_COUNT = 255;
    const std::size_t MIN_PATTERN_SIZE = 5;
    const std::size_t MAX_PATTERN_SIZE = 64;
    const std::size_t MAX_PATTERNS_COUNT = 32;
    const std::size_t MAX_SAMPLES_COUNT = 16;
    const std::size_t MAX_ORNAMENTS_COUNT = 16;
    const std::size_t MAX_SAMPLE_SIZE = 64;

    /*
      Typical module structure

      Header
      Positions,0xff
      Patterns (6 bytes each)
      Patterns data
      Samples
      Ornaments
    */

    struct RawHeader
    {
      uint8_t Tempo;
      uint8_t Length;
      uint8_t Loop;
      std::array<le_uint16_t, MAX_SAMPLES_COUNT> SamplesOffsets;
      std::array<le_uint16_t, MAX_ORNAMENTS_COUNT> OrnamentsOffsets;
      le_uint16_t PatternsOffset;
      std::array<char, 30> Name;
      uint8_t Positions[1];
    };

    const uint8_t POS_END_MARKER = 0xff;

    struct RawObject
    {
      uint8_t Size;
      uint8_t Loop;

      uint_t GetSize() const
      {
        return Size;
      }
    };

    struct RawSample : RawObject
    {
      struct Line
      {
        // HHHHaaaa
        // NTsnnnnn
        // LLLLLLLL

        // HHHHLLLLLLLL - vibrato
        // s - vibrato sign
        // a - level
        // N - noise off
        // T - tone off
        // n - noise value
        uint8_t LevelHiVibrato;
        uint8_t NoiseAndFlags;
        uint8_t LoVibrato;

        bool GetNoiseMask() const
        {
          return 0 != (NoiseAndFlags & 128);
        }

        bool GetToneMask() const
        {
          return 0 != (NoiseAndFlags & 64);
        }

        uint_t GetNoise() const
        {
          return NoiseAndFlags & 31;
        }

        uint_t GetLevel() const
        {
          return LevelHiVibrato & 15;
        }

        int_t GetVibrato() const
        {
          const int_t val((int_t(LevelHiVibrato & 0xf0) << 4) | LoVibrato);
          return (NoiseAndFlags & 32) ? val : -val;
        }
      };

      std::size_t GetUsedSize() const
      {
        return sizeof(RawObject) + std::min<std::size_t>(GetSize() * sizeof(Line), 256);
      }

      Line GetLine(uint_t idx) const
      {
        const auto* const src = safe_ptr_cast<const uint8_t*>(this + 1);
        // using 8-bit offsets
        auto offset = static_cast<uint8_t>(idx * sizeof(Line));
        Line res;
        res.LevelHiVibrato = src[offset++];
        res.NoiseAndFlags = src[offset++];
        res.LoVibrato = src[offset++];
        return res;
      }
    };

    struct RawOrnament
    {
      using Line = int8_t;
      Line Data[1];

      static std::size_t GetSize()
      {
        return MAX_SAMPLE_SIZE;
      }

      static std::size_t GetUsedSize()
      {
        return MAX_SAMPLE_SIZE;
      }

      Line GetLine(uint_t idx) const
      {
        return Data[idx];
      }
    };

    struct RawPattern
    {
      std::array<le_uint16_t, 3> Offsets;
    };

    static_assert(sizeof(RawHeader) * alignof(RawHeader) == 100, "Invalid layout");
    static_assert(sizeof(RawSample) * alignof(RawSample) == 2, "Invalid layout");
    static_assert(sizeof(RawOrnament) * alignof(RawOrnament) == 1, "Invalid layout");

    class StubBuilder : public Builder
    {
    public:
      MetaBuilder& GetMetaBuilder() override
      {
        return GetStubMetaBuilder();
      }
      void SetInitialTempo(uint_t /*tempo*/) override {}
      void SetSample(uint_t /*index*/, Sample /*sample*/) override {}
      void SetOrnament(uint_t /*index*/, Ornament /*ornament*/) override {}
      void SetPositions(Positions /*positions*/) override {}
      PatternBuilder& StartPattern(uint_t /*index*/) override
      {
        return GetStubPatternBuilder();
      }
      void StartChannel(uint_t /*index*/) override {}
      void SetRest() override {}
      void SetNote(uint_t /*note*/) override {}
      void SetSample(uint_t /*sample*/) override {}
      void SetOrnament(uint_t /*ornament*/) override {}
      void SetVolume(uint_t /*vol*/) override {}
      void SetEnvelope(uint_t /*type*/, uint_t /*value*/) override {}
      void SetNoEnvelope() override {}
    };

    class StatisticCollectingBuilder : public Builder
    {
    public:
      explicit StatisticCollectingBuilder(Builder& delegate)
        : Delegate(delegate)
        , UsedPatterns(0, MAX_PATTERNS_COUNT - 1)
        , UsedSamples(0, MAX_SAMPLES_COUNT - 1)
        , UsedOrnaments(0, MAX_ORNAMENTS_COUNT - 1)
      {
        UsedSamples.Insert(0);
        UsedOrnaments.Insert(0);
      }

      MetaBuilder& GetMetaBuilder() override
      {
        return Delegate.GetMetaBuilder();
      }

      void SetInitialTempo(uint_t tempo) override
      {
        return Delegate.SetInitialTempo(tempo);
      }

      void SetSample(uint_t index, Sample sample) override
      {
        assert(index == 0 || UsedSamples.Contain(index));
        return Delegate.SetSample(index, std::move(sample));
      }

      void SetOrnament(uint_t index, Ornament ornament) override
      {
        assert(index == 0 || UsedOrnaments.Contain(index));
        return Delegate.SetOrnament(index, std::move(ornament));
      }

      void SetPositions(Positions positions) override
      {
        UsedPatterns.Assign(positions.Lines.begin(), positions.Lines.end());
        Require(!UsedPatterns.Empty());
        return Delegate.SetPositions(std::move(positions));
      }

      PatternBuilder& StartPattern(uint_t index) override
      {
        assert(UsedPatterns.Contain(index));
        return Delegate.StartPattern(index);
      }

      void StartChannel(uint_t index) override
      {
        return Delegate.StartChannel(index);
      }

      void SetRest() override
      {
        return Delegate.SetRest();
      }

      void SetNote(uint_t note) override
      {
        return Delegate.SetNote(note);
      }

      void SetSample(uint_t sample) override
      {
        UsedSamples.Insert(sample);
        return Delegate.SetSample(sample);
      }

      void SetOrnament(uint_t ornament) override
      {
        UsedOrnaments.Insert(ornament);
        return Delegate.SetOrnament(ornament);
      }

      void SetVolume(uint_t vol) override
      {
        return Delegate.SetVolume(vol);
      }

      void SetEnvelope(uint_t type, uint_t value) override
      {
        return Delegate.SetEnvelope(type, value);
      }

      void SetNoEnvelope() override
      {
        return Delegate.SetNoEnvelope();
      }

      const Indices& GetUsedPatterns() const
      {
        return UsedPatterns;
      }

      const Indices& GetUsedSamples() const
      {
        return UsedSamples;
      }

      const Indices& GetUsedOrnaments() const
      {
        return UsedOrnaments;
      }

    private:
      Builder& Delegate;
      Indices UsedPatterns;
      Indices UsedSamples;
      Indices UsedOrnaments;
    };

    class RangesMap
    {
    public:
      explicit RangesMap(std::size_t limit)
        : ServiceRanges(RangeChecker::CreateShared(limit))
        , TotalRanges(RangeChecker::CreateSimple(limit))
        , FixedRanges(RangeChecker::CreateSimple(limit))
      {}

      void AddService(std::size_t offset, std::size_t size) const
      {
        Require(ServiceRanges->AddRange(offset, size));
        Add(offset, size);
      }

      void AddFixed(std::size_t offset, std::size_t size) const
      {
        Require(FixedRanges->AddRange(offset, size));
        Add(offset, size);
      }

      void Add(std::size_t offset, std::size_t size) const
      {
        Dbg(" Affected range {}..{}", offset, offset + size);
        Require(TotalRanges->AddRange(offset, size));
      }

      std::size_t GetSize() const
      {
        return TotalRanges->GetAffectedRange().second;
      }

      RangeChecker::Range GetFixedArea() const
      {
        return FixedRanges->GetAffectedRange();
      }

    private:
      const RangeChecker::Ptr ServiceRanges;
      const RangeChecker::Ptr TotalRanges;
      const RangeChecker::Ptr FixedRanges;
    };

    std::size_t GetHeaderSize(Binary::View data)
    {
      if (const auto* hdr = data.As<RawHeader>())
      {
        const uint8_t* const dataBegin = &hdr->Tempo;
        const uint8_t* const dataEnd = dataBegin + data.Size();
        const uint8_t* const lastPosition = std::find(hdr->Positions, dataEnd, POS_END_MARKER);
        if (lastPosition != dataEnd
            && std::none_of(hdr->Positions, lastPosition, [](auto b) { return b >= MAX_PATTERNS_COUNT; }))
        {
          return lastPosition + 1 - dataBegin;
        }
      }
      return 0;
    }

    void CheckTempo(uint_t tempo)
    {
      Require(tempo >= 2);
    }

    class Format
    {
    public:
      explicit Format(Binary::View data)
        : Data(data)
        , Ranges(Data.Size())
        , Source(*Data.As<RawHeader>())
      {
        Ranges.AddService(0, GetHeaderSize(Data));
      }

      void ParseCommonProperties(Builder& builder) const
      {
        const uint_t tempo = Source.Tempo;
        CheckTempo(tempo);
        builder.SetInitialTempo(tempo);
        MetaBuilder& meta = builder.GetMetaBuilder();
        meta.SetProgram(PROGRAM);
        meta.SetTitle(Strings::OptimizeAscii(Source.Name));
      }

      void ParsePositions(Builder& builder) const
      {
        Positions positions;
        for (const uint8_t* pos = Source.Positions; *pos != POS_END_MARKER; ++pos)
        {
          const uint_t patNum = *pos;
          positions.Lines.push_back(patNum);
        }
        Require(Math::InRange<std::size_t>(positions.GetSize(), 1, MAX_POSITIONS_COUNT));
        positions.Loop = Source.Loop;
        Dbg("Positions: {} entries, loop to {} (header length is {})", positions.GetSize(), positions.GetLoop(),
            uint_t(Source.Length));
        builder.SetPositions(std::move(positions));
      }

      void ParsePatterns(const Indices& pats, Builder& builder) const
      {
        Dbg("Patterns: {} to parse", pats.Count());
        const std::size_t minOffset = Source.PatternsOffset + pats.Maximum() * sizeof(RawPattern);
        bool hasValidPatterns = false;
        for (Indices::Iterator it = pats.Items(); it; ++it)
        {
          const uint_t patIndex = *it;
          Dbg("Parse pattern {}", patIndex);
          if (ParsePattern(patIndex, minOffset, builder))
          {
            hasValidPatterns = true;
          }
        }
        Require(hasValidPatterns);
      }

      void ParseSamples(const Indices& samples, Builder& builder) const
      {
        Dbg("Samples: {} to parse", samples.Count());
        bool hasValidSamples = false;
        bool hasPartialSamples = false;
        for (Indices::Iterator it = samples.Items(); it; ++it)
        {
          const uint_t samIdx = *it;
          Sample result;
          if (const std::size_t samOffset = Source.SamplesOffsets[samIdx])
          {
            const std::size_t availSize = Data.Size() - samOffset;
            if (const auto* src = PeekObject<RawSample>(samOffset))
            {
              const std::size_t usedSize = src->GetUsedSize();
              if (usedSize <= availSize)
              {
                Dbg("Parse sample {}", samIdx);
                Ranges.Add(samOffset, usedSize);
                ParseSample(*src, src->GetSize(), result);
                hasValidSamples = true;
              }
              else
              {
                Dbg("Parse partial sample {}", samIdx);
                Ranges.Add(samOffset, availSize);
                const uint_t availLines = (availSize - sizeof(*src)) / sizeof(RawSample::Line);
                ParseSample(*src, availLines, result);
                hasPartialSamples = true;
              }
            }
            else
            {
              Dbg("Stub sample {}", samIdx);
            }
          }
          else
          {
            Dbg("Parse invalid sample {}", samIdx);
            const auto& invalidLine = *Data.As<RawSample::Line>();
            result.Lines.push_back(ParseSampleLine(invalidLine));
          }
          builder.SetSample(samIdx, std::move(result));
        }
        Require(hasValidSamples || hasPartialSamples);
      }

      void ParseOrnaments(const Indices& ornaments, Builder& builder) const
      {
        Dbg("Ornaments: {} to parse", ornaments.Count());
        for (Indices::Iterator it = ornaments.Items(); it; ++it)
        {
          const uint_t ornIdx = *it;
          Ornament result;
          if (const std::size_t ornOffset = Source.OrnamentsOffsets[ornIdx])
          {
            const std::size_t availSize = Data.Size() - ornOffset;
            if (const auto* src = PeekObject<RawOrnament>(ornOffset))
            {
              const std::size_t usedSize = src->GetUsedSize();
              if (usedSize <= availSize)
              {
                Dbg("Parse ornament {}", ornIdx);
                Ranges.Add(ornOffset, usedSize);
                ParseOrnament(*src, src->GetSize(), result);
              }
              else
              {
                Dbg("Parse partial ornament {}", ornIdx);
                Ranges.Add(ornOffset, availSize);
                const uint_t availLines = (availSize - sizeof(*src)) / sizeof(RawOrnament::Line);
                ParseOrnament(*src, availLines, result);
              }
            }
            else
            {
              Dbg("Stub ornament {}", ornIdx);
            }
          }
          else
          {
            Dbg("Parse invalid ornament {}", ornIdx);
            result.Lines.push_back(*Data.As<RawOrnament::Line>());
          }
          builder.SetOrnament(ornIdx, std::move(result));
        }
      }

      std::size_t GetSize() const
      {
        return Ranges.GetSize();
      }

      RangeChecker::Range GetFixedArea() const
      {
        return Ranges.GetFixedArea();
      }

    private:
      template<class T>
      const T* PeekObject(std::size_t offset) const
      {
        return Data.SubView(offset).As<T>();
      }

      const RawPattern& GetPattern(uint_t patIdx) const
      {
        const std::size_t patOffset = Source.PatternsOffset + patIdx * sizeof(RawPattern);
        Ranges.AddService(patOffset, sizeof(RawPattern));
        return *PeekObject<RawPattern>(patOffset);
      }

      uint8_t PeekByte(std::size_t offset) const
      {
        const auto* data = PeekObject<uint8_t>(offset);
        Require(data != nullptr);
        return *data;
      }

      struct DataCursors : public std::array<std::size_t, 3>
      {
        explicit DataCursors(const RawPattern& src)
        {
          std::copy(src.Offsets.begin(), src.Offsets.end(), begin());
        }
      };

      struct ParserState
      {
        struct ChannelState
        {
          std::size_t Offset = 0;
          uint_t Period = 0;
          uint_t Counter = 0;

          ChannelState() = default;

          void Skip(uint_t toSkip)
          {
            Counter -= toSkip;
          }

          static bool CompareByCounter(const ChannelState& lh, const ChannelState& rh)
          {
            return lh.Counter < rh.Counter;
          }
        };

        std::array<ChannelState, 3> Channels;

        explicit ParserState(const DataCursors& src)
        {
          for (std::size_t idx = 0; idx != src.size(); ++idx)
          {
            Channels[idx].Offset = src[idx];
          }
        }

        uint_t GetMinCounter() const
        {
          return std::min_element(Channels.begin(), Channels.end(), &ChannelState::CompareByCounter)->Counter;
        }

        void SkipLines(uint_t toSkip)
        {
          for (auto& chan : Channels)
          {
            chan.Skip(toSkip);
          }
        }
      };

      bool ParsePattern(uint_t patIndex, std::size_t minOffset, Builder& builder) const
      {
        const RawPattern& pat = GetPattern(patIndex);
        const DataCursors rangesStarts(pat);
        Require(
            rangesStarts.end()
            == std::find_if(rangesStarts.begin(), rangesStarts.end(), Math::NotInRange(minOffset, Data.Size() - 1)));

        PatternBuilder& patBuilder = builder.StartPattern(patIndex);
        ParserState state(rangesStarts);
        uint_t lineIdx = 0;
        for (; lineIdx < MAX_PATTERN_SIZE; ++lineIdx)
        {
          // skip lines if required
          if (const uint_t linesToSkip = state.GetMinCounter())
          {
            state.SkipLines(linesToSkip);
            lineIdx += linesToSkip;
          }
          if (!HasLine(state))
          {
            patBuilder.Finish(std::max<uint_t>(lineIdx, MIN_PATTERN_SIZE));
            break;
          }
          patBuilder.StartLine(lineIdx);
          ParseLine(state, patBuilder, builder);
        }
        for (uint_t chanNum = 0; chanNum != rangesStarts.size(); ++chanNum)
        {
          const std::size_t start = rangesStarts[chanNum];
          if (start >= Data.Size())
          {
            Dbg("Invalid offset ({})", start);
          }
          else
          {
            const std::size_t stop = std::min(Data.Size(), state.Channels[chanNum].Offset + 1);
            Ranges.AddFixed(start, stop - start);
          }
        }
        return lineIdx >= MIN_PATTERN_SIZE;
      }

      bool HasLine(const ParserState& src) const
      {
        for (uint_t chan = 0; chan < 3; ++chan)
        {
          const ParserState::ChannelState& state = src.Channels[chan];
          if (state.Counter)
          {
            continue;
          }
          if (state.Offset >= Data.Size() || (0 == chan && 0xff == PeekByte(state.Offset)))
          {
            return false;
          }
        }
        return true;
      }

      void ParseLine(ParserState& src, PatternBuilder& patBuilder, Builder& builder) const
      {
        for (uint_t chan = 0; chan < 3; ++chan)
        {
          ParserState::ChannelState& state = src.Channels[chan];
          if (state.Counter)
          {
            --state.Counter;
            continue;
          }
          builder.StartChannel(chan);
          ParseChannel(state, patBuilder, builder);
          state.Counter = state.Period;
        }
      }

      void ParseChannel(ParserState::ChannelState& state, PatternBuilder& patBuilder, Builder& builder) const
      {
        while (state.Offset < Data.Size())
        {
          const uint_t cmd = PeekByte(state.Offset++);
          if (cmd <= 0x5f)
          {
            builder.SetNote(cmd);
            break;
          }
          else if (cmd <= 0x6f)
          {
            builder.SetSample(cmd - 0x60);
          }
          else if (cmd <= 0x7f)
          {
            builder.SetOrnament(cmd - 0x70);
          }
          else if (cmd == 0x80)
          {
            builder.SetRest();
            break;
          }
          else if (cmd == 0x81)
          {
            builder.SetNoEnvelope();
          }
          else if (cmd <= 0x8f)
          {
            const uint_t type = cmd - 0x81;
            const uint_t tone = PeekByte(state.Offset) | (uint_t(PeekByte(state.Offset + 1)) << 8);
            state.Offset += 2;
            builder.SetEnvelope(type, tone);
          }
          else if (cmd == 0x90)
          {
            break;
          }
          else if (cmd <= 0xa0)
          {
            patBuilder.SetTempo(cmd - 0x91);
          }
          else if (cmd <= 0xb0)
          {
            builder.SetVolume(cmd - 0xa1);
          }
          else
          {
            state.Period = cmd - 0xb1;
          }
        }
      }

      static void ParseSample(const RawSample& src, uint_t size, Sample& dst)
      {
        dst.Lines.resize(src.GetSize());
        for (uint_t idx = 0; idx < size; ++idx)
        {
          const RawSample::Line& line = src.GetLine(idx);
          dst.Lines[idx] = ParseSampleLine(line);
        }
        dst.Loop = std::min<uint_t>(src.Loop, dst.Lines.size());
      }

      static Sample::Line ParseSampleLine(const RawSample::Line& line)
      {
        Sample::Line res;
        res.Level = line.GetLevel();
        res.Noise = line.GetNoise();
        res.ToneMask = line.GetToneMask();
        res.NoiseMask = line.GetNoiseMask();
        res.Vibrato = line.GetVibrato();
        return res;
      }

      static void ParseOrnament(const RawOrnament& src, uint_t size, Ornament& dst)
      {
        dst.Lines.resize(src.GetSize());
        for (uint_t idx = 0; idx < size; ++idx)
        {
          dst.Lines[idx] = src.GetLine(idx);
        }
      }

    private:
      const Binary::View Data;
      RangesMap Ranges;
      const RawHeader& Source;
    };

    enum AreaTypes
    {
      HEADER,
      PATTERNS,
      END
    };

    struct Areas : public AreaController
    {
      Areas(const RawHeader& header, std::size_t size)
      {
        AddArea(HEADER, 0);
        AddArea(PATTERNS, header.PatternsOffset);
        AddArea(END, size);
      }

      bool CheckHeader(std::size_t headerSize) const
      {
        return GetAreaSize(HEADER) >= headerSize && Undefined == GetAreaSize(END);
      }

      bool CheckPatterns(std::size_t headerSize) const
      {
        return Undefined != GetAreaSize(PATTERNS) && headerSize == GetAreaAddress(PATTERNS);
      }
    };

    Binary::View MakeContainer(Binary::View rawData)
    {
      return rawData.SubView(0, MAX_SIZE);
    }

    bool FastCheck(Binary::View data)
    {
      const std::size_t hdrSize = GetHeaderSize(data);
      if (!Math::InRange<std::size_t>(hdrSize, sizeof(RawHeader) + 1, sizeof(RawHeader) + MAX_POSITIONS_COUNT))
      {
        return false;
      }
      const auto& hdr = *data.As<RawHeader>();
      const Areas areas(hdr, data.Size());
      if (!areas.CheckHeader(hdrSize))
      {
        return false;
      }
      if (!areas.CheckPatterns(hdrSize))
      {
        return false;
      }
      return true;
    }

    const auto DESCRIPTION = PROGRAM;
    const auto FORMAT =
        "02-0f"  // uint8_t Tempo; 2..15
        "01-ff"  // uint8_t Length;
        "00-fe"  // uint8_t Loop;
        // std::array<uint16_t, 16> SamplesOffsets;
        "(?00-28){16}"
        // std::array<uint16_t, 16> OrnamentsOffsets;
        "(?00-28){16}"
        "?00-01"    // uint16_t PatternsOffset;
        "?{30}"     // char Name[30];
        "00-1f"     // uint8_t Positions[1]; at least one
        "ff|00-1f"  // next position or limit
        ""sv;

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::CreateFormat(FORMAT, MIN_SIZE))
      {}

      StringView GetDescription() const override
      {
        return DESCRIPTION;
      }

      Binary::Format::Ptr GetFormat() const override
      {
        return Format;
      }

      bool Check(Binary::View rawData) const override
      {
        const auto data = MakeContainer(rawData);
        return Format->Match(data) && FastCheck(data);
      }

      Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const override
      {
        if (!Format->Match(rawData))
        {
          return {};
        }
        Builder& stub = GetStubBuilder();
        return Parse(rawData, stub);
      }

    private:
      const Binary::Format::Ptr Format;
    };

    Formats::Chiptune::Container::Ptr Parse(const Binary::Container& rawData, Builder& target)
    {
      const auto data = MakeContainer(rawData);

      if (!FastCheck(data))
      {
        return {};
      }

      try
      {
        const Format format(data);

        format.ParseCommonProperties(target);

        StatisticCollectingBuilder statistic(target);
        format.ParsePositions(statistic);
        const Indices& usedPatterns = statistic.GetUsedPatterns();
        format.ParsePatterns(usedPatterns, statistic);
        const Indices& usedSamples = statistic.GetUsedSamples();
        format.ParseSamples(usedSamples, target);
        const Indices& usedOrnaments = statistic.GetUsedOrnaments();
        format.ParseOrnaments(usedOrnaments, target);

        Require(format.GetSize() >= MIN_SIZE);
        auto subData = rawData.GetSubcontainer(0, format.GetSize());
        const auto fixedRange = format.GetFixedArea();
        return CreateCalculatingCrcContainer(std::move(subData), fixedRange.first,
                                             fixedRange.second - fixedRange.first);
      }
      catch (const std::exception&)
      {
        Dbg("Failed to create");
        return {};
      }
    }

    Builder& GetStubBuilder()
    {
      static StubBuilder stub;
      return stub;
    }
  }  // namespace ProTracker1

  Decoder::Ptr CreateProTracker1Decoder()
  {
    return MakePtr<ProTracker1::Decoder>();
  }
}  // namespace Formats::Chiptune
