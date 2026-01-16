//
//  SimpleHAPDecoderApp.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include <AX/AX-MP4.h>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/CinderImGui.h"
#include "cinder/Breakpoint.h"

#include "hap.h"
#include <iostream>
#include "circular/circular.h"

namespace ui = ImGui;

using namespace ci;
using namespace ci::app;

extern "C"
{
    __declspec( dllexport ) unsigned int NvOptimusEnablement = 0x1;
}

#if AX_LIVEPP_ENABLED
#include "LPP_API_x64_CPP.h"

namespace AX
{
    static void InitLivePP ( )
    {
        static struct LivePP
        {
            LivePP ( )
            {
                std::printf ( "Live++ Starting\n" );

                auto path = app::getAssetPath ( "LivePP" );
                _agent = lpp::LppCreateSynchronizedAgent ( nullptr, path.c_str ( ) );
                if ( !lpp::LppIsValidSynchronizedAgent ( &_agent ) )
                {
                    std::printf ( "Error creating Live++ agent\n" );
                    return;
                }

                _agent.EnableModule ( lpp::LppGetCurrentModulePath ( ), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES, nullptr, nullptr );
                _updateConnection = app::App::get ( )->getSignalUpdate ( ).connect ( [=]
                {
                    if ( _agent.WantsReload ( lpp::LPP_RELOAD_OPTION_SYNCHRONIZE_WITH_COMPILATION_AND_RELOAD ) )
                    {
                        std::printf ( "Live++ Reloading\n" );
                        _agent.Reload ( lpp::LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED );
                    }

                    if ( _agent.WantsRestart ( ) )
                    {
                        std::printf ( "Live++ Restarting\n" );
                        _agent.Restart ( lpp::LPP_RESTART_BEHAVIOUR_INSTANT_TERMINATION, 0u, nullptr );
                    }
                } );

                app::App::get ( )->getSignalCleanup ( ).connect ( [=]
                {
                    _updateConnection.disconnect ( );
                    lpp::LppDestroySynchronizedAgent ( &_agent );
                } );
            }

            lpp::LppSynchronizedAgent   _agent{};
            signals::Connection         _updateConnection;

        } kLivePP;
    }
}

#else
namespace AX { static void InitLivePP ( ) {} };
#endif

static const char* kGenericVS = CI_GLSL ( 150,
                                            uniform mat4 ciModelViewProjection;
                                            in vec4 ciPosition;
                                            in vec2 ciTexCoord0;

                                            out vec2 UV;

                                            void main ( )
                                            {
                                                gl_Position = ciModelViewProjection * ciPosition;
                                                UV = ciTexCoord0;
                                            }
                                         );

static const char* kYCoCgFS = CI_GLSL ( 150,
                                                uniform sampler2D uTex0;
                                                
                                                in vec2 UV;
                                                out vec4 FinalColor;

                                                const vec4 kOffsets = vec4 ( -0.50196078431373, -0.50196078431373, 0.0, 0.0 );

                                                void main ( )
                                                {
                                                    vec4 CoCgSY = texture ( uTex0, UV );

                                                    CoCgSY += kOffsets;

                                                    float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;

                                                    float Co = CoCgSY.x / scale;
                                                    float Cg = CoCgSY.y / scale;
                                                    float Y = CoCgSY.w;

                                                    vec4 rgba = vec4 ( Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0 );

                                                    FinalColor = rgba;
                                                }
                                             );

static const char* kYCoCgAlphaFS = CI_GLSL ( 150,
                                                uniform sampler2D uTex0;
                                                uniform sampler2D uTex1;

                                                in vec2 UV;
                                                out vec4 FinalColor;

                                                const vec4 kOffsets = vec4 ( -0.50196078431373, -0.50196078431373, 0.0, 0.0 );

                                                void main ( )
                                                {
                                                    vec4 CoCgSY = texture ( uTex0, UV );
                                                    vec4 theAlpha = texture ( uTex1, UV );

                                                    CoCgSY += kOffsets;

                                                    float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;

                                                    float Co = CoCgSY.x / scale;
                                                    float Cg = CoCgSY.y / scale;
                                                    float Y = CoCgSY.w;

                                                    vec4 rgba = vec4 ( Y + Co - Cg, Y + Cg, Y - Co - Cg, theAlpha.r );

                                                    FinalColor = rgba;
                                                }
                                             );

static gl::GlslProgRef kYCoCgAlphaShader ( )
{
    static gl::GlslProgRef kShader;
    if ( !kShader )
    {
        try
        {
            kShader = gl::GlslProg::create ( gl::GlslProg::Format ( ).vertex ( kGenericVS ).fragment ( kYCoCgAlphaFS ) );
            kShader->uniform ( "uTex0", 0 );
            kShader->uniform ( "uTex1", 1 );
        } catch ( const std::exception& e )
        {
            std::printf ( "Shader: %s\n", e.what ( ) );
        }
    }

    return kShader;
}

