#pragma once

#include "audio2/Context.h"
#include "audio2/Dsp.h"

namespace audio2 {

	typedef std::shared_ptr<class EffectNode> EffectNodeRef;

	class EffectNode : public Node {
	public:
		EffectNode( const Format &format = Format() ) : Node( format ) {
			setAutoEnabled();
		}
		virtual ~EffectNode() {}
	};

	struct RingMod : public EffectNode {
		RingMod( const Format &format = Format() ) : EffectNode( format ), mSineGen( 440.0f, 1.0f )	{ mTag = "RingMod"; }

		virtual void initialize() override {
			mSineGen.setSampleRate( getContext()->getSampleRate() );
		}

		virtual void process( Buffer *buffer ) override {
			size_t numFrames = buffer->getNumFrames();
			if( mSineBuffer.size() < numFrames )
				mSineBuffer.resize( numFrames );
			mSineGen.process( &mSineBuffer );

			for ( size_t c = 0; c < buffer->getNumChannels(); c++ ) {
				float *channel = buffer->getChannel( c );
				for( size_t i = 0; i < numFrames; i++ )
					channel[i] *= mSineBuffer[i];
			}
		}

		SineGen mSineGen;
		std::vector<float> mSineBuffer;
	};

} // namespace audio2