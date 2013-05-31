#pragma once

#include "audio2/Graph.h"
#include "audio2/Dsp.h"

namespace audio2 {

	typedef std::shared_ptr<class Effect> EffectRef;

	class Effect : public Node {
	public:
		Effect() : Node() { mSources.resize( 1 ); }
		virtual ~Effect() {}
	};

	struct RingMod : public Effect {
		RingMod()
			: mSineGen( 440.0f, 1.0f )	{
				mTag = "RingMod";
		}

		virtual void initialize() override {
			mSineGen.setSampleRate( mFormat.getSampleRate() );
		}

		virtual void render( BufferT *buffer ) override {
			size_t numSamples = buffer->at( 0 ).size();
			if( mSineBuffer.size() < numSamples )
				mSineBuffer.resize( numSamples );
			mSineGen.render( &mSineBuffer );

			for ( size_t c = 0; c < buffer->size(); c++ ) {
				ChannelT &channel = buffer->at( c );
				for( size_t i = 0; i < channel.size(); i++ )
					channel[i] *= mSineBuffer[i];
			}
		}

		SineGen mSineGen;
		ChannelT mSineBuffer;
	};

} // namespace audio2