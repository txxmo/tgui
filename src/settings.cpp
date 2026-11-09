#include "tg/settings.h"
#include "tg/context.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <cstring>
#include <sstream>

namespace fs = std::filesystem;

namespace tg
{
    static void Collect( Widget& Root, std::vector<Widget*>& Out )
    {
        std::vector<Widget*> St = { &Root };
        while ( !St.empty() )
        {
            Widget* W = St.back();
            St.pop_back();
            if ( W->CanBind() && !W->BindIsAction() && !W->BindName().empty() ) Out.push_back( W );
            auto& K = W->Children();
            for ( auto It = K.rbegin(); It != K.rend(); ++It ) St.push_back( It->get() );
        }
    }

    std::string SaveValues( Widget& Root )
    {
        std::vector<Widget*> Ws;
        Collect( Root, Ws );
        std::string Out;
        std::map<std::string, int> Seen; // the same label twice: "Name", "Name#2"
        for ( Widget* W : Ws )
        {
            std::string N = W->BindName();
            for ( char& C : N )
                if ( C == '\t' || C == '\n' || C == '\r' ) C = ' ';
            int K = ++Seen[ N ];
            if ( K > 1 ) N += "#" + std::to_string( K );
            Out += N + '\t';
            BindValue V = W->BindGet();
            char B[ 32 ];
            for ( size_t i = 0; i < V.V.size(); i++ )
            {
                if ( i ) Out += ',';
                if ( std::isnan( V.V[ i ] ) ) Out += '-';
                else { std::snprintf( B, sizeof( B ), "%.9g", V.V[ i ] ); Out += B; }
            }
            Out += '\n';
        }
        return Out;
    }

    int LoadValues( const std::string& Text, Widget& Root )
    {
        std::vector<Widget*> Ws;
        Collect( Root, Ws );
        std::map<std::string, Widget*> ByName;
        std::map<std::string, int> Seen;
        for ( Widget* W : Ws )
        {
            std::string N = W->BindName();
            int K = ++Seen[ N ];
            ByName[ K > 1 ? N + "#" + std::to_string( K ) : N ] = W;
        }
        int Count = 0;
        std::istringstream In( Text );
        std::string Line;
        while ( std::getline( In, Line ) )
        {
            size_t Tab = Line.find( '\t' );
            if ( Tab == std::string::npos ) continue;
            auto It = ByName.find( Line.substr( 0, Tab ) );
            if ( It == ByName.end() ) continue;
            BindValue V;
            std::istringstream Vs( Line.substr( Tab + 1 ) );
            for ( std::string X; std::getline( Vs, X, ',' ); ) V.V.push_back( X == "-" ? NAN : std::atof( X.c_str() ) );
            It->second->BindSet( V );
            Count++;
        }
        return Count;
    }

    // ------------------------------------------------------------ profiles

    Profiles::Profiles( Context& C, std::string F ) : Ctx( C ), Folder( std::move( F ) )
    {
        std::error_code Ec;
        fs::create_directories( Folder, Ec );
    }

    std::string Profiles::Path( const std::string& Name ) const
    {
        std::string Safe;
        for ( char C : Name ) Safe += ( C && std::strchr( "\\/:*?\"<>|", C ) ) ? '_' : C;
        return ( fs::path( Folder ) / ( Safe + ".tgprofile" ) ).string();
    }

    std::vector<std::string> Profiles::List() const
    {
        std::vector<std::string> Out;
        std::error_code Ec;
        for ( auto& E : fs::directory_iterator( Folder, Ec ) )
            if ( E.path().extension() == ".tgprofile" ) Out.push_back( E.path().stem().string() );
        std::sort( Out.begin(), Out.end() );
        return Out;
    }

    bool Profiles::Exists( const std::string& Name ) const { std::error_code Ec; return fs::exists( Path( Name ), Ec ); }

    std::string Profiles::Export() const
    {
        return "[values]\n" + SaveValues( Ctx.Root() ) + "[binds]\n" + Ctx.Binds.Save();
    }

    bool Profiles::Import( const std::string& Text ) const
    {
        size_t V = Text.find( "[values]\n" ), B = Text.find( "[binds]\n" );
        if ( V == std::string::npos ) return false;
        size_t VEnd = B == std::string::npos ? Text.size() : B;
        LoadValues( Text.substr( V + 9, VEnd - V - 9 ), Ctx.Root() );
        if ( B != std::string::npos ) Ctx.Binds.Load( Text.substr( B + 8 ), Ctx.Root() );
        return true;
    }

    bool Profiles::Save( const std::string& Name ) const
    {
        std::ofstream F( Path( Name ), std::ios::binary );
        if ( !F ) return false;
        F << Export();
        return bool( F );
    }

    bool Profiles::Load( const std::string& Name ) const
    {
        std::ifstream F( Path( Name ), std::ios::binary );
        if ( !F ) return false;
        std::stringstream S;
        S << F.rdbuf();
        return Import( S.str() );
    }

    bool Profiles::Remove( const std::string& Name ) const { std::error_code Ec; return fs::remove( Path( Name ), Ec ); }
}
