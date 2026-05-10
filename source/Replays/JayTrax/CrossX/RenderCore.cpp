#include "SoundEngine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//internal rendering structure to speed up innerloop
struct channelvar
{
	char issample;
	short *wavepnt;
	unsigned short localwavepos;
	short panl;
	short panr;
	short echovolumel;
	short echovolumer;
	short freqoffset;
	short wavelength;
	long  samplepos;
	short curdirecflg;
	int	  waveend;
	short loopflg;
	short bidirecflg;
	int   waveloop;
	short volume;
	short echovolume;
};


// optimized stereo renderer... High quality overlapmixbuf and interpolation
int SoundEngine::RenderChannels1(short *renderbuf, int nrofsamples, int frequency)
{
	channelvar channelvars[SE_MAXCHANS];
	channelvar *curvars;
	short i,p,c;
	int r;
	short nos;										//number of samples
	int lsample,rsample,echosamplel,echosampler;	//current state of samples
	short nrtomix;									// nr of wave streams to mix(is number of channels +1 for the echo)
	short amplification;							// amount to amplify afterwards
	unsigned short echodelaytime;					//delaytime for echo (differs for MIDI or songplayback)

	// eerst uitzoeken hoeveel er gerendered moet worden...
	// dit moet met de cylcische directsound buffer
	// nu nemen we effe voor het gemak aan dat het 1kb is

// nu gaan we nrofsamples berekenen maar in blokjes van timecnt (de songspd)
	if(m_NrOfChannels==0)
	{
		for(i=0;i<nrofsamples;i++)
		{
			renderbuf[i]=0;
		}
		return 0;
	}
	r=0;
	nrtomix=m_NrOfChannels+1;  //1 extra for the echoes
	while(nrofsamples>0)
	{
		// rePlayer begin
		if (m_HasLooped)
		{
			m_HasLooped &= r != 0;
			return r / 2;
		}
		// rePlayer end
// 2 mogelijkheden... we calcen een heel timecnt blokje...of een laatste stukje van een timecnt blokje...
		if(m_TimeCnt<nrofsamples)
		{
			nos = m_TimeCnt;   //geheel blok
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		}
		else
		{
			nos = nrofsamples;  // laatste stukje
			m_TimeCnt=m_TimeCnt-nos;  //m_TimeSpd-nos
		}
		nrofsamples-=nos;


		// preparation of wave pointers en freq offset
		for(i=0;i<m_NrOfChannels;i++)
		{
			int inst,instnr;
			instnr = m_ChannelData[i]->instrument;
			if (instnr == -1) // mute?
			{
				channelvars[i].wavepnt=m_EmptyWave;
				channelvars[i].issample=0;
				channelvars[i].wavelength = 0;
			}
			else
			{
				if(m_ChannelData[i]->sampledata)
				{
					channelvars[i].samplepos=m_ChannelData[i]->samplepos;
					channelvars[i].wavepnt=(short *)m_ChannelData[i]->sampledata;
					channelvars[i].waveloop=m_ChannelData[i]->looppoint; 
					channelvars[i].waveend=m_ChannelData[i]->endpoint; 
					channelvars[i].loopflg=m_ChannelData[i]->loopflg;
					channelvars[i].bidirecflg=m_ChannelData[i]->bidirecflg;
					channelvars[i].curdirecflg=m_ChannelData[i]->curdirecflg;
					channelvars[i].issample=1;
					channelvars[i].wavelength = 0;
				}
				else
				{
					inst = m_Instruments[instnr]->waveform;
					channelvars[i].wavepnt=&m_ChannelData[i]->waves[256*inst];
					channelvars[i].wavelength=((m_Instruments[instnr]->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					channelvars[i].issample=0;
				}
			}

			channelvars[i].localwavepos = m_WavePosition[i];

	//calcen frequency
			if(m_ChannelData[i]->curfreq<10)m_ChannelData[i]->curfreq=10;
			channelvars[i].freqoffset = (256*m_ChannelData[i]->curfreq)/frequency;
			channelvars[i].volume = (m_ChannelData[i]->curvol+10000)/39;
			if(channelvars[i].volume>256) channelvars[i].volume=256;

			if(m_ChannelData[i]->curpan==0) //panning?
			{
				channelvars[i].panl=256; //geen panning
				channelvars[i].panr=256;
			}
			else
			{
				if(m_ChannelData[i]->curpan>0)
				{
					channelvars[i].panl = 256-(m_ChannelData[i]->curpan);
					channelvars[i].panr = 256;
				}
				else
				{
					channelvars[i].panr = 256+(m_ChannelData[i]->curpan);
					channelvars[i].panl=256;
				}
			}

			channelvars[i].echovolume = m_Songs[m_CurrentSubsong]->delayamount[i];

			// stereo versnelling 1... alvast de panning vermenigvuldigen met de mainvolume
			channelvars[i].panl = (channelvars[i].panl * channelvars[i].volume)>>8;
			channelvars[i].panr = (channelvars[i].panr * channelvars[i].volume)>>8;

			//stereo versnelling...alvast de panning meeberekenen voor de echo
			channelvars[i].echovolumel = (channelvars[i].echovolume * channelvars[i].panl)>>8;
			channelvars[i].echovolumer = (channelvars[i].echovolume * channelvars[i].panr)>>8;

		}
		amplification = m_Songs[m_CurrentSubsong]->amplification;
		echodelaytime = m_Songs[m_CurrentSubsong]->delaytime;

		if(renderbuf)  // dry rendering? or actual rendering?
		{
			for(p=0;p<nos;p++)
			{
				lsample=0;
				rsample=0;
				echosamplel=0;
				echosampler=0;
				for(c=0;c<m_NrOfChannels;c++)
				{
					curvars = &channelvars[c];
					int a,b;
					switch (curvars->issample)
					{
					case 0: //is waveform
					{
						b=curvars->wavepnt[curvars->localwavepos>>8];
						a=curvars->wavepnt[(( unsigned short)(curvars->localwavepos+256))>>8];
						a*=(curvars->localwavepos&255);
						b*=(255-(curvars->localwavepos&255));
						a=(a>>8)+(b>>8);
						lsample += (a*curvars->panl)>>8;
						rsample += (a*curvars->panr)>>8;
						echosamplel += (a*curvars->echovolumel)>>8;
						echosampler += (a*curvars->echovolumer)>>8;
						curvars->localwavepos+=curvars->freqoffset;
						curvars->localwavepos&=curvars->wavelength;
					}
					break;
					case 1:
					{
						if(curvars->samplepos!=0xffffffff)
						{
							b=curvars->wavepnt[curvars->samplepos>>8];
							a=curvars->wavepnt[(curvars->samplepos+256)>>8];
							a*=(curvars->samplepos&255);
							b*=(255-(curvars->samplepos&255));
							a=(a>>8)+(b>>8);
							lsample += (a*curvars->panl)>>8;
							rsample += (a*curvars->panr)>>8;
							echosamplel += (a*curvars->echovolumel)>>8;
							echosampler += (a*curvars->echovolumer)>>8;
							if(curvars->curdirecflg==0) //richting end?
							{
								curvars->samplepos+=(int)curvars->freqoffset;
								if(curvars->samplepos>=curvars->waveend)
								{
									if(curvars->loopflg==0) //one shot?
									{
										curvars->samplepos=0xffffffff;
									}
									else // looping
									{
										if(curvars->bidirecflg==0) //1 richting??
										{
											curvars->samplepos-=(curvars->waveend-curvars->waveloop);
										}
										else
										{
											curvars->samplepos-=(int)curvars->freqoffset;
											curvars->curdirecflg=1; //richting omdraaien
										}
									}
								}
							}
							else   // weer bidirectioneel terug naar looppoint
							{
								curvars->samplepos-=(int)curvars->freqoffset;
								if(curvars->samplepos<=curvars->waveloop)
								{
									curvars->samplepos+=(int)curvars->freqoffset;
									curvars->curdirecflg=0; //richting omdraaien
								}
							}
						}
					}
					break;
					}
				}
				lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				rsample = ((rsample/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;

	// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

				lsample *= amplification;
				lsample /= 100;
				short smpl;
				if(lsample>32760) lsample=32760;
				if(lsample<-32760) lsample=-32760;

				rsample *= amplification;
				rsample /= 100;
				if(rsample>32760) rsample=32760;
				if(rsample<-32760) rsample=-32760;

				smpl=lsample;
				if(m_OverlapCnt<SE_OVERLAP)
				{
					smpl=(((int)smpl*(m_OverlapCnt))/SE_OVERLAP)+(((int)m_OverlapBuffer[m_OverlapCnt]*(SE_OVERLAP-m_OverlapCnt))/SE_OVERLAP);
					m_OverlapCnt++;
				}
				renderbuf[r++]=smpl;

				smpl=rsample;
				if(m_OverlapCnt<SE_OVERLAP)
				{
					smpl=(((int)smpl*(m_OverlapCnt))/SE_OVERLAP)+(((int)m_OverlapBuffer[m_OverlapCnt]*(SE_OVERLAP-m_OverlapCnt))/SE_OVERLAP);
					m_OverlapCnt++;
				}
				renderbuf[r++]=smpl;  //stereo


				m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				m_RightDelayBuffer[m_DelayCnt]=((echosampler/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;
				m_DelayCnt++;
				m_DelayCnt%=echodelaytime/(44100/frequency);

			}
		}
		else  // was this a dry render?  then we should increment r anyway.
		{
			r += (nos*2);   // times 2 since we must think stereo
		}
// 

		for(i=0;i<m_NrOfChannels;i++)
		{
			m_WavePosition[i] = channelvars[i].localwavepos;
			if(channelvars[i].issample)
			{
				m_ChannelData[i]->samplepos = channelvars[i].samplepos;
				m_ChannelData[i]->curdirecflg = channelvars[i].curdirecflg;
			}
		}

		if(m_TimeCnt==((m_TimeSpd*frequency)/44100))
		{
// Hier komt een speciaal gedeelte
// we berekenen nog 'SE_OVERLAP' samples door zodat we die straks kunnen mengen met de 'SE_OVERLAP' volgende samples
// om zo eventuele tikken weg te werken

			unsigned short tempwavepos[SE_MAXCHANS];		// laatste positie per channel in de waveform(256 te groot)(om tijdelijk op te slaan)
			unsigned short tempdelaycnt;		// ditto

			m_OverlapCnt=0; //resetten van de overlap counter
			//opbergen van tellers
			tempdelaycnt=m_DelayCnt;
			for(c=0;c<SE_MAXCHANS;c++)
			{
				tempwavepos[c] = channelvars[c].localwavepos;
			}

			if(renderbuf)  // dry rendering? or actual rendering?
			{
				for(p=0;p<SE_OVERLAP;p++)
				{
					lsample=0;
					rsample=0;
					echosamplel=0;
					echosampler=0;
					for(c=0;c<m_NrOfChannels;c++)
					{
						int a,b;
						curvars = &channelvars[c];

						switch (curvars->issample)
						{
						case 0: //is waveform
						{
							b=curvars->wavepnt[curvars->localwavepos>>8];
							a=curvars->wavepnt[(( unsigned short)(curvars->localwavepos+256))>>8];
							a*=(curvars->localwavepos&255);
							b*=(255-(curvars->localwavepos&255));
							a=(a>>8)+(b>>8);
							lsample += (a*curvars->panl)>>8;
							rsample += (a*curvars->panr)>>8;
							echosamplel += (a*curvars->echovolumel)>>8;
							echosampler += (a*curvars->echovolumer)>>8;
							curvars->localwavepos+=curvars->freqoffset;
							curvars->localwavepos&=curvars->wavelength;
						}
						break;
						case 1:
						{
							if(curvars->samplepos!=0xffffffff)
							{
								b=curvars->wavepnt[curvars->samplepos>>8];
								a=curvars->wavepnt[(curvars->samplepos+256)>>8];
								a*=(curvars->samplepos&255);
								b*=(255-(curvars->samplepos&255));
								a=(a>>8)+(b>>8);
								lsample += (a*curvars->panl)>>8;
								rsample += (a*curvars->panr)>>8;
								echosamplel += (a*curvars->echovolumel)>>8;
								echosampler += (a*curvars->echovolumer)>>8;
								if(curvars->curdirecflg==0) //richting end?
								{
									curvars->samplepos+=(int)curvars->freqoffset;
									if(curvars->samplepos>=curvars->waveend)
									{
										if(curvars->loopflg==0) //one shot?
										{
											curvars->samplepos=0xffffffff;
										}
										else // looping
										{
											if(curvars->bidirecflg==0) //1 richting??
											{
												curvars->samplepos-=(curvars->waveend-curvars->waveloop);
											}
											else
											{
												curvars->samplepos-=(int)curvars->freqoffset;
												curvars->curdirecflg=1; //richting omdraaien
											}
										}
									}
								}
								else   // weer bidirectioneel terug naar looppoint
								{
									curvars->samplepos-=(int)curvars->freqoffset;
									if(curvars->samplepos<=curvars->waveloop)
									{
										curvars->samplepos+=(int)curvars->freqoffset;
										curvars->curdirecflg=0; //richting omdraaien
									}
								}
							}
						}
						break;
						}
					}
					lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
					rsample = ((rsample/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;

		// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

					lsample *= amplification;
					lsample /= 100;
					if(lsample>32760) lsample=32760;
					if(lsample<-32760) lsample=-32760;

					rsample *= amplification;
					rsample /= 100;
					if(rsample>32760) rsample=32760;
					if(rsample<-32760) rsample=-32760;

					m_OverlapBuffer[p*2]=lsample;
					m_OverlapBuffer[(p*2)+1]=rsample;  //stereo

				//			delaybuf[m_DelayCnt]=((echosample/m_NrOfChannels)+delaybuf[m_DelayCnt])/2;  // bij de overlap NIET de delaybuf updaten!
					m_DelayCnt++;
					m_DelayCnt%=echodelaytime/(44100/frequency);
				}
			}   // end of dry buffer run
			// na de SE_OVERLAP samples berekent te hebben moeten we alle tellers terugzetten alsof er niets gebeurt is

			m_DelayCnt=tempdelaycnt;

			for(c=0;c<SE_MAXCHANS;c++)
			{
				m_WavePosition[c] = tempwavepos[c];
			}
			
			// song pointers doorknallen

			AdvanceSong();
		}
	}
	return r/2;
}


// slower stereo renderer... High quality overlapmixbuf and interpolation
void SoundEngine::RenderChannels2(short *renderbuf, int nrofsamples, int frequency)
{
	short i,p,c;
	int r;
	short nos; //number of samples
	long  samplepos[SE_MAXCHANS];		//last position in the sample per channel
	short *wavepnt[SE_MAXCHANS];		//pointers to used waves or to sampledata
	int   waveloop[SE_MAXCHANS];		//looppoint of sample(in case of sample)
	int   waveend[SE_MAXCHANS];			//length of sample(in case of sample)
	short loopflg[SE_MAXCHANS];         //one shht or looping sample?
	short bidirecflg[SE_MAXCHANS];      //one way looping or bidirectional looping?
	short curdirecflg[SE_MAXCHANS];     //Which direction are we looping??
	char  issample[SE_MAXCHANS];		//flags...if 1 then it's a sample..otherwise a waveform
	short freqoffset[SE_MAXCHANS];		//table with offsets for the frequency
	short volume[SE_MAXCHANS];			//table with volume
	short panl[SE_MAXCHANS];			//table with panlvolume
	short panr[SE_MAXCHANS];			//table with panrvolume
	short echovolume[SE_MAXCHANS];		//table with volume for echo
	short echovolumel[SE_MAXCHANS];		//table with volume for echo (left channel)
	short echovolumer[SE_MAXCHANS];		//table with volume for echo (right channel)
	short wavelength[SE_MAXCHANS];		//table with the wavelengths
	int lsample,rsample,echosamplel,echosampler;  //current state of samples
	short nrtomix;						//nr of wave streams to mix(is number of channels +1 for the echo)
	short amplification;				//amount to amplify afterwards
	unsigned short echodelaytime;		//delaytime for echo (differs for MIDI or songplayback)

	// eerst uitzoeken hoeveel er gerendered moet worden...
	// dit moet met de cylcische directsound buffer
	// nu nemen we effe voor het gemak aan dat het 1kb is

// nu gaan we nrofsamples berekenen maar in blokjes van timecnt (de songspd)
	if(m_NrOfChannels==0)
	{
		for(i=0;i<nrofsamples;i++)
		{
			renderbuf[i]=0;
		}
		return;
	}
	r=0;
	nrtomix=m_NrOfChannels+1;  //1 extra for the echoes
	while(nrofsamples>0)
	{
//		nos=nrofsamples;
//		nrofsamples=0;

// 2 mogelijkheden... we calcen een heel timecnt blokje...of een laatste stukje van een timecnt blokje...
		if(m_TimeCnt<nrofsamples)
		{
			nos = m_TimeCnt;   //geheel blok
//			m_TimeCnt=m_TimeSpd/(44100/frequency);
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		}
		else
		{
			nos = nrofsamples;  // laatste stukje
			m_TimeCnt=m_TimeCnt-nos;  //m_TimeSpd-nos
		}
//		if(nos>nrofsamples) //laatste beetje?
//		{
//			nos=nrofsamples;
//			m_TimeCnt=m_TimeSpd-nos;
//		}
		nrofsamples-=nos;


		// preparation of wave pointers en freq offset
		for(i=0;i<m_NrOfChannels;i++)
		{
			int inst,instnr;
			instnr = m_ChannelData[i]->instrument;
			if (instnr == -1) // mute?
			{
				wavepnt[i]=m_EmptyWave;
				issample[i]=0;
			}
			else
			{
				if(m_ChannelData[i]->sampledata)
				{
					samplepos[i]=m_ChannelData[i]->samplepos;
					wavepnt[i]=(short *)m_ChannelData[i]->sampledata;
					waveloop[i]=m_ChannelData[i]->looppoint; 
					waveend[i]=m_ChannelData[i]->endpoint; 
					loopflg[i]=m_ChannelData[i]->loopflg;
					bidirecflg[i]=m_ChannelData[i]->bidirecflg;
					curdirecflg[i]=m_ChannelData[i]->curdirecflg;
					issample[i]=1;
				}
				else
				{
					inst = m_Instruments[instnr]->waveform;
					wavepnt[i]=&m_ChannelData[i]->waves[256*inst];
					wavelength[i]=((m_Instruments[instnr]->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					issample[i]=0;
				}
			}
	//calcen frequency
			if(m_ChannelData[i]->curfreq<10)m_ChannelData[i]->curfreq=10;
			freqoffset[i] = (256*m_ChannelData[i]->curfreq)/frequency;
			volume[i] = (m_ChannelData[i]->curvol+10000)/39;
			if(volume[i]>256) volume[i]=256;

			if(m_ChannelData[i]->curpan==0) //panning?
			{
				panl[i]=256; //geen panning
				panr[i]=256;
			}
			else
			{
				if(m_ChannelData[i]->curpan>0)
				{
					panl[i] = 256-(m_ChannelData[i]->curpan);
					panr[i]=256;
				}
				else
				{
					panr[i] = 256+(m_ChannelData[i]->curpan);
					panl[i]=256;
				}
			}

			echovolume[i]=m_Songs[m_CurrentSubsong]->delayamount[i];

			// stereo versnelling 1... alvast de panning vermenigvuldigen met de mainvolume

			panl[i] = (panl[i]*volume[i])>>8;
			panr[i] = (panr[i]*volume[i])>>8;

			//stereo versnelling...alvast de panning meeberekenen voor de echo

			echovolumel[i]=(echovolume[i]*panl[i])>>8;
			echovolumer[i]=(echovolume[i]*panr[i])>>8;

		}
		amplification = m_Songs[m_CurrentSubsong]->amplification;
		echodelaytime = m_Songs[m_CurrentSubsong]->delaytime;

		if(renderbuf)  // dry rendering? or actual rendering?
		{
			for(p=0;p<nos;p++)
			{
				lsample=0;
				rsample=0;
				echosamplel=0;
				echosampler=0;
				for(c=0;c<m_NrOfChannels;c++)
				{
					int a,b;
					switch (issample[c])
					{
					case 0: //is waveform
					{
						b=wavepnt[c][m_WavePosition[c]>>8];
						a=wavepnt[c][(( unsigned short)(m_WavePosition[c]+256))>>8];
						a*=(m_WavePosition[c]&255);
						b*=(255-(m_WavePosition[c]&255));
						a=(a>>8)+(b>>8);
	//					a=(a*volume[c])>>8;
						lsample += (a*panl[c])>>8;
						rsample += (a*panr[c])>>8;
						echosamplel += (a*echovolumel[c])>>8;
						echosampler += (a*echovolumer[c])>>8;
		//				sample += (b*volume[c])>>8;;
						m_WavePosition[c]+=freqoffset[c];
						m_WavePosition[c]&=wavelength[c];
					}
					break;
					case 1:
					{
						if(samplepos[c]!=0xffffffff)
						{
							b=wavepnt[c][samplepos[c]>>8];
							a=wavepnt[c][(samplepos[c]+256)>>8];
							a*=(samplepos[c]&255);
							b*=(255-(samplepos[c]&255));
							a=(a>>8)+(b>>8);
	//						a=(a*volume[c])>>8;
							lsample += (a*panl[c])>>8;
							rsample += (a*panr[c])>>8;
							echosamplel += (a*echovolumel[c])>>8;
							echosampler += (a*echovolumer[c])>>8;
			//				sample += (b*volume[c])>>8;;
							if(curdirecflg[c]==0) //richting end?
							{
								samplepos[c]+=(int)freqoffset[c];
								if(samplepos[c]>=waveend[c])
								{
									if(loopflg[c]==0) //one shot?
									{
										samplepos[c]=0xffffffff;
									}
									else // looping
									{
										if(bidirecflg[c]==0) //1 richting??
										{
											samplepos[c]-=(waveend[c]-waveloop[c]);
										}
										else
										{
											samplepos[c]-=(int)freqoffset[c];
											curdirecflg[c]=1; //richting omdraaien
										}
									}
								}
							}
							else   // weer bidirectioneel terug naar looppoint
							{
								samplepos[c]-=(int)freqoffset[c];
								if(samplepos[c]<=waveloop[c])
								{
									samplepos[c]+=(int)freqoffset[c];
									curdirecflg[c]=0; //richting omdraaien
								}
							}
						}
					}
					break;
					}
				}
				lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				rsample = ((rsample/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;

	// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

				lsample *= amplification;
				lsample /= 100;
				short smpl;
				if(lsample>32760) lsample=32760;
				if(lsample<-32760) lsample=-32760;

				rsample *= amplification;
				rsample /= 100;
				if(rsample>32760) rsample=32760;
				if(rsample<-32760) rsample=-32760;

				smpl=lsample;
				if(m_OverlapCnt<SE_OVERLAP)
				{
					smpl=(((int)smpl*(m_OverlapCnt))/SE_OVERLAP)+(((int)m_OverlapBuffer[m_OverlapCnt]*(SE_OVERLAP-m_OverlapCnt))/SE_OVERLAP);
					m_OverlapCnt++;
				}
				renderbuf[r++]=smpl;

				smpl=rsample;
				if(m_OverlapCnt<SE_OVERLAP)
				{
					smpl=(((int)smpl*(m_OverlapCnt))/SE_OVERLAP)+(((int)m_OverlapBuffer[m_OverlapCnt]*(SE_OVERLAP-m_OverlapCnt))/SE_OVERLAP);
					m_OverlapCnt++;
				}
				renderbuf[r++]=smpl;  //stereo


				m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				m_RightDelayBuffer[m_DelayCnt]=((echosampler/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;
				m_DelayCnt++;
				m_DelayCnt%=echodelaytime/(44100/frequency);

			}
		}
		else  // was this a dry render?  then we should increment r anyway.
		{
			r += (nos*2);   // times 2 since we must think stereo
		}
// 

		for(i=0;i<m_NrOfChannels;i++)
		{
			if(issample[i])
			{
				m_ChannelData[i]->samplepos = samplepos[i];
				m_ChannelData[i]->curdirecflg = curdirecflg[i];
			}
		}

//		if(m_TimeCnt==m_TimeSpd/(44100/frequency))
		if(m_TimeCnt==((m_TimeSpd*frequency)/44100))
		{
// Hier komt een speciaal gedeelte
// we berekenen nog 'SE_OVERLAP' samples door zodat we die straks kunnen mengen met de 'SE_OVERLAP' volgende samples
// om zo eventuele tikken weg te werken

			unsigned short tempwavepos[SE_MAXCHANS];		// laatste positie per channel in de waveform(256 te groot)(om tijdelijk op te slaan)
			unsigned short tempdelaycnt;		// ditto

			m_OverlapCnt=0; //resetten van de overlap counter
			//opbergen van tellers
			tempdelaycnt=m_DelayCnt;
			memcpy(tempwavepos,m_WavePosition,sizeof(m_WavePosition));

			if(renderbuf)  // dry rendering? or actual rendering?
			{
				for(p=0;p<SE_OVERLAP;p++)
				{
					lsample=0;
					rsample=0;
					echosamplel=0;
					echosampler=0;
					for(c=0;c<m_NrOfChannels;c++)
					{
						int a,b;
						switch (issample[c])
						{
						case 0: //is waveform
						{
							b=wavepnt[c][m_WavePosition[c]>>8];
							a=wavepnt[c][(( unsigned short)(m_WavePosition[c]+256))>>8];
							a*=(m_WavePosition[c]&255);
							b*=(255-(m_WavePosition[c]&255));
							a=(a>>8)+(b>>8);
		//					a=(a*volume[c])>>8;
							lsample += (a*panl[c])>>8;
							rsample += (a*panr[c])>>8;
							echosamplel += (a*echovolumel[c])>>8;
							echosampler += (a*echovolumer[c])>>8;
			//				sample += (b*volume[c])>>8;;
							m_WavePosition[c]+=freqoffset[c];
							m_WavePosition[c]&=wavelength[c];
						}
						break;
						case 1:
						{
							if(samplepos[c]!=0xffffffff)
							{
								b=wavepnt[c][samplepos[c]>>8];
								a=wavepnt[c][(samplepos[c]+256)>>8];
								a*=(samplepos[c]&255);
								b*=(255-(samplepos[c]&255));
								a=(a>>8)+(b>>8);
		//						a=(a*volume[c])>>8;
								lsample += (a*panl[c])>>8;
								rsample += (a*panr[c])>>8;
								echosamplel += (a*echovolumel[c])>>8;
								echosampler += (a*echovolumer[c])>>8;
				//				sample += (b*volume[c])>>8;;
								if(curdirecflg[c]==0) //richting end?
								{
									samplepos[c]+=(int)freqoffset[c];
									if(samplepos[c]>=waveend[c])
									{
										if(loopflg[c]==0) //one shot?
										{
											samplepos[c]=0xffffffff;
										}
										else // looping
										{
											if(bidirecflg[c]==0) //1 richting??
											{
												samplepos[c]-=(waveend[c]-waveloop[c]);
											}
											else
											{
												samplepos[c]-=(int)freqoffset[c];
												curdirecflg[c]=1; //richting omdraaien
											}
										}
									}
								}
								else   // weer bidirectioneel terug naar looppoint
								{
									samplepos[c]-=(int)freqoffset[c];
									if(samplepos[c]<=waveloop[c])
									{
										samplepos[c]+=(int)freqoffset[c];
										curdirecflg[c]=0; //richting omdraaien
									}
								}
							}
						}
						break;
						}
					}
					lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
					rsample = ((rsample/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;

		// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

					lsample *= amplification;
					lsample /= 100;
					if(lsample>32760) lsample=32760;
					if(lsample<-32760) lsample=-32760;

					rsample *= amplification;
					rsample /= 100;
					if(rsample>32760) rsample=32760;
					if(rsample<-32760) rsample=-32760;

					m_OverlapBuffer[p*2]=lsample;
					m_OverlapBuffer[(p*2)+1]=rsample;  //stereo

				//			delaybuf[m_DelayCnt]=((echosample/m_NrOfChannels)+delaybuf[m_DelayCnt])/2;  // bij de overlap NIET de delaybuf updaten!
					m_DelayCnt++;
					m_DelayCnt%=echodelaytime/(44100/frequency);
				}
			}   // end of dry buffer run
			// na de SE_OVERLAP samples berekent te hebben moeten we alle tellers terugzetten alsof er niets gebeurt is

			m_DelayCnt=tempdelaycnt;
			memcpy(m_WavePosition,tempwavepos,sizeof(m_WavePosition));
			
			// song pointers doorknallen

			AdvanceSong();
		}
	}
}


// Optimized stereo renderer... No high quality overlapmixbuf
void SoundEngine::RenderChannels3(short *renderbuf, int nrofsamples, int frequency)
{
	channelvar channelvars[SE_MAXCHANS];
	channelvar *curvars;
	short i,p,c;
	int r;
	short nos;										//number of samples
	int lsample,rsample,echosamplel,echosampler;	//current state of samples
	short nrtomix;									// nr of wave streams to mix(is number of channels +1 for the echo)
	short amplification;							// amount to amplify afterwards
	unsigned short echodelaytime;					//delaytime for echo (differs for MIDI or songplayback)

	// we calc nrofsamples samples in blocks of 'm_TimeCnt' big (de songspd)

	if(m_NrOfChannels==0)	// no active channels?
	{
		for(i=0;i<nrofsamples;i++)	//c lean renderbuffer
		{
			renderbuf[i]=0;
		}
		return;
	}

	r=0;
	nrtomix=m_NrOfChannels+1;  //1 extra for the echoes
	while(nrofsamples>0)
	{
// 2 possibilities:  we calc a complete m_TimeCnt block...or the last part of a m_TimeCnt block...
		if(m_TimeCnt<nrofsamples)
		{
			nos = m_TimeCnt;   //Complete block
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		}
		else
		{
			nos = nrofsamples;  //Last piece
			m_TimeCnt=m_TimeCnt-nos; 
		}
		nrofsamples-=nos;

		// preparation of wave pointers and freq offset
		for(i=0;i<m_NrOfChannels;i++)
		{
			int inst,instnr;
			instnr = m_ChannelData[i]->instrument;
			if (instnr == -1) // mute?
			{
				channelvars[i].wavepnt=m_EmptyWave;
				channelvars[i].issample=0;
			}
			else
			{
				if(m_ChannelData[i]->sampledata)
				{
					channelvars[i].samplepos=m_ChannelData[i]->samplepos;
					channelvars[i].wavepnt=(short *)m_ChannelData[i]->sampledata;
					channelvars[i].waveloop=m_ChannelData[i]->looppoint; 
					channelvars[i].waveend=m_ChannelData[i]->endpoint; 
					channelvars[i].loopflg=m_ChannelData[i]->loopflg;
					channelvars[i].bidirecflg=m_ChannelData[i]->bidirecflg;
					channelvars[i].curdirecflg=m_ChannelData[i]->curdirecflg;
					channelvars[i].issample=1;
				}
				else
				{
					inst = m_Instruments[instnr]->waveform;
					channelvars[i].wavepnt=&m_ChannelData[i]->waves[256*inst];
					channelvars[i].wavelength=((m_Instruments[instnr]->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					channelvars[i].issample=0;
				}
			}

			channelvars[i].localwavepos = m_WavePosition[i];

	//calcen frequency
			if(m_ChannelData[i]->curfreq<10)m_ChannelData[i]->curfreq=10;
			channelvars[i].freqoffset = (256*m_ChannelData[i]->curfreq)/frequency;
			channelvars[i].volume = (m_ChannelData[i]->curvol+10000)/39;
			if(channelvars[i].volume>256) channelvars[i].volume=256;

			if(m_ChannelData[i]->curpan==0) //panning?
			{
				channelvars[i].panl=256; //geen panning
				channelvars[i].panr=256;
			}
			else
			{
				if(m_ChannelData[i]->curpan>0)
				{
					channelvars[i].panl = 256-(m_ChannelData[i]->curpan);
					channelvars[i].panr = 256;
				}
				else
				{
					channelvars[i].panr = 256+(m_ChannelData[i]->curpan);
					channelvars[i].panl=256;
				}
			}

			channelvars[i].echovolume = m_Songs[m_CurrentSubsong]->delayamount[i];

			//stereo optimimalisation 1... precalc panning multiplied with mainvolume
			channelvars[i].panl = (channelvars[i].panl * channelvars[i].volume)>>8;
			channelvars[i].panr = (channelvars[i].panr * channelvars[i].volume)>>8;

			//stereo optimimalisation 1... precalc panning multiplied with echo
			channelvars[i].echovolumel = (channelvars[i].echovolume * channelvars[i].panl)>>8;
			channelvars[i].echovolumer = (channelvars[i].echovolume * channelvars[i].panr)>>8;

		}
		amplification = m_Songs[m_CurrentSubsong]->amplification;
		echodelaytime = m_Songs[m_CurrentSubsong]->delaytime;

		if(renderbuf)  // dry rendering? or actual rendering?
		{
			for(p=0;p<nos;p++)
			{
				lsample=0;
				rsample=0;
				echosamplel=0;
				echosampler=0;
				for(c=0;c<m_NrOfChannels;c++)
				{
					curvars = &channelvars[c];
					int b;
					switch (curvars->issample)
					{
					case 0: //is waveform
					{
						b=curvars->wavepnt[curvars->localwavepos>>8];
						lsample += (b*curvars->panl)>>8;
						rsample += (b*curvars->panr)>>8;
						echosamplel += (b*curvars->echovolumel)>>8;
						echosampler += (b*curvars->echovolumer)>>8;
						curvars->localwavepos+=curvars->freqoffset;
						curvars->localwavepos&=curvars->wavelength;
					}
					break;
					case 1:
					{
						if(curvars->samplepos!=0xffffffff)
						{
							b=curvars->wavepnt[curvars->samplepos>>8];
							lsample += (b*curvars->panl)>>8;
							rsample += (b*curvars->panr)>>8;
							echosamplel += (b*curvars->echovolumel)>>8;
							echosampler += (b*curvars->echovolumer)>>8;
							if(curvars->curdirecflg==0) //richting end?
							{
								curvars->samplepos+=(int)curvars->freqoffset;
								if(curvars->samplepos>=curvars->waveend)
								{
									if(curvars->loopflg==0) //one shot?
									{
										curvars->samplepos=0xffffffff;
									}
									else // looping
									{
										if(curvars->bidirecflg==0) //1 richting??
										{
											curvars->samplepos-=(curvars->waveend-curvars->waveloop);
										}
										else
										{
											curvars->samplepos-=(int)curvars->freqoffset;
											curvars->curdirecflg=1; //richting omdraaien
										}
									}
								}
							}
							else   // weer bidirectioneel terug naar looppoint
							{
								curvars->samplepos-=(int)curvars->freqoffset;
								if(curvars->samplepos<=curvars->waveloop)
								{
									curvars->samplepos+=(int)curvars->freqoffset;
									curvars->curdirecflg=0; //richting omdraaien
								}
							}
						}
					}
					break;
					}
				}
				lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				rsample = ((rsample/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;

	// if we are still in the bottom SE_OVERLAP part then we have to mix the previous wave through (to avoid clicks)

				lsample *= amplification;
				lsample /= 100;
				short smpl;
				if(lsample>32760) lsample=32760;
				if(lsample<-32760) lsample=-32760;

				rsample *= amplification;
				rsample /= 100;
				if(rsample>32760) rsample=32760;
				if(rsample<-32760) rsample=-32760;

				smpl=lsample;
				renderbuf[r++]=smpl;

				smpl=rsample;
				renderbuf[r++]=smpl;  //stereo


				m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				m_RightDelayBuffer[m_DelayCnt]=((echosampler/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;
				m_DelayCnt++;
				m_DelayCnt%=echodelaytime/(44100/frequency);

			}
		}
		else  // was this a dry render?  then we should increment r anyway.
		{
			r += (nos*2);   // times 2 since we must think stereo
		}
// 

		for(i=0;i<m_NrOfChannels;i++)
		{
			m_WavePosition[i] = channelvars[i].localwavepos;
			if(channelvars[i].issample)
			{
				m_ChannelData[i]->samplepos = channelvars[i].samplepos;
				m_ChannelData[i]->curdirecflg = channelvars[i].curdirecflg;
			}
		}

		if(m_TimeCnt==((m_TimeSpd*frequency)/44100))
		{
			unsigned short tempwavepos[SE_MAXCHANS];		// last position per channel in the waveform(8bit fixed point)(temporarily storage)
			unsigned short tempdelaycnt;		// ditto

			m_OverlapCnt=0; //reset overlap counter
			tempdelaycnt=m_DelayCnt;
			for(c=0;c<SE_MAXCHANS;c++)
			{
				tempwavepos[c] = channelvars[c].localwavepos;
			}

			m_DelayCnt=tempdelaycnt;

			for(c=0;c<SE_MAXCHANS;c++)
			{
				m_WavePosition[c] = tempwavepos[c];
			}
			
			//Update song pointers
			AdvanceSong();
		}
	}
}


// optimized mono renderer... High quality overlapmixbuf and interpolation
void SoundEngine::RenderChannels1Mono(short *renderbuf, int nrofsamples, int frequency)
{
	channelvar channelvars[SE_MAXCHANS];
	channelvar *curvars;
	short i,p,c;
	int r;
	short nos; //number of samples
	int lsample,rsample,echosamplel,echosampler;  //current state of samples
	short nrtomix;        // nr of wave streams to mix(is number of channels +1 for the echo)
	short amplification;    // amount to amplify afterwards
	unsigned short echodelaytime; //delaytime for echo (differs for MIDI or songplayback)

	// eerst uitzoeken hoeveel er gerendered moet worden...
	// dit moet met de cylcische directsound buffer
	// nu nemen we effe voor het gemak aan dat het 1kb is

// nu gaan we nrofsamples berekenen maar in blokjes van m_TimeCnt (de songspd)
	if(m_NrOfChannels==0)
	{
		for(i=0;i<nrofsamples;i++)
		{
			renderbuf[i]=0;
		}
		return;
	}
	r=0;
	nrtomix=m_NrOfChannels+1;  //1 extra for the echoes
	while(nrofsamples>0)
	{
//		nos=nrofsamples;
//		nrofsamples=0;

// 2 mogelijkheden... we calcen een heel m_TimeCnt blokje...of een laatste stukje van een m_TimeCnt blokje...
		if(m_TimeCnt<nrofsamples)
		{
			nos = m_TimeCnt;   //geheel blok
//			m_TimeCnt=m_TimeSpd/(44100/frequency);
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		}
		else
		{
			nos = nrofsamples;  // laatste stukje
			m_TimeCnt=m_TimeCnt-nos;  //m_TimeSpd-nos
		}
//		if(nos>nrofsamples) //laatste beetje?
//		{
//			nos=nrofsamples;
//			m_TimeCnt=m_TimeSpd-nos;
//		}
		nrofsamples-=nos;


		// preparation of wave pointers en freq offset
		for(i=0;i<m_NrOfChannels;i++)
		{
			int inst,instnr;
			instnr = m_ChannelData[i]->instrument;
			if (instnr == -1) // mute?
			{
				channelvars[i].wavepnt=m_EmptyWave;
				channelvars[i].issample=0;
			}
			else
			{
				if(m_ChannelData[i]->sampledata)
				{
					channelvars[i].samplepos=m_ChannelData[i]->samplepos;
					channelvars[i].wavepnt=(short *)m_ChannelData[i]->sampledata;
					channelvars[i].waveloop=m_ChannelData[i]->looppoint; 
					channelvars[i].waveend=m_ChannelData[i]->endpoint; 
					channelvars[i].loopflg=m_ChannelData[i]->loopflg;
					channelvars[i].bidirecflg=m_ChannelData[i]->bidirecflg;
					channelvars[i].curdirecflg=m_ChannelData[i]->curdirecflg;
					channelvars[i].issample=1;
				}
				else
				{
					inst = m_Instruments[instnr]->waveform;
					channelvars[i].wavepnt=&m_ChannelData[i]->waves[256*inst];
					channelvars[i].wavelength=((m_Instruments[instnr]->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					channelvars[i].issample=0;
				}
			}

			channelvars[i].localwavepos = m_WavePosition[i];

	//calcen frequency
			if(m_ChannelData[i]->curfreq<10)m_ChannelData[i]->curfreq=10;
			channelvars[i].freqoffset = (256*m_ChannelData[i]->curfreq)/frequency;
			channelvars[i].volume = (m_ChannelData[i]->curvol+10000)/39;
			if(channelvars[i].volume>256) channelvars[i].volume=256;

			if(m_ChannelData[i]->curpan==0) //panning?
			{
				channelvars[i].panl=256; //geen panning
				channelvars[i].panr=256;
			}
			else
			{
				if(m_ChannelData[i]->curpan>0)
				{
					channelvars[i].panl = 256-(m_ChannelData[i]->curpan);
					channelvars[i].panr = 256;
				}
				else
				{
					channelvars[i].panr = 256+(m_ChannelData[i]->curpan);
					channelvars[i].panl=256;
				}
			}

			channelvars[i].echovolume = m_Songs[m_CurrentSubsong]->delayamount[i];

			// stereo versnelling 1... alvast de panning vermenigvuldigen met de mainvolume

			channelvars[i].panl = (channelvars[i].panl * channelvars[i].volume)>>8;
			channelvars[i].panr = (channelvars[i].panr * channelvars[i].volume)>>8;

			//stereo versnelling...alvast de panning meeberekenen voor de echo

			channelvars[i].echovolume = (channelvars[i].echovolume * channelvars[i].volume)>>8;

//			channelvars[i].echovolumel = (channelvars[i].echovolume * channelvars[i].panl)>>8;
//			channelvars[i].echovolumer = (channelvars[i].echovolume * channelvars[i].panr)>>8;

		}
		amplification = m_Songs[m_CurrentSubsong]->amplification;
		echodelaytime = m_Songs[m_CurrentSubsong]->delaytime;

		if(renderbuf)  // dry rendering? or actual rendering?
		{
			for(p=0;p<nos;p++)
			{
				lsample=0;
				rsample=0;
				echosamplel=0;
				echosampler=0;
				for(c=0;c<m_NrOfChannels;c++)
				{
					curvars = &channelvars[c];
					int a,b;
					switch (curvars->issample)
					{
					case 0: //is waveform
					{
						b=curvars->wavepnt[curvars->localwavepos>>8];
						a=curvars->wavepnt[(( unsigned short)(curvars->localwavepos+256))>>8];
						a*=(curvars->localwavepos&255);
						b*=(255-(curvars->localwavepos&255));
						a=(a>>8)+(b>>8);
						lsample += (a*curvars->volume)>>8;
						echosamplel += (a*curvars->echovolume)>>8;
						curvars->localwavepos+=curvars->freqoffset;
						curvars->localwavepos&=curvars->wavelength;
					}
					break;
					case 1:
					{
						if(curvars->samplepos!=0xffffffff)
						{
							b=curvars->wavepnt[curvars->samplepos>>8];
							a=curvars->wavepnt[(curvars->samplepos+256)>>8];
							a*=(curvars->samplepos&255);
							b*=(255-(curvars->samplepos&255));
							a=(a>>8)+(b>>8);
							lsample += (a*curvars->volume)>>8;
							echosamplel += (a*curvars->echovolume)>>8;
							if(curvars->curdirecflg==0) //richting end?
							{
								curvars->samplepos+=(int)curvars->freqoffset;
								if(curvars->samplepos>=curvars->waveend)
								{
									if(curvars->loopflg==0) //one shot?
									{
										curvars->samplepos=0xffffffff;
									}
									else // looping
									{
										if(curvars->bidirecflg==0) //1 richting??
										{
											curvars->samplepos-=(curvars->waveend-curvars->waveloop);
										}
										else
										{
											curvars->samplepos-=(int)curvars->freqoffset;
											curvars->curdirecflg=1; //richting omdraaien
										}
									}
								}
							}
							else   // weer bidirectioneel terug naar looppoint
							{
								curvars->samplepos-=(int)curvars->freqoffset;
								if(curvars->samplepos<=curvars->waveloop)
								{
									curvars->samplepos+=(int)curvars->freqoffset;
									curvars->curdirecflg=0; //richting omdraaien
								}
							}
						}
					}
					break;
					}
				}
				lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;

	// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

				lsample *= amplification;
				lsample /= 100;
				short smpl;
				if(lsample>32760) lsample=32760;
				if(lsample<-32760) lsample=-32760;

				smpl=lsample;
				if(m_OverlapCnt<SE_OVERLAP)
				{
					smpl=(((int)smpl*(m_OverlapCnt))/SE_OVERLAP)+(((int)m_OverlapBuffer[m_OverlapCnt]*(SE_OVERLAP-m_OverlapCnt))/SE_OVERLAP);
					m_OverlapCnt++;
				}
				renderbuf[r++]=smpl;

				m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				m_DelayCnt++;
				m_DelayCnt%=echodelaytime/(44100/frequency);

			}
		}
		else  // was this a dry render?  then we should increment r anyway.
		{
			r += (nos*2);   // times 2 since we must think stereo
		}
// 

		for(i=0;i<m_NrOfChannels;i++)
		{
			m_WavePosition[i] = channelvars[i].localwavepos;
			if(channelvars[i].issample)
			{
				m_ChannelData[i]->samplepos = channelvars[i].samplepos;
				m_ChannelData[i]->curdirecflg = channelvars[i].curdirecflg;
			}
		}

		if(m_TimeCnt==((m_TimeSpd*frequency)/44100))
		{
// Hier komt een speciaal gedeelte
// we berekenen nog 'SE_OVERLAP' samples door zodat we die straks kunnen mengen met de 'SE_OVERLAP' volgende samples
// om zo eventuele tikken weg te werken

			unsigned short tempwavepos[SE_MAXCHANS];		// laatste positie per channel in de waveform(256 te groot)(om tijdelijk op te slaan)
			unsigned short tempdelaycnt;		// ditto

			m_OverlapCnt=0; //resetten van de overlap counter
			//opbergen van tellers
			tempdelaycnt=m_DelayCnt;
			for(c=0;c<SE_MAXCHANS;c++)
			{
				tempwavepos[c] = channelvars[c].localwavepos;
			}

			if(renderbuf)  // dry rendering? or actual rendering?
			{
				for(p=0;p<SE_OVERLAP;p++)
				{
					lsample=0;
					rsample=0;
					echosamplel=0;
					echosampler=0;
					for(c=0;c<m_NrOfChannels;c++)
					{
						int a,b;
						curvars = &channelvars[c];

						switch (curvars->issample)
						{
						case 0: //is waveform
						{
							b=curvars->wavepnt[curvars->localwavepos>>8];
							a=curvars->wavepnt[(( unsigned short)(curvars->localwavepos+256))>>8];
							a*=(curvars->localwavepos&255);
							b*=(255-(curvars->localwavepos&255));
							a=(a>>8)+(b>>8);
							lsample += (a*curvars->volume)>>8;
							echosamplel += (a*curvars->echovolume)>>8;
							curvars->localwavepos+=curvars->freqoffset;
							curvars->localwavepos&=curvars->wavelength;
						}
						break;
						case 1:
						{
							if(curvars->samplepos!=0xffffffff)
							{
								b=curvars->wavepnt[curvars->samplepos>>8];
								a=curvars->wavepnt[(curvars->samplepos+256)>>8];
								a*=(curvars->samplepos&255);
								b*=(255-(curvars->samplepos&255));
								a=(a>>8)+(b>>8);
								lsample += (a*curvars->volume)>>8;
								echosamplel += (a*curvars->echovolume)>>8;
								if(curvars->curdirecflg==0) //richting end?
								{
									curvars->samplepos+=(int)curvars->freqoffset;
									if(curvars->samplepos>=curvars->waveend)
									{
										if(curvars->loopflg==0) //one shot?
										{
											curvars->samplepos=0xffffffff;
										}
										else // looping
										{
											if(curvars->bidirecflg==0) //1 richting??
											{
												curvars->samplepos-=(curvars->waveend-curvars->waveloop);
											}
											else
											{
												curvars->samplepos-=(int)curvars->freqoffset;
												curvars->curdirecflg=1; //richting omdraaien
											}
										}
									}
								}
								else   // weer bidirectioneel terug naar looppoint
								{
									curvars->samplepos-=(int)curvars->freqoffset;
									if(curvars->samplepos<=curvars->waveloop)
									{
										curvars->samplepos+=(int)curvars->freqoffset;
										curvars->curdirecflg=0; //richting omdraaien
									}
								}
							}
						}
						break;
						}
					}
					lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;

		// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

					lsample *= amplification;
					lsample /= 100;
					if(lsample>32760) lsample=32760;
					if(lsample<-32760) lsample=-32760;

					m_OverlapBuffer[p]=lsample;

					m_DelayCnt++;
					m_DelayCnt%=echodelaytime/(44100/frequency);
				}
			}   // end of dry buffer run
			// na de SE_OVERLAP samples berekent te hebben moeten we alle tellers terugzetten alsof er niets gebeurt is

			m_DelayCnt=tempdelaycnt;

			for(c=0;c<SE_MAXCHANS;c++)
			{
				m_WavePosition[c] = tempwavepos[c];
			}
			
			// song pointers doorknallen

			AdvanceSong();
		}
	}
}

// Slow mono renderer... High quality overlapmixbuf and interpolation
void SoundEngine::RenderChannels2Mono(short *renderbuf, int nrofsamples, int frequency)
{
	short i,p,c;
	int r;
	short nos; //number of samples
	long  samplepos[SE_MAXCHANS];		//last position in the sample per channel
	short *wavepnt[SE_MAXCHANS];		//pointers to used waves or to sampledata
	int   waveloop[SE_MAXCHANS];		//looppoint of sample(in case of sample)
	int   waveend[SE_MAXCHANS];			//length of sample(in case of sample)
	short loopflg[SE_MAXCHANS];         //one shht or looping sample?
	short bidirecflg[SE_MAXCHANS];      //one way looping or bidirectional looping?
	short curdirecflg[SE_MAXCHANS];     //Which direction are we looping??
	char  issample[SE_MAXCHANS];		//flags...if 1 then it's a sample..otherwise a waveform
	short freqoffset[SE_MAXCHANS];		//table with offsets for the frequency
	short volume[SE_MAXCHANS];			//table with volume
	short panl[SE_MAXCHANS];			//table with panlvolume
	short panr[SE_MAXCHANS];			//table with panrvolume
	short echovolume[SE_MAXCHANS];		//table with volume for echo
	short wavelength[SE_MAXCHANS];		//table with the wavelengths
	int lsample,rsample,echosamplel,echosampler;  //current state of samples
	short nrtomix;						//nr of wave streams to mix(is number of channels +1 for the echo)
	short amplification;				//amount to amplify afterwards
	unsigned short echodelaytime;		//delaytime for echo (differs for MIDI or songplayback)

	// we calc nrofsamples samples in blocks of 'm_TimeCnt' big (de songspd)
	if(m_NrOfChannels==0)
	{
		for(i=0;i<nrofsamples;i++)
		{
			renderbuf[i]=0;
		}
		return;
	}
	r=0;
	nrtomix=m_NrOfChannels+1;  //1 extra for the echoes
	while(nrofsamples>0)
	{
// 2 mogelijkheden... we calcen een heel m_TimeCnt blokje...of een laatste stukje van een m_TimeCnt blokje...
		if(m_TimeCnt<nrofsamples)
		{
			nos = m_TimeCnt;   //geheel blok
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		}
		else
		{
			nos = nrofsamples;  // laatste stukje
			m_TimeCnt=m_TimeCnt-nos;  //m_TimeSpd-nos
		}
		nrofsamples-=nos;


		// preparation of wave pointers en freq offset
		for(i=0;i<m_NrOfChannels;i++)
		{
			int inst,instnr;
			instnr = m_ChannelData[i]->instrument;
			if (instnr == -1) // mute?
			{
				wavepnt[i]=m_EmptyWave;
				issample[i]=0;
			}
			else
			{
				if(m_ChannelData[i]->sampledata)
				{
					samplepos[i]=m_ChannelData[i]->samplepos;
					wavepnt[i]=(short *)m_ChannelData[i]->sampledata;
					waveloop[i]=m_ChannelData[i]->looppoint; 
					waveend[i]=m_ChannelData[i]->endpoint; 
					loopflg[i]=m_ChannelData[i]->loopflg;
					bidirecflg[i]=m_ChannelData[i]->bidirecflg;
					curdirecflg[i]=m_ChannelData[i]->curdirecflg;
					issample[i]=1;
				}
				else
				{
					inst = m_Instruments[instnr]->waveform;
					wavepnt[i]=&m_ChannelData[i]->waves[256*inst];
					wavelength[i]=((m_Instruments[instnr]->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					issample[i]=0;
				}
			}
	//calcen frequency
			if(m_ChannelData[i]->curfreq<10)m_ChannelData[i]->curfreq=10;
			freqoffset[i] = (256*m_ChannelData[i]->curfreq)/frequency;
			volume[i] = (m_ChannelData[i]->curvol+10000)/39;
			if(volume[i]>256) volume[i]=256;

			if(m_ChannelData[i]->curpan==0) //panning?
			{
				panl[i]=256; //geen panning
				panr[i]=256;
			}
			else
			{
				if(m_ChannelData[i]->curpan>0)
				{
					panl[i] = 256-(m_ChannelData[i]->curpan);
					panr[i]=256;
				}
				else
				{
					panr[i] = 256+(m_ChannelData[i]->curpan);
					panl[i]=256;
				}
			}

			echovolume[i]=m_Songs[m_CurrentSubsong]->delayamount[i];

			// stereo versnelling 1... alvast de panning vermenigvuldigen met de mainvolume
			panl[i] = (panl[i]*volume[i])>>8;
			panr[i] = (panr[i]*volume[i])>>8;

			//stereo versnelling...alvast de panning meeberekenen voor de echo
			echovolume[i]=(echovolume[i]*volume[i])>>8;

		}
		amplification = m_Songs[m_CurrentSubsong]->amplification;
		echodelaytime = m_Songs[m_CurrentSubsong]->delaytime;

		if(renderbuf)  // dry rendering? or actual rendering?
		{
			for(p=0;p<nos;p++)
			{
				lsample=0;
				rsample=0;
				echosamplel=0;
				echosampler=0;
				for(c=0;c<m_NrOfChannels;c++)
				{
					int a,b;
					switch (issample[c])
					{
					case 0: //is waveform
					{
						b=wavepnt[c][m_WavePosition[c]>>8];
						a=wavepnt[c][(( unsigned short)(m_WavePosition[c]+256))>>8];
						a*=(m_WavePosition[c]&255);
						b*=(255-(m_WavePosition[c]&255));
						a=(a>>8)+(b>>8);
						lsample += (a*volume[c])>>8;
						echosamplel += (a*echovolume[c])>>8;
						m_WavePosition[c]+=freqoffset[c];
						m_WavePosition[c]&=wavelength[c];
					}
					break;
					case 1:
					{
						if(samplepos[c]!=0xffffffff)
						{
							b=wavepnt[c][samplepos[c]>>8];
							a=wavepnt[c][(samplepos[c]+256)>>8];
							a*=(samplepos[c]&255);
							b*=(255-(samplepos[c]&255));
							a=(a>>8)+(b>>8);
							lsample += (a*volume[c])>>8;
							echosamplel += (a*echovolume[c])>>8;
							if(curdirecflg[c]==0) //richting end?
							{
								samplepos[c]+=(int)freqoffset[c];
								if(samplepos[c]>=waveend[c])
								{
									if(loopflg[c]==0) //one shot?
									{
										samplepos[c]=0xffffffff;
									}
									else // looping
									{
										if(bidirecflg[c]==0) //1 richting??
										{
											samplepos[c]-=(waveend[c]-waveloop[c]);
										}
										else
										{
											samplepos[c]-=(int)freqoffset[c];
											curdirecflg[c]=1; //richting omdraaien
										}
									}
								}
							}
							else   // weer bidirectioneel terug naar looppoint
							{
								samplepos[c]-=(int)freqoffset[c];
								if(samplepos[c]<=waveloop[c])
								{
									samplepos[c]+=(int)freqoffset[c];
									curdirecflg[c]=0; //richting omdraaien
								}
							}
						}
					}
					break;
					}
				}
				lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;

	// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

				lsample *= amplification;
				lsample /= 100;
				short smpl;
				if(lsample>32760) lsample=32760;
				if(lsample<-32760) lsample=-32760;

				smpl=lsample;
				if(m_OverlapCnt<SE_OVERLAP)
				{
					smpl=(((int)smpl*(m_OverlapCnt))/SE_OVERLAP)+(((int)m_OverlapBuffer[m_OverlapCnt]*(SE_OVERLAP-m_OverlapCnt))/SE_OVERLAP);
					m_OverlapCnt++;
				}
				renderbuf[r++]=smpl;

				m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				m_DelayCnt++;
				m_DelayCnt%=echodelaytime/(44100/frequency);

			}
		}
		else  // was this a dry render?  then we should increment r anyway.
		{
			r += (nos*2);   // times 2 since we must think stereo
		}
// 

		for(i=0;i<m_NrOfChannels;i++)
		{
			if(issample[i])
			{
				m_ChannelData[i]->samplepos = samplepos[i];
				m_ChannelData[i]->curdirecflg = curdirecflg[i];
			}
		}

		if(m_TimeCnt==((m_TimeSpd*frequency)/44100))
		{
// Hier komt een speciaal gedeelte
// we berekenen nog 'SE_OVERLAP' samples door zodat we die straks kunnen mengen met de 'SE_OVERLAP' volgende samples
// om zo eventuele tikken weg te werken

			unsigned short tempwavepos[SE_MAXCHANS];		// laatste positie per channel in de waveform(256 te groot)(om tijdelijk op te slaan)
			unsigned short tempdelaycnt;		// ditto

			m_OverlapCnt=0; //resetten van de overlap counter
			//opbergen van tellers
			tempdelaycnt=m_DelayCnt;
			memcpy(tempwavepos,m_WavePosition,sizeof(m_WavePosition));

			if(renderbuf)  // dry rendering? or actual rendering?
			{
				for(p=0;p<SE_OVERLAP;p++)
				{
					lsample=0;
					rsample=0;
					echosamplel=0;
					echosampler=0;
					for(c=0;c<m_NrOfChannels;c++)
					{
						int a,b;
						switch (issample[c])
						{
						case 0: //is waveform
						{
							b=wavepnt[c][m_WavePosition[c]>>8];
							a=wavepnt[c][(( unsigned short)(m_WavePosition[c]+256))>>8];
							a*=(m_WavePosition[c]&255);
							b*=(255-(m_WavePosition[c]&255));
							a=(a>>8)+(b>>8);
							lsample += (a*volume[c])>>8;
							echosamplel += (a*echovolume[c])>>8;
							m_WavePosition[c]+=freqoffset[c];
							m_WavePosition[c]&=wavelength[c];
						}
						break;
						case 1:
						{
							if(samplepos[c]!=0xffffffff)
							{
								b=wavepnt[c][samplepos[c]>>8];
								a=wavepnt[c][(samplepos[c]+256)>>8];
								a*=(samplepos[c]&255);
								b*=(255-(samplepos[c]&255));
								a=(a>>8)+(b>>8);
								lsample += (a*volume[c])>>8;
								echosamplel += (a*echovolume[c])>>8;
								if(curdirecflg[c]==0) //richting end?
								{
									samplepos[c]+=(int)freqoffset[c];
									if(samplepos[c]>=waveend[c])
									{
										if(loopflg[c]==0) //one shot?
										{
											samplepos[c]=0xffffffff;
										}
										else // looping
										{
											if(bidirecflg[c]==0) //1 richting??
											{
												samplepos[c]-=(waveend[c]-waveloop[c]);
											}
											else
											{
												samplepos[c]-=(int)freqoffset[c];
												curdirecflg[c]=1; //richting omdraaien
											}
										}
									}
								}
								else   // weer bidirectioneel terug naar looppoint
								{
									samplepos[c]-=(int)freqoffset[c];
									if(samplepos[c]<=waveloop[c])
									{
										samplepos[c]+=(int)freqoffset[c];
										curdirecflg[c]=0; //richting omdraaien
									}
								}
							}
						}
						break;
						}
					}
					lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;

		// Als we nog in het onderste SE_OVERLAP gedeelte zitten moeten we hier nog de vorige wave erdoorheen mixen (om tikken tegen te gaan)

					lsample *= amplification;
					lsample /= 100;
					if(lsample>32760) lsample=32760;
					if(lsample<-32760) lsample=-32760;


					m_OverlapBuffer[p]=lsample;

					m_DelayCnt++;
					m_DelayCnt%=echodelaytime/(44100/frequency);
				}
			}   // end of dry buffer run
			// na de SE_OVERLAP samples berekent te hebben moeten we alle tellers terugzetten alsof er niets gebeurt is

			m_DelayCnt=tempdelaycnt;
			memcpy(m_WavePosition,tempwavepos,sizeof(m_WavePosition));
			
			// song pointers doorknallen

			AdvanceSong();
		}
	}
}


// Optimized mono renderer... No high quality overlapmixbuf
void SoundEngine::RenderChannels3Mono(short *renderbuf, int nrofsamples, int frequency)
{
	channelvar channelvars[SE_MAXCHANS];
	channelvar *curvars;
	short i,p,c;
	int r;
	short nos;										//number of samples
	int lsample,rsample,echosamplel,echosampler;	//current state of samples
	short nrtomix;									// nr of wave streams to mix(is number of channels +1 for the echo)
	short amplification;							// amount to amplify afterwards
	unsigned short echodelaytime;					//delaytime for echo (differs for MIDI or songplayback)

	// we calc nrofsamples samples in blocks of 'm_TimeCnt' big (de songspd)

	if(m_NrOfChannels==0)	// no active channels?
	{
		for(i=0;i<nrofsamples;i++)	//c lean renderbuffer
		{
			renderbuf[i]=0;
		}
		return;
	}

	r=0;
	nrtomix=m_NrOfChannels+1;  //1 extra for the echoes
	while(nrofsamples>0)
	{
// 2 possibilities:  we calc a complete m_TimeCnt block...or the last part of a m_TimeCnt block...
		if(m_TimeCnt<nrofsamples)
		{
			nos = m_TimeCnt;   //Complete block
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		}
		else
		{
			nos = nrofsamples;  //Last piece
			m_TimeCnt=m_TimeCnt-nos; 
		}
		nrofsamples-=nos;

		// preparation of wave pointers and freq offset
		for(i=0;i<m_NrOfChannels;i++)
		{
			int inst,instnr;
			instnr = m_ChannelData[i]->instrument;
			if (instnr == -1) // mute?
			{
				channelvars[i].wavepnt=m_EmptyWave;
				channelvars[i].issample=0;
			}
			else
			{
				if(m_ChannelData[i]->sampledata)
				{
					channelvars[i].samplepos=m_ChannelData[i]->samplepos;
					channelvars[i].wavepnt=(short *)m_ChannelData[i]->sampledata;
					channelvars[i].waveloop=m_ChannelData[i]->looppoint; 
					channelvars[i].waveend=m_ChannelData[i]->endpoint; 
					channelvars[i].loopflg=m_ChannelData[i]->loopflg;
					channelvars[i].bidirecflg=m_ChannelData[i]->bidirecflg;
					channelvars[i].curdirecflg=m_ChannelData[i]->curdirecflg;
					channelvars[i].issample=1;
				}
				else
				{
					inst = m_Instruments[instnr]->waveform;
					channelvars[i].wavepnt=&m_ChannelData[i]->waves[256*inst];
					channelvars[i].wavelength=((m_Instruments[instnr]->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					channelvars[i].issample=0;
				}
			}

			channelvars[i].localwavepos = m_WavePosition[i];

	//calcen frequency
			if(m_ChannelData[i]->curfreq<10)m_ChannelData[i]->curfreq=10;
			channelvars[i].freqoffset = (256*m_ChannelData[i]->curfreq)/frequency;
			channelvars[i].volume = (m_ChannelData[i]->curvol+10000)/39;
			if(channelvars[i].volume>256) channelvars[i].volume=256;

			if(m_ChannelData[i]->curpan==0) //panning?
			{
				channelvars[i].panl=256; //geen panning
				channelvars[i].panr=256;
			}
			else
			{
				if(m_ChannelData[i]->curpan>0)
				{
					channelvars[i].panl = 256-(m_ChannelData[i]->curpan);
					channelvars[i].panr = 256;
				}
				else
				{
					channelvars[i].panr = 256+(m_ChannelData[i]->curpan);
					channelvars[i].panl=256;
				}
			}

			channelvars[i].echovolume = m_Songs[m_CurrentSubsong]->delayamount[i];

			//stereo optimimalisation 1... precalc panning multiplied with mainvolume
			channelvars[i].panl = (channelvars[i].panl * channelvars[i].volume)>>8;
			channelvars[i].panr = (channelvars[i].panr * channelvars[i].volume)>>8;

			//stereo optimimalisation 1... precalc panning multiplied with echo
			channelvars[i].echovolume = (channelvars[i].echovolume * channelvars[i].volume)>>8;


		}
		amplification = m_Songs[m_CurrentSubsong]->amplification;
		echodelaytime = m_Songs[m_CurrentSubsong]->delaytime;

		if(renderbuf)  // dry rendering? or actual rendering?
		{
			for(p=0;p<nos;p++)
			{
				lsample=0;
				rsample=0;
				echosamplel=0;
				echosampler=0;
				for(c=0;c<m_NrOfChannels;c++)
				{
					curvars = &channelvars[c];
					int b;
					switch (curvars->issample)
					{
					case 0: //is waveform
					{
						b=curvars->wavepnt[curvars->localwavepos>>8];
						lsample += (b*curvars->volume)>>8;
						echosamplel += (b*curvars->echovolume)>>8;
						curvars->localwavepos+=curvars->freqoffset;
						curvars->localwavepos&=curvars->wavelength;
					}
					break;
					case 1:
					{
						if(curvars->samplepos!=0xffffffff)
						{
							b=curvars->wavepnt[curvars->samplepos>>8];
							lsample += (b*curvars->volume)>>8;
							echosamplel += (b*curvars->echovolume)>>8;
							if(curvars->curdirecflg==0) //direction of end?
							{
								curvars->samplepos+=(int)curvars->freqoffset;
								if(curvars->samplepos>=curvars->waveend)
								{
									if(curvars->loopflg==0) //one shot?
									{
										curvars->samplepos=0xffffffff;
									}
									else // looping
									{
										if(curvars->bidirecflg==0) //1 direction??
										{
											curvars->samplepos-=(curvars->waveend-curvars->waveloop);
										}
										else
										{
											curvars->samplepos-=(int)curvars->freqoffset;
											curvars->curdirecflg=1; //reverse direction
										}
									}
								}
							}
							else   // bidirectioneel, back to looppoint
							{
								curvars->samplepos-=(int)curvars->freqoffset;
								if(curvars->samplepos<=curvars->waveloop)
								{
									curvars->samplepos+=(int)curvars->freqoffset;
									curvars->curdirecflg=0; //reverse direction
								}
							}
						}
					}
					break;
					}
				}
				lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;

	// if we are still in the bottom SE_OVERLAP part then we have to mix the previous wave through (to avoid clicks)

				lsample *= amplification;
				lsample /= 100;
				short smpl;
				if(lsample>32760) lsample=32760;
				if(lsample<-32760) lsample=-32760;

				smpl=lsample;
				renderbuf[r++]=smpl;

				m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
				m_DelayCnt++;
				m_DelayCnt%=(echodelaytime/(44100/frequency));

			}
		}
		else  // was this a dry render?  then we should increment r anyway.
		{
			r += (nos*2);   // times 2 since we must think stereo
		}
// 

		for(i=0;i<m_NrOfChannels;i++)
		{
			m_WavePosition[i] = channelvars[i].localwavepos;
			if(channelvars[i].issample)
			{
				m_ChannelData[i]->samplepos = channelvars[i].samplepos;
				m_ChannelData[i]->curdirecflg = channelvars[i].curdirecflg;
			}
		}

		if(m_TimeCnt==((m_TimeSpd*frequency)/44100))
		{
			unsigned short tempwavepos[SE_MAXCHANS];		// last position per channel in the waveform(8bit fixed point)(temporarily storage)
			unsigned short tempdelaycnt;					// ditto

			m_OverlapCnt=0; //reset overlap counter
			tempdelaycnt=m_DelayCnt;
			for(c=0;c<SE_MAXCHANS;c++)
			{
				tempwavepos[c] = channelvars[c].localwavepos;
			}

			m_DelayCnt=tempdelaycnt;

			for(c=0;c<SE_MAXCHANS;c++)
			{
				m_WavePosition[c] = tempwavepos[c];
			}
			
			//Update song pointers
			AdvanceSong();
		}
	}
}
