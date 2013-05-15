#pragma once

#include "audio2/Device.h"

namespace audio2 {

class DeviceOuputXAudio : public Device {
  public:

	virtual ~DeviceOuputXAudio();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

  private:
	DeviceOuputXAudio( const std::string &key );

};

} // namespace audio2
