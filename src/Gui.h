// note: this minimal gui set is not meant to be reusable beyond the scope of cinder's audio tests.

#pragma once

#include "cinder/gl/gl.h"
#include <boost/format.hpp>

#include <vector>

#define FONT_SIZE 22

using namespace ci;

struct TestWidget {
	TestWidget() : hidden( false ), textIsCentered( true ) {}

	virtual void draw() {}

	Rectf bounds;
	ColorA backgroundColor;
	Font font;
	bool hidden, textIsCentered;
};

void drawWidgets( const std::vector<TestWidget *> &widgets ) {
	for( auto w : widgets )
		w->draw();
}

struct Button : public TestWidget {
	Button( bool isToggle = false, const std::string& titleNormal = "", const std::string& titleEnabled = "" )
	: TestWidget(), isToggle( isToggle ), titleNormal( titleNormal ), titleEnabled( titleEnabled )
	{
		setEnabled( false );
		textColor = Color::white();
	}

	void setEnabled( bool b ) {
		if( b ) {
			backgroundColor = Color( 0.0f, 0.0f, 0.7f );
		} else {
			backgroundColor = Color( 0.3f, 0.3f, 0.3f );
		}
		enabled = b;
	}

	bool hitTest( const Vec2i &pos ) {
		if( hidden )
			return false;

		bool b = bounds.contains( pos );
		if( b )
			setEnabled( ! enabled );
		return b;
	}

	void draw() {
		if( hidden )
			return;
		if( ! font )
			font = Font( Font::getDefault().getName(), FONT_SIZE );

		gl::color( backgroundColor );
		gl::drawSolidRoundedRect( bounds, 4 );

		std::string& title = enabled ? titleEnabled : titleNormal;

		if( textIsCentered )
			gl::drawStringCentered( title, bounds.getCenter(), textColor, font );
		else
			gl::drawString( title, Vec2f( bounds.x1 + 10.0f, bounds.getCenter().y ), textColor, font );
	}

	ColorA textColor;
	std::string titleNormal, titleEnabled;
	bool enabled, isToggle;
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
		if( ! font )
			font = Font( Font::getDefault().getName(), FONT_SIZE );

		gl::color( backgroundColor );
		gl::drawSolidRect( bounds );

		auto valFormatted = boost::format( "%0.3f" ) % valueScaled;

		std::string str = title + ": " + valFormatted.str();
		gl::drawString( str, Vec2f( bounds.x1 + 10.0f, bounds.getCenter().y ), textColor, font );

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

		if( ! font )
			font = Font( Font::getDefault().getName(), FONT_SIZE );
		
		gl::color( backgroundColor );
		gl::drawSolidRect( bounds );

		float sectionHeight = bounds.getHeight() / segments.size();
		Rectf section( bounds.x1, bounds.y1, bounds.x2, bounds.y1 + sectionHeight );
		gl::color( unselectedColor );
		for( size_t i = 0; i < segments.size(); i++ ) {
			if( i != currentSectionIndex ) {
				gl::drawStrokedRect( section );
				if( textIsCentered )
					gl::drawStringCentered( segments[i], section.getCenter(), unselectedColor, font );
				else
					gl::drawString( segments[i], Vec2f( section.x1 + 10.0f, section.getCenter().y ), unselectedColor, font );
			}
			section += Vec2f( 0.0f, sectionHeight );
		}

		gl::color( selectedColor );

		section.y1 = currentSectionIndex * sectionHeight;
		section.y2 = section.y1 + sectionHeight;
		gl::drawStrokedRect( section );
//		gl::drawStringCentered( segments[currentSectionIndex], section.getCenter(), selectedColor, font );
		if( textIsCentered )
			gl::drawStringCentered( segments[currentSectionIndex], section.getCenter(), selectedColor, font );
		else
			gl::drawString( segments[currentSectionIndex], Vec2f( section.x1 + 10.0f, section.getCenter().y ), selectedColor, font );
	}

	std::vector<std::string> segments;
	ColorA selectedColor, unselectedColor;
	size_t currentSectionIndex;
};
