//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		difficulty file loader and container
//
// $NoKeywords: $osudiff
//===============================================================================//

#include "OsuBeatmapDifficulty.h"

#include "Engine.h"
#include "ResourceManager.h"
#include "ConVar.h"

#include "Osu.h"
#include "OsuNotificationOverlay.h"
#include "OsuSkin.h"

#include "OsuHitObject.h"
#include "OsuCircle.h"
#include "OsuSlider.h"
#include "OsuSpinner.h"

OsuBeatmapDifficulty::OsuBeatmapDifficulty(Osu *osu, UString filepath, UString folder)
{
	m_osu = osu;

	loaded = false;
	m_sFilePath = filepath;
	m_sFolder = folder;

	// default values
	stackLeniency = 0.7f;

	beatmapId = -1;

	CS = 5;
	AR = 5;
	OD = 5;
	HP = 5;

	sliderMultiplier = 1;
	sliderTickRate = 1;

	previewTime = 0;
	mode = 0;

	backgroundImage = NULL;
	localoffset = 0;
	minBPM = 0;
	maxBPM = 0;
}

bool OsuBeatmapDifficulty::loadMetadata()
{
	if (Osu::debug->getBool())
		debugLog("OsuBeatmapDifficulty::loadMetadata() : %s\n", m_sFilePath.toUtf8());

	// open osu file
	std::ifstream file(m_sFilePath.toUtf8());
	if (!file.good())
	{
		//UString errorMessage = "Error: Couldn't load beatmap file";
		//errorMessage.append(m_sFilePath);
		//engine->showMessageError("Error", errorMessage);
		///m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
		debugLog("Osu Error: Couldn't fopen() file %s\n", m_sFilePath.toUtf8());
		return false;
	}

	// load metadata only
	int curBlock = -1;

	bool foundAR = false;

	std::string curLine;
	while (std::getline(file, curLine))
	{
		const char *curLineChar = curLine.c_str();

		if (curLine.find("//") == std::string::npos) // ignore comments
		{
			if (curLine.find("[General]") != std::string::npos)
				curBlock = 0;
			else if (curLine.find("[Metadata]") != std::string::npos)
				curBlock = 1;
			else if (curLine.find("[Difficulty]") != std::string::npos)
				curBlock = 2;
			else if (curLine.find("[Events]") != std::string::npos)
				curBlock = 3;
			else if (curLine.find("[Colours]") != std::string::npos)
				curBlock = 4;
			else if (curLine.find("[TimingPoints]") != std::string::npos)
				curBlock = 5;
			else if (curLine.find("[HitObjects]") != std::string::npos)
				break; // stop early

			switch (curBlock)
			{
			case 0: // General
				{
					char stringBuffer[1024];
					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "AudioFilename:%1023[^\n]", stringBuffer) == 1)
					{
						audioFileName = UString(stringBuffer);
						audioFileName = audioFileName.trim();
					}

					sscanf(curLineChar, "StackLeniency:%f\n", &stackLeniency);
					sscanf(curLineChar, "PreviewTime:%lu\n", &previewTime);
					sscanf(curLineChar, "Mode:%i\n", &mode);
				}
				break;

			case 1: // Metadata
				{
					char stringBuffer[1024];
					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "Title:%1023[^\n]", stringBuffer) == 1)
					{
						title = UString(stringBuffer);
						title = title.trim();
					}

					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "Artist:%1023[^\n]", stringBuffer) == 1)
					{
						artist = UString(stringBuffer);
						artist = artist.trim();
					}

					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "Creator:%1023[^\n]", stringBuffer) == 1)
					{
						creator = UString(stringBuffer);
						creator = creator.trim();
					}

					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "Version:%1023[^\n]", stringBuffer) == 1)
					{
						name = UString(stringBuffer);
						name = name.trim();
					}

					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "Source:%1023[^\n]", stringBuffer) == 1)
					{
						source = UString(stringBuffer);
						source = source.trim();
					}

					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "Tags:%1023[^\n]", stringBuffer) == 1)
					{
						tags = UString(stringBuffer);
						tags = tags.trim();
					}

					memset(stringBuffer, '\0', 1024);
					if (sscanf(curLineChar, "BeatmapID:%1023[^\n]", stringBuffer) == 1)
					{
						// FUCK stol(), causing crashes in e.g. std::stol("--123456");
						//beatmapId = std::stol(stringBuffer);
						beatmapId = 0;
					}
				}
				break;

			case 2: // Difficulty
				sscanf(curLineChar, "CircleSize:%f\n", &CS);
				if (sscanf(curLineChar, "ApproachRate:%f\n", &AR) == 1)
					foundAR = true;
				sscanf(curLineChar, "HPDrainRate:%f\n", &HP);
				sscanf(curLineChar, "OverallDifficulty:%f\n", &OD);
				sscanf(curLineChar, "SliderMultiplier:%f\n", &sliderMultiplier);
				sscanf(curLineChar, "SliderTickRate:%f\n", &sliderTickRate);
				break;
			case 3: // Events
				{
					char stringBuffer[1024];
					memset(stringBuffer, '\0', 1024);
					float temp;
					if (sscanf(curLineChar, "%f,%f,\"%1023[^\"]\"", &temp, &temp, stringBuffer) == 3)
					{
						backgroundImageName = UString(stringBuffer);
					}
				}
				break;
			case 4: // Colours
				{
					int comboNum;
					int r,g,b;
					if (sscanf(curLineChar, "Combo%i : %i,%i,%i\n", &comboNum, &r, &g, &b) == 4)
						combocolors.push_back(COLOR(255, r, g, b));
				}
				break;
			case 5: // TimingPoints

				// old beatmaps: Offset, Milliseconds per Beat
				// new beatmaps: Offset, Milliseconds per Beat, Meter, Sample Type, Sample Set, Volume, Inherited, Kiai Mode

				double tpOffset;
				float tpMSPerBeat;
				int tpMeter;
				int tpSampleType,tpSampleSet;
				int tpVolume;
				if (sscanf(curLineChar, "%lf,%f,%i,%i,%i,%i", &tpOffset, &tpMSPerBeat, &tpMeter, &tpSampleType, &tpSampleSet, &tpVolume) == 6)
				{
					TIMINGPOINT t;
					t.offset = (long)std::round(tpOffset);
					t.msPerBeat = tpMSPerBeat;
					t.sampleType = tpSampleType;
					t.sampleSet = tpSampleSet;
					t.volume = tpVolume;

					timingpoints.push_back(t);
				}
				else if (sscanf(curLineChar, "%lf,%f", &tpOffset, &tpMSPerBeat) == 2)
				{
					TIMINGPOINT t;
					t.offset = (long)std::round(tpOffset);
					t.msPerBeat = tpMSPerBeat;

					t.sampleType = 0;
					t.sampleSet = 0;
					t.volume = 100;

					timingpoints.push_back(t);
				}
				break;
			}
		}
	}

	// only allow osu!standard diffs for now
	if (mode != 0)
		return false;

	// check if the sound file exists
	fullSoundFilePath = m_sFolder;
	fullSoundFilePath.append(audioFileName);
	if (!env->fileExists(fullSoundFilePath))
	{
		UString errorMessage = "Error: Couldn't find sound file";
		debugLog("Osu Error: Couldn't find sound file (%s)!\n", fullSoundFilePath.toUtf8());
		//m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
		return false;
	}

	// calculate BPM range
	if (timingpoints.size() > 0)
	{
		if (Osu::debug->getBool())
			debugLog("OsuBeatmapDifficulty::loadMetadata() : calculating BPM range ...\n");

		// sort timingpoints by time
		struct TimingPointSortComparator
		{
		    bool operator() (TIMINGPOINT const &a, TIMINGPOINT const &b) const
		    {
		    	// first condition: offset
		    	// second condition: if offset is the same, non-inherited timingpoints go before inherited timingpoints
		        return (a.offset < b.offset) || (a.offset == b.offset && a.msPerBeat >= 0 && b.msPerBeat < 0);
		    }
		};
		std::sort(timingpoints.begin(), timingpoints.end(), TimingPointSortComparator());

		float tempMinBPM = 0;
		float tempMaxBPM = std::numeric_limits<float>::max();

		float beatLength = std::abs(timingpoints[0].msPerBeat);

		// get first non-inherited timingpoint as a base
		for (int i=0; i<timingpoints.size(); i++)
		{
			if (timingpoints[i].msPerBeat >= 0) // NOT inherited
			{
				beatLength = timingpoints[i].msPerBeat;
				break;
			}
		}
		// now go through all normally
		for (int i=0; i<timingpoints.size(); i++)
		{
			TIMINGPOINT *t = &timingpoints[i];

			if (t->msPerBeat >= 0) // NOT inherited
				beatLength = std::abs(t->msPerBeat);

			if (beatLength > tempMinBPM)
				tempMinBPM = beatLength;

			if (beatLength < tempMaxBPM)
				tempMaxBPM = beatLength;
		}

		// convert from msPerBeat to BPM
		float msPerMinute = 1 * 60 * 1000;
		if (tempMinBPM != 0)
			tempMinBPM = msPerMinute / tempMinBPM;
		if (tempMaxBPM != 0)
			tempMaxBPM = msPerMinute / tempMaxBPM;

		minBPM = (int)std::round(tempMinBPM);
		maxBPM = (int)std::round(tempMaxBPM);
	}

	// old beatmaps: AR = OD, there is no ApproachRate stored
	if (!foundAR)
		AR = OD;

	return true;
}

