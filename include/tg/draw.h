// tg/draw.h — DrawList: what widgets draw into, what backends consume.
//
// Everything becomes textured, colored triangles. Shapes sample a single white
// texel in the font atlas, so a whole UI is usually 1 texture + a few clip rects
// = a handful of draw calls. Edges are anti-aliased with a 1px alpha fringe.
#pragma once
#include "core.h"
#include <string_view>
#include <vector>

namespace tg
{
    class Font;
    struct Glyph;

    struct Vertex
    {
        Vec2 Pos;
        Vec2 UV;
        uint32_t Col; // Color::Pack()
    };

    struct DrawCmd
    {
        Rect Clip;          // scissor rect in pixels
        TextureId Texture;  // what to sample
        uint32_t IdxOffset; // first index in DrawList::Idx
        uint32_t IdxCount;
    };

    enum Corners : uint8_t
    {
        CornerTL = 1, CornerTR = 2, CornerBR = 4, CornerBL = 8,
        CornersTop = CornerTL | CornerTR, CornersBottom = CornerBL | CornerBR,
        CornersAll = 15,
    };

    class DrawList
    {
    public:
        std::vector<Vertex> Vtx;
        std::vector<uint32_t> Idx;
        std::vector<DrawCmd> Cmds;

        // Set up by Context each frame.
        TextureId AtlasTexture = 0;
        Vec2 WhiteUV;
        Rect Viewport;
        float Scale = 1; // UI units -> pixels; used to snap text to the pixel grid

        void Reset( Rect Viewport, float Scale, TextureId Atlas, Vec2 WhiteUV );

        // ---- clipping ----
        void PushClip( Rect r, bool IntersectWithCurrent = true );
        void PopClip();
        Rect CurrentClip() const { return ClipStack.empty() ? Viewport : ClipStack.back(); }

        // ---- shapes ----
        void RectFilled( Rect r, Color c, float Radius = 0, uint8_t Corners = CornersAll );
        void RectGradient( Rect r, Color Top, Color Bottom, float Radius = 0, uint8_t Corners = CornersAll );
        // Square-cornered rect with a color per corner (color pickers, hue bars).
        void RectMultiColor( Rect r, Color TL, Color TR, Color BR, Color BL );
        void RectStroke( Rect r, Color c, float Radius = 0, float Thickness = 1, uint8_t Corners = CornersAll );
        void CircleFilled( Vec2 Center, float Radius, Color c );
        void CircleStroke( Vec2 Center, float Radius, Color c, float Thickness = 1 );
        void Line( Vec2 a, Vec2 b, Color c, float Thickness = 1 );
        void Polyline( const Vec2* Pts, int Count, Color c, float Thickness = 1, bool Closed = false );
        void ConvexFill( const Vec2* Pts, int Count, Color c ); // points clockwise on screen
        // Area between a polyline (left->right) and a horizontal baseline, vertical gradient. For charts.
        void AreaFill( const Vec2* Pts, int Count, float BaseY, Color Top, Color Bottom );
        // Soft drop shadow behind a rounded rect (cheap: a few expanding translucent rings).
        void Shadow( Rect r, float Radius, Color c, float Spread = 8, Vec2 Offset = { 0, 2 } );

        // ---- images / text ----
        void Image( TextureId Tex, Rect r, Vec2 UV0 = { 0, 0 }, Vec2 UV1 = { 1, 1 }, Color Tint = { 255, 255, 255 }, float Radius = 0 );
        // Draws UTF-8 text. WrapWidth > 0 wraps on word boundaries. Returns the size it covered.
        Vec2 Text( const Font& F, Vec2 Pos, std::string_view Str, Color c, float WrapWidth = 0 );
        // One glyph with its pen at Pos.x and the top of its line at Pos.y (grids: terminals, editors).
        void Glyph( const Font& F, uint32_t Codepoint, Vec2 Pos, Color c );
        void Glyph( const Font& F, const tg::Glyph& G, Vec2 Pos, Color c ); // e.g. from Font::FindSequence

        // ---- path helpers (used by the shape functions; public so custom widgets can use them) ----
        std::vector<Vec2> Path;
        void PathRoundedRect( Rect r, float Radius, uint8_t Corners = CornersAll );
        void PathArc( Vec2 Center, float Radius, float A0, float A1, int Segments = 0 );

        // Multiply the alpha of everything drawn since VtxStart (fade a whole subtree: tab transitions).
        void FadeFrom( size_t VtxStart, float Alpha );
        // Scale / rotate / move everything drawn since VtxStart around Center (open/close animations).
        // Clip rects of commands added since CmdStart follow the scale and offset (they can't rotate).
        void TransformFrom( size_t VtxStart, size_t CmdStart, Vec2 Center, float Scale, float Angle = 0, Vec2 Offset = {} );
        // Fill the part of a convex polygon (clockwise) that lies inside a rounded rect: ripples,
        // checkerboards in rounded swatches.
        void ConvexFillInRounded( const Vec2* Pts, int Count, Color c, Rect R, float Radius );

        // Recolor vertices [VtxStart, end) with a vertical gradient, keeping each vertex's alpha factor.
        void ShadeVertical( size_t VtxStart, float Y0, float Y1, Color Top, Color Bottom );

    private:
        std::vector<Rect> ClipStack;
        void SetTexture( TextureId Tex );
        void Tri( uint32_t a, uint32_t b, uint32_t c );
        void PrimQuadUV( Vec2 a, Vec2 c, Vec2 uva, Vec2 uvc, uint32_t col );
        // AA convex fill; if Tex != 0, UVs are mapped from UVRect -> [UV0, UV1].
        void ConvexFillImpl( const Vec2* Pts, int Count, Color c, TextureId Tex, const Rect* UVRect, Vec2 UV0, Vec2 UV1 );
        bool Culled( const Rect& r ) const;
        std::vector<Vec2> Normals;
    };
}
