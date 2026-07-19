// =============================================================================
// LianCore - ResourceDownloader 资源下载管理器实现
// 首次运行自动从 GitHub Release 下载预设库、波表库和 AI 模型
// =============================================================================
#include "ResourceDownloader.h"
#include <JuceHeader.h>

namespace LianCore {

// =============================================================================
// 构造与析构
// =============================================================================
ResourceDownloader::ResourceDownloader()
    : juce::Thread("LianCore-ResourceDownloader") {
}

ResourceDownloader::~ResourceDownloader() {
    cancelled_ = true;
    stopThread(5000);
}

// =============================================================================
// 资源目录路径
// =============================================================================
juce::File ResourceDownloader::getResourceDirectory() {
    // Windows: %APPDATA%/LianCore/
    // macOS:   ~/Library/Application Support/LianCore/
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto lianCoreDir = appDataDir.getChildFile("LianCore");

    if (!lianCoreDir.exists()) {
        lianCoreDir.createDirectory();
    }

    return lianCoreDir;
}

juce::File ResourceDownloader::getPresetLibraryPath() {
    return getResourceDirectory().getChildFile("preset_library_1M.db");
}

juce::File ResourceDownloader::getFactoryPresetsPath() {
    return getResourceDirectory().getChildFile("factory_presets.db");
}

juce::File ResourceDownloader::getWavetableDirectory() {
    return getResourceDirectory().getChildFile("Wavetables");
}

juce::File ResourceDownloader::getModelDirectory() {
    return getResourceDirectory().getChildFile("Models");
}

// =============================================================================
// 检查所有必需资源是否已存在
// =============================================================================
bool ResourceDownloader::areResourcesAvailable() const {
    bool hasPresetLib = getPresetLibraryPath().existsAsFile();
    bool hasFactoryPresets = getFactoryPresetsPath().existsAsFile();
    bool hasWavetables = getWavetableDirectory().exists() &&
                         getWavetableDirectory().getNumberOfChildFiles(
                             juce::File::findFiles, "*.wav") > 0;
    bool hasModels = getModelDirectory().exists() &&
                     getModelDirectory().getNumberOfChildFiles(
                         juce::File::findFiles, "*.onnx") > 0;

    DBG("[LianCore] 资源检查: 预设库=" << (hasPresetLib ? "有" : "无")
        << " 工厂预设=" << (hasFactoryPresets ? "有" : "无")
        << " 波表库=" << (hasWavetables ? "有" : "无")
        << " AI模型=" << (hasModels ? "有" : "无"));

    return hasPresetLib && hasFactoryPresets && hasWavetables && hasModels;
}

// =============================================================================
// 开始下载（异步）
// =============================================================================
void ResourceDownloader::startDownload() {
    if (downloading_) {
        return;
    }

    downloading_ = true;
    cancelled_ = false;
    startThread(juce::Thread::Priority::background);
}

// =============================================================================
// 后台下载线程
// =============================================================================
void ResourceDownloader::run() {
    juce::StringArray messages;
    bool allSuccess = true;

    // 获取 GitHub Release 资源下载 URL
    auto assetUrls = getAssetDownloadUrls();

    if (assetUrls.isEmpty()) {
        if (completionCallback_) {
            completionCallback_(false, "无法获取 GitHub Release 资源列表，请检查网络连接");
        }
        downloading_ = false;
        return;
    }

    // 确定需要下载的资源
    struct DownloadTask {
        juce::String assetName;
        juce::String url;
        juce::File destination;
    };

    std::vector<DownloadTask> tasks;

    // 检查预设库
    if (!getPresetLibraryPath().existsAsFile()) {
        // 预设库可能被分片上传（GitHub 2 GB 限制），需要下载所有分片后重组
        bool allPartsDownloaded = true;
        juce::Array<juce::File> partFiles;
        int partIndex = 0;

        // 先下载所有分片
        while (true) {
            juce::String partName = "preset_library_1M.db.part" + juce::String::formatted("%03d", partIndex);
            auto partFile = getResourceDirectory().getChildFile(partName);

            if (partFile.existsAsFile()) {
                // 分片已存在，跳过下载
                partFiles.add(partFile);
                partIndex++;
                continue;
            }

            // 查找分片下载 URL
            bool found = false;
            for (const auto& url : assetUrls) {
                if (url.contains(partName)) {
                    tasks.push_back({partName, url, partFile});
                    allPartsDownloaded = false;
                    found = true;
                    break;
                }
            }

            if (!found) {
                // 没有更多分片
                break;
            }
            partIndex++;
        }

        // 如果所有分片已下载，重组文件
        if (allPartsDownloaded && partFiles.size() > 0) {
            DBG("[LianCore] 预设库分片已下载，正在重组...");
            juce::FileOutputStream output(getPresetLibraryPath());
            if (output.openedOk()) {
                for (const auto& partFile : partFiles) {
                    juce::FileInputStream input(partFile);
                    if (input.openedOk()) {
                        output.writeFromInputStream(input, -1);
                    }
                }
                output.flush();
                DBG("[LianCore] 预设库重组完成: " << getPresetLibraryPath().getFullPathName());

                // 删除分片文件
                for (const auto& partFile : partFiles) {
                    partFile.deleteFile();
                }
            }
        }
    } else {
        // 预设库已存在，检查并清理残留的分片文件
        for (int i = 0; i < 10; ++i) {
            juce::String partName = "preset_library_1M.db.part" + juce::String::formatted("%03d", i);
            auto partFile = getResourceDirectory().getChildFile(partName);
            if (partFile.existsAsFile()) {
                partFile.deleteFile();
            }
        }
    }

    // 检查工厂预设
    if (!getFactoryPresetsPath().existsAsFile()) {
        for (const auto& url : assetUrls) {
            if (url.contains("factory_presets.db")) {
                tasks.push_back({"factory_presets.db", url, getFactoryPresetsPath()});
                break;
            }
        }
    }

    // 检查波表库
    if (!getWavetableDirectory().exists() ||
        getWavetableDirectory().getNumberOfChildFiles(juce::File::findFiles, "*.wav") == 0) {
        for (const auto& url : assetUrls) {
            if (url.contains("factory_wavetables.zip")) {
                tasks.push_back({"factory_wavetables.zip", url,
                    getResourceDirectory().getChildFile("factory_wavetables.zip")});
                break;
            }
        }
    }

    // 检查 AI 模型
    if (!getModelDirectory().exists()) {
        getModelDirectory().createDirectory();
    }

    // 需要下载的模型文件列表（实际文件名）
    static const char* modelFiles[] = {
        "liancore_ai_model.onnx",
        "audio_encoder.onnx",
        "param_regressor.onnx",
        "wavetable_vae_decoder.onnx",
        "transformer_encoder.onnx",
    };

    for (const auto* modelName : modelFiles) {
        auto modelFile = getModelDirectory().getChildFile(modelName);
        if (!modelFile.existsAsFile()) {
            for (const auto& url : assetUrls) {
                if (url.contains(modelName)) {
                    tasks.push_back({modelName, url, modelFile});
                    break;
                }
            }
        }
    }

    if (tasks.empty()) {
        DBG("[LianCore] 所有资源已就绪，无需下载");
        if (completionCallback_) {
            completionCallback_(true, "所有资源已就绪");
        }
        downloading_ = false;
        return;
    }

    DBG("[LianCore] 需要下载 " << static_cast<int>(tasks.size()) << " 个资源文件");

    // 依次下载
    for (size_t i = 0; i < tasks.size(); ++i) {
        if (cancelled_ || threadShouldExit()) {
            allSuccess = false;
            messages.add("下载已取消");
            break;
        }

        auto& task = tasks[i];

        if (progressCallback_) {
            progressCallback_(static_cast<int64>(i), static_cast<int64>(tasks.size()), task.assetName);
        }

        juce::URL downloadUrl(task.url);
        if (downloadFile(downloadUrl, task.destination, task.assetName)) {
            messages.add(task.assetName + " 下载完成");

            // 如果是波表库 zip，需要解压
            if (task.assetName == "factory_wavetables.zip") {
                auto wavetableDir = getWavetableDirectory();
                if (!wavetableDir.exists()) {
                    wavetableDir.createDirectory();
                }
                if (extractZip(task.destination, wavetableDir)) {
                    messages.add("波表库解压完成");
                    // 删除 zip 文件
                    task.destination.deleteFile();
                } else {
                    messages.add("波表库解压失败");
                    allSuccess = false;
                }
            }
        } else {
            messages.add(task.assetName + " 下载失败");
            allSuccess = false;
        }
    }

    if (progressCallback_) {
        progressCallback_(static_cast<int64>(tasks.size()), static_cast<int64>(tasks.size()), "完成");
    }

    downloading_ = false;

    // 下载完成后，检查是否有预设库分片需要重组
    if (allSuccess) {
        // 检查是否存在分片文件
        juce::Array<juce::File> partFiles;
        for (int i = 0; i < 10; ++i) {
            juce::String partName = "preset_library_1M.db.part" + juce::String::formatted("%03d", i);
            auto partFile = getResourceDirectory().getChildFile(partName);
            if (partFile.existsAsFile()) {
                partFiles.add(partFile);
            } else {
                break; // 假设分片是连续的
            }
        }

        if (partFiles.size() > 0 && !getPresetLibraryPath().existsAsFile()) {
            DBG("[LianCore] 正在重组预设库分片 (" << partFiles.size() << " 个分片)...");
            juce::FileOutputStream output(getPresetLibraryPath());
            if (output.openedOk()) {
                for (const auto& partFile : partFiles) {
                    juce::FileInputStream input(partFile);
                    if (input.openedOk()) {
                        output.writeFromInputStream(input, -1);
                    }
                }
                output.flush();
                DBG("[LianCore] 预设库重组完成: " << getPresetLibraryPath().getFullPathName());

                // 删除分片文件
                for (const auto& partFile : partFiles) {
                    partFile.deleteFile();
                }
                messages.add("预设库重组完成 (" + juce::String(partFiles.size()) + " 个分片)");
            } else {
                messages.add("预设库重组失败: 无法创建输出文件");
                allSuccess = false;
            }
        }
    }

    if (completionCallback_) {
        completionCallback_(allSuccess, messages.joinIntoString("\n"));
    }
}

// =============================================================================
// 下载单个文件
// =============================================================================
bool ResourceDownloader::downloadFile(const juce::URL& url, const juce::File& destination,
                                       const juce::String& assetName) {
    DBG("[LianCore] 开始下载: " << assetName << " -> " << destination.getFullPathName());

    // 使用 juce::URL 的 InputStream 进行下载
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(10000)
                       .withProgressCallback([this, &assetName](int64 downloaded, int64 total) {
                           if (progressCallback_) {
                               progressCallback_(downloaded, total, assetName);
                           }
                           return !cancelled_ && !threadShouldExit();
                       });

    auto stream = url.createInputStream(options);
    if (!stream) {
        DBG("[LianCore] 下载失败: 无法创建输入流");
        return false;
    }

    // 确保目标目录存在
    auto parentDir = destination.getParentDirectory();
    if (!parentDir.exists()) {
        parentDir.createDirectory();
    }

    // 写入文件
    juce::FileOutputStream fileStream(destination);
    if (!fileStream.openedOk()) {
        DBG("[LianCore] 下载失败: 无法创建文件 " << destination.getFullPathName());
        return false;
    }

    // 分块复制
    constexpr int bufferSize = 65536; // 64 KB
    juce::HeapBlock<char> buffer(bufferSize);
    int64 totalBytes = 0;

    while (!stream->isExhausted() && !cancelled_ && !threadShouldExit()) {
        int bytesRead = stream->read(buffer.getData(), bufferSize);
        if (bytesRead <= 0) break;

        if (!fileStream.write(buffer.getData(), bytesRead)) {
            DBG("[LianCore] 下载失败: 写入文件出错");
            destination.deleteFile();
            return false;
        }

        totalBytes += bytesRead;
    }

    fileStream.flush();
    DBG("[LianCore] " << assetName << " 下载完成: " << (totalBytes / 1024 / 1024) << " MB");
    return true;
}

// =============================================================================
// 从 GitHub Release API 获取资源下载 URL
// =============================================================================
juce::StringArray ResourceDownloader::getAssetDownloadUrls() {
    juce::StringArray urls;

    // 使用 GitHub Release API 获取资产列表
    juce::URL apiUrl(GITHUB_RELEASE_API);
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(10000);

    auto stream = apiUrl.createInputStream(options);
    if (!stream) {
        DBG("[LianCore] 无法连接 GitHub API");
        return urls;
    }

    // 读取完整响应
    juce::MemoryBlock responseData;
    constexpr int bufferSize = 4096;
    char buffer[bufferSize];
    while (!stream->isExhausted()) {
        int bytesRead = stream->read(buffer, bufferSize - 1);
        if (bytesRead <= 0) break;
        buffer[bytesRead] = '\0';
        responseData.append(buffer, static_cast<size_t>(bytesRead));
    }

    juce::String response = juce::String::fromUTF8(
        static_cast<const char*>(responseData.getData()),
        static_cast<int>(responseData.getSize()));

    // 解析 JSON 获取资产下载 URL
    auto json = juce::JSON::parse(response);
    if (json.isObject()) {
        auto assets = json.getProperty("assets", juce::var());
        if (assets.isArray()) {
            for (const auto& asset : *assets.getArray()) {
                auto downloadUrl = asset.getProperty("browser_download_url", "");
                if (downloadUrl.toString().isNotEmpty()) {
                    urls.add(downloadUrl.toString());
                }
            }
        }
    }

    DBG("[LianCore] GitHub Release 有 " << urls.size() << " 个资产");
    return urls;
}

// =============================================================================
// 解压 zip 文件到目标目录
// 使用 JUCE ZipFile 进行解压
// =============================================================================
bool ResourceDownloader::extractZip(const juce::File& zipFile, const juce::File& destDir) {
    DBG("[LianCore] 解压: " << zipFile.getFullPathName() << " -> " << destDir.getFullPathName());

    if (!zipFile.existsAsFile()) {
        return false;
    }

    juce::ZipFile zip(zipFile);
    if (zip.getNumEntries() == 0) {
        DBG("[LianCore] zip 文件为空");
        return false;
    }

    juce::Result result = zip.uncompressTo(destDir);
    if (result.failed()) {
        DBG("[LianCore] 解压失败: " << result.getErrorMessage());
        return false;
    }

    DBG("[LianCore] 解压完成: " << zip.getNumEntries() << " 个文件");
    return true;
}

} // namespace LianCore