// Screenshot of a screen area through DXGI desktop duplication: it sees what the display shows, including GL and
// Direct3D windows that GDI copies (BitBlt, PrintWindow) return black. Saves a PNG through WIC.
// usage: dupshot.exe <out.png> [x y w h]   (physical pixels of the primary output; default the whole output)
// build: cl /O2 /EHsc dupshot.cpp d3d11.lib dxgi.lib windowscodecs.lib ole32.lib
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

template <class T>
static void Release(T*& p)
{
    if (p) p->Release();
    p = nullptr;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 && argc != 6) {
        std::printf("usage: dupshot <out.png> [x y w h]\n");
        return 2;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev,
                                 nullptr, &ctx))) {
        std::printf("no D3D11 device\n");
        return 1;
    }
    IDXGIDevice* dxgiDev = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIOutput* output = nullptr;
    IDXGIOutput1* output1 = nullptr;
    IDXGIOutputDuplication* dup = nullptr;
    dev->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDev));
    dxgiDev->GetAdapter(&adapter);
    if (FAILED(adapter->EnumOutputs(0, &output)) ||
        FAILED(output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&output1))) ||
        FAILED(output1->DuplicateOutput(dev, &dup))) {
        std::printf("desktop duplication unavailable\n");
        return 1;
    }
    DXGI_OUTDUPL_DESC desc;
    dup->GetDesc(&desc);
    const int W = static_cast<int>(desc.ModeDesc.Width), H = static_cast<int>(desc.ModeDesc.Height);

    // the first frames can be empty: take the first one that carries an image
    ID3D11Texture2D* frame = nullptr;
    for (int attempt = 0; attempt < 20 && !frame; ++attempt) {
        DXGI_OUTDUPL_FRAME_INFO info;
        IDXGIResource* res = nullptr;
        if (FAILED(dup->AcquireNextFrame(500, &info, &res))) continue;
        if (info.LastPresentTime.QuadPart)
            res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frame));
        Release(res);
        if (!frame) dup->ReleaseFrame();
    }
    if (!frame) {
        std::printf("no frame\n");
        return 1;
    }
    D3D11_TEXTURE2D_DESC td;
    frame->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    td.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    dev->CreateTexture2D(&td, nullptr, &staging);
    ctx->CopyResource(staging, frame);
    dup->ReleaseFrame();

    int x = 0, y = 0, w = W, h = H;
    if (argc == 6) {
        x = _wtoi(argv[2]);
        y = _wtoi(argv[3]);
        w = _wtoi(argv[4]);
        h = _wtoi(argv[5]);
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    D3D11_MAPPED_SUBRESOURCE map;
    ctx->Map(staging, 0, D3D11_MAP_READ, 0, &map);
    std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
    for (int row = 0; row < h; ++row)
        std::memcpy(&pixels[static_cast<size_t>(row) * w * 4],
                    static_cast<unsigned char*>(map.pData) + static_cast<size_t>(y + row) * map.RowPitch + x * 4,
                    static_cast<size_t>(w) * 4);
    ctx->Unmap(staging, 0);

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* wic = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* fe = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
    wic->CreateStream(&stream);
    if (FAILED(stream->InitializeFromFilename(argv[1], GENERIC_WRITE))) {
        std::printf("cannot write the file\n");
        return 1;
    }
    wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
    enc->Initialize(stream, WICBitmapEncoderNoCache);
    enc->CreateNewFrame(&fe, nullptr);
    fe->Initialize(nullptr);
    fe->SetSize(w, h);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    fe->SetPixelFormat(&fmt);
    fe->WritePixels(h, w * 4, static_cast<UINT>(pixels.size()), pixels.data());
    fe->Commit();
    enc->Commit();
    std::printf("%dx%d at %d,%d\n", w, h, x, y);
    Release(fe);
    Release(enc);
    Release(stream);
    Release(wic);
    Release(staging);
    Release(frame);
    Release(dup);
    Release(output1);
    Release(output);
    Release(adapter);
    Release(dxgiDev);
    Release(ctx);
    Release(dev);
    return 0;
}
