//
//  AX-TrackDecoder.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include <AX/AX-TrackDecoder.h>

namespace AX
{
	namespace Media
	{
		void ITrackDecoder::DecodedFrame::SetPixelData ( const ci::Surface8uRef & surface )
		{
			_pixelData = surface;
			PixelBufferSize = surface->getRowBytes ( ) * surface->getHeight ( );
			PixelBuffer = surface->getData ( );
		}

		void ITrackDecoder::DecodedFrame::SetPixelData ( const ci::Channel8uRef & channel )
		{
			_pixelData = channel;
			PixelBufferSize = channel->getRowBytes ( ) * channel->getHeight ( );
			PixelBuffer = channel->getData ( );
		}

		void ITrackDecoder::DecodedFrame::SetPixelData ( const RawBufferRef & raw )
		{
			_pixelData = raw;
			PixelBufferSize = raw->size ( );
			PixelBuffer = raw->data ( );
		}
	}
}