bool OsuBeatmapDifficulty::load(OsuBeatmap *beatmap, std::vector<OsuHitObject*> *hitobjects)
{
	unload();

	// open osu file
	std::ifstream file(m_sFilePath.toUtf8());
	if (!file.good())
	{
		UString errorMessage = "Error: Couldn't load beatmap file";
		debugLog("Osu Error: Couldn't load beatmap file (%s)!\n", m_sFilePath.toUtf8());
		if (m_osu != NULL)
			m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
		return false;
	}

	// load beatmap skin
	beatmap->getOsu()->getSkin()->setBeatmapComboColors(combocolors);
	beatmap->getOsu()->getSkin()->loadBeatmapOverride(m_sFolder);

	// load the actual beatmap
	int colorCounter = 1;
	int comboNumber = 1;
	int curBlock = -1;

	std::string curLine;
	while (std::getline(file, curLine))
	{
		const char *curLineChar = curLine.c_str();

		if (curLine.find("//") == std::string::npos) // ignore comments
		{
			if (curLine.find("[TimingPoints]") != std::string::npos)
				curBlock = 3;
			else if (curLine.find("[HitObjects]") != std::string::npos)
				curBlock = 4;

			switch (curBlock)
			{
			case 3: // TimingPoints

				// old beatmaps: Offset, Milliseconds per Beat
				// new beatmaps: Offset, Milliseconds per Beat, Meter, Sample Type, Sample Set, Volume, Inherited, Kiai Mode

				double tpOffset;
				float tpMSPerBeat;
				int tpMeter;
				int tpSampleType,tpSampleSet;
				int tpVolume;
				if (sscanf(curLineChar, "%lf,%f,%i,%i,%i,%i", &tpOffset, &tpMSPerBeat, &tpMeter, &tpSampleType, &tpSampleSet, &tpVolume) == 6)
				{
					TIMINGPOINT t;
					t.offset = (long)std::round(tpOffset);
					t.msPerBeat = tpMSPerBeat;
					t.sampleType = tpSampleType;
					t.sampleSet = tpSampleSet;
					t.volume = tpVolume;

					timingpoints.push_back(t);
				}
				else if (sscanf(curLineChar, "%lf,%f", &tpOffset, &tpMSPerBeat) == 2)
				{
					TIMINGPOINT t;
					t.offset = (long)std::round(tpOffset);
					t.msPerBeat = tpMSPerBeat;

					t.sampleType = 0;
					t.sampleSet = 0;
					t.volume = 100;

					timingpoints.push_back(t);
				}
				break;

			case 4: // HitObjects

				// circles:
				// x,y,time,type,hitSound,addition
				// sliders:
				// x,y,time,type,hitSound,sliderType|curveX:curveY|...,repeat,pixelLength,edgeHitsound,edgeAddition,addition
				// spinners:
				// x,y,time,type,hitSound,endTime,addition

				int x,y;
				long time;
				int type;
				int hitSound;

				if (sscanf(curLineChar, "%i,%i,%li,%i,%i", &x, &y, &time, &type, &hitSound) == 5)
				{
					if (type & 0x4) // new combo
					{
						comboNumber = 1;
						colorCounter++;
					}

					if (type & 0x1) // circle
					{
						HITCIRCLE c;

						c.x = x;
						c.y = y;
						c.time = time;
						c.sampleType = hitSound;
						c.number = comboNumber++;
						c.colorCounter = colorCounter;
						c.clicked = false;

						hitcircles.push_back(c);
					}
					else if (type & 0x2) // slider
					{
						// infinity sanity check (this only exists because of https://osu.ppy.sh/b/1029976)
						// not a very elegant check, but it does the job
						if (curLine.find("E") != std::string::npos || curLine.find("e") != std::string::npos)
						{
							debugLog("Bullshit slider in beatmap: %s\n\ncurLine = %s\n", m_sFilePath.toUtf8(), curLineChar);
							continue;
						}

						UString curLineString = UString(curLineChar);
						std::vector<UString> tokens = curLineString.split(",");
						if (tokens.size() < 8)
						{
							debugLog("Invalid slider in beatmap: %s\n\ncurLine = %s\n", m_sFilePath.toUtf8(), curLineChar);
							continue;
							//engine->showMessageError("Error", UString::format("Invalid slider in beatmap: %s\n\ncurLine = %s", m_sFilePath.toUtf8(), curLine));
							//return false;
						}
						std::vector<UString> sliderTokens = tokens[5].split("|");
						if (sliderTokens.size() < 2)
						{
							debugLog("Invalid slider tokens: %s\n\nIn beatmap: %s\n", curLineChar, m_sFilePath.toUtf8());
							continue;
							//engine->showMessageError("Error", UString::format("Invalid slider tokens: %s\n\nIn beatmap: %s", curLineChar, m_sFilePath.toUtf8()));
							//return false;
						}

						const float sanityRange = 65536/2; // infinity sanity check, same as before
						std::vector<Vector2> points;
						points.push_back(Vector2(clamp<float>(x, -sanityRange, sanityRange), clamp<float>(y, -sanityRange, sanityRange)));
						for (int i=1; i<sliderTokens.size(); i++)
						{
							std::vector<UString> sliderXY = sliderTokens[i].split(":");
							if (sliderXY.size() != 2)
							{
								debugLog("Invalid slider positions: %s\n\nIn Beatmap: %s\n", curLineChar, m_sFilePath.toUtf8());
								continue;
								//engine->showMessageError("Error", UString::format("Invalid slider positions: %s\n\nIn beatmap: %s", curLine, m_sFilePath.toUtf8()));
								//return false;
							}
							points.push_back(Vector2((int)clamp<float>(sliderXY[0].toFloat(), -sanityRange, sanityRange), (int)clamp<float>(sliderXY[1].toFloat(), -sanityRange, sanityRange)));
						}

						SLIDER s;
						s.type = sliderTokens[0][0];
						s.repeat = (int)tokens[6].toFloat();
						s.pixelLength = tokens[7].toFloat();
						s.time = time;
						s.sampleType = hitSound;
						s.number = comboNumber++;
						s.colorCounter = colorCounter;
						s.points = points;

						sliders.push_back(s);
					}
					else if (type & 0x8) // spinner
					{
						UString curLineString = UString(curLineChar);
						std::vector<UString> tokens = curLineString.split(",");
						if (tokens.size() < 6)
						{
							debugLog("Invalid spinner in beatmap: %s\n\ncurLine = %s\n", m_sFilePath.toUtf8(), curLineChar);
							continue;
							//engine->showMessageError("Error", UString::format("Invalid spinner in beatmap: %s\n\ncurLine = %s", m_sFilePath.toUtf8(), curLine));
							//return false;
						}

						SPINNER s;
						s.x = x;
						s.y = y;
						s.time = time;
						s.sampleType = hitSound;
						s.endTime = tokens[5].toFloat();

						spinners.push_back(s);
					}
				}
				break;
			}
		}
	}

	// check if we have any timingpoints at all
	if (timingpoints.size() == 0)
	{
		UString errorMessage = "Error: No timingpoints in beatmap";
		debugLog("Osu Error: No timingpoints in beatmap (%s)!\n", m_sFilePath.toUtf8());
		if (m_osu != NULL)
			m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
		return false;
	}

	// sort timingpoints by time
	struct TimingPointSortComparator
	{
	    bool operator() (TIMINGPOINT const &a, TIMINGPOINT const &b) const
	    {
	    	// first condition: offset
	    	// second condition: if offset is the same, non-inherited timingpoints go before inherited timingpoints
	        return (a.offset < b.offset) || (a.offset == b.offset && a.msPerBeat >= 0 && b.msPerBeat < 0);
	    }
	};
	std::sort(timingpoints.begin(), timingpoints.end(), TimingPointSortComparator());

	// calculate sliderTimes, and build clicks and ticks
	for (int i=0; i<sliders.size(); i++)
	{
		SLIDER *s = &sliders[i];
		s->sliderTimeWithoutRepeats = getSliderTimeForSlider(s);
		s->sliderTime = s->sliderTimeWithoutRepeats * s->repeat;

		// start & end clicks
		s->clicked.push_back( (struct SLIDER_CLICK) {s->time, false} );
		s->clicked.push_back( (struct SLIDER_CLICK) {s->time + (long)s->sliderTime, false} );

		// repeat clicks
		if (s->repeat > 1)
		{
			long part = (long) (s->sliderTime / (float) s->repeat);
			for (int c=1; c<s->repeat; c++)
			{
				s->clicked.push_back( (struct SLIDER_CLICK) {s->time + (long)(part*c), false} );
			}
		}

		// ticks
		float tickLengthDiv = ((100.0f * sliderMultiplier) / sliderTickRate) / getTimingPointMultiplierForSlider(s);
		int tickCount = (int) std::ceil(s->pixelLength / tickLengthDiv) - 1;
		if (tickCount > 0)
		{
			float tickTOffset = 1.0f / (tickCount + 1);
			float t = tickTOffset;
			for (int i = 0; i < tickCount; i++, t += tickTOffset)
			{
				s->ticks.push_back(t);
			}
		}
	}

	// build all hitobjects
	for (int i=0; i<hitcircles.size(); i++)
	{
		OsuBeatmapDifficulty::HITCIRCLE *c = &hitcircles[i];
		hitobjects->push_back(new OsuCircle(c->x, c->y, c->time, c->sampleType, c->number, c->colorCounter, beatmap));
	}
	for (int i=0; i<sliders.size(); i++)
	{
		OsuBeatmapDifficulty::SLIDER *s = &sliders[i];
		hitobjects->push_back(new OsuSlider(s->type, s->repeat, s->pixelLength, s->points, s->ticks, s->sliderTime, s->sliderTimeWithoutRepeats, s->time, s->sampleType, s->number, s->colorCounter, beatmap));
	}
	for (int i=0; i<spinners.size(); i++)
	{
		OsuBeatmapDifficulty::SPINNER *s = &spinners[i];
		hitobjects->push_back(new OsuSpinner(s->x, s->y, s->time, s->sampleType, s->endTime, beatmap));
	}

	// sort hitobjects by time
	struct HitObjectSortComparator
	{
	    bool operator() (OsuHitObject const *a, OsuHitObject const *b) const
	    {
	        return a->getTime() < b->getTime();
	    }
	};
	std::sort(hitobjects->begin(), hitobjects->end(), HitObjectSortComparator());

	// special rule for first hitobject (for 1 approach circle with HD)
	if (hitobjects->size() > 0)
		(*hitobjects)[0]->setForceDrawApproachCircle(true);

	loaded = true;
	return true;
}

