#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include "../math/math.h"
#include "color.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

struct Vertex
{
    float x, y, z, w;   // Transformed vertices
    D3DCOLOR color;
};

#define VERTEX_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

// Tune these for your expected peak batch size
static const size_t kMaxBatchVerts = 4096;

class CRender
{
public:
    CRender(IDirect3DDevice9* device);
    CRender() = default;

    bool InitDevice();
    void Shutdown();

    // Optional per-frame scope
    void Begin();
    void End();

    // Drawing
    void Rect(Vector2D pos, Vector2D size, Color c);
    void RectFilled(Vector2D pos, Vector2D size, Color c);
    void Line(Vector2D startPos, Vector2D endPos, Color c);
    void Gradient(Vector2D pos, Vector2D size, Color c1, Color c2, bool horizontal = false);

    void Text(Vector2D pos, LPD3DXFONT font, const std::string& text, Color c, bool center = false, int shadow = 0, Color sc = Color());
    int GetTextWidth(const char* text, LPD3DXFONT font);

    // Viewport
    void SetViewport(const D3DVIEWPORT9& vp);
    D3DVIEWPORT9 GetViewport() const;

    void SetClipRect(const RECT& rc);
    void ResetClip();

public:
    struct
    {
        LPD3DXFONT Segoe = nullptr;
        LPD3DXFONT SegoeBold = nullptr;
    } F;

private:
    // batching helpers
    void FlushTriangles(size_t triCount);
    void FlushLines(size_t lineCount);
    void PushVertex(const Vertex& v);

private:
    IDirect3DDevice9* Device = nullptr;
    D3DVIEWPORT9 ScreenSize{};

    // resources we own
    IDirect3DVertexBuffer9* VertexBuffer = nullptr;
    IDirect3DStateBlock9* StateBlock = nullptr;

    // CPU-side batch buffer
    Vertex* CPUBuffer = nullptr;
    size_t CPUBufferCapacity = 0;
    size_t CPUBufferCount = 0;

    // draw mode tracking: TRIANGLESTRIP or LINELIST
    enum class BatchMode { None, Triangles, Lines } Mode = BatchMode::None;
};

extern CRender* Draw;
