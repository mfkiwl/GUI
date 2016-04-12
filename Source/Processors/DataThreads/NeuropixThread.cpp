/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2016 Allen Institute for Brain Science

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

#include "NeuropixThread.h"

#include "../SourceNode/SourceNode.h"

NeuropixThread::NeuropixThread(SourceNode* sn) : DataThread(sn), baseStationAvailable(false)
{

	dataBuffer = new DataBuffer(384, 10000); // start with 768 channels and automatically resize
	dataBuffer2 = new DataBuffer(384, 10000); // start with 768 channels and automatically resize

	// channel selections:
	// Options 1 & 2 -- fixed 384 channels
	// Option 3 -- select 384 of 960 shank electrodes
	// Option 4 -- select 276 of 966 shank electrodes

	for (int i = 0; i < 384; i++)
	{
		lfpGains.add(0);
		apGains.add(0);
		channelMap.add(i);
		outputOn.add(true);
	}

	gains.add(50);
	gains.add(125);
	gains.add(250);
	gains.add(500);
	gains.add(1000);
	gains.add(1500);
	gains.add(2000);
	gains.add(2500);

	refs.add(0);
	refs.add(37);
	refs.add(76);
	refs.add(113);
	refs.add(152);
	refs.add(189);
	refs.add(228);
	refs.add(265);
	refs.add(304);
	refs.add(341);
	refs.add(380);

	openConnection();

}

NeuropixThread::~NeuropixThread()
{
	closeConnection();
}

void NeuropixThread::openConnection()
{
	OpenErrorCode errorCode = neuropix.neuropix_open(); // establishes a data connection with the basestation

	if (errorCode == OPEN_SUCCESS)
	{
		std::cout << "Open success!" << std::endl;
	}
	else {
		CoreServices::sendStatusMessage("Failure with error code " + String(errorCode));
		std::cout << "Failure with error code " << String(errorCode) << std::endl;
		baseStationAvailable = false;
		return;
	}

	baseStationAvailable = true;
	internalTrigger = true;
	sendLfp = true;
	sendAp = true;
	recordToNpx = false;
	recordingNumber = 0;

	// // GET SYSTEM INFO:
	ErrorCode error1 = neuropix.neuropix_getHardwareVersion(&hw_version);
	ConfigAccessErrorCode error2 = neuropix.neuropix_getBSVersion(bs_version);
	ConfigAccessErrorCode error3 = neuropix.neuropix_getBSRevision(bs_revision);
	vn = neuropix.neuropix_getAPIVersion();
	EepromErrorCode error4 = neuropix.neuropix_readId(asicId);

	std::cout << "  Hardware version number: " << hw_version.major << "." << hw_version.minor << std::endl;
	std::cout << "  Basestation version number: " << String(bs_version) << "." << String(bs_revision) << std::endl;
	std::cout << "  API version number: " << vn.major << "." << vn.minor << std::endl;
	std::cout << "  Asic info: " << String(asicId.probeType) << std::endl;

	// prepare probe for streaming data
	ErrorCode err1 = neuropix.neuropix_datamode(true);
	std::cout << "set datamode error code: " << err1 << std::endl;
	DigitalControlErrorCode err0 = neuropix.neuropix_mode(ASIC_RECORDING);
	std::cout << "set mode error code: " << err0 << std::endl;
	DigitalControlErrorCode err3 = neuropix.neuropix_nrst(false);
	std::cout << "nrst 1 error code: " << err3 << std::endl;
	ErrorCode err4 = neuropix.neuropix_resetDatapath();
	std::cout << "reset datapath error code: " << err4 << std::endl;

	// set default parameters
	getProbeOption();
	setAllApGains(3);
	setAllLfpGains(2);
	
	if (option >= 2)
	{
		int totalChans;

		if (option == 2)
			totalChans = 384;
		else
			totalChans = 276;

		for (int i = 0; i < totalChans; i++)
		{
			selectElectrode(i, 0, false);
		}
		selectElectrode(totalChans - 1, 0, true);

		//for (int i = 0; i < 50; i++)
		//{
		//	selectElectrode(i, 1, false);
		//}
		//selectElectrode(totalChans, 1, true);

		//setAllReferences(37, 1);

		//for (int i = 0; i < 50; i++)
		//{
		//	selectElectrode(i, 0, false);
		//}
		//selectElectrode(totalChans, 0, true);
	}

	setAllReferences(0, 0);
}

void NeuropixThread::closeConnection()
{
	neuropix.neuropix_close(); // closes the data and configuration link 
}

/** Returns true if the data source is connected, false otherwise.*/
bool NeuropixThread::foundInputSource()
{
	return baseStationAvailable;
}

void NeuropixThread::getInfo(String& hwVersion, String& bsVersion, String& apiVersion, String& asicInfo)
{
	hwVersion = String(hw_version.major) + "." + String(hw_version.minor);
	bsVersion = String(bs_version) + "." + String(bs_revision);
	apiVersion = String(vn.major) + "." + String(vn.minor);
	asicInfo = String(asicId.probeType);
}

