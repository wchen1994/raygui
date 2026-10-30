# raygui 控件捕获（阶段一实现说明）

> 状态：阶段一实现完成，`tests/capture_core_test.c` 通过。
> 日期：2026-10-30
> 版本：raygui 5.1-dev

## 1. 目标

raygui 是立即模式 GUI，控件每次绘制时重新求值。多个控件重叠时（浮动按钮、弹出层、滚动内容），
传统实现会让鼠标下方所有控件同时参与交互。阶段一实现“自动 ID + 命中栈 + 两阶段捕获”：

1. 控件按绘制顺序自动分配 ID，业务层不再手动设置。
2. 按下帧只登记命中，最上层控件成为暂定归属，不执行任何副作用。
3. 超过拖拽阈值（默认 5px）后按方向仲裁，把捕获转移给第一个支持该方向的命中控件。
4. 抬起帧只有归属控件可以返回结果；一帧最多一个离散控件触发。
5. 编辑中的 TextBox、展开的 Dropdown 具有模态捕获，点击外部只结束自身。
6. hover 只高亮 capture 持有者，或最上层命中控件。

> **绘制顺序约束**：自动 ID 依赖调用顺序稳定。数量会变化的动态列表（网格、循环生成的按钮等）
> 必须放在所有固定控件之后绘制，否则列表项数变化会使后续控件 ID 整体偏移，正在拖动的
> Slider/ScrollPanel 会失去 capture。`scroll_panel` 示例已按此顺序调整。

## 2. 帧 API

```c
GuiBeginFromRaylib();       // 从 raylib 指针输入取快照并开始一帧
// ... 绘制控件 ...
GuiEndFromRaylib();         // 帧末结算 tentative/hover
```

`RAYGUI_STANDALONE` 或自定义输入后端使用低层入口：

```c
GuiBeginFrame();            // 从 GUI_POINTER_POSITION / GUI_BUTTON_* 宏取快照
// ... 绘制控件 ...
GuiEndFrame();
```

阶段二会把 `GuiBeginFrame()` 替换为 `GuiBegin(GuiInput*, int)` 并加入多 context。
在不调用任何帧入口时，控件仍可绘制，但不会交互。

查询接口：

```c
unsigned int GuiGetCapturedId(void);   // press 帧返回 tentative，确认后返回 captured，空闲为 0
bool GuiIsEditing(void);               // 当前是否有文本控件正在编辑
```

## 3. 状态机

核心状态位于 `src/raygui.h` 的 `GuiCaptureState`（阶段二迁入 `GuiContext`）：

| 字段 | 含义 |
|---|---|
| `nextId` | 帧首重置为 1，控件绘制时自增 |
| `hitStack` / `hitStackCount` | 当前帧命中栈，每帧重建 |
| `pressHitStack` | press 帧命中栈快照，供方向仲裁 |
| `tentativeId` | press 帧暂定归属（栈顶或模态控件） |
| `capturedId` | 仲裁/抬起后确认的归属 |
| `isConfirmed` / `didDrag` | 是否已确认 / 是否真正越过阈值 |
| `modalId` | 当前模态控件（文本编辑或展开的 Dropdown） |
| `editId` | 当前文本编辑控件 ID，单编辑焦点的唯一事实来源 |
| `dropdownOpenId` | 当前展开的 Dropdown ID |
| `commitId` | 外部点击产生的待提交标记，在抬起帧交付 |

帧边界行为：

| 帧 | 行为 |
|---|---|
| idle | 清空 capture/tentative/drag；hover 由上一帧命中栈决定 |
| press | 只登记命中，帧末把栈顶写为 tentative；不触发控件 |
| down（未过阈值） | 不触发任何控件 |
| down（过阈值） | 按主轴方向从 `pressHitStack` 栈顶向下仲裁 |
| release | 未确认则 capture=tentative；归属控件按规则返回结果 |
| 下一 idle | 清空 capture，`GuiGetCapturedId()` 归零 |

release 帧的返回规则：

- **离散控件**（Button/Toggle/CheckBox/ComboBox/ListView 等，方向 `GUI_DRAG_NONE`）：
  指针必须仍在控件范围内，否则视为 Cancel，不返回结果。
- **连续控件**（Slider/ScrollBar/ColorPanel/ColorBar）：
  - 已越过阈值并在拖动：允许在范围外 release，继续使用最后更新的值；
  - 未拖动（快速点击）：release 也必须在范围内，否则取消。
- **模态控件**（编辑中 TextBox、展开的 Dropdown）：点击外部本身就是提交/关闭，仍按返回。

## 4. 方向仲裁

- Slider/SliderBar、ColorBarAlpha/Hue：水平；ScrollBar：按长宽推断水平/垂直；
  ScrollPanel、ColorPanel：双向。
- Button、TextBox、Dropdown 等不声明方向，拖拽时由父容器接管；若没有任何命中控件支持
  该方向，则保留 press 帧的暂定归属（普通按钮拖动后松开仍会触发）。
- 移出 bounds 不自动释放 capture；离散控件在范围外 release 视为 Cancel，连续拖动可在范围外结束。

## 5. 控件返回值

- 离散控件（Button/Toggle/CheckBox/ComboBox/ListView/TabBar/StatusBar/DummyRec 等）：
  release 帧返回，`RESULT_PRESSED` / `RESULT_CHANGED` / 自定义结果走一帧一次闸门。
