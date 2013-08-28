/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

// note: this minimal gui set is not meant to be reusable beyond the scope of cinder's audio tests.

#pragma once

#include "cinder/gl/gl.h"
#include "cinder/gl/TextureFont.h"
#include <boost/format.hpp>

#include <vector>

using namespace ci;

static gl::TextureFontRef getTestWidgetTexFont() {
	static gl::TextureFontRef sTestWidgetTexFont;
	if( ! sTestWidgetTexFont )
		sTestWidgetTexFont = gl::TextureFont::create( Font( Font::getDefault().getName(), 22 ) );
	return sTestWidgetTexFont;
}

struct TestWidget {
	TestWidget() : mHidden( false ), mPadding( 10.0f ) {}

	virtual void draw() {}

	Rectf mBounds;
	ColorA mBackgroundColor;

	gl::TextureFontRef mTexFont;
	bool mHidden;
	float mPadding;
};

inline void drawWidgets( const std::vector<TestWidget *> &widgets ) {
	for( auto w : widgets )
		w->draw();
}

struct Button : public TestWidget {
	Button( bool isToggle = false, const std::string& titleNormal = "", const std::string& titleEnabled = "" )
	: TestWidget(), mIsToggle( isToggle ), mTitleNormal( titleNormal ), mTitleEnabled( titleEnabled )
	{
		mTextColor = Color::white();
		mNormalColor = Color( 0.3f, 0.3f, 0.3f );
		mEnabledColor = Color( 0.0f, 0.0f, 0.7f );
		setEnabled( false );
		mTimeout = 30;
		mFadeFrames = 0;
	}

	void setEnabled( bool b ) {
		if( b ) {
			mBackgroundColor = mEnabledColor;
		} else {
			mBackgroundColor = mNormalColor;
		}
		mEnabled = b;
	}

	bool hitTest( const Vec2i &pos ) {
		if( mHidden )
			return false;

		bool b = mBounds.contains( pos );
		if( b ) {
			if( mIsToggle )
				setEnabled( ! mEnabled );
			else {
				setEnabled( true );
				mFadeFrames = mTimeout;
			}
		}

		return b;
	}

	void draw() {
		if( mHidden )
			return;
		if( ! mTexFont )
			mTexFont = getTestWidgetTexFont();

		if( mIsToggle || ! mFadeFrames )
			gl::color( mBackgroundColor );
		else {
			mFadeFrames--;
			setEnabled( false );
			gl::color( lerp( mNormalColor, mEnabledColor, (float)mFadeFrames / (float)mTimeout ) );
		}

		gl::drawSolidRoundedRect( mBounds, 4 );

		std::string& title = mEnabled ? mTitleEnabled : mTitleNormal;

		gl::color( mTextColor );
		mTexFont->drawString( title, Vec2f( mBounds.x1 + mPadding, mBounds.getCenter().y + mTexFont->getFont().getDescent() ) );
	}

	ColorA mTextColor;
	std::string mTitleNormal, mTitleEnabled;
	ColorA mNormalColor, mEnabledColor;
	bool mEnabled, mIsToggle;
	size_t mTimeout, mFadeFrames;
};

struct HSlider : public TestWidget {
	HSlider() : TestWidget() {
		mValue = mValueScaled = 0.0f;
		mMin = 0.0f;
		mMax = 1.0f;
		mBackgroundColor = ColorA( 0.0f, 1.0f , 0.0f, 0.3f );
		mValueColor = ColorA( 0.0f, 1.0f , 0.0f, 0.95f );
		mTextColor = Color::white();
	}

	void set( float val ) {
		mValueScaled = val;
		mValue = ( mValueScaled - mMin ) / ( mMax - mMin );
	}

	bool hitTest( const Vec2i &pos ) {
		if( mHidden )
			return false;

		bool b = mBounds.contains( pos );
		if( b ) {
			mValue = ( pos.x - mBounds.x1 ) / mBounds.getWidth();
			mValueScaled = (mMax - mMin) * mValue + mMin;
		}
		return b;
	}

	void draw() {
		if( mHidden )
			return;
		if( ! mTexFont )
			mTexFont = getTestWidgetTexFont();

		gl::color( mBackgroundColor );
		gl::drawSolidRect( mBounds );

		auto valFormatted = boost::format( "%0.3f" ) % mValueScaled;

		std::string str = mTitle + ": " + valFormatted.str();
		gl::color( mTextColor );
		mTexFont->drawString( str, Vec2f( mBounds.x1 + mPadding, mBounds.getCenter().y + mTexFont->getFont().getDescent() ) );

		gl::color( mValueColor );
		gl::drawStrokedRect( mBounds );

		float offset = mBounds.x1 + mBounds.getWidth() * mValue;
		float w = 2.0f;
		Rectf valRect( offset - w, mBounds.y1, offset + w, mBounds.y2 );
		gl::drawSolidRoundedRect( valRect, w );

	}

	float mValue, mValueScaled, mMin, mMax;
	ColorA mTextColor, mValueColor;
	std::string mTitle;
};

struct VSelector : public TestWidget {
	VSelector() : TestWidget() {
		mCurrentSectionIndex = 0;
		mBackgroundColor = ColorA( 0.0f, 0.0f , 1.0f, 0.3f );
		mSelectedColor = ColorA( 0.0f, 1.0f , 0.0f, 0.95f );
		mUnselectedColor = ColorA::gray( 0.5 );
	}

	bool hitTest( const Vec2i &pos ) {
		if( mHidden )
			return false;

		bool b = mBounds.contains( pos );
		if( b ) {
			int offset = pos.y - (int)mBounds.y1;
			int sectionHeight = (int)mBounds.getHeight() / mSegments.size();
			mCurrentSectionIndex = std::min<size_t>( offset / sectionHeight, mSegments.size() - 1 );
		}
		return b;
	}

	const std::string& currentSection() const	{ return mSegments[mCurrentSectionIndex]; }

	void draw() {
		if( mHidden )
			return;

		if( ! mTexFont )
			mTexFont = getTestWidgetTexFont();

		gl::color( mBackgroundColor );
		gl::drawSolidRect( mBounds );

		float sectionHeight = mBounds.getHeight() / mSegments.size();
		Rectf section( mBounds.x1, mBounds.y1, mBounds.x2, mBounds.y1 + sectionHeight );
		gl::color( mUnselectedColor );
		for( size_t i = 0; i < mSegments.size(); i++ ) {
			if( i != mCurrentSectionIndex ) {
				gl::drawStrokedRect( section );
				gl::color( mUnselectedColor );
				mTexFont->drawString( mSegments[i], Vec2f( section.x1 + mPadding, section.getCenter().y + mTexFont->getFont().getDescent() ) );
			}
			section += Vec2f( 0.0f, sectionHeight );
		}

		gl::color( mSelectedColor );

		section.y1 = mCurrentSectionIndex * sectionHeight;
		section.y2 = section.y1 + sectionHeight;
		gl::drawStrokedRect( section );

		gl::color( mSelectedColor );
		mTexFont->drawString( mSegments[mCurrentSectionIndex], Vec2f( section.x1 + mPadding, section.getCenter().y + mTexFont->getFont().getDescent() ) );
	}

	std::vector<std::string> mSegments;
	ColorA mSelectedColor, mUnselectedColor;
	size_t mCurrentSectionIndex;
};
