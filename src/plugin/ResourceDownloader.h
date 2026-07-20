// =============================================================================
// LianCore - ResourceDownloader 资源下载管理器
// 首次运行自动从 GitHub Release 下载预设库、波表库和 AI 模型
// 使用 juce::URL 进行 HTTP 下载，支持进度回调
// 
// 完全异步设计：
//   - 构造函数零 IO 操作，不阻塞 UI 线程
//   - 所有文件检查、下载、解压、重组都在 run() 后台线程中执行
//   - 通过 isDownloadComplete() 和 isDownloading() 查询状态
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>

namespace LianCore {

class ResourceDownloader : private juce::Thread {
public:
    // 下载进度回调
    // downloaded: 已下载字节数
    // total: 总字节数 (-1 表示未知)
    // filename: 当前下载的文件名
    using ProgressCallback = std::function<void(int64 downloaded, int64 total, const juce::String& filename)>;

    // 下载完成回调
    // success: 是否全部成功
    // message: 结果消息
    using CompletionCallback = std::function<void(bool success, const juce::String& message)>;

    ResourceDownloader();
    ~ResourceDownloader() override;

    // 设置进度回调（必须在 startDownload() 之前调用）
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = std::move(callback); }

    // 设置完成回调（必须在 startDownload() 之前调用）
    void setCompletionCallback(CompletionCallback callback) { completionCallback_ = std::move(callback); }

    // 检查所有必需资源是否已存在（仅检查缓存结果，不触碰文件系统）
    // 注意：此方法仅在 run() 完成后才返回准确结果
    bool areResourcesAvailable() const { return resourcesAvailable_.load(); }

    // 开始下载缺失的资源（异步，在后台线程运行）
    // 构造函数零 IO 操作，所有文件检查和下载都在 run() 中执行
    void startDownload();

    // 是否正在下载
    bool isDownloading() const { return downloading_.load(); }

    // 下载是否已完成（无论成功或失败）
    bool isDownloadComplete() const { return downloadComplete_.load(); }

    // 获取资源存储目录
    static juce::File getResourceDirectory();

    // 获取预设库文件路径
    static juce::File getPresetLibraryPath();

    // 获取工厂预设文件路径
    static juce::File getFactoryPresetsPath();

    // 获取波表库目录路径
    static juce::File getWavetableDirectory();

    // 获取 AI 模型目录路径
    static juce::File getModelDirectory();

    // GitHub Release API URL
    static constexpr const char* GITHUB_RELEASE_API =
        "https://api.github.com/repos/Lianyilin-rgb/liancore-synth/releases/tags/v3.0.0";

private:
    void run() override;

    // 下载单个文件
    bool downloadFile(const juce::URL& url, const juce::File& destination, const juce::String& assetName);

    // 从 GitHub Release API 获取资源下载 URL
    juce::StringArray getAssetDownloadUrls();

    // 解压 zip 文件到目标目录
    bool extractZip(const juce::File& zipFile, const juce::File& destDir);

    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;

    std::atomic<bool> downloading_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> downloadComplete_{false};
    std::atomic<bool> resourcesAvailable_{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResourceDownloader)
};

} // namespace LianCore