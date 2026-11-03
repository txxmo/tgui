#include "render.h"
#include <algorithm>
#include <vector>
#include <cstring>

CRender* Draw = nullptr;

CRender::CRender(IDirect3DDevice9* device)
{
    Device = device;
    if (Device)
        Device->GetViewport(&ScreenSize);
}

bool CRender::InitDevice()
{
    if (!Device) return false;

    // Create a dynamic vertex buffer once
    HRESULT hr = Device->CreateVertexBuffer(
        sizeof(Vertex) * kMaxBatchVerts,
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        VERTEX_FVF,
        D3DPOOL_DEFAULT,
        &VertexBuffer,
        nullptr
    );
    if (FAILED(hr)) return false;

    // CPU-side staging buffer
    CPUBufferCapacity = kMaxBatchVerts;
    CPUBuffer = new Vertex[CPUBufferCapacity];
    CPUBufferCount = 0;

    // Create a full state block to capture and restore app state
    hr = Device->CreateStateBlock(D3DSBT_ALL, &StateBlock);
    if (FAILED(hr)) return false;

    // Optional: create default fonts
    // D3DXCreateFont(Device, 16, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
    //     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI", &F.Segoe);
    // D3DXCreateFont(Device, 16, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
    //     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI", &F.SegoeBold);

    return true;
}

void CRender::Shutdown()
{
    // Release only what we own
    if (F.Segoe) { F.Segoe->Release(); F.Segoe = nullptr; }
    if (F.SegoeBold) { F.SegoeBold->Release(); F.SegoeBold = nullptr; }

    if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
    if (StateBlock) { StateBlock->Release(); StateBlock = nullptr; }

    delete[] CPUBuffer; CPUBuffer = nullptr;
    CPUBufferCapacity = 0;
    CPUBufferCount = 0;

    if (Device != NULL) Device->Release();
}

void CRender::Begin()
{
    if (!Device) return;
    if (StateBlock) StateBlock->Capture();

    // Set overlay pipeline state
    Device->SetVertexShader(nullptr);
    Device->SetPixelShader(nullptr);
    Device->SetFVF(VERTEX_FVF);

    Device->SetRenderState(D3DRS_LIGHTING, FALSE);
    Device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    Device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

    Device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    Device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    Device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    Device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

    Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    Device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    Device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    Device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_INVDESTALPHA);
    Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    Device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);

    Device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    Device->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
        D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

    Device->SetTexture(0, nullptr);
    Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    Device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    Device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);

    Mode = BatchMode::None;
    CPUBufferCount = 0;
}

void CRender::End()
{
    // Flush any pending batch
    if (Mode == BatchMode::Triangles && CPUBufferCount >= 3)
        FlushTriangles((CPUBufferCount - 2));
    else if (Mode == BatchMode::Lines && CPUBufferCount >= 2)
        FlushLines(CPUBufferCount / 2);

    // Restore app state
    if (StateBlock) StateBlock->Apply();
    Mode = BatchMode::None;
    CPUBufferCount = 0;
}

void CRender::SetViewport(const D3DVIEWPORT9& vp)
{
    if (!Device) return;
    Device->SetViewport(&vp);
    ScreenSize = vp;
}

D3DVIEWPORT9 CRender::GetViewport() const
{
    D3DVIEWPORT9 vp{};
    if (Device) Device->GetViewport(&vp);
    return vp;
}

// ------------------------------
// Batch helpers
// ------------------------------
void CRender::PushVertex(const Vertex& v)
{
    if (CPUBufferCount >= CPUBufferCapacity) {
        // If buffer is full, flush based on current mode
        if (Mode == BatchMode::Triangles && CPUBufferCount >= 3)
            FlushTriangles((CPUBufferCount - 2));
        else if (Mode == BatchMode::Lines && CPUBufferCount >= 2)
            FlushLines(CPUBufferCount / 2);
        CPUBufferCount = 0;
    }
    CPUBuffer[CPUBufferCount++] = v;
}

