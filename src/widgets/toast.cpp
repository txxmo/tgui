#include "tg/widgets/toast.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    namespace
    {
        class ToastItem : public Widget
        {
        public:
            ToastItem( std::string T, Paint A, float S ) : Text( std::move( T ) ), Accent( A ), Life( S ) { Interactive = true; }

            std::string Text;
            Paint Accent;
            float Life, Age = 0, InT = 0;
            bool Leaving = false;

            Vec2 Measure( float ) override
            {
                Vec2 S = GetTheme().Body->Measure( Text, 300 );
                return { std::ceil( S.x ) + 34, S.y + 22 };
            }

            void Draw( DrawList& D ) override
            {
                Context* C = GetContext();
                Age += C ? C->DeltaTime() : 0;
                if ( Life > 0 && Age >= Life ) Leaving = true;
                Animate( InT, Leaving ? 0.f : 1.f, 12.f );
                if ( Leaving && InT <= 0.01f && C )
                {
                    Visible = false;
                    C->Post( [ this ] { if ( GetParent() ) GetParent()->Remove( this ); } );
                    return;
                }
                if ( Life > 0 && C ) C->RequestFrameIn( std::max( 0.f, Life - Age ) + 0.01 );

                const Theme& T = GetTheme();
                size_t V0 = D.Vtx.size();
                Rect R = Bounds.Offset( { ( 1 - InT ) * 40, 0 } ); // slides in from the right
                D.Shadow( R, 8, Color( 0, 0, 0, 110 ), 10, { 0, 4 } );
                D.RectFilled( R, T[ Tok::Raised ], 8 );
                D.RectStroke( R, T[ Tok::BorderStrong ], 8 );
                D.RectFilled( { R.x + 8, R.y + 9, 3, R.h - 18 }, Accent.Resolve(), 1.5f );
                D.Text( *T.Body, { R.x + 22, R.y + 11 }, Text, T[ Tok::Text ], 300 );
                D.FadeFrom( V0, InT );
            }

            bool OnEvent( const Event& E ) override
            {
                if ( E.Type == EventType::MouseUp && E.Button == MouseButton::Left ) { Leaving = true; Redraw(); return true; }
                return E.Type == EventType::MouseDown;
            }
        };

        // Full-screen, click-through layer that stacks the toasts bottom-right.
        class ToastHost : public Widget
        {
        public:
            ToastHost() { Layout.Width = Grow(); Layout.Height = Grow(); }

            void Arrange( Rect R ) override
            {
                Bounds = R;
                float Y = R.Bottom() - 16;
                for ( auto It = Children().rbegin(); It != Children().rend(); ++It )
                {
                    Widget& K = **It;
                    if ( !K.Visible ) continue;
                    Vec2 S = K.MeasureCached( 340 );
                    Y -= S.y;
                    K.Arrange( { R.Right() - 16 - S.x, Y, S.x, S.y } );
                    Y -= 8;
                }
            }

            Widget* HitTest( Vec2 P ) override
            {
                for ( auto It = Children().rbegin(); It != Children().rend(); ++It )
                    if ( Widget* H = ( *It )->HitTest( P ) ) return H;
                return nullptr; // click-through everywhere else
            }
        };

        ToastHost& Host( Context& Ctx )
        {
            for ( auto& K : Ctx.Overlay().Children() )
                if ( auto* H = dynamic_cast<ToastHost*>( K.get() ) ) return *H;
            return Ctx.Overlay().Add<ToastHost>();
        }
    }

    void Toast( Context& Ctx, std::string Text, Paint Accent, float Seconds )
    {
        ToastHost& H = Host( Ctx );
        H.BringToFront(); // above dialogs
        H.Add<ToastItem>( std::move( Text ), Accent, Seconds );
    }

    int ToastCount( Context& Ctx )
    {
        int N = 0;
        for ( auto& K : Ctx.Overlay().Children() )
            if ( auto* H = dynamic_cast<ToastHost*>( K.get() ) )
                for ( auto& T : H->Children() ) N += T->Visible;
        return N;
    }
}
