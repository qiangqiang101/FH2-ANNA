<h1 align="center">FH2 ANNA</h1>

<p align="center"><a href="README.md">English</a> | 简体中文</p>

**ANNA** 是《极限竞速：地平线 3/4/5/6》里的车载助手，现在来到了 PC 版（Azimuth）的《极限竞速：地平线 2》。《地平线 2》其实一直藏着 ANNA 的 HUD；FH2 ANNA 把它重新打开，换上《地平线 3》的样子：圆形小地图、发光的边框和旁边的菜单列表，并在里面加入了许多开车时可以做的事。其他模组也可以往这个菜单里添加自己的选项。

FH2 ANNA 是 [FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform) 的模组。

## 菜单里有什么

**车辆**

- **启用自动驾驶**：车辆沿着你的 GPS 路线自己开。设置路线之前为灰色不可选。
  - 自动驾驶时，小地图的光环会亮起，并显示 “Auto Drive On”（自动驾驶开启）。
  - 按 LB（使用 FH2 Mouse & Keyboard 时为 Shift）切换到缓慢的电影镜头：视野更窄，HUD 隐藏，屏幕上下两条黑边滑入。再按一次 LB 切换回来；在此之前，ANNA 的方向键下不起作用。
  - 在路口转弯前和变换车道时会打转向灯，进入隧道时自动打开大灯（出隧道后恢复原状），到达目的地时刹车直到完全停下。
  - 自动驾驶时会关闭 Nitro System 的氮气。
  - 踩油门、刹车或转动方向即可接管。在线模式下也能用。
- **灯光**：大灯、双闪、左右转向灯。大灯在白天也会亮，隐藏式大灯会跟着升起和收回。
- **车门**：全部打开或关闭，以及车顶、引擎盖、后备箱、每扇车门、储物格、隐藏式大灯和主动尾翼，车辆能动的都可以。灯光和车门列表选完后不会关闭，方便来回切换。
- **调校车辆**：打开游戏的调校界面，和暂停菜单里的“调校车辆”一样（退出后会回到暂停菜单）。

**设置路线至最近的**：赛事、心愿清单、嘉年华、谷仓寻宝或奖励看板。

**开始配对**：在线公路旅行、在线自由漫游、私人在线公路旅行或私人在线自由漫游，直接在路上开始，不用打开暂停菜单。可以在模组页面隐藏。

## 语音命令

像在 Xbox 上用 Kinect 一样和 ANNA 对话。在模组页面打开 **Voice commands（语音命令）**，然后开车时：

| 说 | ANNA |
|---|---|
| “ANNA” | 醒来并聆听：小地图周围的光环亮起 |
| “Set Route To Nearest” | 打开这个列表 |
| “Race” | 选中它，显示已选中，然后执行 |
| “ANNA, Enable Auto Drive” | 立即执行 |
| “ANNA, set GPS to the nearest Bonus Boards” | 也可以：列表名称、复数和常见的问法都能识别 |
| “Back” / “Close” | 返回上一个列表 / 让 ANNA 休眠 |

菜单当前显示的任何选项都可以说，按它现在的文字来说（例如引擎盖打开后说 “Close Hood”），其他模组的选项也一样。ANNA 醒着的时候（无论是说了 “ANNA” 还是按了方向键下），不用再说 “ANNA”；安静 10 秒后它会重新休眠。

语音命令使用 Windows 自带的离线语音识别和你的默认麦克风。耳机麦克风的效果比外放好得多。Windows 只为部分语言提供语音识别（英语、法语、德语、西班牙语、日语和中文）；如果没有游戏语言对应的识别，而 Windows 装有英语识别，ANNA 就用英语聆听。如果没反应，请检查 **Windows 设置 > 隐私和安全性 > 麦克风 > 允许桌面应用访问你的麦克风**。

## 语言

ANNA 使用游戏的文字语言（或 Extra Options 设置的语言）：英语、法语、德语、意大利语、西班牙语（西班牙和墨西哥）、巴西葡萄牙语、荷兰语、波兰语、俄语、日语、韩语和繁体中文。地点和模式的名称用的是游戏自己的翻译。捷克语和匈牙利语和游戏一样保持英文。游戏没有简体中文，所以 ANNA 也没有。更改语言后，模组页面的选项要到下次启动才会跟着变。

