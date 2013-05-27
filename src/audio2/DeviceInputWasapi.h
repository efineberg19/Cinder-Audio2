#pragma once

#include "audio2/Graph.h"

namespace audio2 {

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

	//std::shared_ptr<DeviceAudioUnit> mDevice;
	std::unique_ptr<RingBuffer> mRingBuffer;
};

} // namespace audio2
