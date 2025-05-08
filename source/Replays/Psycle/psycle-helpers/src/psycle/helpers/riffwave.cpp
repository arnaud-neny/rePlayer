#include "riffwave.hpp"
#include "sampleconverter.hpp"
namespace psycle { namespace helpers {

	const IffChunkId RiffWave::RF64 = {'R','F','6','4'};
	const IffChunkId RiffWave::WAVE = {'W','A','V','E'};
	const IffChunkId RiffWave::ds64 = {'d','s','6','4'};
	const IffChunkId RiffWave::data = {'d','a','t','a'};
	const IffChunkId RiffWave::fact = {'f','a','c','t'};
	const IffChunkId RiffWave::smpl = {'s','m','p','l'};
	const IffChunkId RiffWave::inst = {'i','n','s','t'};

	const IffChunkId RiffWaveFmtChunk::fmt = {'f','m','t',' '};
	const uint16_t RiffWaveFmtChunk::FORMAT_PCM = 1;
	const uint16_t RiffWaveFmtChunk::FORMAT_FLOAT = 3;
	const uint16_t RiffWaveFmtChunk::FORMAT_EXTENSIBLE = 0xFFFEU;
	const std::size_t RiffWaveFmtChunk::SIZEOF = 16;
	const std::size_t RiffWaveFmtChunkExtensible::SIZEOF = 22;
	const std::size_t RiffWaveSmplChunk::SIZEOF = 36;
	const std::size_t RiffWaveInstChunk::SIZEOF = 7;

	RiffWaveFmtChunk::RiffWaveFmtChunk() {}
	RiffWaveFmtChunk::RiffWaveFmtChunk(const WaveFormat_Data& config)
	{
		wFormatTag = config.isfloat ? FORMAT_FLOAT : FORMAT_PCM;
		wChannels = config.nChannels;
		dwSamplesPerSec = config.nSamplesPerSec;
		wBitsPerSample = config.nBitsPerSample;
		dwAvgBytesPerSec = config.nChannels * config.nSamplesPerSec * config.nBitsPerSample / 8;
		wBlockAlign = static_cast<uint16_t>(config.nChannels * config.nBitsPerSample / 8);
	}

	RiffWaveFmtChunkExtensible::RiffWaveFmtChunkExtensible() {}
	RiffWaveFmtChunkExtensible::RiffWaveFmtChunkExtensible(const RiffWaveFmtChunk& copy)
		:RiffWaveFmtChunk(copy)
		, extensionSize(SIZEOF)
		, numberOfValidBits(0)
		, speakerPositionMask(0)
	{
		subFormatTag.data1=0;
		subFormatTag.data2=0;
		subFormatTag.data3=0;
		subFormatTag.data4[0]='\0';
	}
	RiffWaveFmtChunkExtensible::RiffWaveFmtChunkExtensible(const WaveFormat_Data& config)
		: RiffWaveFmtChunk(config)
	{
	}

	void WaveFormat_Data::Config(uint32_t NewSamplingRate, uint16_t NewBitsPerSample, uint16_t NewNumChannels, bool useFloat)
	{
		isfloat = useFloat;
		nSamplesPerSec = NewSamplingRate;
		nChannels = NewNumChannels;
		nUsableBitsPerSample = nBitsPerSample = NewBitsPerSample;
	}
	void WaveFormat_Data::Config(const RiffWaveFmtChunk& chunk) 
	{
		Config(chunk.dwSamplesPerSec,chunk.wBitsPerSample, chunk.wChannels,
			chunk.wFormatTag == RiffWaveFmtChunk::FORMAT_FLOAT);

		if (chunk.wFormatTag != RiffWaveFmtChunk::FORMAT_PCM &&
			chunk.wFormatTag != RiffWaveFmtChunk::FORMAT_FLOAT) {
				nSamplesPerSec=0; //Indicator of bad format
		}
	}
	void WaveFormat_Data::Config(const RiffWaveFmtChunkExtensible& chunk)
	{
		const RiffWaveFmtChunkExtensible& chunkExt = reinterpret_cast<const RiffWaveFmtChunkExtensible&>(chunk);
		//TODO: finish WAVEFORMATEXTENSIBLE
		Config(chunk.dwSamplesPerSec,chunk.wBitsPerSample, chunk.wChannels,
			false /*chunkExt.subFormatTag*/);
		nUsableBitsPerSample = chunkExt.numberOfValidBits;
/*
		if (chunkExt.subFormatTag != RiffWaveFmtChunk::FORMAT_PCM &&
			chunk.wFormatTag != RiffWaveFmtChunk::FORMAT_FLOAT) {
				nSamplesPerSec=0; //Indicator of bad format
		}
*/
	}

