//
//  Decoders.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include "Decoders.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/Timer.h"
#include "hap.h"

using namespace ci;

bool MJPEGDecoder::Decode ( const AX::Sample& sample )
{
    try
    {
        Timer timer{ true };
        auto stream = IStreamMem::create ( sample.Data ( ), sample.Length ( ) );
        auto image = loadImage ( DataSourceBuffer::create ( loadStreamBuffer ( stream ) ), ImageSource::Options ( ), "jpg" );
        auto surface = Surface8u::create ( image );

        _decodedFrames.push_back ( {} );
        
        auto& frame = _decodedFrames.back ( );
        frame.IsCompressed = false;
        frame.GPUFormat = surface->hasAlpha() ? GL_RGBA : GL_RGB;
        frame.Channels = surface->hasAlpha ( ) ? 4 : 3;
        frame.DecodeTime = static_cast<float>( timer.getSeconds ( ) );
        frame.Index = sample.Index ( );
        frame.Width = surface->getWidth ( );
        frame.Height = surface->getHeight ( );
        frame.SetPixelData ( surface );
        
        return true;

    } catch ( const std::exception& e )
    {
        std::printf ( "Error decoding MJPEG frame: %s\n", e.what ( ) );
        return false;
    }
}

gl::TextureRef MJPEGDecoder::CreateTexture ( AX::u32 index, const gl::Texture::Format& fmt ) const
{
    assert ( app::isMainThread ( ) && "GL calls must be invoked on the main thread" );
    auto& decoded = FrameAt ( index );

    auto format = fmt;
    format.label ( AX::FourCCToString ( Handler ( ) ) );
    return gl::Texture::create ( decoded.PixelBuffer, decoded.GPUFormat, decoded.Width, decoded.Height );
}

bool HAPDecoder::Decode ( const AX::Sample& sample )
{
    unsigned long uncompressedLen = ( ( sample.Width() + 3 ) / 4 ) * ( ( sample.Height() + 3 ) / 4 ) * 8;

    auto* sampleData = sample.Data ( );
    auto sampleDataLength = sample.Length ( );

    #define HAP_RETURN_IF_FAILED(x) { auto result = (x); if ( result != HapResult_No_Error ) { return false; } };

    uint32_t textureCount = 0;
    HAP_RETURN_IF_FAILED ( HapGetFrameTextureCount ( sampleData, sampleDataLength, &textureCount ) );
    
    for ( uint32_t index = 0; index < textureCount; index++ )
    {
        Timer timer{ true };

        uint32_t format{ 0 };
        uint32_t status = HapGetFrameTextureFormat ( sampleData, sampleDataLength, index, &format );

        unsigned long actualSize{ 0 };
        auto DecodeCallback = []( HapDecodeWorkFunction function, void* p, uint32_t count, void* )
        {
            std::printf ( "DecodeCallback: %d\n", count );
            for ( uint32_t i = 0; i < count; i++ )
            {
                function ( p, i );
            }
        };

        uint32_t chunks{ 0 };
        HAP_RETURN_IF_FAILED ( HapMaxEncodedLength ( 1, &uncompressedLen, &format, &chunks ) );
        std::printf ( "HapMaxEncodedLength(%d) = %lu\n", status, uncompressedLen );
        uncompressedLen *= 2;

        DecodedFrame frame{};
        frame.Index = index;
        frame.Width = sample.Width ( );
        frame.Height = sample.Height ( );
        frame.IsCompressed = true;

        auto buffer = std::make_shared<std::vector<AX::u8>> ( );
        buffer->resize ( uncompressedLen );
        
        HAP_RETURN_IF_FAILED ( HapDecode ( sampleData, sampleDataLength, index, DecodeCallback, nullptr, buffer->data ( ), uncompressedLen, &actualSize, &format ) );
        buffer->resize ( actualSize );
        assert ( buffer->size ( ) == actualSize );

        std::printf ( "DecodeStatus: %d\n", status );
        if ( status == HapResult_No_Error )
        {
            std::printf ( "HapFormat: %x\n", format );
            GLenum glFormat = 0;
            
            switch ( format )
            {
                case HapTextureFormat_RGB_DXT1:
                {
                    glFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
                    frame.Channels = 3;
                    break;
                }

                case HapTextureFormat_RGBA_DXT5:
                {
                    glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                    frame.Channels = 4;
                    break;
                }

                case HapTextureFormat_A_RGTC1:
                {
                    glFormat = GL_COMPRESSED_RED_RGTC1_EXT;
                    frame.Channels = 1;
                    break;
                }

                case HapTextureFormat_YCoCg_DXT5:
                {
                    glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                    frame.Channels = 4;
                    break;
                }

                case HapTextureFormat_RGBA_BPTC_UNORM:
                {
                    glFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
                    frame.Channels = 4;
                    break;
                }
            }

            if ( glFormat != 0 )
            {
                try
                {
                    frame.GPUFormat = glFormat;
                    frame.SetPixelData ( buffer );
                    frame.DecodeTime = static_cast<float> ( timer.getSeconds ( ) );
                    _decodedFrames.push_back ( frame );
                } catch ( std::exception& e )
                {
                    std::printf ( "Error decoding HAP frame[%d]: %s\n", index, e.what ( ) );
                    return false;
                }
            }
        }
    }

    return true;
}

gl::TextureRef HAPDecoder::CreateTexture ( AX::u32 index, const gl::Texture::Format& fmt ) const
{
	assert ( app::isMainThread ( ) && "GL calls must be invoked on the main thread" );

    auto& decoded = FrameAt ( index );

    GLuint texId{ 0 };
    GLenum target = GL_TEXTURE_2D;

    glGenTextures ( 1, &texId );
    glHint ( GL_TEXTURE_COMPRESSION_HINT, GL_NICEST );

    gl::ScopedTextureBind tex0{ target, texId };
    glCompressedTexImage2D ( target, 0, decoded.GPUFormat, decoded.Width, decoded.Height, 0, static_cast<GLsizei>(decoded.PixelBufferSize), decoded.PixelBuffer );

    auto tex = gl::Texture::create ( target, texId, decoded.Width, decoded.Height, false );
    tex->setMinFilter ( fmt.getMinFilter() );
    tex->setMagFilter ( fmt.getMagFilter() );
    tex->setLabel ( AX::FourCCToString ( Handler() ) );
    CI_CHECK_GL ( );
    std::printf ( "ok!\n" );
    return tex;
    
}