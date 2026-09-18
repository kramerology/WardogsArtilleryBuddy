#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

namespace
{
using namespace winrt;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

struct Image
{
    int width{};
    int height{};
    int stride{};
    std::vector<std::uint8_t> pixels;
};

Image LoadPng(const std::wstring& path)
{
    const auto file = StorageFile::GetFileFromPathAsync(path).get();
    const auto stream = file.OpenReadAsync().get();
    const auto decoder = BitmapDecoder::CreateAsync(stream).get();
    const auto pixels = decoder.GetPixelDataAsync(
        BitmapPixelFormat::Bgra8,
        BitmapAlphaMode::Ignore,
        BitmapTransform{},
        ExifOrientationMode::IgnoreExifOrientation,
        ColorManagementMode::DoNotColorManage).get();
    auto bytes = pixels.DetachPixelData();

    Image image;
    image.width = static_cast<int>(decoder.PixelWidth());
    image.height = static_cast<int>(decoder.PixelHeight());
    image.stride = image.width * 4;
    image.pixels.assign(bytes.begin(), bytes.end());
    return image;
}

Image Crop(const Image& source, double leftFraction, double topFraction,
           double rightFraction, double bottomFraction)
{
    const int left = std::clamp(static_cast<int>(source.width * leftFraction),
                                0, source.width - 1);
    const int top = std::clamp(static_cast<int>(source.height * topFraction),
                               0, source.height - 1);
    const int right = std::clamp(static_cast<int>(source.width * rightFraction),
                                 left + 1, source.width);
    const int bottom = std::clamp(static_cast<int>(source.height * bottomFraction),
                                  top + 1, source.height);

    Image image;
    image.width = right - left;
    image.height = bottom - top;
    image.stride = image.width * 4;
    image.pixels.resize(static_cast<size_t>(image.stride) * image.height);
    for (int y = 0; y < image.height; ++y)
    {
        const auto* sourceRow = source.pixels.data() +
            static_cast<size_t>(top + y) * source.stride +
            static_cast<size_t>(left) * 4;
        auto* destinationRow = image.pixels.data() +
            static_cast<size_t>(y) * image.stride;
        std::copy_n(sourceRow, image.stride, destinationRow);
    }
    return image;
}

enum class Mode
{
    Original,
    Bright,
    Dark,
    LocalDark,
};

Image Prepare(const Image& source, Mode mode, int scale)
{
    Image prepared;
    prepared.width = source.width * scale;
    prepared.height = source.height * scale;
    prepared.stride = prepared.width * 4;
    prepared.pixels.resize(static_cast<size_t>(prepared.stride) * prepared.height);

    std::vector<std::uint8_t> luminances;
    std::vector<int> integral;
    if (mode == Mode::LocalDark)
    {
        luminances.resize(static_cast<size_t>(source.width) * source.height);
        integral.resize(static_cast<size_t>(source.width + 1) * (source.height + 1));
        for (int y = 0; y < source.height; ++y)
        {
            int rowSum = 0;
            for (int x = 0; x < source.width; ++x)
            {
                const auto* pixel = source.pixels.data() +
                    static_cast<size_t>(y) * source.stride + static_cast<size_t>(x) * 4;
                const int luminance = (pixel[2] * 299 + pixel[1] * 587 + pixel[0] * 114) / 1000;
                luminances[static_cast<size_t>(y) * source.width + x] =
                    static_cast<std::uint8_t>(luminance);
                rowSum += luminance;
                integral[static_cast<size_t>(y + 1) * (source.width + 1) + x + 1] =
                    integral[static_cast<size_t>(y) * (source.width + 1) + x + 1] + rowSum;
            }
        }
    }

    for (int y = 0; y < prepared.height; ++y)
    {
        const int sourceY = y / scale;
        for (int x = 0; x < prepared.width; ++x)
        {
            const int sourceX = x / scale;
            const auto* sourcePixel = source.pixels.data() +
                static_cast<size_t>(sourceY) * source.stride + static_cast<size_t>(sourceX) * 4;
            auto* destinationPixel = prepared.pixels.data() +
                static_cast<size_t>(y) * prepared.stride + static_cast<size_t>(x) * 4;

            int luminance = (sourcePixel[2] * 299 + sourcePixel[1] * 587 + sourcePixel[0] * 114) / 1000;
            bool foreground = false;
            if (mode == Mode::Bright)
            {
                foreground = luminance >= 180;
            }
            else if (mode == Mode::Dark)
            {
                foreground = luminance <= 105;
            }
            else if (mode == Mode::LocalDark)
            {
                constexpr int radius = 7;
                const int left = std::max(0, sourceX - radius);
                const int top = std::max(0, sourceY - radius);
                const int right = std::min(source.width - 1, sourceX + radius);
                const int bottom = std::min(source.height - 1, sourceY + radius);
                const auto at = [&integral, width = source.width](int px, int py)
                {
                    return integral[static_cast<size_t>(py) * (width + 1) + px];
                };
                const int sum = at(right + 1, bottom + 1) - at(left, bottom + 1) -
                    at(right + 1, top) + at(left, top);
                const int area = (right - left + 1) * (bottom - top + 1);
                foreground = luminance + 18 < (area > 0 ? sum / area : luminance);
            }

            const std::uint8_t value = mode == Mode::Original
                ? 0
                : static_cast<std::uint8_t>(foreground ? 255 : 0);
            if (mode == Mode::Original)
            {
                destinationPixel[0] = sourcePixel[0];
                destinationPixel[1] = sourcePixel[1];
                destinationPixel[2] = sourcePixel[2];
            }
            else
            {
                destinationPixel[0] = value;
                destinationPixel[1] = value;
                destinationPixel[2] = value;
            }
            destinationPixel[3] = 255;
        }
    }
    return prepared;
}

SoftwareBitmap ToBitmap(const Image& image)
{
    Buffer buffer(static_cast<std::uint32_t>(image.pixels.size()));
    buffer.Length(static_cast<std::uint32_t>(image.pixels.size()));
    auto access = buffer.as<winrt::impl::IBufferByteAccess>();
    std::uint8_t* destination = nullptr;
    winrt::check_hresult(access->Buffer(&destination));
    std::copy(image.pixels.begin(), image.pixels.end(), destination);
    return SoftwareBitmap::CreateCopyFromBuffer(
        buffer, BitmapPixelFormat::Bgra8, image.width, image.height,
        BitmapAlphaMode::Ignore);
}

std::string Narrow(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::wcerr << L"usage: windows_ocr_probe.exe <png>\n";
        return 2;
    }

    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    const auto source = LoadPng(argv[1]);
    const auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (engine == nullptr)
    {
        std::wcerr << L"No Windows OCR engine available.\n";
        return 3;
    }