int NeuropixThread::getProbeOption()
{
	//option = asicId.probeType;
	//asicId.probeType = option - 1;
	//neuropix.neuropix_writeId(asicId);
	option = neuropix.neuropix_getOption();

	if (option < 3)
		numRefs = 10;
	else
		numRefs = 7;

	return option + 1;
}


/** Initializes data transfer.*/
bool NeuropixThread::startAcquisition()
{

	// clear the internal buffer
	dataBuffer->clear();
	dataBuffer2->clear();

	// stop data stream
	DigitalControlErrorCode err3 = neuropix.neuropix_nrst(false);
	std::cout << "nrst 1 error code: " << err3 << std::endl;

	// clear the buffer
	ErrorCode err4 = neuropix.neuropix_resetDatapath();
	std::cout << "reset datapath error code: " << err4 << std::endl;


	if (internalTrigger)
	{

		if (recordToNpx)
		{
			recordingNumber++;
			std::string filename = "recording";
			filename += std::to_string(recordingNumber);
			filename += ".npx";
			const std::string fname = filename;
			ErrorCode caec = neuropix.neuropix_startRecording(fname);
			std::cout << "Recording to file: " << filename << std::endl;
		}

		// // setNeuralStart() doesn't work yet!
		//else
		//{
		//	ConfigAccessErrorCode caec = neuropix.neuropix_setNeuralStart();
		//}
		//if (caec != CONFIG_SUCCESS)
		//{
		//	std::cout << "start failed with error code " << caec << std::endl;
		//	return false;
		//}
	}

	counter = 0;
	timestamp = 0;
	eventCode = 0;
	maxCounter = 0;
	  
	startTimer(500);
	//startThread();

	return true;
}

void NeuropixThread::timerCallback()
{

	stopTimer();

	// start data stream
	DigitalControlErrorCode err5 = neuropix.neuropix_nrst(true);
	std::cout << "nrst 2 error code: " << err5 << std::endl;

	startThread();

}


/** Stops data transfer.*/
bool NeuropixThread::stopAcquisition()
{

	if (isThreadRunning())
	{
		signalThreadShouldExit();
	}

	if (recordToNpx)
		neuropix.neuropix_stopRecording();

	// stop data stream
	DigitalControlErrorCode err3 = neuropix.neuropix_nrst(false);
	std::cout << "nrst 1 error code: " << err3 << std::endl;

	// clear the buffer
	ErrorCode err4 = neuropix.neuropix_resetDatapath();
	std::cout << "reset datapath error code: " << err4 << std::endl;

	return true;
}

void NeuropixThread::updateChannels()
{
	if (sendLfp)
	{
		for (int i = getNumHeadstageOutputs() / 2; i < getNumHeadstageOutputs(); i++)
		{
			//Channel* ch = new Channel(this, i + 1, HEADSTAGE_CHANNEL);
			//ch->setProcessor(this);
			sn->channels[i]->sampleRate = 2500.0;
			sn->channels[i]->sourceNodeId = 99;
			sn->channels[i]->nodeId = 99;
		}
	}
}

void NeuropixThread::toggleApData(bool state)
{
	 sendAp = state;
}

void NeuropixThread::toggleLfpData(bool state)
{
	 sendLfp = state;
}

/** Returns the number of continuous headstage channels the data source can provide.*/
int NeuropixThread::getNumHeadstageOutputs()
{
	int totalChans = 0;

	for (int i = 0; i < outputOn.size(); i++)
	{
		if (outputOn[i])
			totalChans++;
	}

	dataBuffer->resize(totalChans, 10000);
	dataBuffer2->resize(totalChans, 10000);

	if (sendAp && sendLfp)
		totalChans *= 2; // account for LFP channels

	return totalChans; 
}

/** Returns the number of continuous aux channels the data source can provide.*/
int NeuropixThread::getNumAuxOutputs()
{
	return 0;
}

/** Returns the number of continuous ADC channels the data source can provide.*/
int NeuropixThread::getNumAdcOutputs()
{
	return 0;
}

/** Returns the sample rate of the data source.*/
float NeuropixThread::getSampleRate()
{
	return 30000.;
}

/** Returns the volts per bit of the data source.*/
float NeuropixThread::getBitVolts(Channel* chan)
{
	return 0.195f;
}

/** Returns the number of event channels of the data source.*/
int NeuropixThread::getNumEventChannels()
{
	return 16;
}

void NeuropixThread::selectElectrode(int chNum, int connection, bool transmit)
{

	if (!refs.contains(chNum+1))
		neuropix.neuropix_selectElectrode(chNum, connection, transmit);
	else
		neuropix.neuropix_selectElectrode(chNum, 0xFF, transmit);

	//std::cout << "Connecting input " << chNum << " to channel " << connection << "; error code = " << scec << std::endl;

}

void NeuropixThread::setReference(int chNum, int refSetting)
{

	BaseConfigErrorCode bcec = neuropix.neuropix_setReference(chNum, refSetting);

	std::cout << "Set channel " << chNum << " reference to " << refSetting << "; error code = " << bcec << std::endl;
}