	//////////////////////////////////////////////
	RiffWave::RiffWave() : ds64_pos(0),pcmdata_pos(0),numsamples(0)
	{
	}

	RiffWave::~RiffWave() {
	}

	void RiffWave::Open(const std::string& fname)
	{
		MsRiff::Open(fname);
		if (isValid) {
			if (Expect(WAVE)) {
				ReadFormat();
				if (formatdata.nSamplesPerSec==0) {
					isValid=false;
				}
			}
			else {
				isValid=false;
			}
		}
		else {
			//Not a RIFF or RIFX. Let's try RF64.
			MsRiff::Close();
			AbstractIff::Open(fname);
			isLittleEndian=true;
			currentHeader=&currentHeaderLE;
			const BaseChunkHeader& header = readHeader();
			if (header.matches(RF64) && Expect(WAVE)) {
				isValid=true;
				findChunk(ds64);
				ds64_pos = headerPosition;
				Skip(16); // skip riff and data sizes
				Long64LE sample;
				Read(sample);
				numsamples=static_cast<uint32_t>(sample.unsignedValue());
				skipThisChunk();
				ReadFormat();
				if (formatdata.nSamplesPerSec==0) {
					isValid=false;
				}
			}
		}
	}
    void RiffWave::Create(const std::string& fname, bool const  overwrite, bool const littleEndian)
	{
		MsRiff::Create(fname, overwrite, littleEndian);
		Write(WAVE);
		ds64_pos = 0; pcmdata_pos = 0; numsamples = 0;
	}
	void RiffWave::Close() { 
		if (isWriteMode() && isValid) {
			std::streamsize size = GetPos() - static_cast<std::streampos>(RiffChunkHeader::SIZEOF);
			UpdateFormSize(headerPosition,size - headerPosition);
			UpdateFormSize(0,size);
		}
		MsRiff::Close();
	}

	//TODO: Improvement map of chunk ids and positions, so that allowWrap is only used to seek back to
	// an already found chunk, not to read the whole file again.
	const BaseChunkHeader& RiffWave::findChunk(const IffChunkId& id, bool allowWrap)
	{
		while(!Eof()) {
			//TODO: Handle better the LIST form and subchunks
			readHeader();
			if (currentHeader->matches(id)) {
				return *currentHeader;
			}
			else if (tryPaddingFix) {
				Seek(headerPosition-static_cast<std::streampos>(1));
				continue;
			}
			else if (currentHeader->length() > 0) {
				Skip(currentHeader->length());
			}
			else {
				throw std::runtime_error(std::string("Couldn't read the file. found 0 byte header ") + currentHeader->idString());
			}
		};
		if (allowWrap && Eof()) {
			Seek(12);//Skip RIFF/WAVE header.
			return findChunk(id,false);
		}
		RiffChunkHeader head(id,0);
		throw std::runtime_error(std::string("couldn't find chunk ") + head.idString());
	}

