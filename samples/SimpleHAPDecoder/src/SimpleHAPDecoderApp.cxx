//
//  SimpleHAPDecoderApp.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include <AX/AX-MP4.h>
#include "Decoders.h"

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/CinderImGui.h"
#include "cinder/Breakpoint.h"

#include <iostream>
#include "circular/circular.h"
#include "cinder/ConcurrentCircularBuffer.h"

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

    bool            LoadMP4             ( const DataSourceRef& source );
    void            DecodeFrameAt       ( int index );
    void            DecodeFrameAtAsync  ( int index );
    void            OnSampleDecoded     ( const AX::ITrackDecoderRef& decoded );

    int                     _currentSample{ 0 };
    AX::ITrackDecoderRef    _decoded;
    gl::TextureRef  _frame;
    
    gl::TextureRef  _YCoCgPlane;
    gl::TextureRef  _alphaPlane;
    
    AX::MP4Ref      _mp4;
    AX::MovieRef    _movie;
    AX::TrackRef    _track{ nullptr };
    float           _playRate{ 0.0f };
    float           _time{ 0 };
    
    bool                    _async{ false };
    circular_buffer<float>  _decodeTimeHistory{ 64 };
    circular_buffer<float>  _fpsHistory{ 128 };
};

void SimpleHAPDecoderApp::setup ( )
{
    gl::enableVerticalSync ( false );
    setFrameRate ( 3000 );
    setFpsSampleInterval ( 1.0f / 60.0f );

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

const uint32_t kHAP1 = 'Hap1';
const uint32_t kHAP5 = 'Hap5';
const uint32_t kHAP7 = 'Hap7';
const uint32_t kHAPY = 'HapY';
const uint32_t kHAPM = 'HapM';
const uint32_t kJPEG = 'jpeg';

bool SimpleHAPDecoderApp::LoadMP4 ( const DataSourceRef& source )
{
    try
    {
        _mp4 = AX::MP4::Create ( source, AX::MP4::Format{}.TrackProperties ( true ).PreloadIntoMemory ( true ) );
        if ( _mp4 )
        {
            if ( _mp4->IsValid ( ) )
            {
                _movie = AX::Movie::Create ( _mp4 );
                _track = _movie->GetTrack ( AX::TrackType::kVideo, 0 );
                _track->RegisterDecoder<HAPDecoder> ( kHAP1 );
                _track->RegisterDecoder<HAPDecoder> ( kHAP5 );
                _track->RegisterDecoder<HAPDecoder> ( kHAP7 );
                _track->RegisterDecoder<HAPDecoder> ( kHAPY );
                _track->RegisterDecoder<HAPDecoder> ( kHAPM );
                _track->RegisterDecoder<MJPEGDecoder> ( kJPEG );
                
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
    static float kAccum = 0.0f;
    kAccum += dt;
    if ( kAccum > getFpsSampleInterval() )
    {
        kAccum = 0.0f;
        _fpsHistory.push_back ( getAverageFps ( ) );
    }

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

void SimpleHAPDecoderApp::DecodeFrameAt ( int index )
{
    if ( !_track ) return;

    if ( _async )
    {
        DecodeFrameAtAsync ( index );
    } else
    {
        OnSampleDecoded ( _track->DecodeSample ( index ) );
    }
}

void SimpleHAPDecoderApp::OnSampleDecoded ( const AX::ITrackDecoderRef& decoder )
{
    assert ( app::isMainThread ( ) );

    if ( !decoder ) return;
    _decoded = decoder;
    
    _YCoCgPlane = nullptr;
    _alphaPlane = nullptr;
    _frame = nullptr;

    if ( _decoded->FrameCount ( ) == 2 )
    {
        _YCoCgPlane = _decoded->CreateTexture ( 0 );
        _alphaPlane = _decoded->CreateTexture ( 1 );
        _decodeTimeHistory.push_back ( _decoded->FrameAt ( 0 ).DecodeTime + _decoded->FrameAt ( 1 ).DecodeTime );
    } else
    {
        if ( _decoded->Handler ( ) == kHAPY )
        {
            _YCoCgPlane = _decoded->CreateTexture ( 0 );
        } else
        {
            _frame = _decoded->CreateTexture ( 0 );
        }

        _decodeTimeHistory.push_back ( _decoded->FrameAt ( 0 ).DecodeTime );
    }
    
}

void SimpleHAPDecoderApp::DecodeFrameAtAsync ( int index )
{
    if ( !_track ) return;
    _track->DecodeSampleAsync ( index, [=]( uint32_t, bool succeeded, const AX::ITrackDecoderRef& decoded )
    {
        if ( succeeded ) OnSampleDecoded ( decoded );
    } );
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
            std::string handler = "No Sample";
            if ( _frame ) handler = _frame->getLabel ( );
            if ( _YCoCgPlane ) handler = _YCoCgPlane->getLabel ( );
            ui::Text ( "%s | %d x %d", handler.c_str(), _track->Width ( ), _track->Height ( ) );
            ui::Text ( "%d samples parsed | %.2f/%.2f", _track->SampleCount ( ), _time, _track->DurationSeconds ( ) );
            ui::Checkbox ( "Async", &_async );

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
                auto ms = sample * 1000.0f;
                avg += ms;
                samples.push_back ( ms );
            }

            avg /= static_cast<float> ( _decodeTimeHistory.size ( ) );
            ui::Text ( "Average Decode Time: %.4fms", avg );
            ui::PlotLines ( "##", samples.data ( ), static_cast<int> ( samples.size ( ) ), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2 ( 0, 32 ) );
        }

        if ( !_fpsHistory.empty ( ) )
        {
            std::vector<float> samples{};
            samples.reserve ( _fpsHistory.size ( ) );

            for ( auto& sample : _fpsHistory )
            {
                samples.push_back ( sample );
            }

            ui::Text ( "FPS History" );
            ui::PlotLines ( "##", samples.data ( ), static_cast<int> ( samples.size ( ) ), 0, nullptr, 0.0f, getFrameRate(), ImVec2 ( 0, 32 ) );
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

    } else if ( _frame )
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
    settings->setConsoleWindowEnabled ( false );
#endif
}

CINDER_APP ( SimpleHAPDecoderApp, RendererGl ( RendererGl::Options ( ) ), Init );
