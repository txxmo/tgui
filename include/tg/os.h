// tg/os.h — the desktop around your window: native file dialogs, notifications, opening links,
// images on the clipboard, and a tray icon.
//
//   auto Files = tg::os::OpenFiles( { "Open images", { { "Images", "*.png;*.jpg" } } } );
//   std::string Out = tg::os::SaveFile( { "Export", { { "CSV", "*.csv" } }, {}, "report.csv" } );
//   tg::os::OpenFilesAsync( Ctx, { "Pick a log" }, [&]( std::vector<std::string> P ) { ... } ); // UI keeps running
//   tg::os::Notify( "Backup finished", "142 files, 3.2 GB" );
//   tg::os::OpenUrl( "https://example.com" );
//
// What's native where:
//   Windows   IFileOpenDialog / IFileSaveDialog, tray balloon notifications, Shell_NotifyIcon tray with a
//             native menu, CF_DIB clipboard images, ShellExecute.
//   Linux     zenity or kdialog for dialogs (whichever is installed), notify-send (or gdbus) for
//             notifications, xdg-open, wl-paste / wl-copy or xclip for clipboard images. No tray icon
//             (StatusNotifierItem needs a D-Bus service; TrayIcon::Supported() is false).
//   macOS     osascript dialogs and notifications, `open`. No tray icon or clipboard images yet.
// Everything returns empty / false when the desktop can't do it, so it's safe to call anywhere.
#pragma once
#include "core.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tg
{
    class Context;

    // RGBA pixels, Color::Pack() order (r in the low byte), straight alpha.
    struct Bitmap
    {
        int Width = 0, Height = 0;
        std::vector<uint32_t> Pixels;
        bool Empty() const { return Width <= 0 || Height <= 0; }
    };

    // PNG / JPEG / GIF (first frame) / BMP / TGA bytes -> RGBA. MaxSide > 0 refuses larger images.
    bool DecodeImage( const void* Data, size_t Size, Bitmap& Out, int MaxSide = 16384 );
    // Resized copy (box filter when shrinking: avatars, thumbnails).
    Bitmap ScaleBitmap( const Bitmap& In, int Width, int Height );

    namespace os
    {
        struct FileFilter
        {
            std::string Name;     // "Images"
            std::string Patterns; // "*.png;*.jpg"
        };

        struct FileDialog
        {
            std::string Title;
            std::vector<FileFilter> Filters;
            std::string Folder;   // start here
            std::string Name;     // suggested file name (save)
            bool Multiple = false;
            void* Owner = nullptr; // HWND / NSWindow* to be modal over (Ctx.NativeWindow); optional
        };

        bool DialogsAvailable(); // false: no zenity / kdialog on this Linux box

        // Blocking. Empty = cancelled.
        std::vector<std::string> OpenFiles( const FileDialog& D );
        std::string OpenFile( const FileDialog& D );
        std::string SaveFile( const FileDialog& D );
        std::string PickFolder( const FileDialog& D );

        // Non-blocking: the dialog runs on a worker thread, Done runs on the UI thread (Ctx.Post).
        void OpenFilesAsync( Context& Ctx, FileDialog D, std::function<void( std::vector<std::string> )> Done );
        void SaveFileAsync( Context& Ctx, FileDialog D, std::function<void( std::string )> Done );
        void PickFolderAsync( Context& Ctx, FileDialog D, std::function<void( std::string )> Done );

        // Desktop notification (toast). False if nothing could show it.
        bool Notify( const std::string& Title, const std::string& Body );

        // A URL in the browser, or a file / folder in its default app.
        bool OpenUrl( const std::string& UrlOrPath );

        // Images on the system clipboard (screenshots, "Copy image" in a browser).
        bool GetClipboardImage( Bitmap& Out );
        bool SetClipboardImage( const Bitmap& Img );
    }

    // An icon in the system tray / notification area with a right-click menu (Windows).
    //   tg::TrayIcon Tray( "My app" );
    //   Tray.Items = { { "Show", [&] { Restore(); } }, {}, { "Quit", [&] { App.Quit(); } } };
    //   Tray.OnClick = [&] { Restore(); };
    class TrayIcon
    {
    public:
        struct Item
        {
            std::string Text;            // empty = separator
            std::function<void()> OnClick;
            bool Checked = false, Enabled = true;
        };

        explicit TrayIcon( std::string Tooltip, const Bitmap* Icon = nullptr ); // nullptr = the app's icon
        ~TrayIcon();
        TrayIcon( const TrayIcon& ) = delete;
        TrayIcon& operator=( const TrayIcon& ) = delete;

        static bool Supported();
        bool Ok() const;                 // shown?
        std::vector<Item> Items;         // the right-click menu (read when it opens)
        std::function<void()> OnClick;   // left click
        void SetTooltip( const std::string& Text );
        void SetIcon( const Bitmap& Icon );
        bool Balloon( const std::string& Title, const std::string& Body ); // a notification from this icon
        void PopupMenu();                // show the menu now (at the cursor)

        struct Impl;

    private:
        std::unique_ptr<Impl> P;
    };
}
