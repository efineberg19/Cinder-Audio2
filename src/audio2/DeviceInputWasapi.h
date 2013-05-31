#pragma once

#if ( _WIN32_WINNT < 0x600 )
#error "WASAPI unsupported for deployment target less than 0x600 (Windows Vista)"
#endif

#include "audio2/Device.h"
#include "audio2/GeneratorNode.h"

namespace audio2 {

class DeviceInputWasapi : public Device {
public:

	virtual ~DeviceInputWasapi();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

private:
	DeviceInputWasapi( const std::string &key );

	friend class DeviceManagerMsw;
};


class InputWasapi : public InputNode {
  public:
	InputWasapi( DeviceRef device );
	virtual ~InputWasapi();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	DeviceRef getDevice() override;

	void render( BufferT *buffer ) override;

  private:

	struct Impl;
	std::unique_ptr<Impl> mImpl;
	std::shared_ptr<DeviceInputWasapi> mDevice;
	ChannelT mInterleavedBuffer;

	size_t mCaptureBlockSize; // per channel. TODO: this should be user settable
};

} // namespace audio2
