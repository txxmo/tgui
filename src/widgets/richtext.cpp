#include "tg/widgets/richtext.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>

namespace tg
{
    RichText::RichText( std::string M ) : Source( std::move( M ) )
    {
        Layout.Width = Grow();
        Interactive = true; // links / mentions
        Parse();
    }

    void RichText::SetText( std::string M )
    {
        if ( M == Source ) return;
        Source = std::move( M );
        Parse();
        LaidWidth = -1;
        Invalidate();
    }

    std::string RichText::PlainText() const
    {
        std::string S;
        for ( auto& R : Runs ) S += R.Text;
        return S;
    }

    static bool TokenColor( const std::string& Tag, Paint& Out )
    {
        static const std::pair<const char*, Tok> Names[] = { { "accent", Tok::Accent }, { "dim", Tok::TextDim }, { "success", Tok::Success },
                                                             { "warning", Tok::Warning }, { "danger", Tok::Danger }, { "text", Tok::Text } };
        for ( auto& [ N, T ] : Names )
            if ( Tag == N ) { Out = T; return true; }
        if ( Tag.size() == 7 && Tag[ 0 ] == '#' )
        {
            Out = Color::Hex( uint32_t( std::strtoul( Tag.c_str() + 1, nullptr, 16 ) ) );
            return true;
        }
        return false;
    }

    void RichText::Parse()
    {
        Runs.clear();
        bool Strong = false, Em = false;
        std::vector<Paint> ColorStack;
        Run Cur;
        auto Flush = [ & ] {
            if ( !Cur.Text.empty() ) Runs.push_back( Cur );
            Cur = Run{};
            Cur.Strong = Strong;
            Cur.K = !ColorStack.empty() ? Colored : Strong ? Bold : Em ? Italic : Plain;
            Cur.Col = ColorStack.empty() ? Paint( Tok::Text ) : ColorStack.back();
        };
        Flush();
        const std::string& S = Source;
        for ( size_t i = 0; i < S.size(); )
        {
            char C = S[ i ];
            if ( C == '\\' && i + 1 < S.size() ) { Cur.Text += S[ i + 1 ]; i += 2; continue; }
            if ( C == '*' && i + 1 < S.size() && S[ i + 1 ] == '*' ) { Flush(); Strong = !Strong; Flush(); i += 2; continue; }
            if ( C == '*' ) { Flush(); Em = !Em; Flush(); i++; continue; }
            if ( C == '`' )
            {
                size_t E = S.find( '`', i + 1 );
                if ( E != std::string::npos )
                {
                    Flush();
                    Run R;
                    R.K = Code;
                    R.Text = S.substr( i + 1, E - i - 1 );
                    Runs.push_back( R );
                    i = E + 1;
                    Flush();
                    continue;
                }
            }
            if ( C == '[' )
            {
                size_t Close = S.find( "](", i );
                size_t End = Close == std::string::npos ? std::string::npos : S.find( ')', Close );
                if ( End != std::string::npos )
                {
                    Flush();
                    Run R;
                    R.K = Link;
                    R.Text = S.substr( i + 1, Close - i - 1 );
                    R.Target = S.substr( Close + 2, End - Close - 2 );
                    Runs.push_back( R );
                    i = End + 1;
                    Flush();
                    continue;
                }
            }
            if ( C == '<' )
            {
                size_t E = S.find( '>', i );
                if ( E != std::string::npos )
                {
                    std::string Tag = S.substr( i + 1, E - i - 1 );
                    Paint P = Tok::Text;
                    if ( Tag == "/" ) { Flush(); if ( !ColorStack.empty() ) ColorStack.pop_back(); Flush(); i = E + 1; continue; }
                    if ( TokenColor( Tag, P ) ) { Flush(); ColorStack.push_back( P ); Flush(); i = E + 1; continue; }
                }
            }
            if ( C == '@' && Style.Mentions && ( i == 0 || S[ i - 1 ] == ' ' || S[ i - 1 ] == '\n' ) )
            {
                size_t E = i + 1;
                while ( E < S.size() && ( std::isalnum( (unsigned char)S[ E ] ) || S[ E ] == '_' || S[ E ] == '-' || S[ E ] == '.' ) ) E++;
                while ( E > i + 1 && ( S[ E - 1 ] == '.' || S[ E - 1 ] == '-' ) ) E--; // "@mo." ends the sentence
                if ( E > i + 1 )
                {
                    Flush();
                    Run R;
                    R.K = Mention;
                    R.Strong = true;
                    R.Text = S.substr( i, E - i );
                    R.Target = S.substr( i + 1, E - i - 1 );
                    Runs.push_back( R );
                    i = E;
                    Flush();
                    continue;
                }
            }
            Cur.Text += C;
            i++;
        }
        Flush();
    }

    const Font& RichText::FontOf( const Run& R ) const
    {
        const Theme& T = GetTheme();
        if ( R.K == Code ) return *T.Mono;
        if ( R.Strong || R.K == Bold ) return *T.Bold;
        if ( R.K == Italic ) return T.Italic ? *T.Italic : *T.Medium;
        return Style.TextFont ? *Style.TextFont : *T.Body;
    }