- Slider/ScrollBar/ScrollPanel/ColorPanel/ColorBar：只在值变化时返回 `RESULT_CHANGED`，
  release 帧值未变不重复返回；快速点击在 release 应用值。
- TextBox/ValueBox/ValueBoxFloat/Spinner/DropdownBox：编辑/展开状态完全由 GUI 内部管理，
  `editId` 是唯一事实来源，业务层不再传 `bool`。
  - 命中进入编辑，不返回值。
  - Enter/ESC 当帧提交并返回 `RESULT_PRESSED`。
  - 编辑中点击外部：press 帧清 `editId`、设置 commit 标记，release 帧返回提交结果，
    底层控件不触发。
  - **同一时刻只有一个文本控件拥有编辑态**：点击另一个 TextBox/ValueBox 会提交旧焦点并切换；
    键盘输入只送当前编辑控件。业务层可用 `GuiIsEditing()` 查询是否有文本正在编辑。
  - **Spinner 只是布局辅助**：由两个 `GuiButton` 和一个内部 `GuiValueBox` 组成，三者各自持有
    capture ID，Spinner 本身不额外 `GuiControlBegin`。编辑中点箭头：第一次只提交（模态），
    第二次才步进。点击 Spinner 中间仍会从其他 TextBox 抢占编辑焦点。
  - **ValueBox / ValueBoxFloat 基于内部 GuiTextBox 实现**：编辑期间保留文本 buffer，只有提交
    （Enter/ESC/点击外部）时才 parse 数值；非法文本保留原值，整数提交时 clamp。光标、选择、
    粘贴等交互与 TextBox 一致；输入数字不会在提交前修改 `*value`。
  - **Dropdown 的展开状态也由 GUI 内部保存**（`dropdownOpenId`），业务层不需要 editMode 变量。
- `GuiGrid()` 不再返回 `RESULT_PRESSED`，只输出 `mouseCell`。

## 6. 示例迁移

所有 examples 在 UI 绘制前后添加：

```c
GuiBeginFromRaylib();
// ...
GuiEndFromRaylib();
```

并完成：

- 删除 `GuiSetCapturedId()` / `GuiPushId()` / `GuiPopId()` 调用。
- 删除 `GuiTextBox/ValueBox/ValueBoxFloat/Spinner/DropdownBox` 的 `editMode` 实参。
- 删除 `if (GuiTextBox(...)) editMode = !editMode;` 模式；提交结果直接用于解析/校验。
- 需要知道是否有文本正在编辑时，使用 `GuiIsEditing()`。
- 展开 Dropdown 不再需要手工 `GuiLock()` 包裹（模态捕获已处理）。

## 7. 测试

`tests/capture_core_test.c`（无窗口，`RAYGUI_STANDALONE` + fake backend）覆盖：

- 自动 ID 稳定、顶层命中、release 才触发；
- 空白按下后释放到控件不触发；
- 阈值内 click、超阈值拖动后保留按钮归属并正常触发；
- 离散控件 release 在范围外（点击或拖动）取消；
- 连续控件拖动越过 bounds 后在范围外 release 不取消；
- Slider 按下不改值、越阈值拖动、release 不重复、快速点击；
- 方向仲裁把 capture 从 Button 转移到下层 Slider；
- hover 只取栈顶，离开后清空；
- TextBox 进入编辑、Enter 提交、外部点击提交且不穿透；
- 两个 TextBox 同时存在时只有一个接收键盘输入，点击另一个会提交旧焦点并切换；
- ValueBox/Spinner 的内部编辑与提交；
- ValueBox 输入期间不修改值、提交时才 parse/clamp，非法文本保留原值；
- Spinner 内部 ValueBox 编辑、Enter 提交、箭头步进，以及编辑中点箭头“提交 + 步进”；
- Spinner 从 TextBox 抢占编辑焦点；
- 两个 Spinner 同时存在时只有一个接收输入，点击另一个会切换焦点；
- Dropdown 展开/选中/外部关闭/`GuiLock()` 下关闭；
- ComboBox selector 仍可翻页；
- ScrollPanel 滚轮与返回值；
- `GuiGrid` 不产生离散触发；
- 一帧最多一个离散控件触发。

构建与运行：

```powershell
cmake -S projects/CMake -B build -DBUILD_RAYGUI_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

或直接：

```powershell
gcc -std=c99 -I src tests/capture_core_test.c -o capture_core_test.exe
./capture_core_test.exe
```

## 8. 已知限制（阶段一）

- hover 有一帧延迟（立即模式无法预知后绘制控件）。
- 同帧 press+release 不合成点击。
- 未调用帧入口时控件只绘制、不交互，并在首次调用时输出警告。
- 阶段一尚无 `GuiCancel/GuiReleaseCapture/GuiSetDragThreshold`，阈值固定 5px；
  多 context/多指路由在阶段二提供。
- 触摸输入仍通过 raylib 主指针宏读取，阶段二改为 `GuiInput` 注入。

## 9. 变更文件

- `src/raygui.h`
- `README.md`
- `docs/control-capture.md`
- `tests/capture_core_test.c`
- `tests/CMakeLists.txt`
- `examples/*`（帧 API、自动 ID、内部编辑态迁移）
