#pragma once

#include "audio2/Device.h"

#include <CoreAudio/CoreAudio.h>
#include <map>

namespace audio2 {

	// ???: alt name: DeviceManagerCocoa
	class DeviceManagerCoreAudio : public DeviceManager {

		DeviceRef getDefaultOutput() override;
		DeviceRef getDefaultInput() override;

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