static gl::GlslProgRef kYCoCgShader ( )
{
    static gl::GlslProgRef kShader;
    if ( !kShader )
    {
        try
        {
            kShader = gl::GlslProg::create ( gl::GlslProg::Format ( ).vertex ( kGenericVS ).fragment ( kYCoCgFS ) );
            kShader->uniform ( "uTex0", 0 );
        } catch ( const std::exception& e )
        {
            std::printf ( "Shader: %s\n", e.what ( ) );
        }
    }

    return kShader;
}

class SimpleHAPDecoderApp : public App
{
public:
    void            setup    ( ) override;
    void            update   ( ) override;
    void            draw     ( ) override;
    void            fileDrop ( FileDropEvent event ) override;

protected:

    bool            LoadMP4       ( const DataSourceRef& source );
    bool            DecodeFrameAt ( int index );

    int             _currentSample{ 0 };
    gl::TextureRef  _frame;
    
    gl::TextureRef  _YCoCgPlane;
    gl::TextureRef  _alphaPlane;
    
    AX::MP4Ref      _mp4;
    AX::MovieRef    _movie;
    AX::TrackRef    _track{ nullptr };
    float           _playRate{ 0.0f };
    float           _time{ 0 };
    
    circular_buffer<float>  _decodeTimeHistory{ 64 };
};

void SimpleHAPDecoderApp::setup ( )
{
    gl::enableVerticalSync ( false );
    setFrameRate ( 100000 );

    ui::Initialize ( );
    AX::InitLivePP ( );
    
    if ( !app::getAssetPath ( "Videos/Drums_Fill1_BG.mov" ).empty() )
    {
        // Sample HAP videos downloadable from
        // https://docs.vidvox.net/vdmx/vdmx_sample_media.html#momo-the-monster-middlman-pacific-coast
        LoadMP4 ( loadAsset ( "Videos/Drums_Fill1_BG.mov" ) );
        
    } else
    {
        std::printf ( "No demo video found\n" );
    }
}

bool SimpleHAPDecoderApp::LoadMP4 ( const DataSourceRef& source )
{
    try
    {
        _mp4 = AX::MP4::Create ( source, AX::MP4::Format{}.TrackProperties() );
        if ( _mp4 )
        {
            if ( _mp4->IsValid ( ) )
            {
                _movie = AX::Movie::Create ( _mp4 );
                _track = _movie->GetTrack ( AX::TrackType::kVideo, 0 );
                _mp4->Dump ( std::cout, true );

                _currentSample = 0;
                _time = 0.0f;

                DecodeFrameAt ( 0 );
                return true;
            } else
            {
                std::printf ( "Error loading MP4: %s\n", AX::MP4ErrorCodeToString ( _mp4->Error ( ) ) );
            }
        }
    } catch ( const std::exception& e )
    {
        std::printf ( "Error loading MP4: %s\n", e.what ( ) );
    }

    return false;
}

void SimpleHAPDecoderApp::update ( )
{
    float dt = ui::GetIO ( ).DeltaTime;

    if ( _track )
    {
        _time += dt * _playRate;
        if ( _time > _track->DurationSeconds ( ) ) _time = 0;
        if ( _time < 0.0f ) _time = _track->DurationSeconds ( );

        int sample = (int)lmap ( _time, 0.0f, _track->DurationSeconds ( ), 0.0f, static_cast<float> ( _track->SampleCount ( ) - 1 ) );
        if ( sample != _currentSample )
        {
            DecodeFrameAt ( sample );
            _currentSample = sample;
        }
    }
}

void SimpleHAPDecoderApp::fileDrop ( FileDropEvent event )
{
    LoadMP4 ( loadFile ( event.getFile ( 0 ) ) );
}

const uint32_t kHAP1 = 'Hap1';
const uint32_t kHAP5 = 'Hap5';
const uint32_t kHAPY = 'HapY';
const uint32_t kHAPM = 'HapM';
const uint32_t kJPEG = 'jpeg';

inline bool IsHapHandler ( uint32_t type )
{
    switch ( type )
    {
        case kHAP1:
        case kHAP5:
        case kHAPY:
        case kHAPM:
        {
            return true;
        }    
    }

    return false;
}