	void RiffWave::ReadFormat()
	{
		findChunk(RiffWaveFmtChunk::fmt);
		RiffWaveFmtChunk chunk;
		Read(chunk.wFormatTag);
		Read(chunk.wChannels);
		Read(chunk.dwSamplesPerSec);
		Read(chunk.dwAvgBytesPerSec);
		Read(chunk.wBlockAlign);
		Read(chunk.wBitsPerSample);
		if (chunk.wFormatTag == RiffWaveFmtChunk::FORMAT_EXTENSIBLE) {
			RiffWaveFmtChunkExtensible chunkExt(chunk);
			if (currentHeader->length() < RiffWaveFmtChunk::SIZEOF + sizeof(chunkExt.extensionSize) ) {
				throw std::runtime_error(std::string("invalid format header length"));
			}
			Read(chunkExt.extensionSize);
			if (chunkExt.extensionSize < RiffWaveFmtChunkExtensible::SIZEOF) {
				throw std::runtime_error(std::string("invalid format header length"));
			}
			Read(chunkExt.numberOfValidBits);
			Read(chunkExt.speakerPositionMask);
			Read(chunkExt.subFormatTag.data1);
			Read(chunkExt.subFormatTag.data2);
			Read(chunkExt.subFormatTag.data3);
			ReadSizedString(chunkExt.subFormatTag.data4,sizeof(chunkExt.subFormatTag.data4));
			formatdata.Config(chunkExt);
		}
		else {
			formatdata.Config(chunk);
		}
		skipThisChunk();
		if ( numsamples == 0 && chunk.wFormatTag != RiffWaveFmtChunk::FORMAT_PCM) {
			try {
				findChunk(fact,true);
				Read(numsamples);
				skipThisChunk();
			}
			catch(const std::runtime_error & /*e*/) {
			}
		}
		if ( numsamples == 0 ) {
			try {
				findChunk(data,true);
				if (headerPosition+static_cast<std::streampos>(RiffChunkHeader::SIZEOF+currentHeader->length()) >= GuessedFilesize()) {
					numsamples = calcSamplesFromBytes(GuessedFilesize()-headerPosition-RiffChunkHeader::SIZEOF);
				}
				else {
					numsamples = calcSamplesFromBytes(currentHeader->length());
				}
				skipThisChunk();
			}
			catch(const std::runtime_error & /*e*/) {
			}
		}
		//Put it back on start, it is better for problematic files...
		Seek(12);//Skip RIFF/WAVE header.
	}
	void RiffWave::addFormat(uint32_t SamplingRate, uint16_t BitsPerSample, uint16_t NumChannels, bool isFloat)
	{
		formatdata.Config(SamplingRate, BitsPerSample, NumChannels, isFloat);
		pcmdata_pos = GetPos();
		RiffChunkHeader	header(RiffWaveFmtChunk::fmt,RiffWaveFmtChunk::SIZEOF);
		addNewChunk(header);
		//TODO WAVEFORMATEXTENSIBLE
		RiffWaveFmtChunk chunk(formatdata);
		Write(chunk.wFormatTag);
		Write(chunk.wChannels);
		Write(chunk.dwSamplesPerSec);
		Write(chunk.dwAvgBytesPerSec);
		Write(chunk.wBlockAlign);
		Write(chunk.wBitsPerSample);
	}
	void RiffWave::addSmplChunk(const RiffWaveSmplChunk& smpl)
	{
		RiffChunkHeader	header(RiffWave::smpl,RiffWaveSmplChunk::SIZEOF
				+ smpl.numLoops*sizeof(RiffWaveSmplLoopChunk)/*+smpl.extraDataSize*/);
		addNewChunk(header);
		Write(smpl.manufacturer);
		Write(smpl.product);
		Write(smpl.samplePeriod);
		Write(smpl.midiNote);
		Write(smpl.midiPitchFr);
		Write(smpl.smpteFormat);
		Write(smpl.smpteOffset);
		Write(smpl.numLoops);
		Write(smpl.extraDataSize);
		if (smpl.numLoops > 0) {
			for (int i=0;i< smpl.numLoops;i++) {
				Write(smpl.loops->cueId);
				Write(smpl.loops->type);
				Write(smpl.loops->start);
				Write(smpl.loops->end);
				Write(smpl.loops->fraction);
				Write(smpl.loops->playCount);
			}
		}
		if (smpl.extraDataSize > 0) {
			//Not implemented.
		}
	}
	void RiffWave::addInstChunk(const RiffWaveInstChunk& inst)
	{
		RiffChunkHeader	header(RiffWave::inst,RiffWaveInstChunk::SIZEOF);
		addNewChunk(header);
		Write(inst.midiNote);
		Write(inst.midiCents);
		Write(inst.gaindB);
		Write(inst.lowNote);
		Write(inst.highNote);
		Write(inst.lowVelocity);
		Write(inst.highVelocity);
	}