void OsuBeatmapDifficulty::unload()
{
	loaded = false;

	hitcircles = std::vector<HITCIRCLE>();
	sliders = std::vector<SLIDER>();
	spinners = std::vector<SPINNER>();
	timingpoints = std::vector<TIMINGPOINT>();
}

void OsuBeatmapDifficulty::loadBackgroundImage()
{
	if (backgroundImage != NULL)
		return;

	UString fullBackgroundImageFilePath = m_sFolder;
	fullBackgroundImageFilePath.append(backgroundImageName);
	if (env->fileExists(fullBackgroundImageFilePath)) // to prevent resource manager warnings
	{
		engine->getResourceManager()->requestNextLoadAsync();
		UString uniqueResourceName = fullBackgroundImageFilePath;
		uniqueResourceName.append(name);
		backgroundImage = engine->getResourceManager()->loadImageAbs(fullBackgroundImageFilePath, uniqueResourceName);
	}
}

void OsuBeatmapDifficulty::unloadBackgroundImage()
{
	//if (backgroundImage != NULL)
	//	debugLog("Unloading %s\n", backgroundImage->getFilePath().toUtf8());

	Image *tempPointer = backgroundImage;
	backgroundImage = NULL;
	engine->getResourceManager()->destroyResource(tempPointer);
}

