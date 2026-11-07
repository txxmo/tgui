// Toast — small notifications that stack in the bottom-right corner and fade out on their own.
//
//   tg::Toast( Ctx, "Saved" );
//   tg::Toast( Ctx, "Deploy failed: exit code 1", tg::Tok::Danger, 6 );
#pragma once
#include "../theme.h"
#include <string>

namespace tg
{
    class Context;

    // Seconds <= 0: stays until clicked.
    void Toast( Context& Ctx, std::string Text, Paint Accent = Tok::Accent, float Seconds = 3.5f );
    int ToastCount( Context& Ctx ); // how many are on screen (tests)
}