    struct CropSpec { const char* name; double left; double top; double right; double bottom; };
    const CropSpec crops[] = {
        {"broad", 0.20, 0.08, 0.80, 0.92},
        {"center", 0.40, 0.34, 0.62, 0.64},
        {"labels", 0.46, 0.40, 0.56, 0.56},
    };
    const std::pair<const char*, Mode> modes[] = {
        {"original", Mode::Original}, {"bright", Mode::Bright},
        {"dark", Mode::Dark}, {"local", Mode::LocalDark},
    };

    for (const auto& cropSpec : crops)
    {
        const auto crop = Crop(source, cropSpec.left, cropSpec.top,
                                cropSpec.right, cropSpec.bottom);
        std::wstring combined;
        for (const auto& mode : modes)
        {
            const auto prepared = Prepare(crop, mode.second, 2);
            const auto result = engine.RecognizeAsync(ToBitmap(prepared)).get();
            const std::wstring recognized = result.Text().c_str();
            std::cout << cropSpec.name << "/" << mode.first << ": "
                      << Narrow(recognized) << "\n";
            if (!combined.empty() && !recognized.empty())
            {
                combined += L"\n";
            }
            combined += recognized;
            if (std::string(cropSpec.name) == "broad")
            {
                for (const auto& line : result.Lines())
                {
                    for (const auto& word : line.Words())
                    {
                        const auto rect = word.BoundingRect();
                        std::cout << "  word '" << Narrow(word.Text().c_str())
                                  << "' @ " << rect.X << "," << rect.Y << " "
                                  << rect.Width << "x" << rect.Height << "\n";
                    }
                }
            }
        }
        const std::wstring number = LR"([-+]?(?:\d+(?:\.\d*)?|\.\d+))";
        const std::wregex xPattern(L"[xX]\\s*[:=]?\\s*(" + number + L")");
        const std::wregex yPattern(L"[yY]\\s*[:=]?\\s*(" + number + L")");
        std::wsmatch xMatch;
        std::wsmatch yMatch;
        std::cout << cropSpec.name << "/combined: ";
        if (std::regex_search(combined, xMatch, xPattern) &&
            std::regex_search(combined, yMatch, yPattern))
        {
            std::cout << "x=" << Narrow(xMatch[1].str())
                      << " y=" << Narrow(yMatch[1].str());
        }
        else
        {
            std::cout << "no pair";
        }
        std::cout << "\n";
    }
    return 0;
}