float OsuBeatmapDifficulty::getSliderTimeForSlider(SLIDER *slider)
{
	return getTimingInfoForTime(slider->time).beatLength * (slider->pixelLength / sliderMultiplier) / 100.0f;
}

float OsuBeatmapDifficulty::getTimingPointMultiplierForSlider(SLIDER *slider)
{
	const TIMING_INFO t = getTimingInfoForTime(slider->time);
	float beatLengthBase = t.beatLengthBase;
	if (beatLengthBase == 0.0f) // sanity check
		beatLengthBase = 1.0f;
	return t.beatLength / beatLengthBase;
}

OsuBeatmapDifficulty::TIMING_INFO OsuBeatmapDifficulty::getTimingInfoForTime(unsigned long positionMS)
{
	TIMING_INFO ti;
	ti.offset = 1;
	ti.beatLengthBase = 1;
	ti.beatLength = 1;
	ti.volume = 1;
	ti.sampleType = 0;
	ti.sampleSet = 0;

	if (timingpoints.size() <= 0)
		return ti;

	// initial values (get first non-inherited timingpoint as base)
	for (int i=0; i<timingpoints.size(); i++)
	{
		TIMINGPOINT *t = &timingpoints[i];
		if (t->msPerBeat >= 0)
		{
			ti.beatLength = std::abs(t->msPerBeat);
			ti.beatLengthBase = ti.beatLength;
			break;
		}
	}

	// go through all timingpoints before positionMS
	for (int i=0; i<timingpoints.size(); i++)
	{
		TIMINGPOINT *t = &timingpoints[i];
		if (t->offset > (long)positionMS)
			break;

		if (t->msPerBeat >= 0) // NOT inherited
		{
			ti.beatLengthBase = ti.beatLength = t->msPerBeat;
			ti.offset = t->offset;
		}
		else // inherited
		{
			// note how msPerBeat is clamped
			ti.beatLength = ti.beatLengthBase * (clamp<float>(std::abs(t->msPerBeat), 10, 1000) / 100.0f); // sliderMultiplier of a timingpoint = (t->velocity / -100.0f)
		}

		ti.volume = t->volume;
		ti.sampleType = t->sampleType;
		ti.sampleSet = t->sampleSet;
	}

	return ti;
}
