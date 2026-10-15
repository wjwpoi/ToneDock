#pragma once
#include <JuceHeader.h>
#include <cstring>
#include <utility>

// Translate presentation only. Effect IDs, preset keys and library categories
// stay in their original form so existing sessions remain portable.
namespace tonedock::ui {
inline constexpr std::pair<const char*, const char*> translations[] {
    {"Import", "导入模型"}, {"Load model", "加载模型"}, {"Favorite", "收藏"}, {"Unfavorite", "取消收藏"},
    {"Save tags", "保存分类与标签"}, {"Remove", "移出模型库"}, {"+ Add", "+ 添加"}, {"Delete", "删除效果"},
    {"Up", "上移"}, {"Down", "下移"}, {"Enable / bypass", "启用 / 旁通"}, {"Bypass", "全局旁通"},
    {"Audio settings", "音频设置"}, {"Save preset", "保存预设"}, {"Open preset", "打开预设"}, {"Open link", "打开链接"},
    {"Search names, tags, creator...", "搜索名称、标签、作者…"}, {"Category", "分类"},
    {"Tags, comma separated", "标签（用英文逗号分隔）"}, {"Paste a TONE3000 tone page URL", "粘贴 TONE3000 音色页面链接"},
    {"All models", "全部模型"}, {"Favorites", "我的收藏"},
    {"Gate", "噪声门"}, {"Compressor", "压缩器"}, {"Drive", "过载"}, {"NAM", "NAM 模型"},
    {"Cabinet IR", "箱体 IR"}, {"EQ", "均衡器"}, {"Chorus", "合唱"}, {"Delay", "延迟"}, {"Reverb", "混响"}, {"Gain", "增益"},
    {"Amplifier", "音箱"}, {"Pedal", "单块效果器"}, {"Unsorted", "未分类"}, {"Convolution", "卷积"},
    {"Threshold", "阈值"}, {"Release", "释放"}, {"Ratio", "压缩比"}, {"Attack", "启动"},
    {"Tone", "音色"}, {"Level", "电平"}, {"Bass", "低频"}, {"Mid", "中频"}, {"Treble", "高频"},
    {"Rate", "速率"}, {"Depth", "深度"}, {"Feedback", "反馈"}, {"Mix", "混合"}, {"Time", "时间"},
    {"Room", "空间"}, {"Damping", "阻尼"}, {"Width", "宽度"},
    {"YOUR TONES. YOUR SIGNAL.", "你的音色，你的声音。"}, {"MODEL LIBRARY", "模型库"}, {"SIGNAL CHAIN", "效果链"},
    {"Drag effects to reorder. Select a row to edit.", "拖动效果可调整顺序，选中后可编辑参数。"},
    {"INPUT", "输入"}, {"OUTPUT", "输出"}, {"Input gain", "输入增益"}, {"Output gain", "输出增益"}, {" samples DSP latency", " 采样点 DSP 延迟"},
    {"Choose a model from your library", "请从模型库选择模型"},
    {"Drop .nam / IR .wav / .zip here\nor import from TONE3000", "将 .nam / IR .wav / .zip 拖入此处\n或从 TONE3000 导入"},
    {"Choose an amplifier or pedal capture from your library.\n\nFor an amp-only capture, add a cabinet IR after NAM.", "从模型库选择音箱或单块效果器模型。\n\n如果模型仅包含音箱，请在 NAM 后添加箱体 IR。"},
    {"Choose a WAV impulse response from your library.\n\nA capture that already includes a cabinet usually needs no additional cabinet block.", "从模型库选择 WAV 格式的箱体脉冲响应。\n\n已包含箱体的模型通常不需要再添加箱体 IR。"},
    {"Import NAM models, IRs or ZIP bundles", "导入 NAM 模型、箱体 IR 或 ZIP 模型包"},
    {"Save ToneDock preset", "保存 ToneDock 预设"}, {"Open ToneDock preset", "打开 ToneDock 预设"},
    {"Cannot save preset", "无法保存预设"}, {"Select input, sample rate and buffer size in your DAW.", "请在宿主软件中设置输入、采样率和缓冲大小。"},
    {"Ready / import a NAM model to begin", "就绪 / 请先导入 NAM 模型"}, {"Ready", "就绪"},
    {"Preparing audio chain...", "正在准备效果链…"},
    {"TONE3000 publishable key: t3k_pub_...", "TONE3000 公开 Key：t3k_pub_…"},
    {"Connect", "连接账号"}, {"Browse", "浏览音色"}, {"Disconnect", "断开连接"}, {"Disconnected", "已断开连接"},
    {"Latest", "最新音色"}, {"Trending", "热门音色"}, {"Created", "我创建的"}, {"Downloaded", "我下载的"},
    {"A2 models", "A2 模型"}, {"A1 / Custom models", "A1 / 自定义模型"},
    {"Refresh", "刷新"}, {"Prev", "上一页"}, {"Next", "下一页"},
    {"Download to library", "下载到模型库"}, {"View on TONE3000", "在 TONE3000 查看"},
    {"Select a tone to inspect its creator, license and captures", "选择音色以查看作者、许可和可下载模型"},
    {"Connect your account, then browse or open a tone link.", "连接账号后，即可浏览音色或打开音色链接。"},
    {"Powered by TONE3000", "音色来自 TONE3000"},
    {"Enter a publishable t3k_pub_ key from TONE3000 Settings > API Keys. No secret key is needed.", "请填写 TONE3000「Settings → API Keys」中的 t3k_pub_ 公开 Key，无需 secret key。"},
    {"Continue in your browser. You will return here after choosing a tone.", "请在浏览器中继续，选择音色后会返回此处。"},
    {"Connected", "已连接"}, {"Canceled", "已取消"}, {"Connect TONE3000 first", "请先连接 TONE3000 账号"},
    {"Loading page ", "正在加载第 "}, {"...", "…"}, {"Page ", "页码 "}, {" tones", " 个音色"},
    {"Loading tone and captures...", "正在加载音色及模型…"}, {"By ", "作者："}, {"Gear: ", "设备："}, {"License: ", "许可："},
    {"No supported captures for this architecture. Try A1 / Custom or A2.", "该架构下没有支持的模型，请切换 A1 / 自定义或 A2。"},
    {"Choose a capture to download into your library.", "请选择一个模型，下载到本地模型库。"}, {"Downloading ", "正在下载 "},
    {"amp", "音箱头"}, {"amp-cab", "音箱与箱体"}, {"full-rig", "完整设备链"}, {"pedal", "单块效果器"},
    {"outboard", "机架设备"}, {"cab", "箱体"}, {"space", "空间"}, {"experimental", "实验设备"}, {"ir", "脉冲响应"},
    {"Maximum 16 effects", "最多可添加 16 个效果"}, {"Remove an effect before adding this model", "请先移除一个效果，再添加此模型"},
    {"Importing and validating models...", "正在导入并验证模型…"}, {"Folder exceeds 256 files per import", "每次导入文件夹最多支持 256 个文件"},
    {"Download complete; validating and adding to library.", "下载完成，正在验证并加入模型库。"},
    {"Open the ToneDock editor to load this TONE3000 link", "请打开 ToneDock 窗口以加载此 TONE3000 链接"},
    {"Use a TONE3000 tone page or tonedock://tone/<numeric-id>", "请输入 TONE3000 音色页面链接，或 tonedock://tone/数字编号"},
    {"Preset is empty or too large", "预设为空或文件过大"}, {"Invalid ToneDock preset", "无效的 ToneDock 预设"},
    {"Unknown effect in preset", "预设中包含未知效果"}, {"Missing preset parameters", "预设缺少参数"}, {"Preset restored", "已恢复预设"},
    {"Cannot create model library", "无法创建模型库"}, {"Library index is invalid; restore library.json before changing the library", "模型库索引无效，请先恢复 library.json 再修改模型库"},
    {"Cannot save library index", "无法保存模型库索引"}, {"Cannot commit library index", "无法更新模型库索引"},
    {"Library is busy in another application", "其他程序正在使用模型库，请稍后重试"}, {"Library is busy", "模型库忙，请稍后重试"},
    {"Invalid model identifier", "模型标识无效"}, {"Model is missing, empty, or larger than 64 MiB", "模型不存在、内容为空或超过 64 MiB"},
    {"Only .nam and .wav IR files are supported", "仅支持 .nam 模型和 .wav 脉冲响应文件"}, {"Cannot read model file", "无法读取模型文件"},
    {"This version loads mono NAM captures; stereo/conditional captures are unsupported", "此版本仅支持单声道 NAM 模型，暂不支持立体声或条件模型"},
    {"IR must be mono/stereo WAV and at most two seconds", "IR 必须是单声道或立体声 WAV，且长度不超过 2 秒"},
    {"Cannot copy model into library", "无法将模型复制到模型库"}, {"Archive larger than 64 MiB", "压缩包超过 64 MiB"},
    {"Archive is empty or has too many files", "压缩包为空或文件数量过多"}, {"Cannot create import staging directory", "无法创建导入临时目录"},
    {"Unsafe ZIP entry path", "ZIP 包含不安全的文件路径"}, {"Expanded archive exceeds size limit", "压缩包解压后超过大小限制"},
    {"Cannot read ZIP entry", "无法读取 ZIP 中的文件"}, {"Incomplete ZIP entry", "ZIP 中的文件不完整"},
    {"Archive contains no supported models or IRs", "压缩包中没有支持的模型或 IR"}, {"Model no longer exists in library", "模型已不在模型库中"},
    {"Only mono NAM captures are supported", "仅支持单声道 NAM 模型"}, {"Unsupported NAM training sample rate", "不支持此 NAM 训练采样率"},
    {"IR must be a mono/stereo WAV of at most two seconds", "IR 必须是单声道或立体声 WAV，且长度不超过 2 秒"},
    {"Cannot decode IR", "无法解码 IR 文件"}, {"Unknown effect type", "未知效果类型"}, {"Maximum 16 effects per chain", "每条效果链最多支持 16 个效果"},
    {"state_mismatch", "登录验证状态不匹配，请重新连接"}, {"missing_code", "登录回调缺少授权码，请重新连接"},
    {"token_exchange_failed", "授权码兑换失败，请重新连接"}, {"access_denied", "授权被拒绝"},
    {"Could not open a local port for the sign-in redirect.", "无法打开登录回调所需的本地端口。"},
    {"Could not open your browser.", "无法打开默认浏览器。"}, {"Not connected to TONE3000.", "尚未连接 TONE3000。"},
    {"Could not reach TONE3000.", "无法连接 TONE3000，请检查网络。"}, {"Cannot commit model download", "无法保存下载的模型"},
    {"Refused insecure redirect", "已拒绝不安全的重定向"}, {"Response exceeds 64 MiB limit", "服务器响应超过 64 MiB"},
    {"Audio interface / sample rate / buffer", "音频接口 / 采样率 / 缓冲"}, {"Default", "默认"},
    // JUCE's standalone audio selector and common dialogs use TRANS().
    {"Audio device type:", "音频接口类型："}, {"Output:", "输出："}, {"Input:", "输入："}, {"Device:", "设备："},
    {"Sample rate:", "采样率："}, {"Audio buffer size:", "音频缓冲大小："}, {"Active output channels:", "启用的输出通道："},
    {"Active input channels:", "启用的输入通道："}, {"Show advanced settings...", "显示高级设置…"},
    {"Control Panel", "控制面板"}, {"Opens the device's own control panel", "打开设备驱动控制面板"}, {"Reset Device", "重置设备"},
    {"Resets the audio interface - sometimes needed after changing a device's properties in its custom control panel", "重置音频接口；在驱动控制面板中修改设备属性后可能需要此操作"},
    {"Test", "测试"}, {"Plays a test tone", "播放测试音"}, {"none", "无"},
    {"(no audio output channels found)", "（未找到音频输出通道）"}, {"(no audio input channels found)", "（未找到音频输入通道）"},
    {"Error when trying to open audio device!", "打开音频设备时出错"}, {"OK", "确定"}, {"Cancel", "取消"}, {"Close", "关闭"},
    {"Copy", "复制"}, {"Cut", "剪切"}, {"Paste", "粘贴"}, {"Select All", "全选"}, {"Undo", "撤销"}, {"Redo", "重做"}
};

inline juce::String text(const juce::String& source) {
    for (const auto& [key, value] : translations)
        if (source == key) return juce::String::fromUTF8(value);
    return source;
}
inline juce::String categoryLabel(const juce::String& value) {
    for (auto key : {"Amplifier", "Pedal", "Cabinet IR", "Unsorted"})
        if (value == key) return text(key);
    return value;
}
inline juce::String categoryValue(const juce::String& label) {
    for (auto key : {"Amplifier", "Pedal", "Cabinet IR", "Unsorted"})
        if (label == text(key)) return key;
    return label;
}
inline juce::String message(const juce::String& source) {
    auto translated = text(source);
    if (translated != source) return translated;
    const std::pair<const char*, const char*> prefixes[] {
        {"Model selected: ", "已选择模型："}, {"Downloaded to library: ", "已下载到模型库："},
        {"Download import failed: ", "下载模型导入失败："}, {"Downloading ", "正在下载 "},
        {"Load failed: ", "加载失败："}, {"Could not reach ", "无法连接 "}
    };
    for (auto [prefix, translation] : prefixes)
        if (source.startsWith(prefix)) return juce::String::fromUTF8(translation) + text(source.substring(int(std::strlen(prefix))));
    if (source.startsWith("Imported / found ")) {
        auto rest = source.substring(17);
        return juce::String::fromUTF8("已导入 / 已存在 ") + rest.upToFirstOccurrenceOf(" model(s)", false, false)
            + juce::String::fromUTF8(" 个模型") + rest.fromFirstOccurrenceOf(" model(s)", false, false);
    }
    if (source.startsWith("Audio device: "))
        return juce::String::fromUTF8("音频设备：") + source.substring(14).replace(". Choose Audio settings.", juce::String::fromUTF8("。请打开音频设置。"));
    return source; // Keep unexpected upstream diagnostics available for troubleshooting.
}
inline juce::String networkError(int status, const juce::String& detail) {
    if (status == 429) return juce::String::fromUTF8("请求过于频繁，请稍后重试。");
    if (status == 401) return juce::String::fromUTF8("TONE3000 登录已过期，请重新连接。");
    auto result = status == 0 ? juce::String::fromUTF8("无法完成 TONE3000 请求，请检查网络。")
        : juce::String::fromUTF8("TONE3000 请求失败（HTTP ") + juce::String(status) + juce::String::fromUTF8("）");
    if (detail.isNotEmpty()) result += " " + message(detail);
    return result;
}
inline juce::String fontName() {
#if JUCE_WINDOWS
    return "Microsoft YaHei UI";
#elif JUCE_MAC
    return "PingFang SC";
#else
    return "Noto Sans CJK SC";
#endif
}
inline juce::Font font(float height) { return juce::Font(juce::FontOptions(fontName(), height, juce::Font::plain)); }
inline void installStandaloneTranslations() {
    juce::String mappings = "language: Simplified Chinese\ncountries: cn\n";
    for (const auto& [key, value] : translations)
        if (!juce::String(key).containsChar('\n'))
            mappings += "\"" + juce::String(key) + "\" = \"" + juce::String::fromUTF8(value) + "\"\n";
    juce::LocalisedStrings::setCurrentMappings(new juce::LocalisedStrings(mappings, false));
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName(fontName());
}
}