	void RiffWave::SeekToSample(uint32_t sampleIndex)
	{
		if (sampleIndex < numsamples ) {
			std::streampos size = formatdata.nChannels * ((formatdata.nBitsPerSample + 7) / 8);
			Seek(pcmdata_pos + std::streamoff(RiffChunkHeader::SIZEOF + size * sampleIndex));
		}
	}
	uint32_t RiffWave::calcSamplesFromBytes(uint32_t length)
	{
		return length/( formatdata.nChannels* ((formatdata.nBitsPerSample + 7) / 8));
	}

	//Psycle only uses 16bit samples, so this implementation is fully functional for Psycle's intentions.
	//It would fail to load multichannel (i.e. more than two) samples.
	void RiffWave::readInterleavedSamples(void* pSamps, uint32_t maxSamples, WaveFormat_Data* convertTo)
	{
		findChunk(data, true);
		uint32_t max = (maxSamples>0) ? maxSamples : numsamples;
		if (convertTo == NULL || *convertTo == formatdata)
		{
			readMonoSamples(pSamps, max*formatdata.nChannels);
		}
		else if (convertTo->nChannels == formatdata.nChannels) {
			//Finish for other destination bitdepths
			switch(convertTo->nBitsPerSample) 
			{
			case 8:readMonoConvertToInteger<uint8_t,assignconverter<uint8_t,uint8_t>,int16touint8,int24touint8,int32touint8,floattouint8,doubletouint8>
					(reinterpret_cast<uint8_t*>(pSamps),max*formatdata.nChannels,127.0);
				break;
			case 16:readMonoConvertToInteger<int16_t,uint8toint16,assignconverter<int16_t,int16_t>,int24toint16,int32toint16,floattoint16,doubletoint16>
					(reinterpret_cast<int16_t*>(pSamps),max*formatdata.nChannels,32767.0);
				break;
			case 24:break;
			case 32:if (convertTo->isfloat) {
					//Finish
				}
				else readMonoConvertToInteger<int32_t,uint8toint32,int16toint32,int24toint32,assignconverter<int32_t,int32_t>,floattoint32,doubletoint32>
					(reinterpret_cast<int32_t*>(pSamps),max,2147483648.0);
				break;
			default:break;
			}
		}
		else if (convertTo->nChannels == 1) {
			//Finish for multichannel (more than two)
		}
		else {
			//Finish for multichannel (more than two)
		}
		skipThisChunk();
	}
	//Psycle only uses 16bit samples, so this implementation is fully functional for Psycle's intentions.
	//It would fail to load multichannel (i.e. more than two) samples.
	void RiffWave::readDeInterleavedSamples(void** pSamps, uint32_t maxSamples, WaveFormat_Data* convertTo)
	{
		findChunk(data, true);
		uint32_t max = (maxSamples>0) ? maxSamples : numsamples;
		if (convertTo == NULL || *convertTo == formatdata)
		{
			if (formatdata.nChannels==1) {
				readMonoSamples(pSamps[0],max);
			}
			else {
				readDeintMultichanSamples(pSamps, max);
			}
		}
		else if (convertTo->nChannels == formatdata.nChannels) {
			//Finish for other destination bitdepths
			switch(convertTo->nBitsPerSample) 
			{
			case 8: {
				if (formatdata.nChannels ==1) {
					readMonoConvertToInteger<uint8_t,assignconverter<uint8_t,uint8_t>,int16touint8,int24touint8,int32touint8,floattouint8,doubletouint8>
					(reinterpret_cast<uint8_t*>(pSamps[0]),max,127.0);
				}
				else {
					readDeintMultichanConvertToInteger<uint8_t,assignconverter<uint8_t,uint8_t>,int16touint8,int24touint8,int32touint8,floattouint8,doubletouint8>
					(reinterpret_cast<uint8_t**>(pSamps),max,127.0);
				}
				break;
			}
			case 16: {
				if (formatdata.nChannels ==1) {
					readMonoConvertToInteger<int16_t,uint8toint16,assignconverter<int16_t,int16_t>,int24toint16,int32toint16,floattoint16,doubletoint16>
					(reinterpret_cast<int16_t*>(pSamps[0]),max,32767.0);
				}
				else {
					readDeintMultichanConvertToInteger<int16_t,uint8toint16,assignconverter<int16_t,int16_t>,int24toint16,int32toint16,floattoint16,doubletoint16>
					(reinterpret_cast<int16_t**>(pSamps),max,32767.0);
				}
				break;
			}
			case 24:break;
			case 32:
				if (convertTo->isfloat) {
					//Finish
				}
				else if (formatdata.nChannels ==1) {
					readMonoConvertToInteger<int32_t,uint8toint32,int16toint32,int24toint32,assignconverter<int32_t,int32_t>,floattoint32,doubletoint32>
					(reinterpret_cast<int32_t*>(pSamps[0]),max,2147483648.0);
				}
				else {
					readDeintMultichanConvertToInteger<int32_t,uint8toint32,int16toint32,int24toint32,assignconverter<int32_t,int32_t>,floattoint32,doubletoint32>
					(reinterpret_cast<int32_t**>(pSamps),max,2147483648.0);
				}

				break;
			default:break;
			}
		}
		else if (convertTo->nChannels == 1) {
			//Finish for multichannel (more than two)
		}
		else {
			//Finish for multichannel (more than two)
		}
		skipThisChunk();
	}

