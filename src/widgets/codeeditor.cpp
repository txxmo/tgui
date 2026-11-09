#include "tg/widgets/codeeditor.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace tg
{
    // ------------------------------------------------------------ highlighting

    struct LangDef
    {
        const char *LineComment, *LineComment2, *BlockOpen, *BlockClose, *Keywords, *Types;
        bool Preproc, JsonKeys;
    };

    static const LangDef& Def( CodeLang L )
    {
        static const LangDef Plain = { nullptr, nullptr, nullptr, nullptr, "", "", false, false };
        static const LangDef Cpp = { "//", nullptr, "/*", "*/",
            " alignas alignof auto break case catch class concept const consteval constexpr constinit continue co_await co_return co_yield"
            " decltype default delete do else enum explicit export extern false for friend goto if inline mutable namespace new noexcept"
            " nullptr operator override private protected public requires return sizeof static static_assert static_cast dynamic_cast"
            " reinterpret_cast const_cast struct switch template this throw true try typedef typeid typename union using virtual volatile while final ",
            " bool char char8_t char16_t char32_t double float int long short signed unsigned void wchar_t size_t int8_t int16_t int32_t int64_t"
            " uint8_t uint16_t uint32_t uint64_t std string vector map ",
            true, false };
        static const LangDef Json = { nullptr, nullptr, nullptr, nullptr, " true false null ", "", false, true };
        static const LangDef Python = { "#", nullptr, nullptr, nullptr,
            " and as assert async await break class continue def del elif else except False finally for from global if import in is lambda"
            " None nonlocal not or pass raise return True try while with yield self ",
            " int float str bool list dict set tuple bytes object type ", false, false };
        static const LangDef Lua = { "--", nullptr, "--[[", "]]",
            " and break do else elseif end false for function goto if in local nil not or repeat return then true until while ", "", false, false };
        static const LangDef Shell = { "#", nullptr, nullptr, nullptr,
            " if then else elif fi for while do done case esac in function return local export echo exit set unset source sudo ", "", false, false };
        switch ( L )
        {
        case CodeLang::Cpp: return Cpp;
        case CodeLang::Json: return Json;
        case CodeLang::Python: return Python;
        case CodeLang::Lua: return Lua;
        case CodeLang::Shell: return Shell;
        default: return Plain;
        }
    }

    static bool InList( const char* List, std::string_view W )
    {
        if ( !List || W.empty() ) return false;
        std::string Key = " " + std::string( W ) + " ";
        return std::strstr( List, Key.c_str() ) != nullptr;
    }

    static bool IdentStart( char C ) { return std::isalpha( (unsigned char)C ) || C == '_' || (unsigned char)C >= 0x80; }
    static bool IdentChar( char C ) { return std::isalnum( (unsigned char)C ) || C == '_' || (unsigned char)C >= 0x80; }

    static void Tokenize( const LangDef& D, std::string_view S, int& State, std::vector<CodeSpan>& Out )
    {
        size_t i = 0, N = S.size();
        auto Starts = [ & ]( const char* P ) { return P && S.compare( i, std::strlen( P ), P ) == 0; };
        // preprocessor: the whole line
        size_t First = S.find_first_not_of( " \t" );
        if ( State == 0 && D.Preproc && First != std::string_view::npos && S[ First ] == '#' )
        {
            Out.push_back( { First, N, CodeToken::Preproc } );
            return;
        }
        while ( i < N )
        {
            if ( State == 1 )
            {
                size_t E = S.find( D.BlockClose, i );
                size_t End = E == std::string_view::npos ? N : E + std::strlen( D.BlockClose );
                Out.push_back( { i, End, CodeToken::Comment } );
                if ( E != std::string_view::npos ) State = 0;
                i = End;
                continue;
            }
            char C = S[ i ];
            if ( Starts( D.BlockOpen ) ) { State = 1; continue; }
            if ( Starts( D.LineComment ) || Starts( D.LineComment2 ) ) { Out.push_back( { i, N, CodeToken::Comment } ); return; }
            if ( C == '"' || C == '\'' || C == '`' )
            {
                size_t j = i + 1;
                while ( j < N && S[ j ] != C ) j += S[ j ] == '\\' ? 2 : 1;
                j = std::min( N, j + 1 );
                // JSON: a string followed by ':' is a key
                size_t k = j;
                while ( k < N && S[ k ] == ' ' ) k++;
                Out.push_back( { i, j, D.JsonKeys && k < N && S[ k ] == ':' ? CodeToken::Type : CodeToken::String } );
                i = j;
                continue;
            }
            if ( std::isdigit( (unsigned char)C ) || ( C == '.' && i + 1 < N && std::isdigit( (unsigned char)S[ i + 1 ] ) ) )
            {
                size_t j = i;
                while ( j < N && ( std::isalnum( (unsigned char)S[ j ] ) || S[ j ] == '.' || S[ j ] == '_' || S[ j ] == '\'' ) ) j++;
                Out.push_back( { i, j, CodeToken::Number } );
                i = j;
                continue;
            }
            if ( IdentStart( C ) )
            {
                size_t j = i;
                while ( j < N && IdentChar( S[ j ] ) ) j++;
                std::string_view W = S.substr( i, j - i );
                size_t k = j;
                while ( k < N && S[ k ] == ' ' ) k++;
                CodeToken K = InList( D.Keywords, W ) ? CodeToken::Keyword : InList( D.Types, W ) ? CodeToken::Type
                            : k < N && S[ k ] == '(' ? CodeToken::Function : CodeToken::Text;
                Out.push_back( { i, j, K } );
                i = j;
                continue;
            }
            if ( std::ispunct( (unsigned char)C ) ) { Out.push_back( { i, i + 1, CodeToken::Punct } ); i++; continue; }
            i++;
        }
    }

    // ------------------------------------------------------------ text model

    CodeEditor::CodeEditor( std::string T, CodeLang L ) : Lang( L ), Text( std::move( T ) )
    {
        Interactive = true;
        Focusable = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
        Sels.push_back( {} );
        Reindex();
    }

    const Font& CodeEditor::Mono() const { return Style.TextFont ? *Style.TextFont : *GetTheme().Mono; }

    void CodeEditor::SetText( std::string T )
    {
        Text = std::move( T );
        Sels.assign( 1, Sel{} );
        UndoStack.clear();
        RedoStack.clear();
        Reindex();
        Redraw();
    }

    void CodeEditor::Reindex( size_t From )
    {
        size_t L0 = Starts.empty() ? 0 : LineOf( std::min( From, Text.size() ) );
        Starts.clear();
        Starts.push_back( 0 );
        for ( size_t i = 0; i < Text.size(); i++ )
            if ( Text[ i ] == '\n' ) Starts.push_back( i + 1 );
        LineState.resize( Starts.size(), -1 );
        for ( size_t i = L0; i < LineState.size(); i++ ) LineState[ i ] = -1;
        if ( !LineState.empty() ) LineState[ 0 ] = 0;
    }

    size_t CodeEditor::LineOf( size_t Pos ) const
    {
        auto It = std::upper_bound( Starts.begin(), Starts.end(), Pos );
        return size_t( It - Starts.begin() ) - 1;
    }

    size_t CodeEditor::LineEnd( size_t L ) const { return L + 1 < Starts.size() ? Starts[ L + 1 ] - 1 : Text.size(); }

    std::string CodeEditor::Line( size_t I ) const { return I < Starts.size() ? Text.substr( Starts[ I ], LineEnd( I ) - Starts[ I ] ) : std::string(); }

    int CodeEditor::ColumnOf( size_t Pos ) const
    {
        size_t L = LineOf( Pos );
        int Col = 0;
        for ( size_t i = Starts[ L ]; i < Pos && i < Text.size(); )
        {
            if ( Text[ i ] == '\t' ) { Col = ( Col / TabSize + 1 ) * TabSize; i++; }
            else { i = NextGrapheme( Text, i ); Col++; }
        }
        return Col;
    }

    size_t CodeEditor::PosAt( size_t L, float Want ) const
    {
        L = std::min( L, Starts.size() - 1 );
        size_t i = Starts[ L ], E = LineEnd( L );
        int Col = 0;
        while ( i < E )
        {
            int Next = Text[ i ] == '\t' ? ( Col / TabSize + 1 ) * TabSize : Col + 1;
            if ( Want < ( Col + Next ) * 0.5f ) break;
            Col = Next;
            i = Text[ i ] == '\t' ? i + 1 : NextGrapheme( Text, i );
        }
        return std::min( i, E );
    }

    Rect CodeEditor::TextRect() const { return { Bounds.x + GutterW + 8, Bounds.y + 6, Bounds.w - GutterW - 14, Bounds.h - 12 }; }

    size_t CodeEditor::PosAtPoint( Vec2 P ) const
    {
        Rect T = TextRect();
        float Y = P.y - T.y + ScrollY;
        size_t L = size_t( std::clamp( std::floor( Y / LH ), 0.f, float( Starts.size() - 1 ) ) );
        return PosAt( L, ( P.x - T.x + ScrollX ) / CW );
    }

    void CodeEditor::Normalize()
    {
        std::sort( Sels.begin(), Sels.end(), []( const Sel& A, const Sel& B ) { return std::min( A.Caret, A.Anchor ) < std::min( B.Caret, B.Anchor ); } );
        std::vector<Sel> Out;
        for ( auto& S : Sels )
        {
            S.Caret = std::min( S.Caret, Text.size() );
            S.Anchor = std::min( S.Anchor, Text.size() );
            if ( !Out.empty() )
            {
                Sel& P = Out.back();
                size_t PMax = std::max( P.Caret, P.Anchor ), SMin = std::min( S.Caret, S.Anchor );
                if ( SMin < PMax || ( SMin == PMax && S.Caret == S.Anchor && P.Caret == P.Anchor ) )
                {
                    size_t Lo = std::min( std::min( P.Caret, P.Anchor ), SMin ), Hi = std::max( PMax, std::max( S.Caret, S.Anchor ) );
                    bool Fwd = P.Caret >= P.Anchor;
                    P.Anchor = Fwd ? Lo : Hi;
                    P.Caret = Fwd ? Hi : Lo;
                    continue;
                }
            }
            Out.push_back( S );
        }
        Sels = Out.empty() ? std::vector<Sel>( 1 ) : Out;
    }

    void CodeEditor::Record( int Kind )
    {
        Context* C = GetContext();
        double Now = C ? C->Time() : 0;
        bool Merge = Kind == 1 && LastEditKind == 1 && Now - LastEditTime < 1.0 && !UndoStack.empty();
        if ( !Merge )
        {
            UndoStack.push_back( { Text, Sels } );
            if ( UndoStack.size() > 300 ) UndoStack.erase( UndoStack.begin() );
        }
        RedoStack.clear();
        LastEditKind = Kind;
        LastEditTime = Now;
    }

    void CodeEditor::Changed()
    {
        if ( Context* C = GetContext() ) BlinkStart = C->Time();
        ScrollToCaret();
        if ( OnChange ) OnChange( Text );
        Redraw();
    }

    void CodeEditor::Edit( const std::function<void( Sel&, std::string&, size_t&, size_t& )>& Fn, int Kind )
    {
        if ( ReadOnly ) return;
        Record( Kind );
        Normalize();
        long long Delta = 0;
        size_t Lowest = Text.size();
        for ( auto& S : Sels )
        {
            S.Caret = size_t( (long long)S.Caret + Delta );
            S.Anchor = size_t( (long long)S.Anchor + Delta );
            size_t From = std::min( S.Caret, S.Anchor ), To = std::max( S.Caret, S.Anchor );
            std::string Ins;
            Fn( S, Ins, From, To );
            From = std::min( From, Text.size() );
            To = std::clamp( To, From, Text.size() );
            Text.erase( From, To - From );
            Text.insert( From, Ins );
            S.Caret = S.Anchor = From + Ins.size();
            S.WantCol = -1;
            Delta += (long long)Ins.size() - (long long)( To - From );
            Lowest = std::min( Lowest, From );
        }
        Reindex( Lowest );
        Normalize();
        Changed();
    }

    void CodeEditor::Insert( const std::string& S )
    {
        Edit( [ & ]( Sel&, std::string& Ins, size_t&, size_t& ) { Ins = S; }, S.size() == 1 && S != "\n" ? 1 : 2 );
    }

    void CodeEditor::SetCaret( size_t L, size_t Col )
    {
        L = std::min( L, Starts.size() - 1 );
        size_t P = std::min( Starts[ L ] + Col, LineEnd( L ) );
        Sels.assign( 1, Sel{ P, P, -1 } );
        ScrollToCaret();
        Redraw();
    }

    std::string CodeEditor::SelectedText() const
    {
        std::string Out;
        for ( auto& S : Sels )
        {
            size_t A = std::min( S.Caret, S.Anchor ), B = std::max( S.Caret, S.Anchor );
            if ( A == B ) continue;
            if ( !Out.empty() ) Out += '\n';
            Out += Text.substr( A, B - A );
        }
        return Out;
    }

    bool CodeEditor::Undo()
    {
        if ( UndoStack.empty() ) return false;
        RedoStack.push_back( { Text, Sels } );
        Text = UndoStack.back().Text;
        Sels = UndoStack.back().Sels;
        UndoStack.pop_back();
        LastEditKind = 0;
        Reindex();
        Normalize();
        Changed();
        return true;
    }

    bool CodeEditor::Redo()
    {
        if ( RedoStack.empty() ) return false;
        UndoStack.push_back( { Text, Sels } );
        Text = RedoStack.back().Text;
        Sels = RedoStack.back().Sels;
        RedoStack.pop_back();
        LastEditKind = 0;
        Reindex();
        Normalize();
        Changed();
        return true;
    }

    // ------------------------------------------------------------ movement & editing helpers

    static size_t WordLeft( const std::string& T, size_t P )
    {
        while ( P > 0 && std::isspace( (unsigned char)T[ P - 1 ] ) ) P--;
        if ( P > 0 && IdentChar( T[ P - 1 ] ) ) { while ( P > 0 && IdentChar( T[ P - 1 ] ) ) P--; }
        else if ( P > 0 ) P--;
        return P;
    }
    static size_t WordRight( const std::string& T, size_t P )
    {
        if ( P < T.size() && IdentChar( T[ P ] ) ) { while ( P < T.size() && IdentChar( T[ P ] ) ) P++; }
        else if ( P < T.size() ) P++;
        while ( P < T.size() && T[ P ] == ' ' ) P++;
        return P;
    }

    void CodeEditor::Move( int Dx, int Dy, bool Word, bool Extend )
    {
        for ( auto& S : Sels )
        {
            if ( Dx )
            {
                if ( !Extend && S.Caret != S.Anchor ) S.Caret = Dx < 0 ? std::min( S.Caret, S.Anchor ) : std::max( S.Caret, S.Anchor );
                else if ( Dx < 0 ) S.Caret = Word ? WordLeft( Text, S.Caret ) : S.Caret > 0 ? PrevGrapheme( Text, S.Caret ) : 0;
                else S.Caret = Word ? WordRight( Text, S.Caret ) : NextGrapheme( Text, S.Caret );
                S.WantCol = -1;
            }
            if ( Dy )
            {
                size_t L = LineOf( S.Caret );
                float Col = S.WantCol >= 0 ? S.WantCol : float( ColumnOf( S.Caret ) );
                long long NL = (long long)L + Dy;
                if ( NL < 0 ) S.Caret = 0;
                else if ( NL >= (long long)Starts.size() ) S.Caret = Text.size();
                else S.Caret = PosAt( size_t( NL ), Col );
                S.WantCol = Col;
            }
            if ( !Extend ) S.Anchor = S.Caret;
        }
        Normalize();
        if ( Context* C = GetContext() ) BlinkStart = C->Time();
        ScrollToCaret();
        Redraw();
    }

    void CodeEditor::ToggleComment()
    {
        const char* Mark = Def( Lang ).LineComment;
        if ( !Mark ) return;
        std::string M = std::string( Mark ) + " ";
        // every line touched by a selection; comment all unless all are already commented
        std::vector<size_t> Lines;
        for ( auto& S : Sels )
        {
            size_t A = LineOf( std::min( S.Caret, S.Anchor ) ), B = LineOf( std::max( S.Caret, S.Anchor ) );
            for ( size_t L = A; L <= B; L++ ) Lines.push_back( L );
        }
        std::sort( Lines.begin(), Lines.end() );
        Lines.erase( std::unique( Lines.begin(), Lines.end() ), Lines.end() );
        bool All = true;
        for ( size_t L : Lines )
        {
            std::string S = Line( L );
            size_t F = S.find_first_not_of( " \t" );
            if ( F != std::string::npos && S.compare( F, std::strlen( Mark ), Mark ) != 0 ) All = false;
        }
        Record( 2 );
        for ( auto It = Lines.rbegin(); It != Lines.rend(); ++It ) // bottom-up keeps earlier offsets valid
        {
            std::string S = Line( *It );
            size_t F = S.find_first_not_of( " \t" );
            if ( F == std::string::npos ) continue;
            size_t At = Starts[ *It ] + F;
            if ( All )
            {
                size_t Len = Text.compare( At, M.size(), M ) == 0 ? M.size() : std::strlen( Mark );
                Text.erase( At, Len );
            }
            else Text.insert( At, M );
        }
        Reindex();
        Sels.assign( 1, Sel{ std::min( Sels.back().Caret, Text.size() ), std::min( Sels.back().Caret, Text.size() ), -1 } );
        Changed();
    }

    void CodeEditor::Indent( bool Out )
    {
        std::vector<size_t> Lines;
        for ( auto& S : Sels )
        {
            size_t A = LineOf( std::min( S.Caret, S.Anchor ) ), B = LineOf( std::max( S.Caret, S.Anchor ) );
            if ( B > A && std::max( S.Caret, S.Anchor ) == Starts[ B ] ) B--; // selection ending at a line start
            for ( size_t L = A; L <= B; L++ ) Lines.push_back( L );
        }
        std::sort( Lines.begin(), Lines.end() );
        Lines.erase( std::unique( Lines.begin(), Lines.end() ), Lines.end() );
        Record( 2 );
        std::string Unit = InsertSpaces ? std::string( size_t( TabSize ), ' ' ) : "\t";
        for ( auto It = Lines.rbegin(); It != Lines.rend(); ++It )
        {
            size_t At = Starts[ *It ];
            if ( !Out ) { Text.insert( At, Unit ); continue; }
            size_t N = 0;
            while ( N < size_t( TabSize ) && At + N < Text.size() && Text[ At + N ] == ' ' ) N++;
            if ( N == 0 && At < Text.size() && Text[ At ] == '\t' ) N = 1;
            Text.erase( At, N );
        }
        // keep the lines selected
        size_t A = Starts[ Lines.front() ];
        Reindex();
        size_t B = LineEnd( std::min( Lines.back(), Starts.size() - 1 ) );
        Sels.assign( 1, Sel{ B, A, -1 } );
        Changed();
    }

    void CodeEditor::AddNextOccurrence()
    {
        Sel& Last = Sels.back();
        size_t A = std::min( Last.Caret, Last.Anchor ), B = std::max( Last.Caret, Last.Anchor );
        if ( A == B ) // nothing selected: select the word under the caret
        {
            size_t Wa, Wb;
            WordAt( Text, A, Wa, Wb );
            Last.Anchor = Wa;
            Last.Caret = Wb;
            Redraw();
            return;
        }
        std::string Needle = Text.substr( A, B - A );
        size_t F = Text.find( Needle, B );
        if ( F == std::string::npos ) F = Text.find( Needle );
        if ( F == std::string::npos || F == A ) return;
        Sels.push_back( { F + Needle.size(), F, -1 } );
        Normalize();
        ScrollToCaret();
        Redraw();
    }

    void CodeEditor::ScrollToCaret()
    {
        if ( Bounds.w <= 0 || Sels.empty() ) return;
        Rect T = TextRect();
        size_t P = Sels.back().Caret;
        float Y = LineOf( P ) * LH, X = ColumnOf( P ) * CW;
        if ( Y < ScrollY ) ScrollY = Y;
        if ( Y + LH > ScrollY + T.h ) ScrollY = Y + LH - T.h;
        if ( X < ScrollX ) ScrollX = std::max( 0.f, X - CW * 4 );
        if ( X + CW > ScrollX + T.w ) ScrollX = X + CW * 4 - T.w;
    }

    // ------------------------------------------------------------ layout & drawing

    Vec2 CodeEditor::Measure( float Avail ) { return { std::min( Avail, 480.f ), 260 }; }

    void CodeEditor::Arrange( Rect R )
    {
        Bounds = R;
        const Font& F = Mono();
        CW = F.Find( 'M' ).Advance;
        LH = std::round( F.LineHeight + 2 );
        int Digits = std::max( 2, int( std::log10( std::max<size_t>( 1, Starts.size() ) ) ) + 1 );
        GutterW = LineNumbers ? Digits * CW + 18 : 4;
    }

    void CodeEditor::HighlightLine( size_t L, std::vector<CodeSpan>& Out )
    {
        Out.clear();
        // states of earlier lines (cached; recomputed only after an edit above)
        size_t K = L;
        while ( K > 0 && LineState[ K ] < 0 ) K--;
        for ( ; K < L; K++ )
        {
            int St = LineState[ K ];
            std::vector<CodeSpan> Tmp;
            std::string S = Line( K );
            if ( Highlighter ) Highlighter( S, St, Tmp );
            else Tokenize( Def( Lang ), S, St, Tmp );
            LineState[ K + 1 ] = St;
        }
        int St = std::max( 0, LineState[ L ] );
        std::string S = Line( L );
        if ( Highlighter ) Highlighter( S, St, Out );
        else Tokenize( Def( Lang ), S, St, Out );
        if ( L + 1 < LineState.size() ) LineState[ L + 1 ] = St;
    }

    void CodeEditor::Draw( DrawList& D )
    {
        const CodeEditorStyle& S = Style;
        const Font& F = Mono();
        const Theme& Th = GetTheme();
        Context* C = GetContext();
        D.RectFilled( Bounds, S.Background, S.Radius );
        if ( LineNumbers ) D.RectFilled( { Bounds.x, Bounds.y, GutterW, Bounds.h }, S.Gutter, S.Radius, CornersTop | CornersBottom );
        Rect T = TextRect();
        float MaxScrollY = std::max( 0.f, Starts.size() * LH - T.h );
        ScrollY = std::clamp( ScrollY, 0.f, MaxScrollY );
        ScrollX = std::max( 0.f, ScrollX );
        size_t First = size_t( ScrollY / LH ), Last = std::min( Starts.size(), size_t( ( ScrollY + T.h ) / LH ) + 2 );
        bool Focused = IsFocused();
        std::vector<size_t> CaretLines;
        for ( auto& Sl : Sels ) CaretLines.push_back( LineOf( Sl.Caret ) );

        D.PushClip( { Bounds.x, T.y, Bounds.w, T.h } );
        std::vector<CodeSpan> Spans;
        for ( size_t L = First; L < Last; L++ )
        {
            float Y = T.y + L * LH - ScrollY;
            bool Cur = std::find( CaretLines.begin(), CaretLines.end(), L ) != CaretLines.end();
            if ( Cur && Focused ) D.RectFilled( { T.x - 8, Y, Bounds.Right() - T.x + 8, LH }, S.CurrentLine );
            if ( LineNumbers )
            {
                std::string N = std::to_string( L + 1 );
                D.Text( F, { Bounds.x + GutterW - 10 - F.Width( N ), Y + 1 }, N, Cur ? S.GutterCurrent : S.GutterText );
            }
            // selections on this line
            size_t Ls = Starts[ L ], Le = LineEnd( L );
            D.PushClip( T );
            for ( auto& Sl : Sels )
            {
                size_t A = std::min( Sl.Caret, Sl.Anchor ), B = std::max( Sl.Caret, Sl.Anchor );
                if ( A == B || B < Ls || A > Le ) continue;
                float X0 = ColumnOf( std::max( A, Ls ) ) * CW, X1 = ColumnOf( std::min( B, Le ) ) * CW + ( B > Le ? CW * 0.5f : 0 );
                D.RectFilled( { T.x - ScrollX + X0, Y, std::max( 2.f, X1 - X0 ), LH }, S.Selection, 2 );
            }
            // text, span by span (tabs expanded)
            HighlightLine( L, Spans );
            std::string Ln = Line( L );
            size_t Pos = 0, SpanI = 0;
            int Col = 0;
            while ( Pos < Ln.size() )
            {
                while ( SpanI < Spans.size() && Spans[ SpanI ].End <= Pos ) SpanI++;
                CodeToken K = SpanI < Spans.size() && Spans[ SpanI ].Begin <= Pos ? Spans[ SpanI ].Kind : CodeToken::Text;
                size_t End = SpanI < Spans.size() ? ( Spans[ SpanI ].Begin > Pos ? Spans[ SpanI ].Begin : Spans[ SpanI ].End ) : Ln.size();
                size_t Tab = Ln.find( '\t', Pos );
                if ( Tab != std::string::npos && Tab < End ) End = Tab;
                if ( End == Pos ) // a tab
                {
                    Col = ( Col / TabSize + 1 ) * TabSize;
                    Pos++;
                    continue;
                }
                std::string_view Piece( Ln.data() + Pos, End - Pos );
                float X = T.x - ScrollX + Col * CW;
                if ( X < T.Right() ) D.Text( F, { X, Y + 1 }, Piece, S.Colors[ int( K ) ] );
                for ( size_t i = 0; i < Piece.size(); i = NextGrapheme( Piece, i ) ) Col++;
                Pos = End;
            }
            D.PopClip();
        }
        // carets (blink while focused)
        if ( Focused && C )
        {
            bool On = std::fmod( C->Time() - BlinkStart, 1.0 ) < 0.6;
            C->RequestFrameIn( 0.3 );
            if ( On )
                for ( auto& Sl : Sels )
                {
                    size_t L = LineOf( Sl.Caret );
                    float X = T.x - ScrollX + ColumnOf( Sl.Caret ) * CW, Y = T.y + L * LH - ScrollY;
                    if ( X >= T.x - 1 && X <= T.Right() ) D.RectFilled( { X - 1, Y + 1, 2, LH - 2 }, S.Caret );
                }
        }
        D.PopClip();
        // scrollbar
        if ( MaxScrollY > 0 )
        {
            float H = std::max( 24.f, T.h * T.h / ( Starts.size() * LH ) );
            D.RectFilled( { Bounds.Right() - 6, T.y + ( T.h - H ) * ( ScrollY / MaxScrollY ), 4, H }, Th[ Tok::BorderStrong ].Fade( 0.7f ), 2 );
        }
        if ( Focused ) D.RectStroke( Bounds, Th[ Tok::Accent ].Fade( 0.45f ), S.Radius );
    }

    // ------------------------------------------------------------ input

    bool CodeEditor::OnEvent( const Event& E )
    {
        Context* C = GetContext();
        switch ( E.Type )
        {
        case EventType::MouseWheel:
            if ( E.Mods & ModShift ) ScrollX = std::max( 0.f, ScrollX - E.Wheel.y * CW * 6 );
            else ScrollY -= E.Wheel.y * LH * 3;
            ScrollX -= E.Wheel.x * CW * 6;
            Redraw();
            return true;
        case EventType::MouseDown:
        {
            if ( E.Button != MouseButton::Left ) return false;
            size_t P = PosAtPoint( E.Pos );
            if ( E.Clicks == 2 ) { size_t A, B; WordAt( Text, P, A, B ); Sels.back() = { B, A, -1 }; }
            else if ( E.Clicks >= 3 ) { size_t L = LineOf( P ); Sels.back() = { L + 1 < Starts.size() ? Starts[ L + 1 ] : Text.size(), Starts[ L ], -1 }; }
            else if ( E.Mods & ModAlt ) Sels.push_back( { P, P, -1 } );
            else if ( E.Mods & ModShift ) Sels.back().Caret = P;
            else Sels.assign( 1, Sel{ P, P, -1 } );
            Normalize();
            Dragging = E.Clicks == 1;
            if ( C ) BlinkStart = C->Time();
            Redraw();
            return true;
        }
        case EventType::MouseMove:
            if ( !Dragging ) return false;
            Sels.back().Caret = PosAtPoint( E.Pos );
            ScrollToCaret();
            Redraw();
            return true;
        case EventType::MouseUp: Dragging = false; return E.Button == MouseButton::Left;
        case EventType::Text:
            if ( ReadOnly || E.Codepoint < 32 || E.Codepoint == 127 ) return E.Codepoint >= 32;
            {
                std::string S;
                uint32_t Cp = E.Codepoint;
                if ( Cp < 0x80 ) S += char( Cp );
                else if ( Cp < 0x800 ) { S += char( 0xC0 | Cp >> 6 ); S += char( 0x80 | ( Cp & 0x3F ) ); }
                else if ( Cp < 0x10000 ) { S += char( 0xE0 | Cp >> 12 ); S += char( 0x80 | ( ( Cp >> 6 ) & 0x3F ) ); S += char( 0x80 | ( Cp & 0x3F ) ); }
                else { S += char( 0xF0 | Cp >> 18 ); S += char( 0x80 | ( ( Cp >> 12 ) & 0x3F ) ); S += char( 0x80 | ( ( Cp >> 6 ) & 0x3F ) ); S += char( 0x80 | ( Cp & 0x3F ) ); }
                Insert( S );
            }
            return true;
        case EventType::KeyDown: break;
        default: return false;
        }

        const bool Ctrl = E.Mods & ModCtrl, Shift = E.Mods & ModShift;
        switch ( E.KeyCode )
        {
        case Key::Left: Move( -1, 0, Ctrl, Shift ); return true;
        case Key::Right: Move( 1, 0, Ctrl, Shift ); return true;
        case Key::Up: Move( 0, -1, false, Shift ); return true;
        case Key::Down: Move( 0, 1, false, Shift ); return true;
        case Key::PageUp: Move( 0, -std::max( 1, int( TextRect().h / LH ) - 1 ), false, Shift ); return true;
        case Key::PageDown: Move( 0, std::max( 1, int( TextRect().h / LH ) - 1 ), false, Shift ); return true;
        case Key::Home:
        case Key::End:
            for ( auto& S : Sels )
            {
                size_t L = LineOf( S.Caret );
                if ( Ctrl ) S.Caret = E.KeyCode == Key::Home ? 0 : Text.size();
                else if ( E.KeyCode == Key::End ) S.Caret = LineEnd( L );
                else // smart home: first non-blank, then column 0
                {
                    std::string Ln = Line( L );
                    size_t F = Ln.find_first_not_of( " \t" );
                    size_t Indent = Starts[ L ] + ( F == std::string::npos ? Ln.size() : F );
                    S.Caret = S.Caret == Indent ? Starts[ L ] : Indent;
                }
                if ( !Shift ) S.Anchor = S.Caret;
                S.WantCol = -1;
            }
            Normalize();
            ScrollToCaret();
            Redraw();
            return true;
        case Key::Escape:
            if ( Sels.size() > 1 ) { Sels.assign( 1, Sels.back() ); Redraw(); return true; }
            return false;
        case Key::Backspace:
        case Key::Delete:
        {
            bool Back = E.KeyCode == Key::Backspace;
            Edit( [ & ]( Sel& S, std::string&, size_t& From, size_t& To ) {
                if ( From != To ) return;
                if ( Back ) From = Ctrl ? WordLeft( Text, From ) : From > 0 ? PrevGrapheme( Text, From ) : 0;
                else To = Ctrl ? WordRight( Text, To ) : NextGrapheme( Text, To );
                (void)S;
            }, 3 );
            return true;
        }
        case Key::Enter:
        case Key::KpEnter:
            Edit( [ & ]( Sel& S, std::string& Ins, size_t& From, size_t& ) {
                // keep the indent; one more after an opening bracket / colon
                size_t L = LineOf( From );
                std::string Ln = Text.substr( Starts[ L ], From - Starts[ L ] );
                size_t F = Ln.find_first_not_of( " \t" );
                std::string Ind = Ln.substr( 0, F == std::string::npos ? Ln.size() : F );
                char Lastc = 0;
                for ( size_t i = Ln.size(); i > 0; i-- )
                    if ( Ln[ i - 1 ] != ' ' && Ln[ i - 1 ] != '\t' ) { Lastc = Ln[ i - 1 ]; break; }
                if ( Lastc == '{' || Lastc == '(' || Lastc == '[' || ( Lastc == ':' && Lang == CodeLang::Python ) )
                    Ind += InsertSpaces ? std::string( size_t( TabSize ), ' ' ) : "\t";
                Ins = "\n" + Ind;
                (void)S;
            }, 2 );
            return true;
        case Key::Tab:
        {
            bool Multi = false;
            for ( auto& S : Sels )
                if ( LineOf( S.Caret ) != LineOf( S.Anchor ) ) Multi = true;
            if ( Shift || Multi ) { if ( !ReadOnly ) Indent( Shift ); return true; }
            if ( InsertSpaces )
                Edit( [ & ]( Sel& S, std::string& Ins, size_t& From, size_t& ) {
                    int Col = ColumnOf( From );
                    Ins.assign( size_t( TabSize - Col % TabSize ), ' ' );
                    (void)S;
                }, 2 );
            else Insert( "\t" );
            return true;
        }
        default: break;
        }
        if ( !Ctrl ) return false;
        switch ( E.KeyCode )
        {
        case Key::A: Sels.assign( 1, Sel{ Text.size(), 0, -1 } ); Redraw(); return true;
        case Key::C:
        case Key::X:
        {
            std::string S = SelectedText();
            if ( S.empty() ) // nothing selected: the whole line, like most editors
            {
                size_t L = LineOf( Sels.back().Caret );
                S = Line( L ) + "\n";
                if ( E.KeyCode == Key::X && !ReadOnly )
                {
                    Sels.assign( 1, Sel{ L + 1 < Starts.size() ? Starts[ L + 1 ] : Text.size(), Starts[ L ], -1 } );
                    Edit( []( Sel&, std::string&, size_t&, size_t& ) {}, 2 );
                }
            }
            else if ( E.KeyCode == Key::X ) Edit( []( Sel&, std::string&, size_t&, size_t& ) {}, 2 );
            if ( C && C->SetClipboard ) C->SetClipboard( S );
            return true;
        }
        case Key::V:
            if ( C && C->GetClipboard )
            {
                std::string S = C->GetClipboard(), Clean;
                for ( size_t i = 0; i < S.size(); i++ )
                    if ( S[ i ] != '\r' ) Clean += S[ i ];
                Insert( Clean );
            }
            return true;
        case Key::Z: if ( Shift ) Redo(); else Undo(); return true;
        case Key::Y: Redo(); return true;
        case Key::D: AddNextOccurrence(); return true;
        case Key::Slash: if ( !ReadOnly ) ToggleComment(); return true;
        default: return false;
        }
    }
}
