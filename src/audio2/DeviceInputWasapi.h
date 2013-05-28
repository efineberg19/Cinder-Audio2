#pragma once

#include "audio2/Device.h"
#include "audio2/Graph.h"


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


class RingBuffer;

class InputWasapi : public Input {
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
	std::unique_ptr<RingBuffer> mRingBuffer;

};

} // namespace audio2
