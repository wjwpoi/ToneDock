# 独立仓库来源

ToneDock从 [wjwpoi/DaisySeedProjects](https://github.com/wjwpoi/DaisySeedProjects/tree/2c972a8951a8cb573835550c9c56b388f8d858f2/ToneDock) 的 `ToneDock/` 子目录拆分，来源提交为 `2c972a8951a8cb573835550c9c56b388f8d858f2`。

独立代码已发布至 [wjwpoi/ToneDock](https://github.com/wjwpoi/ToneDock)，首次发布提交为 `e2859301eb6a376a8098b3441887eb94109a2b3c`。原仓库及硬件工程保留。新仓库首轮 [Windows/macOS 检查](https://github.com/wjwpoi/ToneDock/actions/runs/38731575080) 已触发，结果以该运行页面为准。

使用git-filter-repo 2.47.0保留8条软件提交、原作者与提交说明，将软件子目录提升为仓库根目录。历史提交ID因目录拆分而改变。Windows/macOS工作流从原仓库一同迁移；构建命令、打包工具和文档路径已适配独立根目录。

`src/`、`tests/`、`third_party/`、`CMakeLists.txt`与来源版本一致，保留全部应用与第三方许可证。运行时、JUCE和NAM Core依赖版本保持固定。

验证报告中的 [原仓库原生检查](https://github.com/wjwpoi/DaisySeedProjects/actions/runs/38723754252) 是迁移前的Windows/macOS构建与AU校验基线。独立根目录的Linux构建、49,858项核心检查及独立程序启动/截图/退出检查已通过；独立仓库的Windows/macOS原生检查须以本仓库Actions运行结果为准。