    // Words flow left to right and wrap at Width; a word never splits across runs' fonts.
    Vec2 RichText::Flow( float Width )
    {
        Pieces.clear();
        const Font& Base = Style.TextFont ? *Style.TextFont : *GetTheme().Body;
        float X = 0, Y = 0, LineH = Base.LineHeight, MaxW = 0;
        size_t LineStart = 0;
        auto NewLine = [ & ] {
            // align everything on the line to a shared baseline
            float Asc = 0;
            for ( size_t k = LineStart; k < Pieces.size(); k++ ) Asc = std::max( Asc, FontOf( Runs[ Pieces[ k ].RunIdx ] ).Ascent );
            for ( size_t k = LineStart; k < Pieces.size(); k++ ) Pieces[ k ].R.y = Y + Asc - FontOf( Runs[ Pieces[ k ].RunIdx ] ).Ascent;
            MaxW = std::max( MaxW, X );
            X = 0;
            Y += LineH;
            LineH = Base.LineHeight;
            LineStart = Pieces.size();
        };
        for ( size_t r = 0; r < Runs.size(); r++ )
        {
            const Run& R = Runs[ r ];
            const Font& F = FontOf( R );
            const float Pad = R.K == Code ? 4.f : 0.f;
            if ( R.K == Code || R.K == Link || R.K == Mention ) // one unbreakable piece (code may contain spaces)
            {
                float W = F.Width( R.Text ) + Pad * 2;
                if ( X > 0 && Width > 0 && X + W > Width ) NewLine();
                Pieces.push_back( { r, R.Text, { X + Pad, 0, W - Pad * 2, F.LineHeight } } );
                X += W;
                LineH = std::max( LineH, F.LineHeight );
                continue;
            }
            size_t i = 0;
            while ( i < R.Text.size() )
            {
                if ( R.Text[ i ] == '\n' ) { NewLine(); i++; continue; }
                size_t E = i;
                while ( E < R.Text.size() && R.Text[ E ] != ' ' && R.Text[ E ] != '\n' ) E++;
                size_t Sp = E;
                while ( Sp < R.Text.size() && R.Text[ Sp ] == ' ' ) Sp++;
                std::string Word = R.Text.substr( i, E - i );
                float W = F.Width( Word ), SpW = F.Width( R.Text.substr( E, Sp - E ) );
                if ( X > 0 && Width > 0 && X + W > Width ) NewLine();
                if ( !Word.empty() ) Pieces.push_back( { r, Word, { X, 0, W, F.LineHeight } } );
                X += W + SpW;
                LineH = std::max( LineH, F.LineHeight );
                i = Sp;
            }
        }
        NewLine();
        return { std::ceil( MaxW ), Y };
    }

    Vec2 RichText::Measure( float Avail )
    {
        const Insets& P = Layout.Padding;
        float W = std::max( 1.f, Avail - P.Horizontal() );
        Vec2 S = Flow( W );
        LaidWidth = W;
        LaidSize = S;
        return { S.x + P.Horizontal(), S.y + P.Vertical() };
    }

    void RichText::Arrange( Rect R )
    {
        Bounds = R;
        float W = std::max( 1.f, R.w - Layout.Padding.Horizontal() );
        if ( W != LaidWidth ) { LaidSize = Flow( W ); LaidWidth = W; }
    }

    void RichText::Draw( DrawList& D )
    {
        Vec2 O = { Bounds.x + Layout.Padding.Left, Bounds.y + Layout.Padding.Top };
        for ( auto& P : Pieces )
        {
            const Run& R = Runs[ P.RunIdx ];
            const Font& F = FontOf( R );
            Rect Rr = P.R.Offset( O );
            Color C = Style.Text.Resolve();
            switch ( R.K )
            {
            case Code:
                D.RectFilled( { Rr.x - 4, Rr.y, Rr.w + 8, Rr.h }, Style.CodeFill.Resolve(), 4 );
                C = Style.CodeText.Resolve();
                break;
            case Link: C = Style.Link.Resolve(); break;
            case Mention:
                C = Style.Mention.Resolve();
                D.RectFilled( { Rr.x - 2, Rr.y + 1, Rr.w + 4, Rr.h - 2 }, C.Fade( 0.16f ), 4 );
                break;
            case Colored: C = R.Col.Resolve(); break;
            default: break;
            }
            D.Text( F, { std::floor( Rr.x ), std::floor( Rr.y ) }, P.Text, C.Fade( Enabled ? 1 : 0.5f ) );
            if ( R.K == Link && int( P.RunIdx ) == HoverRun ) D.RectFilled( { Rr.x, Rr.y + F.Ascent + 2, Rr.w, 1 }, C );
        }
    }

    int RichText::RunAt( Vec2 P ) const
    {
        Vec2 O = { Bounds.x + Layout.Padding.Left, Bounds.y + Layout.Padding.Top };
        for ( auto& Pc : Pieces )
            if ( Pc.R.Offset( O ).Contains( P ) && ( Runs[ Pc.RunIdx ].K == Link || Runs[ Pc.RunIdx ].K == Mention ) ) return int( Pc.RunIdx );
        return -1;
    }

    Cursor RichText::GetCursor() const { return HoverRun >= 0 ? Cursor::Hand : Cursor::Arrow; }

    bool RichText::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            int H = RunAt( E.Pos );
            if ( H != HoverRun ) { HoverRun = H; Redraw(); }
            return false;
        }
        case EventType::MouseLeave: if ( HoverRun >= 0 ) { HoverRun = -1; Redraw(); } return false;
        case EventType::MouseDown: return E.Button == MouseButton::Left && RunAt( E.Pos ) >= 0;
        case EventType::MouseUp:
        {
            int R = RunAt( E.Pos );
            if ( E.Button != MouseButton::Left || R < 0 ) return false;
            if ( Runs[ size_t( R ) ].K == Link && OnLink ) OnLink( Runs[ size_t( R ) ].Target );
            if ( Runs[ size_t( R ) ].K == Mention && OnMention ) OnMention( Runs[ size_t( R ) ].Target );
            return true;
        }
        default: return false;
        }
    }
}
