# raygui 控件捕获（Capture ID）开发说明

> 状态：已完成初版实现并通过无窗口测试与示例构建验证。
> 日期：2026-10-30

## 1. 目标

raygui 是立即模式 GUI，控件每次绘制时重新求值。多个控件重叠时（浮动按钮、弹出层、滚动内容、
模态对话框），传统实现会让鼠标下方所有控件同时参与交互，容易出现：

- 点击浮动按钮时，下层控件也被触发（click-through）；
- 按下拖动下层 Slider/ScrollBar 时，上层按钮被 `guiControlExclusiveMode` 挡住；
- UI 在按下帧被触发逻辑修改，导致后续控件的 id/状态错位。

本次改动的目标：

1. 为每个控件绘制调用附加一个稳定的捕获 id。
2. 鼠标按下时按“画家算法”决定归属：最后绘制的命中控件（最上层）获得捕获。
3. 按下帧只决定归属，不执行交互副作用。
4. 点击类结果统一在鼠标抬起帧返回；拖动类在捕获确定后的按住帧开始。
5. 按住期间只有被捕获 id 可以交互，按在空白处锁住 UI 到抬起。
6. 保持向后兼容：未使用捕获 id 的工程行为不变（opt-in）。

## 2. 行为设计

### 2.1 捕获状态机

核心状态位于 `src/raygui.h`：

```c
static int  curGuiId = 0;           // 后续控件使用的 id
static int  captureGuiId = 0;       // 按下帧命中的最上层 id
static bool captureEnabled = false; // 首次设置非 0 id 后启用
static bool captureHeld = false;    // 一次 press..release 周期
static bool captureAnyHit = false;  // 按下帧是否命中过任意控件
static bool capturePicked = false;  // 每个按下帧只重置一次
```

边沿处理：

| 帧 | 行为 |
|---|---|
| idle（无 press/down/release） | 清空捕获状态 |
| press | 只重置候选、登记命中 id，不执行副作用；最后绘制者获胜 |
| down | 只允许 `captureGuiId == curGuiId` 的控件交互 |
| release | 保留捕获，允许被捕获控件返回结果；下一 idle 帧清空 |
| 按在空白处 | `captureAnyHit == false`，所有控件锁到抬起 |
| 按在未打 id 的控件 | `captureGuiId == 0`，退回旧行为 |

### 2.2 触发时机

- **抬起帧返回结果**：Button、LabelButton、Toggle、ToggleGroup、ToggleSlider、CheckBox、
  ComboBox、DropdownBox、ListView、TextBox、ValueBox、ValueBoxFloat、StatusBar、DummyRec、
  MessageBox / TextInputBox 的按钮。
- **按住期间持续更新**：Slider、SliderBar、ScrollBar、ColorPanel、ColorBar（返回
  `RESULT_CHANGED` 等）。
- **快速点击**：没有按住帧时，Slider/ColorBar/ScrollBar 在抬起位置应用值。
- **按下帧**：只捕获，不改值、不进入 `guiControlExclusiveMode`、不打开/关闭 UI。
- **模态控件**：展开的 DropdownBox 用 `GuiCaptureSelf()` 保持捕获，即使点在弹层外或
  `GuiLock()` 生效时也能在抬起时关闭/选择。

### 2.3 使用方式

```c
enum { ID_SCROLL_PANEL = 1, ID_SAVE_BUTTON = 2, ID_CANCEL_BUTTON = 3 };

GuiPushId(ID_SCROLL_PANEL);
GuiScrollPanel(panelRec, NULL, contentRec, &scroll, &view);
GuiPopId();

GuiPushId(ID_SAVE_BUTTON);
if (GuiButton(saveRec, "Save")) save();  // 结果在抬起时返回
GuiPopId();
```

也可以继续使用 `GuiSetCapturedId(id)` / `GuiSetCapturedId(0)`。

**id 要求**：

- 在 press→release 期间必须唯一且稳定；
- 动态列表请按数据身份生成（例如固定 stride 的行列 id），不要用每帧重置的绘制序号；
- `0` 表示该控件不参与捕获；
- 组合控件（ScrollPanel、ColorPicker、MessageBox 等）整体共享调用方设置的 id。

## 3. 修改内容

### 3.1 `src/raygui.h`

- 重构捕获状态与辅助函数：
  - `GuiUpdateCapture()`
  - `GuiCanInteract()`
  - `GuiHitControl()`
  - `GuiCaptureSelf()`
- 所有可交互控件改为在 update 开头调用 `GuiUpdateCapture()`，在命中分支调用
  `GuiHitControl()`。
- 点击类触发从按下帧移到抬起帧。
- 拖动类控件在按下帧只捕获，下一按住帧才开始拖动；补充快速点击的抬起处理。
- TextBox / ValueBox / ValueBoxFloat 的进入编辑、光标定位、点击外部退出编辑改到抬起帧。
- DropdownBox 展开态使用 `GuiCaptureSelf()` 保持模态捕获。
- `GuiGrid` 移除捕获调用（不可交互控件不应抢占）。
- `GuiTooltip` / `GuiScrollPanel` 改用新的交互判断。
- 增强 standalone 模式：
  - `GUI_BUTTON_*` 等输入宏允许外部覆盖；
  - 补充 `RL_MALLOC/RL_CALLOC/RL_FREE` 兼容；
  - 补充 `GetClipboardText`、`GetCodepoint`、`GetCodepointPrevious`、`GetGlyphIndex`、
    `GetScreenWidth/Height`、`DrawTextCodepoint`、`DrawTexturePro`、`UnloadTexture` 等声明。

