#pragma once

#include "audio2/Device.h"

//#include <CoreAudio/CoreAudioTypes.h>
#include <CoreAudio/CoreAudio.h>
#include <map>

// Need a piece of data that can be shared among OutputDevice and Input device, so we can tell
// if the underlining AudioComponentInstance is being using for simultaneous I/O
// ???: is this too fragmented? Move to Device, or DeviceAudioUnitImpl?
//struct DeviceInfo {
//	DeviceInfo() : mDeviceID( kAudioObjectUnknown ), mNumInputChannels( 0 ), mNumOutputChannels( 0 ), mInputConnected( false ), mOutputConnected( false )
//	{}
//
//	::AudioDeviceID mDeviceID;
//	size_t mNumInputChannels, mNumOutputChannels;
//	bool mInputConnected, mOutputConnected;
//};

namespace audio2 {

	// ???: alt name: DeviceManagerCocoa
	class DeviceManagerCoreAudio : public DeviceManager {
		OutputDeviceRef getDefaultOutput() override;
		InputDeviceRef getDefaultInput() override;

		// TODO NEXT: try instead using a vector<pair<DeviceRef, AudioDeviceID> > here - that will be easier to traverse
		// or map<string, pair> ? - this way Device has an opaque handle to get out data, other than shared_from_this()
//		typedef std::map<std::string, std::shared_ptr<DeviceInfo> > DeviceInfoMap;
		typedef std::map<std::string, std::pair<DeviceRef, AudioDeviceID> > DeviceMap;

	private:

		static std::string keyForDeviceID( AudioDeviceID deviceID );

		DeviceRef getDevice( const std::string &key );

		DeviceMap& getDevices();
		DeviceMap mDevices;

	};
	
} // audio2