/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2015 Allen Institute for Brain Science

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef NEUROPIXTHREAD_H_INCLUDED
#define NEUROPIXTHREAD_H_INCLUDED

#include "../../../JuceLibraryCode/JuceHeader.h"

#include <stdio.h>
#include <string.h>

#include "DataThread.h"
#include "../GenericProcessor/GenericProcessor.h"

#include "neuropix-api/Neuropix_basestation_api.h"
#include "neuropix-api/ElectrodePacket.h"


/**

  Communicates with imec Neuropix probes.

  @see DataThread, SourceNode

*/

class NeuropixThread : public DataThread, public Timer
{

public:

    NeuropixThread(SourceNode* sn);
    ~NeuropixThread();

    bool updateBuffer();

	void updateChannels();

    /** Returns true if the data source is connected, false otherwise.*/
    bool foundInputSource();

	/** Returns version info for hardware and API.*/
	void getInfo(String& hwVersion, String& bsVersion, String& apiVersion, String& asicInfo);

    /** Initializes data transfer.*/
    bool startAcquisition();

    /** Stops data transfer.*/
    bool stopAcquisition();

    /** Returns the number of continuous headstage channels the data source can provide.*/
    int getNumHeadstageOutputs();

    /** Returns the number of continuous aux channels the data source can provide.*/
	int getNumAuxOutputs();

    /** Returns the number of continuous ADC channels the data source can provide.*/
	int getNumAdcOutputs();

    /** Returns the sample rate of the data source.*/
    float getSampleRate();

    /** Returns the volts per bit of the data source.*/
    float getBitVolts(Channel* chan);

    /** Returns the number of event channels of the data source.*/
	int getNumEventChannels();

	/** Selects which electrode is connected to each channel. */
	void selectElectrode(int chNum, int connection, bool transmit);

	/** Selects which reference is used for each channel. */
	void setReference(int chNum, int refSetting);

	/** Selects which reference is used for each channel. */
	void setAllReferences(int refSetting, int bankForReference);

	/** Sets the gain for each channel. */
	void setGain(int ch, int apGain, int lfpGain);

	/** Sets the AP gains for all channels. **/
	void setAllApGains(int apGain);

	/** Sets the LFP gains for all channels. **/
	void setAllLfpGains(int lfpGain);

	/** Sets the filter for all channels. */
	void setFilter(int filter);

    /** Toggles between internal and external triggering. */
    void setTriggerMode(bool trigger);

	/** Toggles between saving to NPX file. */
	void setRecordMode(bool record);

	/** Loads calibration settings stored on EEPROM. */
	void calibrateProbe();

	/** Retrieve probe option. */
	int getProbeOption();

	/** Starts data acquisition after a certain time.*/
	void timerCallback();

	/** Turns AP data output on and off.*/
	void toggleApData(bool send);

	/** Turns LFP data output on and off.*/
	void toggleLfpData(bool send);

	CriticalSection* getMutex()
	{
		return &displayMutex;
	}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuropixThread);

private:
    bool baseStationAvailable;
    bool internalTrigger;
	bool recordToNpx;
	bool sendAp;
	bool sendLfp;

	Neuropix_basestation_api neuropix;
	
	long int counter;
	int recordingNumber;

	CriticalSection displayMutex;

	VersionNumber hw_version;
	unsigned char bs_version;
	unsigned char bs_revision;
	VersionNumber vn;
	AsicID asicId;

	Array<int> gains;
	Array<int> apGains;
	Array<int> lfpGains;
	Array<int> channelMap;
	Array<bool> outputOn;
	Array<int> refs;

	void openConnection();
	void closeConnection();

	int64 timestamp;
	uint64 eventCode;
	int maxCounter;

	uint8 option;
	int numRefs;
	int totalChans;

};

#endif  // NEUROPIXTHREAD_H_INCLUDED