gl::Texture2dRef HapDecodeFrameAt ( uint32_t index, uint32_t handler, uint32_t width, uint32_t height, const uint8_t* sampleData, unsigned long sampleDataLength )
{
    unsigned long uncompressedLen = ( ( width + 3 ) / 4 ) * ( ( height + 3 ) / 4 ) * 8;
    
    uint32_t format{ 0 };
    uint32_t status = HapGetFrameTextureFormat ( sampleData, sampleDataLength, 0, &format );

    unsigned long actualSize{ 0 };
    static std::vector<uint8_t> decompressed;

    auto DecodeCallback = []( HapDecodeWorkFunction function, void* p, uint32_t count, void* )
    {
        std::printf ( "DecodeCallback: %d\n", count );
        for ( uint32_t i = 0; i < count; i++ )
        {
            function ( p, i );
        }
    };

    uint32_t chunks{ 0 };
    status = HapMaxEncodedLength ( 1, &uncompressedLen, &format, &chunks );
    std::printf ( "HapMaxEncodedLength(%d) = %lu\n", status, uncompressedLen );
    uncompressedLen *= 2;

    if ( decompressed.size ( ) < uncompressedLen )
    {
        std::printf ( "Resizing decompress buffer: %d\n", uncompressedLen );
        decompressed.resize ( uncompressedLen );
    }

    status = HapDecode ( sampleData, sampleDataLength, index, DecodeCallback, nullptr, decompressed.data ( ), uncompressedLen, &actualSize, &format );
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
                break;
            }

            case HapTextureFormat_RGBA_DXT5:
            {
                glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            }

            case HapTextureFormat_A_RGTC1:
            {
                glFormat = GL_COMPRESSED_RED_RGTC1_EXT;
                break;
            }

            case HapTextureFormat_YCoCg_DXT5:
            {
                glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            }
        }

        if ( glFormat != 0 )
        {
            GLuint texId{ 0 };
            GLenum target = GL_TEXTURE_2D;

            glGenTextures ( 1, &texId );
            glHint ( GL_TEXTURE_COMPRESSION_HINT, GL_NICEST );

            gl::ScopedTextureBind tex0{ target, texId };
            glCompressedTexImage2D ( target, 0, glFormat, width, height, 0, actualSize, decompressed.data ( ) );

            auto tex = gl::Texture::create ( target, texId, width, height, false );
            tex->setMinFilter ( GL_LINEAR );
            tex->setMagFilter ( GL_LINEAR );
            tex->setLabel ( AX::FourCCToString ( handler ) );
            CI_CHECK_GL ( );
            std::printf ( "ok!\n" );
            return tex;
        }
    }

    return nullptr;
}

bool SimpleHAPDecoderApp::DecodeFrameAt ( int index )
{
    AX::Sample sample{};
    if ( _track && _track->ReadSample ( index, sample ) )
    {
        Timer timer{ true };

        std::printf ( "read sample %d %lu (%s)\n", index, sample.Length ( ), AX::FourCCToString ( sample.Handler ( ) ).c_str ( ) );

        if ( sample.Handler ( ) == kJPEG )
        {
            try
            {
                auto stream = IStreamMem::create ( sample.Data ( ), sample.Length ( ) );
                auto image = loadImage ( DataSourceBuffer::create ( loadStreamBuffer ( stream ) ), ImageSource::Options ( ), "jpg" );
                _decodeTimeHistory.push_back ( static_cast<float> ( timer.getSeconds ( ) ) );

                _YCoCgPlane = nullptr;
                _alphaPlane = nullptr;
                _frame = gl::Texture::create ( image, gl::Texture::Format ( ).label ( AX::FourCCToString ( sample.Handler ( ) ) ) );
                return true;
            } catch ( const std::exception& e )
            {
                std::printf ( "err %s\n", e.what ( ) );
                return false;
            }
        } else if ( IsHapHandler ( sample.Handler ( ) ) )
        {
            uint32_t textureCount{ 0 };
            if ( HapGetFrameTextureCount ( sample.Data ( ), sample.Length ( ), &textureCount ) == HapResult_No_Error && textureCount > 0 )
            {
                if ( textureCount == 2 )
                {
                    _frame = nullptr;
                    _YCoCgPlane = HapDecodeFrameAt ( 0, sample.Handler ( ), _track->Width ( ), _track->Height ( ), sample.Data ( ), sample.Length ( ) );
                    _alphaPlane = HapDecodeFrameAt ( 1, sample.Handler ( ), _track->Width ( ), _track->Height ( ), sample.Data ( ), sample.Length ( ) );
                    _decodeTimeHistory.push_back ( static_cast<float> ( timer.getSeconds ( ) ) );

                    if ( _YCoCgPlane && _alphaPlane ) return true;
                    return false;
                } else
                {
                    _YCoCgPlane = nullptr;
                    _alphaPlane = nullptr;
                    _frame = nullptr;

                    if ( sample.Handler ( ) == kHAPY )
                    {
                        _YCoCgPlane = HapDecodeFrameAt ( 0, sample.Handler ( ), _track->Width ( ), _track->Height ( ), sample.Data ( ), sample.Length ( ) );
                    } else
                    {
                        _frame = HapDecodeFrameAt ( 0, sample.Handler ( ), _track->Width ( ), _track->Height ( ), sample.Data ( ), sample.Length ( ) );
                    }
                    _decodeTimeHistory.push_back ( static_cast<float> ( timer.getSeconds ( ) ) );
                    if ( _frame ) return true;
                }
            }
        }
    }
    return false;
}

