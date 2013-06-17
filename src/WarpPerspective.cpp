/*
 Copyright (c) 2010-2013, Paul Houx - All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

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

#include "WarpPerspective.h"

#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"

using namespace ci;
using namespace ci::app;

namespace ph { namespace warping {

WarpPerspective::WarpPerspective(void)
	:Warp(PERSPECTIVE)
{
	//
	mControlsX = 2;
	mControlsY = 2;

	mTransform.setToIdentity();

	//
	mSource[0].x = 0.0f;		
	mSource[0].y = 0.0f;		
	mSource[1].x = (float)mWidth;		
	mSource[1].y = 0.0f;		
	mSource[2].x = (float)mWidth;		
	mSource[2].y = (float)mHeight;		
	mSource[3].x = 0.0f;		
	mSource[3].y =(float) mHeight;		

	//
	reset();
}

WarpPerspective::~WarpPerspective(void)
{
}

Matrix44f WarpPerspective::getTransform() 
{
	// calculate warp matrix using OpenCV
	if(mIsDirty) {
		// update source size
		mSource[1].x = (float)mWidth;
		mSource[2].x = (float)mWidth;
		mSource[2].y = (float)mHeight;
		mSource[3].y = (float)mHeight;

		// convert corners to actual destination pixels
		mDestination[0].x = mPoints[0].x * mWindowSize.x;
		mDestination[0].y = mPoints[0].y * mWindowSize.y;
		mDestination[1].x = mPoints[1].x * mWindowSize.x;
		mDestination[1].y = mPoints[1].y * mWindowSize.y;
		mDestination[2].x = mPoints[2].x * mWindowSize.x;
		mDestination[2].y = mPoints[2].y * mWindowSize.y;
		mDestination[3].x = mPoints[3].x * mWindowSize.x;
		mDestination[3].y = mPoints[3].y * mWindowSize.y;

		// calculate warp matrix
		cv::Mat	warp = cv::getPerspectiveTransform( mSource, mDestination );

		// convert to OpenGL matrix
		mTransform[0]	= warp.ptr<double>(0)[0]; 
        mTransform[4]	= warp.ptr<double>(0)[1]; 
        mTransform[12]	= warp.ptr<double>(0)[2]; 

        mTransform[1]	= warp.ptr<double>(1)[0]; 
        mTransform[5]	= warp.ptr<double>(1)[1]; 
        mTransform[13]	= warp.ptr<double>(1)[2]; 

        mTransform[3]	= warp.ptr<double>(2)[0]; 
        mTransform[7]	= warp.ptr<double>(2)[1]; 
        mTransform[15]	= warp.ptr<double>(2)[2]; 

		// update the inverted matrix
		getInvertedTransform();

		mIsDirty = false;
	}

	return mTransform;
}

Matrix44f	WarpPerspective::getInvertedTransform()
{
	if(mIsDirty) {
		// the following line can only be used with Cinder v0.8.3 or higher,
		//   because earlier versions contain a nasty bug
		mInverted = mTransform.inverted(0.0f);
	}

	return mInverted;
}

void WarpPerspective::reset()
{
	mPoints.clear();
	mPoints.push_back( Vec2f(0.0f, 0.0f) );
	mPoints.push_back( Vec2f(1.0f, 0.0f) );
	mPoints.push_back( Vec2f(1.0f, 1.0f) );
	mPoints.push_back( Vec2f(0.0f, 1.0f) );

	mIsDirty = true;
}

void WarpPerspective::draw(const gl::Texture &texture, const Area &srcArea, const Rectf &destRect)
{
	// TODO: clip against bounds

	gl::pushModelView();
	gl::multModelView( getTransform() );

	gl::draw( texture, srcArea, destRect );

	gl::popModelView();	

	// draw interface
	draw();
}

void WarpPerspective::draw(bool controls)
{
	// only draw grid while editing
	if( isEditModeEnabled() ) {
		// save current drawing color and line width
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT); 

		gl::pushModelView();
		gl::multModelView( getTransform() );

		glLineWidth(1.0f);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glDisable( GL_TEXTURE_2D );

		gl::color( Color::white() );
		for(int i=0;i<=1;i++) {
			float s = i / 1.0f;
			gl::drawLine( Vec2f(s * (float)mWidth, 0.0f), Vec2f(s * (float)mWidth, (float)mHeight) );
			gl::drawLine( Vec2f(0.0f, s * (float)mHeight), Vec2f((float)mWidth, s * (float)mHeight) );
		}

		gl::drawLine( Vec2f(0.0f, 0.0f), Vec2f((float)mWidth, (float)mHeight) );
		gl::drawLine( Vec2f((float)mWidth, 0.0f), Vec2f(0.0f, (float)mHeight) );
		
		gl::popModelView();

		if(controls) {
			// draw control points		
			for(int i=0;i<4;i++) 
				drawControlPoint( toVec2f( mDestination[i] ), i == mSelected );
		}

		// restore drawing color and line width
		glPopAttrib();
	}
}

bool WarpPerspective::keyDown( KeyEvent event )
{
	// let base class handle keys first
	if( Warp::keyDown( event ) )
		return true;

	// disable keyboard input when not in edit mode
	if( ! isEditModeEnabled() ) return false;

	// do not listen to key input if not selected
	if(mSelected < 0 || mSelected >= mPoints.size()) return false;

	switch( event.getCode() ) {
		case KeyEvent::KEY_F9:
			// rotate content ccw
			std::swap(mPoints[1], mPoints[2]);
			std::swap(mPoints[0], mPoints[1]);
			std::swap(mPoints[3], mPoints[0]);
			mSelected = (mSelected + 1) % 4;
			mIsDirty = true;
			return true;
		case KeyEvent::KEY_F10:
			// rotate content cw
			std::swap(mPoints[3], mPoints[0]);
			std::swap(mPoints[0], mPoints[1]);
			std::swap(mPoints[1], mPoints[2]);
			mSelected = (mSelected + 3) % 4;
			mIsDirty = true;
			return true;
		case KeyEvent::KEY_F11:
			// flip content horizontally
			std::swap(mPoints[0], mPoints[1]);
			std::swap(mPoints[2], mPoints[3]);
			if( mSelected % 2) mSelected--;
			else mSelected++;
			mIsDirty = true;
			return true;
		case KeyEvent::KEY_F12:
			// flip content vertically
			std::swap(mPoints[0], mPoints[3]);
			std::swap(mPoints[1], mPoints[2]);
			mSelected = (mPoints.size()-1) - mSelected;
			mIsDirty = true;
			return true;
		default:
			return false;
			break;
	}

	return true;
}

} } // namespace ph::warping