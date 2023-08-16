# 如何更新 SDK？

使用 OpenHarmony 提供的 *SDK命令行工具* 管理 SDK。

每次更新时，先使用工具将 SDK 下载到本地，然后提交到 git 仓库。

## 1. 安装依赖软件

- 需要安装 JDK，版本不低于 11。
- 需要安装 Node.js，版本不低于 14.18.3，建议安装 16.\* 系列版本，可以[从公司内网镜像下载](https://mirrors.tools.huawei.com/nodejs/v16.15.1/)，或者到[官网下载](https://nodejs.org/en/)。
- 将 java 和 node 以及 npm 命令添加到系统 `PATH` 环境变量中。

## 2. 环境设置

设置 Node.js
```
npm config set registry https://mirrors.tools.huawei.com/npm/
npm config set @ohos:registry https://cmc.centralrepo.rnd.huawei.com/artifactory/api/npm/product_npm/
npm config set strict-ssl false
```

## 3. 安装 SDK 命令行工具

1. 从此处[下载SDK命令行工具](https://developer.harmonyos.com/cn/develop/deveco-studio#download_cli_openharmony)。
2. 解压。
3. 检查解压缩后的文件，`oh-command-line-tools/bin/ohsdkmgr` 即为我们需要的 SDK 命令行工具。

## 4. 查看当前 SDK 的版本

```
./ohsdkmgr list --proxy-type=http --proxy=localhost:3128 --no-ssl-verify
```

命令输出如下：
>     Component  | API Version | Version | Stage   | Status        | Available Update
>     ---------- | ----------- | ------- | ------- | ------------- | ----------------
>     ets        | 9           | 3.2.2.5 | Beta1   | Not Installed |
>     ets        | 8           | 3.1.6.5 | Release | Not Installed |
>     js         | 9           | 3.2.2.5 | Beta1   | Not Installed |
>     js         | 8           | 3.1.6.5 | Release | Not Installed |
>     native     | 9           | 3.2.2.5 | Beta1   | Not Installed |
>     native     | 8           | 3.1.6.5 | Release | Not Installed |
>     previewer  | 9           | 3.2.2.5 | Beta1   | Not Installed |
>     previewer  | 8           | 3.1.6.5 | Release | Not Installed |
>     toolchains | 9           | 3.2.2.5 | Beta1   | Not Installed |
>     toolchains | 8           | 3.1.6.5 | Release | Not Installed |

## 5. 安装 SDK 到本地

```
mkdir -p ohos_sdk_new
./ohsdkmgr install ets:9 js:9 native:9 previewer:9 toolchains:9 --proxy-type=http --proxy=localhost:3128 --sdk-directory=`pwd`/ohos_sdk_new --no-ssl-verify --accept-license
```
这个命令中 *ohos_sdk_new* 是用来存放新版本 SDK 文件的目录。
`ets:9` 表示安装 *ets* 的 API 9 版本，具体版本号见上一步骤工具输出的 `Version` 字段。

## 6. 替换旧版本的 SDK

```
cd ohos_sdk_new
tar cfz ohos-sdk-3.2.2.5-Beta1.tar.gz *
```

最后将上面生成的 *ohos-sdk-3.2.2.5-Beta1.tar.gz* 包放到 `//ohos_sdk/` 目录下，并将老版本的 *.tar.gz* 包删除。
接下来的操作均在 `//ohos_sdk/` 目录下执行。

修改 `//ohos_sdk/.install` 文件中 SDK 文件名：
>     latest_version="ohos-sdk-3.2.2.5-Beta1.tar.gz"

修改 `//ohos_sdk/ohos_sdk.gni` 文件中 SDK 的版本号：
>     ohos_sdk_version = "3.2.2.5"

最后提交 `//ohos_sdk/` 目录下修改过的文件：
```
git add ohos-sdk-3.2.2.5-Beta1.tar.gz .install ohos_sdk.gni
```


## 7. 使用新的 SDK 进行编译验证

编译 NWeb 的 hap 包，编译没问题可以提交上库。

有些版本的 SDK 校验规则不一样，可能会出现 hap 工程的配置用新版本的 SDK 校验不通过导致编译失败的情况，
要在这一步发现并修改。

## 8. 手动更新 SDK 包

如果使用转测试的 OHOS SDK 包，需要在以下两个目录下执行 `npm install`，保证被 SDK 依赖的 NPM 包全部包含在 SDK 中：
> js/build-tools/ace-loader
> ets/build-tools/ets-loader