static void Inspect ( AX::Atom* atom )
{
    ui::ScopedId id{ atom };
    
    if ( ui::TreeNodeEx ( AX::AtomTypeToString ( atom->Type ( ) ).c_str ( ), ImGuiTreeNodeFlags_SpanFullWidth ) )
    {
        for ( auto& [name, value] : atom->Properties ( ) )
        {
            ui::BulletText ( "%s = %s", name.c_str ( ), value.c_str ( ) );
        }
        if ( atom->IsContainer ( ) )
        {
            auto* container = static_cast<AX::ContainerAtom*> ( atom );
            for ( auto& child : container->GetChildren ( ) )
            {
                Inspect ( child.get ( ) );
            }
        }
        ui::TreePop ( );
    }
}

void SimpleHAPDecoderApp::draw ( )
{
    gl::clear ( Colorf::gray ( 0.1f ) );
    {
        static auto kRenderer = gl::getString ( GL_RENDERER );

        ui::ScopedWindow window{ "Settings" };
        ui::Text ( "AX MP4 Parser Test | FPS %.2f | %s", getAverageFps ( ), kRenderer.c_str() );

        if ( _track )
        {
            ui::Text ( "%s | %d x %d", _frame ? _frame->getLabel ( ).c_str ( ) : "No Sample", _track->Width ( ), _track->Height ( ) );
            ui::Text ( "%d samples parsed | %.2f/%.2f", _track->SampleCount ( ), _time, _track->DurationSeconds ( ) );

            if ( _playRate == 0.0f )
            {
                if ( ui::Button ( "Play" ) ) _playRate = 1.0f;
            } else
            {
                if ( ui::Button ( "Pause" ) ) _playRate = 0.0f;
            }

            ui::SliderFloat ( "Play rate", &_playRate, -2.0f, 2.0f );

            if ( _track->SampleCount ( ) > 0 )
            {
                int sample = _currentSample;
                if ( ui::SliderInt ( "Frame", &sample, 0, std::max ( _track->SampleCount ( ) - 1, 0u ) ) )
                {
                    _time = (float)sample / ( (float)_track->SampleCount ( ) - 1 ) * _track->DurationSeconds ( );
                }
            }
        }

        if ( !_decodeTimeHistory.empty() )
        {
            std::vector<float> samples{};
            samples.reserve ( _decodeTimeHistory.size ( ) );

            float avg = 0.0f;
            for ( auto& sample : _decodeTimeHistory )
            {
                avg += sample;
                samples.push_back ( sample );
            }

            avg /= static_cast<float> ( _decodeTimeHistory.size ( ) );
            ui::Text ( "Average Decode Time(s): %.4f", avg );
            ui::PlotLines ( "##", samples.data ( ), static_cast<int> ( samples.size ( ) ), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2 ( 0, 32 ) );
        }

        if ( _mp4 ) Inspect ( _mp4.get ( ) );
    }

    if ( _YCoCgPlane )
    {
        auto bounds = Rectf ( _YCoCgPlane->getBounds ( ) ).getCenteredFit ( getWindowBounds ( ), true );
        std::swap ( bounds.y1, bounds.y2 );

        if ( _alphaPlane )
        {
            gl::ScopedTextureBind tex0{ _YCoCgPlane, 0 };
            gl::ScopedTextureBind tex1{ _alphaPlane, 1 };
            gl::ScopedGlslProg shader{ kYCoCgAlphaShader()};
            gl::drawSolidRect ( bounds );
        } else
        {
            gl::ScopedTextureBind tex0{ _YCoCgPlane, 0 };
            gl::ScopedGlslProg shader{ kYCoCgShader()};
            gl::drawSolidRect ( bounds );
        }

    } else
    {
        auto bounds = Rectf ( _frame->getBounds ( ) ).getCenteredFit ( getWindowBounds ( ), true );
        std::swap ( bounds.y1, bounds.y2 );
        gl::draw ( _frame, bounds );
    }
    
}

void Init ( App::Settings* settings )
{
    settings->setWindowSize ( 1280, 720 );
#ifdef CINDER_MSW
    settings->setConsoleWindowEnabled ( true );
#endif
}

CINDER_APP ( SimpleHAPDecoderApp, RendererGl ( RendererGl::Options ( ) ), Init );