	//TODO: Improve readability when the smpl chunk is inside a LIST instead of inside the RIFF form.
	bool RiffWave::GetSmplChunk(RiffWaveSmplChunk& smplchunk)
	{
		try {
			findChunk(smpl,true);
		}
		catch(const std::runtime_error& e)
		{
			return false;
		}
		Read(smplchunk.manufacturer);
		Read(smplchunk.product);
		Read(smplchunk.samplePeriod);
		Read(smplchunk.midiNote);
		Read(smplchunk.midiPitchFr);
		Read(smplchunk.smpteFormat);
		Read(smplchunk.smpteOffset);
		Read(smplchunk.numLoops);
		Read(smplchunk.extraDataSize);
		if (smplchunk.numLoops>0){
			smplchunk.loops = new RiffWaveSmplLoopChunk[smplchunk.numLoops];
			for (size_t i=0; i < smplchunk.numLoops; ++i) {
				Read(smplchunk.loops[i].cueId);
				Read(smplchunk.loops[i].type);
				Read(smplchunk.loops[i].start);
				Read(smplchunk.loops[i].end);
				Read(smplchunk.loops[i].fraction);
				Read(smplchunk.loops[i].playCount);
			}
		}
		skipThisChunk();
		return true;
	}


	bool RiffWave::GetInstChunk(RiffWaveInstChunk& instchunk)
	{
		try {
			findChunk(inst,true);
		}
		catch(const std::runtime_error& e)
		{
			return false;
		}
		Read(instchunk.midiNote);	// 1 - 127
		Read(instchunk.midiCents);	// -50 - +50
		Read(instchunk.gaindB);		// -64 - +64
		Read(instchunk.lowNote);		// 1 - 127
		Read(instchunk.highNote);	// 1 - 127
		Read(instchunk.lowVelocity); // 1 - 127
		Read(instchunk.highVelocity); // 1 - 127
		skipThisChunk();
		return true;
	}