void CRender::FlushTriangles(size_t triStripSegments)
{
    if (!Device || !VertexBuffer || CPUBufferCount < 3) return;

    // Lock and copy data
    void* vbPtr = nullptr;
    if (FAILED(VertexBuffer->Lock(0, sizeof(Vertex) * CPUBufferCount, &vbPtr, D3DLOCK_DISCARD)))
        return;
    std::memcpy(vbPtr, CPUBuffer, sizeof(Vertex) * CPUBufferCount);
    VertexBuffer->Unlock();

    Device->SetStreamSource(0, VertexBuffer, 0, sizeof(Vertex));
    Device->SetFVF(VERTEX_FVF);
    Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, static_cast<UINT>(triStripSegments));

    CPUBufferCount = 0;
    Mode = BatchMode::None;
}

void CRender::FlushLines(size_t lineCount)
{
    if (!Device || !VertexBuffer || CPUBufferCount < 2) return;

    void* vbPtr = nullptr;
    if (FAILED(VertexBuffer->Lock(0, sizeof(Vertex) * CPUBufferCount, &vbPtr, D3DLOCK_DISCARD)))
        return;
    std::memcpy(vbPtr, CPUBuffer, sizeof(Vertex) * CPUBufferCount);
    VertexBuffer->Unlock();

    Device->SetStreamSource(0, VertexBuffer, 0, sizeof(Vertex));
    Device->SetFVF(VERTEX_FVF);
    Device->DrawPrimitive(D3DPT_LINELIST, 0, static_cast<UINT>(lineCount));

    CPUBufferCount = 0;
    Mode = BatchMode::None;
}

// ------------------------------
// Drawing primitives
// ------------------------------
void CRender::Rect(Vector2D pos, Vector2D size, Color c)
{
    Vector2D a = pos;
    Vector2D b = pos + size;

    D3DCOLOR col = D3DCOLOR_RGBA(c.r(), c.g(), c.b(), c.a());

    Vertex verts[5] = {
        { a.x, a.y, 0.0f, 1.0f, col },
        { b.x, a.y, 0.0f, 1.0f, col },
        { b.x, b.y, 0.0f, 1.0f, col },
        { a.x, b.y, 0.0f, 1.0f, col },
        { a.x, a.y, 0.0f, 1.0f, col }
    };

    // Lines: we need linelist, so convert the strip into segments
    // Segments: (0-1), (1-2), (2-3), (3-4)
    Mode = BatchMode::Lines;
    for (int i = 0; i < 4; ++i) {
        PushVertex(verts[i]);
        PushVertex(verts[i + 1]);
    }
    FlushLines(4);
}

void CRender::RectFilled(Vector2D pos, Vector2D size, Color c)
{
    Vector2D a = pos;
    Vector2D b = pos + size;
    D3DCOLOR col = D3DCOLOR_RGBA(c.r(), c.g(), c.b(), c.a());

    Vertex verts[4] = {
        { a.x, a.y, 0.0f, 1.0f, col },
        { b.x, a.y, 0.0f, 1.0f, col },
        { a.x, b.y, 0.0f, 1.0f, col },
        { b.x, b.y, 0.0f, 1.0f, col },
    };

    Mode = BatchMode::Triangles;
    // Triangle strip with 2 segments
    PushVertex(verts[0]);
    PushVertex(verts[1]);
    PushVertex(verts[2]);
    PushVertex(verts[3]);
    FlushTriangles(2);
}

void CRender::Line(Vector2D startPos, Vector2D endPos, Color c)
{
    D3DCOLOR col = D3DCOLOR_RGBA(c.r(), c.g(), c.b(), c.a());
    Vertex verts[2] = {
        { startPos.x, startPos.y, 0.0f, 1.0f, col },
        { endPos.x,   endPos.y,   0.0f, 1.0f, col }
    };

    Mode = BatchMode::Lines;
    PushVertex(verts[0]);
    PushVertex(verts[1]);
    FlushLines(1);
}

