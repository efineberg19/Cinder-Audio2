// note: this minimal gui set is not meant to be reusable beyond the scope of cinder's audio tests.

#pragma once

#include "cinder/gl/gl.h"
//#include "cinder/app/App.h"
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
	TestWidget() : hidden( false ), padding( 10.0f ) {}

	virtual void draw() {}

	Rectf bounds;
	ColorA backgroundColor;

	gl::TextureFontRef texFont;
	bool hidden;
	float padding;
};

inline void drawWidgets( const std::vector<TestWidget *> &widgets ) {
	for( auto w : widgets )
		w->draw();
}

struct Button : public TestWidget {
	Button( bool isToggle = false, const std::string& titleNormal = "", const std::string& titleEnabled = "" )
	: TestWidget(), isToggle( isToggle ), titleNormal( titleNormal ), titleEnabled( titleEnabled )
	{
		textColor = Color::white();
		normalColor = Color( 0.3f, 0.3f, 0.3f );
		enabledColor = Color( 0.0f, 0.0f, 0.7f );
		setEnabled( false );
		timeout = 30;
		fadeFrames = 0;
	}

	void setEnabled( bool b ) {
		if( b ) {
			backgroundColor = enabledColor;
		} else {
			backgroundColor = normalColor;
		}
		enabled = b;
	}

	bool hitTest( const Vec2i &pos ) {
		if( hidden )
			return false;

		bool b = bounds.contains( pos );
		if( b ) {
			if( isToggle )
				setEnabled( ! enabled );
			else {
				setEnabled( true );
				fadeFrames = timeout;
			}
		}

		return b;
	}

	void draw() {
		if( hidden )
			return;
		if( ! texFont )
			texFont = getTestWidgetTexFont();

		if( isToggle || ! fadeFrames )
			gl::color( backgroundColor );
		else {
			fadeFrames--;
			setEnabled( false );
			gl::color( lerp( normalColor, enabledColor, (float)fadeFrames / (float)timeout ) );
		}

		gl::drawSolidRoundedRect( bounds, 4 );

		std::string& title = enabled ? titleEnabled : titleNormal;

		gl::color( textColor );
		texFont->drawString( title, Vec2f( bounds.x1 + padding, bounds.getCenter().y + texFont->getFont().getDescent() ) );
	}

	ColorA textColor;
	std::string titleNormal, titleEnabled;
	ColorA normalColor, enabledColor;
	bool enabled, isToggle;
	size_t timeout, fadeFrames;
};

struct HSlider : public TestWidget {
	HSlider() : TestWidget() {
		value = valueScaled = 0.0f;
		min = 0.0f;
		max = 1.0f;
		backgroundColor = ColorA( 0.0f, 1.0f , 0.0f, 0.3f );
		valueColor = ColorA( 0.0f, 1.0f , 0.0f, 0.95f );
		textColor = Color::white();
	}

	void set( float val ) {
		valueScaled = val;
		value = ( valueScaled - min ) / ( max - min );
	}

	bool hitTest( const Vec2i &pos ) {
		if( hidden )
			return false;

		bool b = bounds.contains( pos );
		if( b ) {
			value = ( pos.x - bounds.x1 ) / bounds.getWidth();
			valueScaled = (max - min) * value + min;
		}
		return b;
	}

	void draw() {
		if( hidden )
			return;
		if( ! texFont )
			texFont = getTestWidgetTexFont();

		gl::color( backgroundColor );
		gl::drawSolidRect( bounds );

		auto valFormatted = boost::format( "%0.3f" ) % valueScaled;

		std::string str = title + ": " + valFormatted.str();
		gl::color( textColor );
		texFont->drawString( str, Vec2f( bounds.x1 + padding, bounds.getCenter().y + texFont->getFont().getDescent() ) );

		gl::color( valueColor );
		gl::drawStrokedRect( bounds );

		float offset = bounds.x1 + bounds.getWidth() * value;
		float w = 2.0f;
		Rectf valRect( offset - w, bounds.y1, offset + w, bounds.y2 );
		gl::drawSolidRoundedRect( valRect, w );

	}

	float value, valueScaled, min, max;
	ColorA textColor, valueColor;
	std::string title;
};

struct VSelector : public TestWidget {
	VSelector() : TestWidget() {
		currentSectionIndex = 0;
		backgroundColor = ColorA( 0.0f, 0.0f , 1.0f, 0.3f );
		selectedColor = ColorA( 0.0f, 1.0f , 0.0f, 0.95f );
		unselectedColor = ColorA::gray( 0.5 );
	}

	bool hitTest( const Vec2i &pos ) {
		if( hidden )
			return false;

		bool b = bounds.contains( pos );
		if( b ) {
			int offset = pos.y - bounds.y1;
			int sectionHeight = bounds.getHeight() / segments.size();
			currentSectionIndex = offset / sectionHeight;
		}
		return b;
	}

	const std::string& currentSection() const	{ return segments[currentSectionIndex]; }

	void draw() {
		if( hidden )
			return;

		if( ! texFont )
			texFont = getTestWidgetTexFont();

		gl::color( backgroundColor );
		gl::drawSolidRect( bounds );

		float sectionHeight = bounds.getHeight() / segments.size();
		Rectf section( bounds.x1, bounds.y1, bounds.x2, bounds.y1 + sectionHeight );
		gl::color( unselectedColor );
		for( size_t i = 0; i < segments.size(); i++ ) {
			if( i != currentSectionIndex ) {
				gl::drawStrokedRect( section );
				gl::color( unselectedColor );
				texFont->drawString( segments[i], Vec2f( section.x1 + padding, section.getCenter().y + texFont->getFont().getDescent() ) );
			}
			section += Vec2f( 0.0f, sectionHeight );
		}

		gl::color( selectedColor );

		section.y1 = currentSectionIndex * sectionHeight;
		section.y2 = section.y1 + sectionHeight;
		gl::drawStrokedRect( section );

		gl::color( selectedColor );
		texFont->drawString( segments[currentSectionIndex], Vec2f( section.x1 + padding, section.getCenter().y + texFont->getFont().getDescent() ) );
	}

	std::vector<std::string> segments;
	ColorA selectedColor, unselectedColor;
	size_t currentSectionIndex;
};
