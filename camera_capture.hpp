#pragma once

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

template <typename T>
class ComHandle {
public:
    ComHandle() = default;
    ~ComHandle() { reset(); }
    ComHandle(const ComHandle&) = delete;
    ComHandle& operator=(const ComHandle&) = delete;

    T** put() {
        reset();
        return &pointer_;
    }

    T* get() const { return pointer_; }
    T* operator->() const { return pointer_; }
    explicit operator bool() const { return pointer_ != nullptr; }

    void reset() {
        if (pointer_ != nullptr) {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }

private:
    T* pointer_ = nullptr;
};

class CameraCapture {
public:
    static void capture_async(const wchar_t* status_tag) {
        auto* request = new std::wstring(status_tag);
        HANDLE thread = CreateThread(nullptr, 0, &CameraCapture::thread_entry, request, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        } else {
            delete request;
        }
    }

    static bool capture_blocking(const std::wstring& status_tag) {
        if (busy_.exchange(true)) {
            return false;
        }
        const bool com_ready = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        const bool media_ready = SUCCEEDED(MFStartup(MF_VERSION));
        bool saved = false;
        if (media_ready) {
            saved = grab_and_save(status_tag);
            MFShutdown();
        }
        if (com_ready) {
            CoUninitialize();
        }
        busy_.store(false);
        return saved;
    }

private:
    static constexpr int kWarmupFrames = 8;
    static constexpr int kMaxReads = 60;

    struct Frame {
        UINT32 width = 0;
        UINT32 height = 0;
        std::vector<std::uint8_t> top_down_bgr32;
    };

    static DWORD WINAPI thread_entry(LPVOID parameter) {
        std::unique_ptr<std::wstring> request(static_cast<std::wstring*>(parameter));
        capture_blocking(*request);
        return 0;
    }

    static bool grab_and_save(const std::wstring& status_tag) {
        Frame frame;
        if (!read_frame(frame)) {
            return false;
        }
        return write_jpeg(frame, build_path(status_tag));
    }

    static bool open_reader(ComHandle<IMFSourceReader>& reader) {
        ComHandle<IMFAttributes> search;
        if (FAILED(MFCreateAttributes(search.put(), 1))) {
            return false;
        }
        search->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        IMFActivate** devices = nullptr;
        UINT32 device_count = 0;
        if (FAILED(MFEnumDeviceSources(search.get(), &devices, &device_count)) ||
            device_count == 0) {
            return false;
        }
        ComHandle<IMFMediaSource> source;
        const HRESULT activated = devices[0]->ActivateObject(IID_PPV_ARGS(source.put()));
        for (UINT32 index = 0; index < device_count; ++index) {
            devices[index]->Release();
        }
        CoTaskMemFree(devices);
        if (FAILED(activated)) {
            return false;
        }
        ComHandle<IMFAttributes> reader_options;
        if (FAILED(MFCreateAttributes(reader_options.put(), 1))) {
            return false;
        }
        reader_options->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        return SUCCEEDED(
            MFCreateSourceReaderFromMediaSource(source.get(), reader_options.get(), reader.put()));
    }

    static bool request_rgb32(IMFSourceReader* reader) {
        ComHandle<IMFMediaType> wanted;
        if (FAILED(MFCreateMediaType(wanted.put()))) {
            return false;
        }
        wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        wanted->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        return SUCCEEDED(
            reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, wanted.get()));
    }