void CRender::Gradient(Vector2D pos, Vector2D size, Color c1, Color c2, bool horizontal)
{
    Vector2D a = pos;
    Vector2D b = pos + size;

    D3DCOLOR col1 = D3DCOLOR_RGBA(c1.r(), c1.g(), c1.b(), c1.a());
    D3DCOLOR col2 = D3DCOLOR_RGBA(c2.r(), c2.g(), c2.b(), c2.a());

    Vertex v[4];
    if (!horizontal) {
        v[0] = { a.x, a.y, 0.0f, 1.0f, col1 };
        v[1] = { b.x, a.y, 0.0f, 1.0f, col1 };
        v[2] = { a.x, b.y, 0.0f, 1.0f, col2 };
        v[3] = { b.x, b.y, 0.0f, 1.0f, col2 };
    }
    else {
        v[0] = { a.x, a.y, 0.0f, 1.0f, col1 };
        v[1] = { b.x, a.y, 0.0f, 1.0f, col2 };
        v[2] = { a.x, b.y, 0.0f, 1.0f, col1 };
        v[3] = { b.x, b.y, 0.0f, 1.0f, col2 };
    }

    Mode = BatchMode::Triangles;
    PushVertex(v[0]); PushVertex(v[1]); PushVertex(v[2]); PushVertex(v[3]);
    FlushTriangles(2);
}

// ------------------------------
// Text
// ------------------------------
int CRender::GetTextWidth(const char* text, LPD3DXFONT font)
{
    if (!font || !text) return 0;
    RECT rc{ 0,0,0,0 };
    font->DrawTextA(nullptr, text, static_cast<int>(std::strlen(text)), &rc, DT_CALCRECT, D3DCOLOR_XRGB(0, 0, 0));
    return rc.right - rc.left;
}

void CRender::Text(Vector2D pos, LPD3DXFONT font, const std::string& text, Color c, bool center, int shadow, Color sc)
{
    if (!font || text.empty()) return;

    if (center)
        pos.x -= (GetTextWidth(text.c_str(), font) / 2);

    pos.x = floor(pos.x + 0.5f);
    pos.y = floor(pos.y + 0.5f);

    // Large rect to avoid clipping; you can tailor this based on viewport
    RECT rc;
    SetRect(&rc, static_cast<int>(pos.x), static_cast<int>(pos.y),
        static_cast<int>(pos.x + 4096), static_cast<int>(pos.y + 1024));

    auto draw = [&](int ox, int oy, D3DCOLOR col) {
        RECT r; SetRect(&r, rc.left + ox, rc.top + oy, rc.right + ox, rc.bottom + oy);
        font->DrawTextA(nullptr, text.c_str(), -1, &r, DT_NOCLIP, col);
        };

    D3DCOLOR mainCol = D3DCOLOR_RGBA(c.r(), c.g(), c.b(), c.a());
    D3DCOLOR shadowCol = D3DCOLOR_RGBA(sc.r(), sc.g(), sc.b(), sc.a());

    if (shadow == 1) {
        draw(1, 0, shadowCol);
        draw(0, 1, shadowCol);
    }
    else if (shadow == 2) {
        draw(1, 0, shadowCol);
        draw(-1, 1, shadowCol);
        draw(0, -1, shadowCol);
        draw(1, 1, shadowCol);
    }

    draw(0, 0, mainCol);
}

void CRender::SetClipRect(const RECT& rc)
{
    if (!Device) return;
    Device->SetScissorRect(&rc);
}

void CRender::ResetClip()
{
    if (!Device) return;

    RECT rc;
    rc.left = ScreenSize.X;
    rc.top = ScreenSize.Y;
    rc.right = ScreenSize.X + ScreenSize.Width;
    rc.bottom = ScreenSize.Y + ScreenSize.Height;

    Device->SetScissorRect(&rc);
}