void NeuropixThread::setAllReferences(int refChan, int bankForReference)
{
	
	// Option 1-3, numRefs = 10
	// Option 4, numRefs = 7
	int refSetting = refs.indexOf(refChan);

	if (true)
	{
		if (option >= 2) // ensure unused references are disconnected:
		{

			int i; 

			for (i = 0; i < numRefs - 1; i++)
			{
				if (i == refSetting)
				{
					if (i == 0)
						neuropix.neuropix_setExtRef(true, false);
					else
						neuropix.neuropix_selectElectrode(refChan-1, bankForReference, false);
				}

				else
				{
					if (i == 0)
						neuropix.neuropix_setExtRef(false, false);
					else
						neuropix.neuropix_selectElectrode(refs[i]-1, 0xFF, false);
				}

			}

			i = numRefs - 1;

			if (i == refSetting)
				neuropix.neuropix_selectElectrode(refs[i]-1, bankForReference, true);
			else
				neuropix.neuropix_selectElectrode(refs[i]-1, 0xFF, true);
		}
	}
	
	// update reference settings for probe:
	BaseConfigErrorCode bcec = neuropix.neuropix_writeAllReferences((unsigned char)refSetting);

	std::cout << "Set all references to " << refSetting << "; error code = " << bcec << std::endl;
}

void NeuropixThread::setGain(int chNum, int apGain, int lfpGain)
{
	BaseConfigErrorCode bcec = neuropix.neuropix_setGain(chNum, apGain, lfpGain);

	std::cout << "Set channel " << chNum << " gain to " << apGain << " and " << lfpGain << "; error code = " << bcec << std::endl;
	apGains.set(chNum, apGain);
	lfpGains.set(chNum, lfpGain);
}

void NeuropixThread::setAllApGains(int apGain)
{
	BaseConfigErrorCode bcec = neuropix.neuropix_writeAllAPGains(apGain);

	std::cout << "Set all AP gains to " << apGain << "; error code = " << bcec << std::endl;

	for (int i = 0; i < 384; i++)
		apGains.set(i, apGain);
}

void NeuropixThread::setAllLfpGains(int lfpGain)
{
	BaseConfigErrorCode bcec = neuropix.neuropix_writeAllLFPGains(lfpGain);

	std::cout << "Set all LFP gains to " << lfpGain << "; error code = " << bcec << std::endl;

	for (int i = 0; i < 384; i++)
		lfpGains.set(i, lfpGain);
}


void NeuropixThread::setFilter(int filter)
{
	BaseConfigErrorCode bcec = neuropix.neuropix_setFilter(filter);

	std::cout << "Set filter to " << filter << "; error code = " << bcec << std::endl;
}

void NeuropixThread::setTriggerMode(bool trigger)
{
	ConfigAccessErrorCode caec = neuropix.neuropix_triggerMode(trigger);
	
	internalTrigger = trigger;
}

void NeuropixThread::setRecordMode(bool record)
{
	recordToNpx = record;
}


void NeuropixThread::calibrateProbe()
{

	neuropix.neuropix_readADCCalibration();
	neuropix.neuropix_readGainCorrection();

}

bool NeuropixThread::updateBuffer()
{

	ElectrodePacket packet;

	ReadErrorCode rec = neuropix.neuropix_readElectrodeData(packet);

	if (rec == READ_SUCCESS)
	{
		float data[384];
		float data2[384];

		//if (counter <= 0)
		//{
		//	std::cout << packet.synchronization[0] << ", ";
		//	std::cout << neuropix.neuropix_fifoFilling() << std::endl;
		//	counter = 5000;
		//}

		//if (packet.ctrs[0][0] > maxCounter)
		//	maxCounter = packet.ctrs[0][0];
		
		//counter--;

		for (int i = 0; i < 12; i++)
		{
			eventCode = (uint64) packet.synchronization[i]; // currently returning 65535

			for (int j = 0; j < 384; j++)
			{
				data[j] = (packet.apData[i][j] - 0.6) / gains[apGains[j]] * -1000000.0f; // convert to microvolts

				if (i == 0 && sendLfp)
					data2[j] = (packet.lfpData[j] - 0.6) / gains[lfpGains[j]] * -1000000.0f; // convert to microvolts
			}

			dataBuffer->addToBuffer(data, &timestamp, &eventCode, 1);
			timestamp += 1;
		}

		if (sendLfp)
			dataBuffer2->addToBuffer(data2, &timestamp, &eventCode, 1);

		//std::cout << "READ SUCCESS!" << std::endl;	
		
	}
	else {
		if (rec == NO_DATA_LINK)
		{
			//std::cout << "NO DATA LINK" << std::endl;
		}
		else if (rec == WRONG_DATA_MODE)
		{
			//std::cout << "WRONG DATA MODE" << std::endl;
		}
		else if (rec == DATA_BUFFER_EMPTY)
		{
			//std::cout << "DATA BUFFER EMPTY" << std::endl;
		}
		else if (rec == DATA_ERROR)
		{
			//std::cout << "DATA ERROR" << std::endl;
		}
		else {
			//std::cout << "ERROR CODE: " << rec << std::endl;
		}
	}
	 
	return true;
}