### 3.2 公共 API

```c
RAYGUIAPI void GuiSetCapturedId(int id); // 为后续控件设置 id，0 表示不参与捕获
RAYGUIAPI int  GuiGetCapturedId(void);   // 当前捕获 id，空闲返回 0
RAYGUIAPI void GuiPushId(int id);        // 保存当前 id 并设置新 id
RAYGUIAPI void GuiPopId(void);           // 恢复上一个 id
```

### 3.3 示例

- `examples/scroll_panel/scroll_panel.c`
  - 删除每帧自增的 `NewCaptureId()`；
  - 使用固定 id 和固定 stride 的网格 id；
  - 浮动按钮使用固定高位 id，继续演示“上层覆盖下层”。
- `examples/controls_test_suite/controls_test_suite.c`
  - 所有可交互控件分配捕获 id；
  - 静态布局下按绘制顺序稳定。

### 3.4 测试与构建

- 新增 `tests/capture_test.c`：`RAYGUI_STANDALONE` + 假输入后端，无窗口运行。
- 新增 `tests/CMakeLists.txt`，并在 `projects/CMake/CMakeLists.txt` 中增加
  `BUILD_RAYGUI_TESTS` 选项。

覆盖场景：

- 最上层控件获胜；
- 下层 Slider 不在按下帧抢值；
- 空白处按下后拖到按钮上抬起不触发；
- Slider 拖动与快速点击；
- 抬起在控件外取消点击；
- ComboBox / ListView / TextBox / ValueBox 结果在抬起帧；
- Dropdown 在 `GuiLock()` 下仍能关闭；
- 未打 id 控件的兼容回退；
- 按住期间重排绘制顺序不会误触发。

## 4. 验证结果

- 无窗口测试：**39 checks, 0 failures**。
- CMake + MSVC（raylib 6.0）：**10 个 example 全部构建成功**。
- `ctest`：**1/1 passed**。

> 注意：仓库内 `projects/CMake/_deps` 缓存的是 raylib 4.5.0，缺少 `TextToFloat`，
> 因此完整 example 链接验证使用了 raylib 6.0 源码。使用 raylib 6.x 构建即可。

## 5. 已知限制

- `GuiTabBar` 尚未接入捕获（源码中仍是 TODO）。
- 同一帧内同时 press + release 的极端快速点击无法被状态机观察到；普通快速点击已在
  抬起帧处理。
- 捕获 id 栈固定 16 层（`RAYGUI_MAX_CAPTURE_ID_STACK`）；嵌套过深时最外层恢复可能不准确。
- 捕获模式一旦通过非 0 id 启用，本进程内不会自动关闭。
- 未打 id 的控件在按住期间可以被其他未打 id 控件交互（兼容回退行为），不参与最上层判定。
- 键盘/手柄焦点捕获未纳入本次范围，捕获只针对鼠标/触摸映射的主按钮。

## 6. 后期优化项

1. **TabBar 捕获支持**：给 TabBar/TabBarEx 的每个 tab 分配 id，并支持 `RESULT_TAB_CLOSE`
   在抬起帧返回。
2. **自动稳定 id 辅助**：提供类似 `GuiHashId(const char *name)` 或调试期 id 冲突检测，
   减少手工维护 id 的成本。
3. **可配置/动态 id 栈**：把 `RAYGUI_MAX_CAPTURE_ID_STACK` 改为编译期可配置，或使用
   轻量动态栈；栈溢出时给出明确行为。
4. **重命名 API**：`GuiSetCapturedId()` 语义更接近“设置当前控件 id”，可考虑新增
   `GuiSetId()` 别名，旧名称保留兼容。
5. **焦点与键盘**：为键盘导航、Enter 激活、Tab 切换引入独立的焦点捕获，与鼠标捕获解耦。
6. **同帧快速点击**：为 press+release 同帧补充一个合成事件，避免极短点击丢失。
7. **失焦取消**：窗口失焦/鼠标离开时主动取消拖动和捕获，避免控件卡在 pressed 状态。
8. **更多回归测试**：补充 ScrollBar 箭头/轨道快速点击、ColorPanel、ColorBarHue、
   Modal 对话框按钮、嵌套 ScrollPanel + 浮动控件等场景。
9. **CI 集成**：把 `BUILD_RAYGUI_TESTS` 与 `ctest` 接入 GitHub Actions，覆盖 MSVC/GCC/Clang。
10. **文档与示例**：增加一个专门的最小 capture 示例，展示“下层 Slider + 上层浮动 Button”
    以及“动态列表稳定 id”的推荐写法。

## 7. 变更文件

- `src/raygui.h`
- `README.md`
- `projects/CMake/CMakeLists.txt`
- `examples/scroll_panel/scroll_panel.c`
- `examples/controls_test_suite/controls_test_suite.c`
- `tests/CMakeLists.txt`
- `tests/capture_test.c`
- `docs/control-capture.md`（本文档）
