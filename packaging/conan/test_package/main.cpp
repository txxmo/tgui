#include "tg/tg.h"
#include "soft_renderer.h"
#include <cstdio>

int main()
{
    tg::SoftRenderer R;
    tg::Context C( R );
    if ( !C.LoadDefaultFonts() ) return 1;
    C.SetViewport( { 200, 80 }, 1 );
    C.Root().Add<tg::Label>( "tg package works" );
    C.Frame();
    C.Render( { 200, 80 } );
    std::printf( "tg test_package: %dx%d rendered\n", R.Width, R.Height );
    return R.Width == 200 ? 0 : 1;
}