## 操作

| 手柄 | 键盘（[FH2 Mouse & Keyboard](https://github.com/qiangqiang101/FH2Mouse)） | 作用 |
|---|---|---|
| 轻按方向键下 | C | 打开 ANNA（再按 C 关闭） |
| 方向键上 / 下 | I / K | 在列表中上下移动（按住可连续移动） |
| 方向键右 | L | 选择 |
| 方向键左 | J | 返回（在最上层列表时关闭 ANNA） |

按住方向键下仍然会和以前一样打开遥测数据。ANNA 打开时，方向键归 ANNA 使用，不会切换电台。

FH2 ANNA 只读取手柄。键盘按键由 FH2 Mouse & Keyboard 提供：它替你按下方向键，并在你用键盘玩时把 ANNA 的按钮图标换成对应的按键。可以在 **暂停 > 设置 > Keyboard & Mouse**（ANNA 那几行）修改按键。

## 安装

1. 从 [Releases](../../releases) 下载最新的 `FH2ANNA-<版本>.zip`。
2. 解压到 Azimuth 文件夹，也就是 `azimuth.exe` 所在的文件夹。解压后 `DWrite.dll` 应该在 `azimuth.exe` 旁边，`FH2ANNA.dll` 应该在 `fh2-mods` 文件夹里。

> [!WARNING]
> `FH2ANNA.dll` 要放在 **`fh2-mods`** 里，不是 Extra Options 的 `mods` 文件夹。FH2 ANNA 是平台模组，不是 Extra Options 插件：放在 `mods` 里无法正常加载。
3. 启动游戏。ANNA 的标签在小地图下方；开车时轻按方向键下。

压缩包里包含 FH2 Mod Platform（`DWrite.dll`）。如果你已经从其他模组装过它，保留版本较新的那个。

**ANNA 需要在游戏的 HUD 选项里打开小地图。** 小地图关闭时，ANNA 也会关闭。

## 设置

在 **暂停 > 设置 > MODS**：

| 设置 | 默认 | 作用 |
|---|---|---|
| Cinematic camera button（电影镜头按键） | LB | 让自动驾驶切换到电影镜头和切换回来的按键：LB、RB、A、B、X、Y、View、L3、R3，或 None（不使用电影镜头）。自动驾驶时，游戏收不到这个按键（Y 不会倒带，RB 不会切换视角） |
| Hide hood and trunk in ANNA（在 ANNA 中隐藏引擎盖和后备箱） | 关 | 车门列表里不显示引擎盖和后备箱，“全部打开”也不会打开它们：适用于后备箱不可见或引擎是假模型的车 |
| Close doors when moving（行驶时关门） | 开 | 车辆开动后，ANNA 打开的车门、引擎盖、后备箱和储物格会自动关上（车顶保持原样） |
| Start Matchmaking in ANNA（在 ANNA 中开始配对） | 开 | 显示“开始配对”列表 |
| Voice commands（语音命令） | 关 | 通过麦克风聆听 “ANNA” 和命令（见“语音命令”） |

这些设置保存在 `fh2-mods\FH2ANNA.ini` 里（电影镜头按键为 `[Autodrive]` 下的 `Cinematic=LB`）。电影镜头只能从追尾视角启动。使用 FH2 Mouse & Keyboard 时，它的 “ANNA: cinematic camera” 按键（左 Shift）会按下你设置的那个按键，提示里也会显示键盘按键。

## 与其他模组一起使用

- **[Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)**：FH2 ANNA 有自己的自动驾驶、转向灯、双闪和大灯，不需要再用 Extra Options 的。Extra Options 把自动驾驶放在方向键下、双闪放在方向键上，这两个正是 ANNA 的按键，所以请在 `FH2_ExtraOptions.ini`（`azimuth.exe` 旁边）的 `[Bindings]` 下关掉它们：

  ```ini
  AutodriveXbox=None
  HazardsXbox=None
  ```

  否则打开 ANNA 时也会启动 Extra Options 的自动驾驶，在 ANNA 列表里往上移动时会切换双闪。Extra Options 的大灯按键可以和 ANNA 同时使用；以最后一次操作为准。
- **FH2 Mouse & Keyboard**：为 ANNA 提供键盘按键和按键图标（见“操作”）。
- **其他语言的模组页面文字**：安装 FH2 ANNA 后，平台的模组页面能正确显示中文、日文、俄文、带重音的字母等。

## 模组开发者：往 ANNA 菜单添加选项

其他平台模组可以通过 ANNA 的接口 [`include/fh2anna.h`](include/fh2anna.h) 添加选项和列表。把头文件复制到你的模组里，或者从本仓库的 GitHub Packages 源获取 **FH2ANNA** NuGet 包，它会把头文件加入 C/C++ 项目的包含路径（Visual Studio：添加 `https://nuget.pkg.github.com/qiangqiang101/index.json` 作为包源，用有读取包权限的 GitHub 令牌登录，然后安装 FH2ANNA）：

```c
#include "fh2anna.h"

static void honk(void* user, const char* key) { /* ... */ }

/* from a thread of yours, after api->wait_for_mods(...) */
const FH2AnnaApi* anna = (const FH2AnnaApi*)api->get_interface(FH2ANNA_INTERFACE, 4);
if (anna) {
    FH2AnnaEntry e = {sizeof e};
    e.key       = "honkhonk.horn";   /* unique: start it with your mod's name */
    e.parent    = "car";             /* or NULL for the top list, or a list of yours */
    e.label     = "Honk";
    e.on_choose = honk;
    anna->add_entry(&e);
}
```

ANNA 自己的列表是 `car`、`car.lights`、`car.doors`、`route` 和 `online`。没有 `on_choose` 的选项本身就是一个列表。选项可以隐藏自己（`shown`）、每次列表打开时更改文字（`label_now`）、选中后让 ANNA 保持打开（`stays_open`），以及显示为灰色（`enabled`）。`on_choose` 在游戏线程上运行。你的选项也可以按它的文字作为语音命令来说。详细说明见头文件。

如果模组有意接管玩家的驾驶输入（例如用摇杆操控的镜头，同时关闭车辆操作），可以用 `hold_input(1)` 告诉 ANNA，交还时调用 `hold_input(0)`（接口版本 5）。这期间自动驾驶会继续行驶，比赛中也一样，而不会把它当作菜单或暂停。

## 编译

需要 CMake，以及 Visual Studio 2022 或 [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)。FH2 Mod Platform 是子模块：

```sh
git clone --recursive https://github.com/qiangqiang101/FH2_ANNA.git
cd FH2_ANNA

# Visual Studio 2022
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target fh2anna     # build/Release/FH2ANNA.dll

# or llvm-mingw (what the release workflow uses)
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=platform/cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build                                       # build/FH2ANNA.dll
```

发布版由 GitHub Actions（[`.github/workflows/build.yml`](.github/workflows/build.yml)）在发布 Release 时编译，也可以在 Actions 页面手动运行（**Run workflow**）。Release 的标签必须和 `CMakeLists.txt` 里的 `VERSION` 一致。发布 Release 时还会把 FH2ANNA NuGet 包（头文件，[`nuget/`](nuget/)）以相同版本发布到 GitHub Packages；手动运行只会把它作为构建产物上传。

### FH3 资源

`assets/` 里是《地平线 3》的 ANNA 贴图（已转换为《地平线 2》的格式），以及移植到《地平线 2》的《地平线 3》ANNA HUD 控件。两者都编译进 DLL。它们由 `tools/` 里的脚本从《地平线 3》的安装目录生成：

```sh
python tools/fh3_textures.py "<FH3 folder>"     # assets/*.swatchbin
python tools/port_anna_xaml.py "<FH3 folder>"   # assets/HUDAnnaControl.xaml (the look: colours, rows, layout)
```

## 致谢

- **Playground Games**：ANNA，以及《地平线 3》ANNA 的设计和贴图。
- RokkuDayo 的 **[Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)**：《地平线 2》的自动驾驶、转向灯、大灯、自动驾驶电影镜头和白天大灯都是参照它移植的。
- **[FH2 Mod Platform](https://github.com/Trinculo54/fh2-mod-platform)**，其中包含 [MinHook](https://github.com/TsudaKageyu/minhook)。

## 许可证

[MIT](LICENSE).
