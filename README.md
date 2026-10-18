# ToneDock

Windows / macOS 的 NAM 模型加载与综合软件效果器。提供独立运行版、VST3，及 macOS AU，使用原生 C++ / JUCE 和官方 NeuralAmpModelerCore。

本仓库独立维护 ToneDock 软件，保留软件开发历史与第三方许可。拆分来源见 [迁移说明](docs/MIGRATION.md)。

Windows 免安装 EXE / ZIP 和 macOS 开发包见 [本仓库 Actions](https://github.com/wjwpoi/ToneDock/actions/workflows/tonedock.yml)。Windows 下载 `ToneDock-Windows-portable`，解压其中的便携 ZIP 后直接打开根目录 `ToneDock.exe`；也可运行单独的 EXE。无需本地编译或安装 Visual C++ 运行库。使用说明见 [Windows 便携版](packaging/PORTABLE-WINDOWS.md)。

当前为首版工程。Linux 云端用于编译与自动测试；Windows/macOS 原生构建、DAW 扫描、声卡实测与真实 TONE3000 账号联调分别记录，不用云端编译替代平台验收。见 [验证状态](docs/VALIDATION.md)。

## 功能

- `.nam`、箱体 IR `.wav`、ZIP 模型包及文件夹导入；支持从文件管理器拖入。
- 内容 SHA-256 去重；导入时读取元数据，自动分为 Amplifier / Pedal / Cabinet IR / Unsorted，支持自定义分类、标签、收藏与本地搜索。类别推断来自文件元数据和名称，不假装通过听音分析模型。
- 模型复制到统一库目录，独立软件与同一用户的插件共享；从原下载文件夹移走文件不影响已导入模型。库索引使用跨进程锁与原子替换；从列表移除不会删除预设仍可能使用的模型文件。
- TONE3000 OAuth PKCE 登录；开发版原生在线目录支持关键词、设备、格式、架构、标签、品牌、作者与认证作者筛选，分页合并 A2 与 A1/Custom。双击或拖入链路会自动下载、校验、入库并加载，保留作者、来源、许可证和标签；也提供收藏、创建及下载记录。全站 API 的生产接入权限见 [接入说明](docs/INTEGRATION.md)。
- `.nam` 和 IR 拖入；TONE3000 页面链接拖入或粘贴；`tonedock://tone/42` 唤起独立软件。网页选取使用官方 OAuth 回调；任意第三方网页上的“发送到软件”需要其提供链接，或使用附带的 [发送页](packaging/send-to-tonedock.html)。不假设 TONE3000 已有 ToneDock 专用按钮。
- 最多16个内置效果，横向效果卡片与整链总览，支持添加、复制、删除、旁通、拖动及前移/后移排序；固定参数区编辑选中的效果。Gate、Compressor、Drive、NAM、Cabinet IR、EQ、Chorus、Delay、Reverb、Gain。
- 输入/输出增益、峰值表、全局旁通、独立版音频设备/采样率/缓冲设置、预设文件与 DAW 工程状态保存。
- NAM 加载和预热、库导入及下载在后台；结构编辑准备新图后交叉淡化，旧图在非音频线程回收。模型采样率与宿主不同时采用 Lanczos 重采样，并向宿主报告额外 DSP 延迟；总演奏延迟仍由声卡、驱动、缓冲和模型计算决定。

## 构建

需要 Git、CMake 3.24+、C++20 编译器。首次配置下载固定提交的 JUCE 和 NAM Core（含其 Eigen / AudioDSPTools 子模块），无需购买软件开发框架即可按本目录 AGPL 开源方式构建。

Windows：Visual Studio 2022 的 Desktop development with C++ 和 Windows SDK。

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

macOS：Xcode Command Line Tools。Universal 构建同时包含 Apple Silicon 与 Intel；需要时可仅设置本机架构缩短编译。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

产物位于 `build/ToneDockApp_artefacts/<配置>/` 和 `build/ToneDock_artefacts/<配置>/{VST3,AU}/`。构建用于开发测试，CI为AU添加ad-hoc签名；正式分发的签名、公证和安装验收尚未完成。参见 [安装与网页唤起](docs/INSTALL.md)。

Debian云端开发可从仓库根目录运行 `bash tools/cloud_build.sh`，它使用工作区内的固定依赖与工具链，完成Linux编译和现有测试；这不替代Windows/macOS验收。

## 使用

默认界面为简体中文，覆盖效果参数、模型库、音频设置与 TONE3000 连接提示。模型名称、作者、许可证及自定义标签保留来源内容；效果和分类的内部标识保持兼容已有预设。

1. 启动独立版，在「设备」页选择驱动、吉他所在单声道输入、立体声输出对、采样率与缓冲。设备页与效果链共用深色样式；新安装默认不启用麦克风输入。插件版的音频设备由 DAW 管理。
2. 拖入 `.nam` 或 IR，先选中目标 NAM / 箱体 IR 卡片，再选择库中的模型并点「加载模型」。未选中同类卡片时会使用第一个同类位置，缺少该类位置时自动添加。
3. 从效果选择框添加效果，拖动卡片或点「前移 / 后移」调整顺序。卡片超出窗口时横向滚动；上方总览可直接跳转到任意位置。选中卡片后调节下方旋钮。
4. TONE3000 首次连接需要应用的 `t3k_pub_...` publishable key，可在 TONE3000 Settings → API Keys 创建，在窗口输入一次。它是客户端公开标识，不是账户密码或 `t3k_cs_` secret。登录令牌在 macOS Keychain / Windows DPAPI 中保存，不写入预设。
5. 点击「在线音色」，登录后直接搜索完整目录。双击一条音色或点「加入效果链」即可自动加载默认捕获；也可先切换捕获下拉框。拖到右侧空白处新增，拖到 NAM / 箱体 IR 卡片则替换对应位置。从网页拖入音色链接也可自动拉取。已经缓存的模型可复用；当前直接播放 NAM / IR，其他格式可浏览。

库默认在 `%APPDATA%/ToneDock`（Windows）或 `~/Library/Application Support/ToneDock`（macOS）。可通过 `TONEDOCK_DATA_DIR` 指定其他位置。独立软件保存并恢复上次退出时的链路，库变更会在打开的编辑器内刷新。跨电脑转移预设时也需要转移对应的 `models/` 文件；预设引用内容 ID，不内嵌或重新分发第三方模型权重。

CI 在构建与测试通过后生成可分发的 Windows ZIP、独立 EXE 和 SHA256SUMS，检查 EXE / VST3 的运行库依赖，再从 ZIP 解压运行实际 EXE 并生成原生界面预览。推送与工程版本一致的 `vX.Y.Z` 标签会准备带附件的 GitHub Release 草稿，审核后再发布。

## 首版边界

- NAM 目前处理单输入、单输出捕获，插件支持单声道或立体声输入、立体声输出；NAM 节点将输入混成单声道，之后的 IR 和时间效果可为立体声。立体声或多条件 NAM 捕获导入会给出不支持提示。
- IR 限制为单/双声道 WAV、最长两秒。旧 NAM 缺少训练采样率时按48 kHz解释；需要核对捕获来源。
- DAW 自动化目前覆盖输入、输出和全局旁通；每个效果的旋钮保存在预设中，尚未开放为独立宿主自动化参数。
- 效果链使用内置处理器，尚不承载第三方 VST/AU 插件；没有承诺任意模型都能在最低声卡缓冲下实时运行。
- TONE3000 搜索接口的商业开放、模型许可证与应用框架许可证是不同事项，详见 [接入与依赖](docs/INTEGRATION.md)。
