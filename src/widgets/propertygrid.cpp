#include "tg/widgets/propertygrid.h"
#include "tg/draw.h"
#include "tg/widgets/checkbox.h"
#include "tg/widgets/collapsible.h"
#include "tg/widgets/colorpicker.h"
#include "tg/widgets/combo.h"
#include "tg/widgets/label.h"
#include "tg/widgets/number.h"
#include "tg/widgets/slider.h"
#include "tg/widgets/textinput.h"
#include <memory>

namespace tg
{
    PropertyGrid::PropertyGrid() : Container( Dir::Column, 0 ) { Layout.Width = Grow(); }

    Container& PropertyGrid::Category( std::string Title, bool Open )
    {
        auto& C = Add<tg::Collapsible>( std::move( Title ) );
        C.Layout.Gap = 0;
        if ( Open ) C.SetOpen( true );
        Into = &C;
        return C;
    }

    Container& PropertyGrid::Row( std::string Name )
    {
        auto& R = Into->Add<Container>( Dir::Row, 10, Insets( 8, 3 ) );
        R.Layout.CrossAlign = Align::Center;
        R.Layout.Height = Px( 34 );
        auto& L = R.Add<tg::Label>( std::move( Name ) );
        L.Layout.Width = Px( NameWidth );
        L.Style.TextColor = Tok::TextDim;
        return R;
    }

    tg::Checkbox& PropertyGrid::Bool( std::string N, bool* V ) { return Row( std::move( N ) ).Add<tg::Checkbox>( "", V ); }

    tg::NumberInput& PropertyGrid::Float( std::string N, float* V, float Mn, float Mx, float St, const char* F )
    {
        auto& X = Row( std::move( N ) ).Add<tg::NumberInput>( V, Mn, Mx, St, F );
        X.Layout.Width = Grow();
        return X;
    }

    tg::NumberInput& PropertyGrid::Int( std::string N, int* V, int Mn, int Mx )
    {
        auto F = std::make_shared<float>( V ? float( *V ) : 0.f );
        auto& X = Row( std::move( N ) ).Add<tg::NumberInput>( F.get(), float( Mn ), float( Mx ), 1.f, "%.0f" );
        X.Layout.Width = Grow();
        X.OnChange = [ F, V ]( float Nv ) { if ( V ) *V = int( Nv ); };
        return X;
    }

    tg::Slider& PropertyGrid::Range( std::string N, float* V, float Mn, float Mx, float St )
    {
        auto& X = Row( std::move( N ) ).Add<tg::Slider>( V, Mn, Mx, St );
        X.Layout.Width = Grow();
        return X;
    }

    tg::TextInput& PropertyGrid::Text( std::string N, std::string* V )
    {
        auto& X = Row( std::move( N ) ).Add<tg::TextInput>();
        X.Layout.Width = Grow();
        if ( V ) { X.SetText( *V ); X.OnChange = [ V ]( const std::string& S ) { *V = S; }; }
        return X;
    }

    tg::Combo& PropertyGrid::Enum( std::string N, std::vector<std::string> O, int* V )
    {
        auto& X = Row( std::move( N ) ).Add<tg::Combo>( std::move( O ), V );
        X.Layout.Width = Grow();
        return X;
    }

    tg::ColorPicker& PropertyGrid::Color( std::string N, tg::Color* V ) { return Row( std::move( N ) ).Add<tg::ColorPicker>( V ); }

    Container& PropertyGrid::Vec2( std::string N, tg::Vec2* V )
    {
        auto& R = Row( std::move( N ) );
        for ( float* P : { &V->x, &V->y } )
        {
            auto& X = R.Add<tg::NumberInput>( P, -1e6f, 1e6f, 0.1f, "%.2f" );
            X.Layout.Width = Grow();
        }
        return R;
    }

    tg::Label& PropertyGrid::ReadOnly( std::string N, std::string V ) { return Row( std::move( N ) ).Add<tg::Label>( std::move( V ) ); }

    void PropertyGrid::Draw( DrawList& D )
    {
        D.RectFilled( Bounds, GetTheme()[ Tok::SurfaceAlt ].Fade( 0.5f ), 6 );
        DrawChildren( D );
    }
}