	void RiffWave::readMonoSamples(void* pSamp, uint32_t samples)
	{
		switch(formatdata.nBitsPerSample)
		{
			case 8: {
				uint8_t* smp8 = static_cast<uint8_t*>(pSamp);
				ReadArray(smp8, samples);
				break;
			}
			case 16: {
				int16_t* smp16 = static_cast<int16_t*>(pSamp);
				ReadArray(smp16, samples);
				break;
			}
			case 24: {
				if (isLittleEndian) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					Long24LE* samps = static_cast<Long24LE*>(pSamp);
					ReadArray(samps,samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					Long24BE* samps = static_cast<Long24BE*>(pSamp);
					ReadWithintegerconverter<Long24LE,Long24BE,endianessconverter<Long24LE,Long24BE> >(samps, samples);
#endif
				}
				else {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					Long24LE* samps = static_cast<Long24LE*>(pSamp);
					ReadWithintegerconverter<Long24BE,Long24LE,endianessconverter<Long24BE,Long24LE> >(samps, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					Long24BE* samps = static_cast<Long24BE*>(pSamp);
					ReadArray(samps,samples);
#endif
				}
				break;
			}
			case 32: {
				if (formatdata.isfloat) {
					float* smp32 = static_cast<float*>(pSamp);
					ReadArray(smp32, samples);
				}
				else {
					int32_t* smp32 = static_cast<int32_t*>(pSamp);
					ReadArray(smp32, samples);
				}
				break;
			}
			case 64: {
				double* smp32 = static_cast<double*>(pSamp);
				ReadArray(smp32, samples);
				break;
			}
			default:
				break;
		}
	}

	void RiffWave::readDeintMultichanSamples(void** pSamps, uint32_t samples)
	{
		switch (formatdata.nBitsPerSample) {
			case  8: readDeinterleaveSamples<uint8_t>(pSamps,formatdata.nChannels, samples);break;
			case 16: readDeinterleaveSamples<int16_t>(pSamps,formatdata.nChannels, samples);break;
			case 24: {
				if (isLittleEndian) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					readDeinterleaveSamples<Long24LE>(pSamps,formatdata.nChannels, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					readDeinterleaveSamplesendian<Long24LE, Long24BE>(pSamps,formatdata.nChannels, samples);
#endif
				}
				else {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					readDeinterleaveSamplesendian<Long24BE,Long24LE>(pSamps,formatdata.nChannels, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					readDeinterleaveSamples<Long24BE>(pSamps,formatdata.nChannels, samples);
#endif
				}
				break;
			}
			case 32: {
				if (formatdata.isfloat) {
					readDeinterleaveSamples<float>(pSamps,formatdata.nChannels, samples);
				}
				else {
					readDeinterleaveSamples<int32_t>(pSamps,formatdata.nChannels, samples);
				}
				break;
			}
			case 64: readDeinterleaveSamples<double>(pSamps,formatdata.nChannels, samples);break;
			default: break; ///< \todo should throw an exception
		}
	}



    void RiffWave::writeFromInterleavedSamples(void* pSamps, uint32_t samples, WaveFormat_Data* convertFrom)
	{
		if (!currentHeader->matches(data)) {
			pcmdata_pos = GetPos();
			RiffChunkHeader header(data,0);
			addNewChunk(header);
		}
		if (convertFrom == NULL || *convertFrom == formatdata)
		{
			writeMonoSamples(pSamps, samples*formatdata.nChannels);
		}
		else if (convertFrom->nChannels == formatdata.nChannels) {
			//Finish for other destination bitdepths
			switch(convertFrom->nBitsPerSample) 
			{
			case 8:break;
			case 16: break;
			case 24:break;
			case 32: {
				if (formatdata.isfloat) {
				}
				else {
				}
				break;
			 }
			default:break;
			}
		}
		else if (convertFrom->nChannels == 1) {
			//Finish for multichannel (more than two)
		}
		else {
			//Finish for multichannel (more than two)
		}
	}
    void RiffWave::writeFromDeInterleavedSamples(void** pSamps, uint32_t samples, WaveFormat_Data* convertFrom)
	{
		if (!currentHeader->matches(data)) {
			pcmdata_pos = GetPos();
			RiffChunkHeader header(data,0);
			addNewChunk(header);
		}
		if (convertFrom == NULL || *convertFrom == formatdata)
		{
			if (formatdata.nChannels==1) {
				writeMonoSamples(pSamps[0],samples);
			}
			else {
				writeDeintMultichanSamples(pSamps, samples);
			}
		}
		else if (convertFrom->nChannels == formatdata.nChannels) {
			//Finish for other destination bitdepths
			switch(convertFrom->nBitsPerSample) 
			{
			case 8:break;
			case 16: break;
			case 24:break;
			case 32: {
				if (formatdata.isfloat) {
				}
				else {
				}
				break;
			 }
			default:break;
			}
		}
		else if (convertFrom->nChannels == 1) {
			//Finish for multichannel (more than two)
		}
		else {
			//Finish for multichannel (more than two)
		}
	}

	void RiffWave::writeMonoSamples(void* pSamp, uint32_t samples)
	{
		switch(formatdata.nBitsPerSample)
		{
			case 8: {
				uint8_t* smp8 = static_cast<uint8_t*>(pSamp);
				WriteArray(smp8, samples);
				break;
			}
			case 16: {
				int16_t* smp16 = static_cast<int16_t*>(pSamp);
				WriteArray(smp16, samples);
				break;
			}
			case 24: {
				if (isLittleEndian) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					Long24LE* samps = static_cast<Long24LE*>(pSamp);
					WriteArray(samps,samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					Long24BE* samps = static_cast<Long24BE*>(pSamp);
					WriteWithintegerconverter<Long24BE,Long24LE,endianessconverter<Long24BE,Long24LE> >(samps, samples);
#endif
				}
				else {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					Long24LE* samps = static_cast<Long24LE*>(pSamp);
					WriteWithintegerconverter<Long24LE,Long24BE,endianessconverter<Long24LE,Long24BE> >(samps, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					Long24BE* samps = static_cast<Long24BE*>(pSamp);
					WriteArray(samps,samples);
#endif
				}
				break;
			}
			case 32: {
				if (formatdata.isfloat) {
					float* smp32 = static_cast<float*>(pSamp);
					WriteArray(smp32, samples);
				}
				else {
					int32_t* smp32 = static_cast<int32_t*>(pSamp);
					WriteArray(smp32, samples);
				}
				break;
			}
			case 64: {
				double* smp32 = static_cast<double*>(pSamp);
				WriteArray(smp32, samples);
				break;
			}
			default:
				break;
		}

	}
	void RiffWave::writeDeintMultichanSamples(void** pSamps, uint32_t samples)
	{
		switch (formatdata.nBitsPerSample) {
			case  8: WriteDeinterleaveSamples<uint8_t>(pSamps,formatdata.nChannels, samples);break;
			case 16: WriteDeinterleaveSamples<int16_t>(pSamps,formatdata.nChannels, samples);break;
			case 24: {
				if (isLittleEndian) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					WriteDeinterleaveSamples<Long24LE>(pSamps,formatdata.nChannels, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					WriteDeinterleaveSamplesendian<Long24LE, Long24BE>(pSamps,formatdata.nChannels, samples);
#endif
				}
				else {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					WriteDeinterleaveSamplesendian<Long24BE,Long24LE>(pSamps,formatdata.nChannels, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					WriteDeinterleaveSamples<Long24BE>(pSamps,formatdata.nChannels, samples);
#endif
				}
				break;
			}
			case 32: {
				if (formatdata.isfloat) {
					WriteDeinterleaveSamples<float>(pSamps,formatdata.nChannels, samples);
				}
				else {
					WriteDeinterleaveSamples<int32_t>(pSamps,formatdata.nChannels, samples);
				}
				break;
			}
			case 64: WriteDeinterleaveSamples<double>(pSamps,formatdata.nChannels, samples);break;
			default: break; ///< \todo should throw an exception
		}
	}



}}
