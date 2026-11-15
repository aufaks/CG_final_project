#include <Windows.h> // GL 헤더보다 먼저 (APIENTRY 재정의 경고 방지)

#include "Render/Texture.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
// 이미지를 RGBA8로 디코딩한다(OpenGL 순서에 맞게 상하 반전). ObjectViewer.cpp의 DecodeImage와 같은 방식.
// main에서 CoInitializeEx를 호출해 둔 스레드에서만 부른다.
bool DecodeRGBA(const std::wstring& path, std::vector<BYTE>& pixels, UINT& width, UINT& height) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) return false;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return false;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;

    ComPtr<IWICBitmapFlipRotator> flipper;
    if (FAILED(factory->CreateBitmapFlipRotator(&flipper))) return false;
    if (FAILED(flipper->Initialize(converter.Get(), WICBitmapTransformFlipVertical))) return false;

    const UINT stride = width * 4;
    pixels.resize(static_cast<size_t>(stride) * height);
    return SUCCEEDED(flipper->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
}

std::wstring ToWide(const std::string& utf8) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (length <= 0) return {};
    std::wstring wide(static_cast<size_t>(length) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), length);
    return wide;
}
} // namespace

Texture::~Texture() {
    if (id) glDeleteTextures(1, &id);
}

bool Texture::Load(const std::string& path) {
    std::vector<BYTE> pixels;
    UINT w = 0, h = 0;
    if (!DecodeRGBA(ToWide(path), pixels, w, h)) {
        std::cerr << "[Texture] 불러오기 실패: " << path << "\n";
        return false;
    }

    if (id) glDeleteTextures(1, &id);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

void Texture::Bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
}
