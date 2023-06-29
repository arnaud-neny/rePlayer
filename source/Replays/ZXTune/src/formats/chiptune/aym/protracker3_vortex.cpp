/**
 *
 * @file
 *
 * @brief  ProTracker v3.x support implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "formats/chiptune/aym/protracker3_detail.h"
#include "formats/chiptune/container.h"
// common includes
#include <contract.h>
#include <make_ptr.h>
// library includes
#include <binary/container_factories.h>
#include <binary/format_factories.h>
#include <binary/input_stream.h>
#include <debug/log.h>
#include <math/numeric.h>
#include <strings/conversion.h>
#include <strings/format.h>
// std includes
#include <array>
#include <cctype>
#include <sstream>
// boost includes
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>

namespace Formats::Chiptune
{
  namespace ProTracker3::VortexTracker2
  {
    const Debug::Stream Dbg("Formats::Chiptune::VortexTracker2");

    const Char EDITOR[] = "VortexTracker (Pro Tracker v{}.{})";

    /*
      Common module structure:

      Header
      Ornaments
      Samples
      Patterns

      Header:
       [Module]
       VortexTrackerII=0/1
       Version=${major}.${minor}
       Title=...
       Author=...
       NoteTable=...
       Speed=...
       PlayOrder=<list of indices, delimited by comma. Single L prefix for loop position>
       <Field>=<Value>
       <empty string(s)>

      Ornament:
       [Ornament${index}]
       <same as PlayOrder>
       <empty string(s)>

      Sample:
       [Sample${index}]
       <sample string>
       <empty string(s)>

      sample string ::= ${toneMask}${noiseMask}${envMask} ${toneOffset}${keepToneOffset}
      ${noiseEnvOffset}${keepNoiseEnvOffset} ${volume}${volSlideAddon}< L if looped>

        toneMask ::= t<true> T<false>
        noiseMask ::= n<true> N<false>
        envMask ::= e<true> E<false>
        toneOffset ::= ${sign}${hex3}
        keepToneOffset ::= ^<true> _<false>

        noiseEnvOffset ::= ${sign}${hex2}
        keepNoiseEnvOffset ::= ^<true> _<false>

        volume ::= ${hex1}
        volSlideAddon ::= +<pos> -<neg> _<zero>

      Pattern:
       [Pattern${index}]
       <pattern string>
       <empty string(s)>

      pattern string ::= ${envBase}|${noiseBase}|${channel}|${channel}|${channel}

      envBase ::= ${hexdot4}
      noiseBase ::= ${hexdot2}
      channel ::= ${note} ${sampleNum}${envType}${ornamentNum}${volume} ${commands}
      note ::= --- or R-- or ${letter}${sharp}${octave}
      sampleNum ::= ${hexdot1}
      envType ::= ${hexdot1}
      ornamentNum ::= ${hexdot1}
      volume ::= ${hexdot1}
      commands ::= ${code}${param1}${param2}
      code ::= ${hexdot1}
      param1 ::= ${hexdot1}
      param2 ::= ${hexdot2}
    */

    /*
      Fundamental types wrappers
    */
    template<char True, char False>
    class BoolObject
    {
    public:
      BoolObject()
        : Value(false)
      {}

      explicit BoolObject(char val)
        : Value(val)
      {
        Require(val == True || val == False);
      }

      explicit BoolObject(bool val)
        : Value(val ? True : False)
      {}

      bool AsBool() const
      {
        return Value == True;
      }

      char AsChar() const
      {
        return Value;
      }

    private:
      const char Value;
    };

    typedef BoolObject<'+', '-'> SignFlag;

    template<char Max, char AltZero>
    class NibbleObject
    {
    public:
      NibbleObject()
        : Value(AltZero)
      {}

      explicit NibbleObject(char val)
        : Value(std::toupper(val))
      {
        Require(std::isdigit(val) || val == AltZero || Math::InRange<char>(val, 'a', std::tolower(Max))
                || Math::InRange<char>(val, 'A', std::toupper(Max)));
      }

      explicit NibbleObject(uint_t val)
        : Value(val >= 10 ? 'A' + val - 10 : (val ? '0' + val : AltZero))
      {
        Require(Value <= std::toupper(Max));
      }

      uint_t AsInt() const
      {
        return std::isdigit(Value) ? (Value - '0') : (Value == AltZero ? 0 : Value - 'A' + 10);
      }

      char AsChar() const
      {
        return Value;
      }

      NibbleObject<Max, AltZero>& operator=(uint_t val)
      {
        return *this = NibbleObject<Max, AltZero>(val);
      }

    private:
      char Value;
    };

    typedef NibbleObject<'F', '0'> SimpleNibble;
    typedef NibbleObject<'F', '.'> DottedNibble;

    template<uint_t Width, char AltZero>
    class UnsignedHexObject
    {
    public:
      UnsignedHexObject()
        : Value(0)
      {}

      explicit UnsignedHexObject(StringView val)
        : Value(0)
      {
        Require(val.size() == Width);
        for (const auto sym : val)
        {
          Value = Value * 16 + DottedNibble(sym).AsInt();
        }
      }

      explicit UnsignedHexObject(uint_t val)
        : Value(val)
      {}

      uint_t AsInt() const
      {
        return Value;
      }

      String AsString() const
      {
        String res(Width, AltZero);
        uint_t val = Value;
        for (uint_t idx = 0; val && idx != Width; ++idx, val >>= 4)
        {
          res[Width - idx - 1] = DottedNibble(val & 15).AsChar();
        }
        // VT export ignores wide numbers
        // Require(val == 0);
        return res;
      }

      UnsignedHexObject<Width, AltZero>& operator=(uint_t val)
      {
        Value = val;
        return *this;
      }

    private:
      uint_t Value;
    };

    template<uint_t Width>
    class SignedHexObject
    {
    public:
      SignedHexObject()
        : Value(0)
      {}

      explicit SignedHexObject(StringView val)
        : Value(0)
      {
        Require(val.size() == Width + 1);
        auto it = val.begin();
        const SignFlag sign(*it);
        for (++it; it != val.end(); ++it)
        {
          Value = Value * 16 + SimpleNibble(*it).AsInt();
        }
        if (!sign.AsBool())
        {
          Value = -Value;
        }
      }

      explicit SignedHexObject(int_t val)
        : Value(val)
      {}

      int_t AsInt() const
      {
        return Value;
      }

      String AsString() const
      {
        String res(Width + 1, '0');
        uint_t val = Math::Absolute(Value);
        for (uint_t idx = 0; val != 0 && idx != Width; ++idx, val >>= 4)
        {
          res[Width - idx] = SimpleNibble(val & 15).AsChar();
        }
        res[0] = SignFlag(Value >= 0).AsChar();
        Require(val == 0);
        return res;
      }

    private:
      int_t Value;
    };

    /*
      Format-specific types
    */

    class SectionHeader
    {
      static const uint_t NO_INDEX = ~uint_t(0);

    public:
      SectionHeader(const String& category, StringView hdr)
        : Category(category)
        , Index(NO_INDEX)
        , Valid(false)
      {
        const String start = '[' + category;
        const String stop = "]";
        if (boost::algorithm::istarts_with(hdr, start) && boost::algorithm::ends_with(hdr, stop))
        {
          Valid = true;
          const auto numStr = hdr.substr(start.size(), hdr.size() - start.size() - stop.size());
          Strings::Parse(numStr, Index);
        }
      }

      explicit SectionHeader(String category)
        : Category(std::move(category))
        , Index(NO_INDEX)
        , Valid(true)
      {}

      SectionHeader(String category, int_t idx)
        : Category(std::move(category))
        , Index(idx)
        , Valid(true)
      {}

      void Dump(std::ostream& str) const
      {
        Require(Valid);
        str << '[' << Category;
        if (Index != NO_INDEX)
        {
          str << Index;
        }
        str << "]\n";
      }

      uint_t GetIndex() const
      {
        Require(Index != NO_INDEX);
        return Index;
      }

      operator bool() const
      {
        return Valid;
      }

    private:
      const String Category;
      uint_t Index;
      String Str;
      bool Valid;
    };

    template<class T>
    struct LoopedList : std::vector<T>
    {
      typedef std::vector<T> Parent;

      LoopedList()
        : Loop(0)
      {}

      explicit LoopedList(StringView str)
        : Loop(0)
      {
        const std::size_t NO_LOOP = ~std::size_t(0);

        std::vector<StringView> elems;
        boost::algorithm::split(elems, str, boost::algorithm::is_from_range(',', ','));
        Parent::resize(elems.size());
        std::size_t resLoop = NO_LOOP;
        for (std::size_t idx = 0; idx != elems.size(); ++idx)
        {
          auto elem = elems[idx];
          Require(!elem.empty());
          if ('L' == elem[0])
          {
            Require(resLoop == NO_LOOP);
            resLoop = idx;
            elem = elem.substr(1);
          }
          Parent::at(idx) = Strings::ConvertTo<T>(elem);
        }
        Loop = resLoop == NO_LOOP ? 0 : resLoop;
      }

      LoopedList(uint_t loop, std::vector<T> vals)
        : Parent(std::move(vals))
        , Loop(loop)
      {
        Require(Math::InRange<std::size_t>(loop, 0, Parent::size() - 1));
      }

      uint_t GetLoop() const
      {
        return Loop;
      }

      void Dump(std::ostream& str) const
      {
        for (uint_t idx = 0; idx != Parent::size(); ++idx)
        {
          if (idx != 0)
          {
            str << ',';
          }
          if (idx == Loop)
          {
            str << 'L';
          }
          str << Parent::at(idx);
        }
      }

    private:
      uint_t Loop;
    };

    class StringStream
    {
    public:
      explicit StringStream(Binary::InputStream& delegate)
        : Delegate(delegate)
      {}

      StringView ReadString()
      {
        return Strings::TrimSpaces(Delegate.ReadString());
      }

      std::size_t GetPosition() const
      {
        return Delegate.GetPosition();
      }

      std::size_t GetRestSize() const
      {
        return Delegate.GetRestSize();
      }

    private:
      Binary::InputStream& Delegate;
    };

    /*
       Format parts types
    */

    struct ModuleHeader
    {
    public:
      ModuleHeader()
        : Version(0)
        , Table(PROTRACKER)
        , Tempo(0)
      {}

      explicit ModuleHeader(StringStream& src)
        : Version(0)
        , Table(PROTRACKER)
        , Tempo(0)
      {
        const SectionHeader hdr("Module", src.ReadString());
        Require(hdr);
        for (auto line = src.ReadString(); !line.empty(); line = src.ReadString())
        {
          Entry entry(line);
          Dbg(" {}={}", entry.Name, entry.Value);
          if (boost::algorithm::iequals(entry.Name, "Version"))
          {
            static const String VERSION("3.");
            Require(boost::algorithm::starts_with(entry.Value, VERSION));
            const String minorVal = entry.Value.substr(VERSION.size());
            const auto minor = Strings::ConvertTo<uint_t>(minorVal);
            Require(minor < 10);
            Version = minor;
          }
          else if (boost::algorithm::iequals(entry.Name, "Title"))
          {
            Title = std::move(entry.Value);
          }
          else if (boost::algorithm::iequals(entry.Name, "Author"))
          {
            Author = std::move(entry.Value);
          }
          else if (boost::algorithm::iequals(entry.Name, "NoteTable"))
          {
            const auto table = Strings::ConvertTo<uint_t>(entry.Value);
            Table = static_cast<NoteTable>(table);
          }
          else if (boost::algorithm::iequals(entry.Name, "Speed"))
          {
            Tempo = Strings::ConvertTo<uint_t>(entry.Value);
          }
          else if (boost::algorithm::iequals(entry.Name, "PlayOrder"))
          {
            PlayOrder = LoopedList<uint_t>(entry.Value);
          }
          else
          {
            OtherFields.push_back(std::move(entry));
          }
        }
        Require(Tempo != 0);
        Require(!PlayOrder.empty());
      }

      void Dump(std::ostream& str) const
      {
        Require(Tempo != 0);
        Require(!PlayOrder.empty());

        SectionHeader("Module").Dump(str);
        Entry("Version", "3." + Strings::ConvertFrom(Version)).Dump(str);
        Entry("Title", Title).Dump(str);
        Entry("Author", Author).Dump(str);
        Entry("NoteTable", Strings::ConvertFrom(static_cast<uint_t>(Table))).Dump(str);
        Entry("Speed", Strings::ConvertFrom(Tempo)).Dump(str);
        str << "PlayOrder=";
        PlayOrder.Dump(str);
        str << '\n';
        for (const auto& field : OtherFields)
        {
          field.Dump(str);
        }
        str << '\n';
      }

      struct Entry
      {
        String Name;
        String Value;

        explicit Entry(StringView str)
        {
          const auto sepPos = str.find('=');
          Require(sepPos != str.npos);
          const auto first = str.substr(0, sepPos);
          const auto second = str.substr(sepPos + 1);
          Name = Strings::TrimSpaces(first).to_string();
          Value = Strings::TrimSpaces(second).to_string();
        }

        Entry(String name, String value)
          : Name(std::move(name))
          , Value(std::move(value))
        {}

        Entry(Entry&& rh) noexcept  // = default
          : Name(std::move(rh.Name))
          , Value(std::move(rh.Value))
        {}

        void Dump(std::ostream& str) const
        {
          str << Name << '=' << Value << '\n';
        }
      };

      uint_t Version;
      String Title;
      String Author;
      NoteTable Table;
      uint_t Tempo;
      LoopedList<uint_t> PlayOrder;
      std::vector<Entry> OtherFields;
    };

    struct OrnamentObject : Ornament
    {
    public:
      OrnamentObject(const SectionHeader& header, StringStream& src)
        : Index(header.GetIndex())
      {
        Require(Math::InRange<uint_t>(Index, 0, MAX_ORNAMENTS_COUNT - 1));
        Dbg("Parse ornament {}", Index);
        const LoopedList<int_t> llist(src.ReadString());
        Require(src.ReadString().empty());
        Loop = llist.GetLoop();
        Lines = llist;
      }

      OrnamentObject(Ornament orn, uint_t index)
        : Ornament(std::move(orn))
        , Index(index)
      {}

      OrnamentObject(const OrnamentObject&) = delete;
      OrnamentObject& operator=(const OrnamentObject&) = delete;

      OrnamentObject(OrnamentObject&& rh) noexcept  // = default
        : Ornament(std::move(rh))
        , Index(rh.Index)
      {}

      uint_t GetIndex() const
      {
        return Index;
      }

      void Dump(std::ostream& str) const
      {
        SectionHeader("Ornament", Index).Dump(str);
        LoopedList<int_t>(Loop, Lines).Dump(str);
        str << "\n\n";
      }

      static SectionHeader ParseHeader(StringView hdr)
      {
        return SectionHeader("Ornament", hdr);
      }

    private:
      uint_t Index;
    };

    struct SampleObject : Sample
    {
      static const std::size_t NO_LOOP = ~std::size_t(0);

    public:
      SampleObject(const SectionHeader& header, StringStream& src)
        : Index(header.GetIndex())
      {
        Require(Math::InRange<uint_t>(Index, 0, MAX_SAMPLES_COUNT - 1));
        Dbg("Parse sample {}", Index);
        std::size_t loop = NO_LOOP;
        for (auto str = src.ReadString(); !str.empty(); str = src.ReadString())
        {
          const LineObject line(str);
          if (line.IsLooped())
          {
            Require(loop == NO_LOOP);
            loop = Lines.size();
          }
          Lines.push_back(line);
        }
        Require(loop != NO_LOOP);
        Loop = loop;
      }

      SampleObject(Sample sam, uint_t idx)
        : Sample(std::move(sam))
        , Index(idx)
      {}

      SampleObject(const SampleObject&) = delete;
      SampleObject& operator=(const SampleObject&) = delete;

      SampleObject(SampleObject&& rh) noexcept  // = default
        : Sample(std::move(rh))
        , Index(rh.Index)
      {}

      uint_t GetIndex() const
      {
        return Index;
      }

      void Dump(std::ostream& str) const
      {
        SectionHeader("Sample", Index).Dump(str);
        if (Lines.empty())
        {
          LineObject(Line(), true).Dump(str);
        }
        for (std::size_t idx = 0; idx != Lines.size(); ++idx)
        {
          LineObject(Lines[idx], idx == Loop).Dump(str);
        }
        str << '\n';
      }

      static SectionHeader ParseHeader(StringView hdr)
      {
        return SectionHeader("Sample", hdr);
      }

    private:
      struct LineObject : Line
      {
      public:
        explicit LineObject(const StringView str)
          : Looped(false)
        {
          std::vector<StringView> fields;
          boost::algorithm::split(fields, str, boost::algorithm::is_from_range(' ', ' '));
          switch (fields.size())
          {
          case 5:
            Require(fields[4] == "L"_sv);
            Looped = true;
            [[fallthrough]];
          case 4:
            ParseMasks(fields[0]);
            ParseToneOffset(fields[1]);
            ParseNoiseOffset(fields[2]);
            ParseVolume(fields[3]);
            break;
          default:
            Require(false);
          }
        }

        LineObject(Line src, bool looped)
          : Sample::Line(std::move(src))
          , Looped(looped)
        {}

        bool IsLooped() const
        {
          return Looped;
        }

        void Dump(std::ostream& str) const
        {
          str << UnparseMasks() << ' ' << UnparseToneOffset() << ' ' << UnparseNoiseOffset() << ' ' << UnparseVolume();
          if (Looped)
          {
            str << " L";
          }
          str << '\n';
        }

      private:
        typedef BoolObject<'t', 'T'> ToneFlag;
        typedef BoolObject<'n', 'N'> NoiseFlag;
        typedef BoolObject<'e', 'E'> EnvelopeFlag;
        typedef BoolObject<'^', '_'> AccumulatorFlag;
        typedef SignedHexObject<3> ToneValue;
        typedef SignedHexObject<2> NoiseEnvelopeValue;
        typedef SimpleNibble VolumeValue;

        void ParseMasks(StringView str)
        {
          Require(str.size() == 3);
          ToneMask = ToneFlag(str[0]).AsBool();
          NoiseMask = NoiseFlag(str[1]).AsBool();
          EnvMask = EnvelopeFlag(str[2]).AsBool();
        }

        String UnparseMasks() const
        {
          String res(3, ' ');
          res[0] = ToneFlag(ToneMask).AsChar();
          res[1] = NoiseFlag(NoiseMask).AsChar();
          res[2] = EnvelopeFlag(EnvMask).AsChar();
          return res;
        }

        void ParseToneOffset(StringView str)
        {
          Require(str.size() == 5);
          ToneOffset = ToneValue(str.substr(0, 4)).AsInt();
          KeepToneOffset = AccumulatorFlag(str[4]).AsBool();
        }

        String UnparseToneOffset() const
        {
          return ToneValue(ToneOffset).AsString() + AccumulatorFlag(KeepToneOffset).AsChar();
        }

        void ParseNoiseOffset(StringView str)
        {
          Require(str.size() == 4);
          NoiseOrEnvelopeOffset = NoiseEnvelopeValue(str.substr(0, 3)).AsInt();
          KeepNoiseOrEnvelopeOffset = AccumulatorFlag(str[3]).AsBool();
        }

        String UnparseNoiseOffset() const
        {
          return NoiseEnvelopeValue(NoiseOrEnvelopeOffset).AsString()
                 + AccumulatorFlag(KeepNoiseOrEnvelopeOffset).AsChar();
        }

        void ParseVolume(StringView str)
        {
          Require(str.size() == 2);
          Level = VolumeValue(str[0]).AsInt();
          VolumeSlideAddon = str[1] == '_' ? 0 : (SignFlag(str[1]).AsBool() ? +1 : -1);
        }

        String UnparseVolume() const
        {
          String res(1, VolumeValue(Level).AsChar());
          res += VolumeSlideAddon == 0 ? '_' : SignFlag(VolumeSlideAddon > 0).AsChar();
          return res;
        }

      private:
        bool Looped;
      };

    private:
      uint_t Index;
    };

    const std::array<String, 12> NOTES = {{"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"}};

    const String EMPTY_NOTE("---");
    const String REST_NOTE("R--");

    class NoteObject
    {
    public:
      NoteObject()
        : Val(EMPTY_NOTE)
      {}

      explicit NoteObject(StringView val)
        : Val(val.to_string())
      {
        Require(val.size() == 3);
      }

      explicit NoteObject(uint_t val)
      {
        const uint_t halftone = val % NOTES.size();
        const uint_t octave = 1 + val / NOTES.size();
        Require(Math::InRange<uint_t>(octave, 1, 8));
        Val = NOTES[halftone] + char('0' + octave);
      }

      void Parse(Builder& builder) const
      {
        if (Val == REST_NOTE)
        {
          builder.SetRest();
        }
        else if (Val != EMPTY_NOTE)
        {
          const uint_t res = AsInt();
          builder.SetNote(res);
        }
      }

      String AsString() const
      {
        return Val;
      }

      static NoteObject CreateRest()
      {
        return NoteObject(REST_NOTE);
      }

    private:
      uint_t AsInt() const
      {
        const auto notePos = std::find(NOTES.begin(), NOTES.end(), Val.substr(0, 2));
        Require(notePos != NOTES.end());
        const uint_t halftone = notePos - NOTES.begin();
        const char octave = Val[2];
        Require(Math::InRange(octave, '1', '8'));
        return NOTES.size() * (octave - '1') + halftone;
      }

    private:
      String Val;
    };

    class NoteParametersObject
    {
    public:
      NoteParametersObject() {}

      explicit NoteParametersObject(StringView str)
      {
        Require(str.size() == 4);
        Sample = SampleNumber(str[0]);
        Envelope = EnvelopeType(str[1]);
        Ornament = OrnamentNumber(str[2]);
        Volume = VolumeLevel(str[3]);
      }

      void Parse(uint_t envBase, Builder& builder) const
      {
        if (const uint_t sample = Sample.AsInt())
        {
          builder.SetSample(sample);
        }
        const uint_t ornament = Ornament.AsInt();
        if (const uint_t envType = Envelope.AsInt())
        {
          if (15 == envType)
          {
            builder.SetNoEnvelope();
          }
          else
          {
            builder.SetEnvelope(envType, envBase);
          }
          builder.SetOrnament(ornament);
        }
        else if (ornament)
        {
          builder.SetOrnament(ornament);
        }
        if (const int_t volume = Volume.AsInt())
        {
          builder.SetVolume(volume);
        }
      }

      String AsString() const
      {
        String res(4, ' ');
        res[0] = Sample.AsChar();
        res[1] = Envelope.AsChar();
        res[2] = Ornament.AsChar();
        res[3] = Volume.AsChar();
        return res;
      }

      typedef NibbleObject<'Z', '.'> SampleNumber;
      typedef DottedNibble EnvelopeType;
      typedef DottedNibble OrnamentNumber;
      typedef DottedNibble VolumeLevel;

      SampleNumber Sample;
      EnvelopeType Envelope;
      OrnamentNumber Ornament;
      VolumeLevel Volume;
    };

    class NoteCommandObject
    {
    public:
      static const uint_t GLISS_UP = 1;
      static const uint_t GLISS_DOWN = 2;
      static const uint_t GLISS_NOTE = 3;
      static const uint_t OFFSET_SAMPLE = 4;
      static const uint_t OFFSET_ORNAMENT = 5;
      static const uint_t VIBRATE = 6;
      static const uint_t ENVSLIDE_UP = 9;
      static const uint_t ENVSLIDE_DOWN = 10;
      static const uint_t TEMPO = 11;

      NoteCommandObject() {}

      explicit NoteCommandObject(StringView str)
      {
        Require(str.size() == 4);
        Command = CommandCode(str[0]);
        Period = CommandPeriod(str[1]);
        Param = CommandParameter(str.substr(2, 2));
      }

      void Parse(PatternBuilder& patBuilder, Builder& builder) const
      {
        const int_t period = Period.AsInt();
        const int_t param = Param.AsInt();
        switch (Command.AsInt())
        {
        case 0:
          break;  // no cmd
        case GLISS_UP:
          builder.SetGlissade(period, param);
          break;
        case GLISS_DOWN:
          builder.SetGlissade(period, static_cast<int16_t>(0xff00 + ((-param) & 0xff)));
          break;
        case GLISS_NOTE:
          builder.SetNoteGliss(period, param, 0 /*ignored*/);
          break;
        case OFFSET_SAMPLE:
          builder.SetSampleOffset(param);
          break;
        case OFFSET_ORNAMENT:
          builder.SetOrnamentOffset(param);
          break;
        case VIBRATE:
          builder.SetVibrate(param / 16, param % 16);
          break;
        case ENVSLIDE_UP:
          builder.SetEnvelopeSlide(period, param);
          break;
        case ENVSLIDE_DOWN:
          builder.SetEnvelopeSlide(period, -param);
          break;
        case TEMPO:
          if (param)
          {
            patBuilder.SetTempo(param);
          }
          break;
        default:
          break;
        }
      }

      String AsString() const
      {
        String res;
        res += Command.AsChar();
        res += Period.AsChar();
        res += Param.AsString();
        return res;
      }

      typedef DottedNibble CommandCode;
      typedef DottedNibble CommandPeriod;
      typedef UnsignedHexObject<2, '.'> CommandParameter;

      CommandCode Command;
      CommandPeriod Period;
      CommandParameter Param;
    };

    class ChannelObject
    {
    public:
      ChannelObject() {}

      explicit ChannelObject(StringView str)
      {
        std::vector<StringView> fields;
        boost::algorithm::split(fields, str, boost::algorithm::is_from_range(' ', ' '));
        Require(fields.size() == 3);
        Note = NoteObject(fields[0]);
        Parameters = NoteParametersObject(fields[1]);
        Command = NoteCommandObject(fields[2]);
      }

      void Parse(uint_t envBase, PatternBuilder& patBuilder, Builder& builder) const
      {
        Parameters.Parse(envBase, builder);
        Command.Parse(patBuilder, builder);
        Note.Parse(builder);
      }

      void Dump(std::ostream& str) const
      {
        str << Note.AsString() << ' ' << Parameters.AsString() << ' ' << Command.AsString();
      };

      NoteObject Note;
      NoteParametersObject Parameters;
      NoteCommandObject Command;
    };

    class PatternLineObject
    {
    public:
      PatternLineObject() {}

      explicit PatternLineObject(StringView str)
      {
        std::vector<StringView> fields;
        boost::algorithm::split(fields, str, boost::algorithm::is_from_range('|', '|'));
        Require(fields.size() == 5);
        Envelope = EnvelopeBase(fields[0]);
        Noise = NoiseBase(fields[1]);
        Channels[0] = ChannelObject(fields[2]);
        Channels[1] = ChannelObject(fields[3]);
        Channels[2] = ChannelObject(fields[4]);
      }

      void Parse(PatternBuilder& patBuilder, Builder& builder) const
      {
        const uint_t envBase = Envelope.AsInt();
        for (uint_t idx = 0; idx != 3; ++idx)
        {
          builder.StartChannel(idx);
          Channels[idx].Parse(envBase, patBuilder, builder);
        }
        builder.SetNoiseBase(Noise.AsInt());
      }

      void Dump(std::ostream& str) const
      {
        str << Envelope.AsString() << '|' << Noise.AsString();
        for (const auto& chan : Channels)
        {
          str << '|';
          chan.Dump(str);
        }
        str << '\n';
      }

      typedef UnsignedHexObject<4, '.'> EnvelopeBase;
      typedef UnsignedHexObject<2, '.'> NoiseBase;

      EnvelopeBase Envelope;
      NoiseBase Noise;
      std::array<ChannelObject, 3> Channels;
    };

    class PatternObject
    {
    public:
      PatternObject()
        : Index()
      {}

      explicit PatternObject(uint_t idx)
        : Index(idx)
      {
        Lines.reserve(MAX_PATTERN_SIZE);
      }

      PatternObject(const SectionHeader& header, StringStream& src)
        : Index(header.GetIndex())
      {
        Require(Math::InRange<uint_t>(Index, 0, MAX_PATTERNS_COUNT - 1));
        Dbg("Parse pattern {}", Index);
        Lines.reserve(MAX_PATTERN_SIZE);
        for (auto line = src.ReadString(); !line.empty();
             line = 0 != src.GetRestSize() ? src.ReadString() : StringView())
        {
          Lines.push_back(PatternLineObject(line));
        }
      }

      void Parse(Builder& builder) const
      {
        PatternBuilder& patBuilder = builder.StartPattern(Index);
        for (std::size_t idx = 0; idx != Lines.size(); ++idx)
        {
          patBuilder.StartLine(idx);
          Lines[idx].Parse(patBuilder, builder);
        }
        patBuilder.Finish(Lines.size());
      }

      void Dump(std::ostream& str) const
      {
        SectionHeader("Pattern", Index).Dump(str);
        for (const auto& line : Lines)
        {
          line.Dump(str);
        }
        str << '\n';
      }

      std::size_t GetSize() const
      {
        return Lines.size();
      }

      void AddLines(std::size_t count)
      {
        Lines.resize(Lines.size() + count);
      }

      PatternLineObject& AddLine()
      {
        AddLines(1);
        return Lines.back();
      }

      static SectionHeader ParseHeader(StringView str)
      {
        return SectionHeader("Pattern", str);
      }

    private:
      uint_t Index;
      std::vector<PatternLineObject> Lines;
    };

    class Format
    {
    public:
      Format(Binary::InputStream& source, Builder& target)
        : Source(source)
        , Target(target)
      {}

      void ParseHeader()
      {
        Dbg("Parse header");
        const ModuleHeader hdr(Source);
        MetaBuilder& meta = Target.GetMetaBuilder();
        meta.SetProgram(Strings::Format(EDITOR, 3, hdr.Version));
        Target.SetVersion(hdr.Version);
        meta.SetTitle(DecodeString(hdr.Title));
        meta.SetAuthor(DecodeString(hdr.Author));
        Target.SetNoteTable(hdr.Table);
        Target.SetInitialTempo(hdr.Tempo);
        Positions pos;
        pos.Loop = hdr.PlayOrder.GetLoop();
        pos.Lines = hdr.PlayOrder;
        Target.SetPositions(std::move(pos));
      }

      std::size_t ParseBody()
      {
        Dbg("Parse body");
        for (;;)
        {
          const std::size_t startLinePos = Source.GetPosition();
          if (0 == Source.GetRestSize())
          {
            return startLinePos;
          }
          const auto line = Source.ReadString();
          if (line.empty())
          {
            return Source.GetPosition();
          }
          if (const SectionHeader ornHdr = OrnamentObject::ParseHeader(line))
          {
            Target.SetOrnament(ornHdr.GetIndex(), OrnamentObject(ornHdr, Source));
          }
          else if (const SectionHeader samHdr = SampleObject::ParseHeader(line))
          {
            Target.SetSample(samHdr.GetIndex(), SampleObject(samHdr, Source));
          }
          else if (const SectionHeader patHdr = PatternObject::ParseHeader(line))
          {
            const PatternObject pat(patHdr, Source);
            pat.Parse(Target);
          }
          else
          {
            return startLinePos;
          }
        }
      }

    private:
      StringStream Source;
      Builder& Target;
    };

    const Char DESCRIPTION[] = "VortexTracker II";
    const auto FORMAT = "'['M'o'd'u'l'e']"_sv;

    const std::size_t MIN_SIZE = 256;

    void CheckIsSubset(const Indices& used, const Indices& available)
    {
      for (Indices::Iterator it(used); it; ++it)
      {
        Require(available.Contain(*it));
      }
    }

    Formats::Chiptune::Container::Ptr ParseText(const Binary::Container& data, Builder& target)
    {
      try
      {
        Binary::InputStream input(data);
        StatisticCollectingBuilder stat(target);
        VortexTracker2::Format format(input, stat);
        format.ParseHeader();
        const std::size_t limit = format.ParseBody();
        CheckIsSubset(stat.GetUsedPatterns(), stat.GetAvailablePatterns());
        CheckIsSubset(stat.GetUsedSamples(), stat.GetAvailableSamples());
        stat.SetOrnament(DEFAULT_ORNAMENT, Ornament());
        CheckIsSubset(stat.GetUsedOrnaments(), stat.GetAvailableOrnaments());

        auto subData = data.GetSubcontainer(0, limit);
        return CreateCalculatingCrcContainer(std::move(subData), 0, limit);
      }
      catch (const std::exception&)
      {
        Dbg("Failed to create");
        return {};
      }
    }

    class TextDecoder : public Decoder
    {
    public:
      TextDecoder()
        : Format(Binary::CreateFormat(FORMAT, MIN_SIZE))
      {}

      String GetDescription() const override
      {
        return DESCRIPTION;
      }

      Binary::Format::Ptr GetFormat() const override
      {
        return Format;
      }

      bool Check(Binary::View rawData) const override
      {
        return Format->Match(rawData);
      }

      Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const override
      {
        if (!Format->Match(rawData))
        {
          return {};
        }
        Builder& stub = GetStubBuilder();
        return ParseText(rawData, stub);
      }

      Formats::Chiptune::Container::Ptr Parse(const Binary::Container& data, Builder& target) const override
      {
        return ParseText(data, target);
      }

    private:
      const Binary::Format::Ptr Format;
    };

    class TextBuilder
      : public ChiptuneBuilder
      , public MetaBuilder
      , public PatternBuilder
    {
    public:
      TextBuilder()
        : Context(Patterns)
      {}

      MetaBuilder& GetMetaBuilder() override
      {
        return *this;
      }

      void SetProgram(StringView /*program*/) override {}

      void SetTitle(StringView title) override
      {
        Header.Title = title.to_string();
      }

      void SetAuthor(StringView author) override
      {
        Header.Author = author.to_string();
      }

      void SetStrings(const Strings::Array& /*strings*/) override {}

      void SetVersion(uint_t version) override
      {
        Header.Version = version;
      }

      void SetNoteTable(NoteTable table) override
      {
        Header.Table = table;
      }

      void SetMode(uint_t /*mode*/) override {}

      void SetInitialTempo(uint_t tempo) override
      {
        Header.Tempo = tempo;
      }

      void SetSample(uint_t index, Sample sample) override
      {
        Samples.push_back(SampleObject(std::move(sample), index));
      }

      void SetOrnament(uint_t index, Ornament ornament) override
      {
        if (index != DEFAULT_ORNAMENT)
        {
          Ornaments.push_back(OrnamentObject(std::move(ornament), index));
        }
      }

      void SetPositions(Positions positions) override
      {
        Header.PlayOrder = LoopedList<uint_t>(positions.Loop, std::move(positions.Lines));
      }

      PatternBuilder& StartPattern(uint_t index) override
      {
        Context.SetPattern(index);
        return *this;
      }

      void Finish(uint_t size) override
      {
        Context.FinishPattern(size);
      }

      void StartLine(uint_t index) override
      {
        Context.SetLine(index);
      }

      void SetTempo(uint_t tempo) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Command = cmd.TEMPO;
        cmd.Param = tempo;
      }

      void StartChannel(uint_t index) override
      {
        Context.SetChannel(index);
      }

      void SetRest() override
      {
        Context.CurChannel->Note = NoteObject::CreateRest();
      }

      void SetNote(uint_t note) override
      {
        Context.CurChannel->Note = NoteObject(note);
      }

      void SetSample(uint_t sample) override
      {
        Context.CurChannel->Parameters.Sample = NoteParametersObject::SampleNumber(sample);
      }

      void SetOrnament(uint_t ornament) override
      {
        Context.CurChannel->Parameters.Ornament = NoteParametersObject::OrnamentNumber(ornament);
      }

      void SetVolume(uint_t vol) override
      {
        Context.CurChannel->Parameters.Volume = NoteParametersObject::VolumeLevel(vol);
      }

      void SetGlissade(uint_t period, int_t val) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Period = period;
        if (val >= 0)
        {
          cmd.Command = cmd.GLISS_UP;
          cmd.Param = uint_t(val);
        }
        else
        {
          cmd.Command = cmd.GLISS_DOWN;
          cmd.Param = uint_t(-val);
        }
      }

      void SetNoteGliss(uint_t period, int_t val, uint_t /*limit*/) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Command = cmd.GLISS_NOTE;
        cmd.Period = period;
        cmd.Param = Math::Absolute(val);
      }

      void SetSampleOffset(uint_t offset) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Command = cmd.OFFSET_SAMPLE;
        cmd.Param = offset;
      }

      void SetOrnamentOffset(uint_t offset) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Command = cmd.OFFSET_ORNAMENT;
        cmd.Param = offset;
      }

      void SetVibrate(uint_t ontime, uint_t offtime) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Command = cmd.VIBRATE;
        cmd.Param = 16 * ontime + offtime;
      }

      void SetEnvelopeSlide(uint_t period, int_t val) override
      {
        NoteCommandObject& cmd = Context.CurChannel->Command;
        cmd.Period = period;
        if (val >= 0)
        {
          cmd.Command = cmd.ENVSLIDE_UP;
          cmd.Param = uint_t(val);
        }
        else
        {
          cmd.Command = cmd.ENVSLIDE_DOWN;
          cmd.Param = uint_t(-val);
        }
      }

      void SetEnvelope(uint_t type, uint_t value) override
      {
        Context.CurChannel->Parameters.Envelope = type;
        Context.CurLine->Envelope = value;
      }

      void SetNoEnvelope() override
      {
        Context.CurChannel->Parameters.Envelope = 15;
      }

      void SetNoiseBase(uint_t val) override
      {
        Context.SetNoiseBase(val);
      }

      Binary::Data::Ptr GetResult() const override
      {
        std::ostringstream str;
        Header.Dump(str);
        for (const auto& ornament : Ornaments)
        {
          ornament.Dump(str);
        }
        for (const auto& sample : Samples)
        {
          sample.Dump(str);
        }
        for (const auto& pattern : Patterns)
        {
          pattern.Dump(str);
        }
        const auto& res = str.str();
        return Binary::CreateContainer(Binary::View(res.data(), res.size()));
      }

    private:
      struct BuildContext
      {
        std::vector<PatternObject>& Patterns;
        PatternObject* CurPattern;
        PatternLineObject* CurLine;
        ChannelObject* CurChannel;
        uint_t CurNoiseBase;

        BuildContext(std::vector<PatternObject>& patterns)
          : Patterns(patterns)
          , CurPattern()
          , CurLine()
          , CurChannel()
          , CurNoiseBase()
        {}

        void SetPattern(uint_t idx)
        {
          Patterns.push_back(PatternObject(idx));
          CurPattern = &Patterns.back();
          CurLine = nullptr;
          CurChannel = nullptr;
          CurNoiseBase = 0;
        }

        void SetLine(uint_t idx)
        {
          FitTo(idx);
          AddLine();
        }

        void SetChannel(uint_t idx)
        {
          CurChannel = &CurLine->Channels[idx];
        }

        void FinishPattern(uint_t size)
        {
          FitTo(size);
          CurLine = nullptr;
          CurPattern = nullptr;
        }

        void SetNoiseBase(uint_t val)
        {
          CurLine->Noise = CurNoiseBase = val;
        }

      private:
        void FitTo(uint_t size)
        {
          const std::size_t skipped = size - CurPattern->GetSize();
          if (skipped != 0)
          {
            if (CurNoiseBase != 0)
            {
              for (uint_t lines = 0; lines != skipped; ++lines)
              {
                AddLine();
              }
            }
            else
            {
              CurPattern->AddLines(skipped);
            }
          }
        }

        void AddLine()
        {
          CurLine = &CurPattern->AddLine();
          CurLine->Noise = CurNoiseBase;
          CurChannel = nullptr;
        }
      };

      ModuleHeader Header;
      std::vector<OrnamentObject> Ornaments;
      std::vector<SampleObject> Samples;
      std::vector<PatternObject> Patterns;
      BuildContext Context;
    };

    Decoder::Ptr CreateDecoder()
    {
      return MakePtr<TextDecoder>();
    }

    ChiptuneBuilder::Ptr CreateBuilder()
    {
      return MakePtr<TextBuilder>();
    }
  }  // namespace ProTracker3::VortexTracker2

  Decoder::Ptr CreateVortexTracker2Decoder()
  {
    return ProTracker3::VortexTracker2::CreateDecoder();
  }
}  // namespace Formats::Chiptune
