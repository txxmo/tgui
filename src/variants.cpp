#include "tg/variants.h"
#include "tg/widgets/box.h"
#include "tg/widgets/button.h"

namespace tg
{
    void RegisterBuiltinVariants()
    {
        SetVariant<ButtonStyle>( "default", Button::Defaults() );
        SetVariant<ButtonStyle>( "primary", Button::Primary() );
        SetVariant<ButtonStyle>( "danger", Button::Danger() );
        SetVariant<ButtonStyle>( "ghost", Button::Ghost() );
        SetVariant<BoxStyle>( "card", Box::Card() );
        SetVariant<BoxStyle>( "panel", Box::Panel() );
    }
}
