#pragma once

#include "audio2/Device.h"

#include <CoreAudio/CoreAudio.h>
#include <vector>

namespace audio2 {

	// ???: alt name: DeviceManagerCocoa
	class DeviceManagerCoreAudio : public DeviceManager {

		DeviceRef getDefaultOutput() override;
		DeviceRef getDefaultInput() override;

		void setActiveDevice( const std::string &key ) override;

	private:

		static std::string keyForDeviceID( ::AudioDeviceID deviceID );

		struct DeviceInfo {
			std::string			key;
			::AudioDeviceID		deviceID;
			DeviceRef			device;
		};
		typedef std::vector<DeviceInfo> DeviceContainerT;

		DeviceRef getDevice( const std::string &key );
		DeviceContainerT& getDevices();

		DeviceContainerT mDevices;
	};
	
} // audio2