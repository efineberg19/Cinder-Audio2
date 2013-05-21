#pragma once

#include "cinder/gl/gl.h"
#include <boost/format.hpp>

#define FONT_SIZE 22

using namespace ci;


struct Button {
	Button( bool isToggle = false, const std::string& titleNormal = "", const std::string& titleEnabled = "" )
	: isToggle( isToggle ), titleNormal( titleNormal ), titleEnabled( titleEnabled )
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
		bool b = bounds.contains( pos );
		if( b )
			setEnabled( ! enabled );
		return b;
	}

	void draw() {
		if( ! font ) {
			font = Font( Font::getDefault().getName(), FONT_SIZE );
		}
		gl::color( backgroundColor );
		gl::drawSolidRoundedRect( bounds, 4 );

		std::string& title = enabled ? titleEnabled : titleNormal;
		gl::drawStringCentered( title, bounds.getCenter(), textColor, font );
	}

	Rectf bounds;
	ColorA backgroundColor, textColor;
	std::string titleNormal, titleEnabled;
	Font font;
	bool enabled, isToggle;
};

struct HSlider {
	HSlider() {
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
		bool b = bounds.contains( pos );
		if( b ) {
			value = ( pos.x - bounds.x1 ) / bounds.getWidth();
			valueScaled = (max - min) * value + min;
		}
		return b;
	}

	void draw() {

		gl::color( backgroundColor );
		gl::drawSolidRect( bounds );

		auto valFormatted = boost::format( "%0.3f" ) % valueScaled;

		std::string str = title + ": " + valFormatted.str();
		if( ! font )
			font = Font( Font::getDefault().getName(), FONT_SIZE );
		gl::drawString( str, Vec2f( bounds.x1 + 10.0f, bounds.getCenter().y ), textColor, font );

		gl::color( valueColor );
		gl::drawStrokedRect( bounds );

		float offset = bounds.x1 + bounds.getWidth() * value;
		float w = 2.0f;
		Rectf valRect( offset - w, bounds.y1, offset + w, bounds.y2 );
		gl::drawSolidRoundedRect( valRect, w );

	}

	float value, valueScaled, min, max;
	Rectf bounds;
	ColorA backgroundColor, textColor, valueColor;
	std::string title;
	Font font;
};