    static bool read_frame(Frame& frame) {
        ComHandle<IMFSourceReader> reader;
        if (!open_reader(reader) || !request_rgb32(reader.get())) {
            return false;
        }
        ComHandle<IMFMediaType> actual;
        if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, actual.put()))) {
            return false;
        }
        if (FAILED(MFGetAttributeSize(actual.get(), MF_MT_FRAME_SIZE, &frame.width, &frame.height)) ||
            frame.width == 0 || frame.height == 0) {
            return false;
        }
        LONG stride = 0;
        UINT32 stored_stride = 0;
        if (SUCCEEDED(actual->GetUINT32(MF_MT_DEFAULT_STRIDE, &stored_stride))) {
            stride = static_cast<LONG>(stored_stride);
        } else if (FAILED(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1, frame.width,
                                                          &stride))) {
            return false;
        }
        ComHandle<IMFSample> kept;
        int delivered = 0;
        for (int attempt = 0; attempt < kMaxReads && delivered < kWarmupFrames; ++attempt) {
            DWORD flags = 0;
            ComHandle<IMFSample> sample;
            if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags,
                                          nullptr, sample.put())) ||
                (flags & MF_SOURCE_READERF_ERROR) != 0) {
                return false;
            }
            if (sample) {
                ++delivered;
                kept.reset();
                *kept.put() = sample.get();
                kept->AddRef();
            }
        }
        return kept && copy_top_down(kept.get(), stride, frame);
    }

    static bool copy_top_down(IMFSample* sample, LONG stride, Frame& frame) {
        ComHandle<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(buffer.put()))) {
            return false;
        }
        BYTE* data = nullptr;
        DWORD length = 0;
        if (FAILED(buffer->Lock(&data, nullptr, &length))) {
            return false;
        }
        const std::size_t row_bytes = static_cast<std::size_t>(frame.width) * 4;
        const std::size_t pitch = static_cast<std::size_t>(stride < 0 ? -stride : stride);
        const bool bottom_up = stride < 0;
        bool complete = pitch >= row_bytes &&
                        length >= static_cast<DWORD>(pitch * frame.height);
        if (complete) {
            frame.top_down_bgr32.resize(row_bytes * frame.height);
            for (UINT32 row = 0; row < frame.height; ++row) {
                const UINT32 source_row = bottom_up ? frame.height - 1 - row : row;
                const BYTE* from = data + static_cast<std::size_t>(source_row) * pitch;
                std::uint8_t* to = frame.top_down_bgr32.data() + static_cast<std::size_t>(row) * row_bytes;
                std::copy(from, from + row_bytes, to);
            }
        }
        buffer->Unlock();
        return complete;
    }

    static bool write_jpeg(const Frame& frame, const std::wstring& path) {
        if (path.empty()) {
            return false;
        }
        ComHandle<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(factory.put())))) {
            return false;
        }
        ComHandle<IWICBitmap> bitmap;
        if (FAILED(factory->CreateBitmapFromMemory(
                frame.width, frame.height, GUID_WICPixelFormat32bppBGR, frame.width * 4,
                static_cast<UINT>(frame.top_down_bgr32.size()),
                const_cast<BYTE*>(frame.top_down_bgr32.data()), bitmap.put()))) {
            return false;
        }
        ComHandle<IWICBitmapSource> converted;
        if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat24bppBGR, bitmap.get(),
                                          converted.put()))) {
            return false;
        }
        ComHandle<IWICStream> stream;
        ComHandle<IWICBitmapEncoder> encoder;
        ComHandle<IWICBitmapFrameEncode> frame_encoder;
        ComHandle<IPropertyBag2> options;
        if (FAILED(factory->CreateStream(stream.put())) ||
            FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) ||
            FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, encoder.put())) ||
            FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache)) ||
            FAILED(encoder->CreateNewFrame(frame_encoder.put(), options.put())) ||
            FAILED(frame_encoder->Initialize(options.get())) ||
            FAILED(frame_encoder->SetSize(frame.width, frame.height))) {
            return false;
        }
        WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
        if (FAILED(frame_encoder->SetPixelFormat(&format)) ||
            FAILED(frame_encoder->WriteSource(converted.get(), nullptr)) ||
            FAILED(frame_encoder->Commit()) || FAILED(encoder->Commit())) {
            return false;
        }
        return true;
    }

    static std::wstring build_path(const std::wstring& status_tag) {
        wchar_t module_path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            return {};
        }
        std::wstring folder(module_path, length);
        folder.erase(folder.find_last_of(L'\\'));
        folder += L"\\AccessLogs";
        CreateDirectoryW(folder.c_str(), nullptr);

        SYSTEMTIME now{};
        GetLocalTime(&now);
        wchar_t stamp[32];
        swprintf(stamp, 32, L"%04d-%02d-%02d_%02d-%02d-%02d", now.wYear, now.wMonth, now.wDay,
                 now.wHour, now.wMinute, now.wSecond);

        std::wstring path = folder + L"\\" + stamp + L"_" + status_tag + L".jpg";
        for (int suffix = 2; GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; ++suffix) {
            path = folder + L"\\" + stamp + L"_" + status_tag + L"_" + std::to_wstring(suffix) + L".jpg";
        }
        return path;
    }

    static inline std::atomic<bool> busy_{false};
};
