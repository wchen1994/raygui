# raygui CONTROL CAPTURE 设计文档

> **落地状态（2026-10-31）**
> - 阶段一 + 阶段二均已落地：自动 ID、hitStack、两阶段 capture、方向仲裁、hover、模态拦截、
>   每个 context 每帧一个离散触发、内部单编辑焦点（`editId`），以及 `GuiInput` 注入、
>   平级 `GuiContext`、多指路由、`GuiCancel` / `GuiReleaseCapture` / `GuiSetDragThreshold`。
> - 阶段二保留 `GuiBeginFrame()`/`GuiEndFrame()` 作为宏输入便捷入口，新增
>   `GuiBegin(GuiInput*, int)` / `GuiEnd()`。
> - 阶段一实现说明见 `docs/control-capture.md`。
> - 与本文冲突处，以本状态与 `docs/control-capture.md` 为准（见 5.7、5.11）。

## 一、核心哲学

`if (GuiXXX())` 通过 = 业务层获得合法修改权。UI 层只在对应用户明确意图时交付触发信号，业务层无需防御。

---

## 二、第一阶段：Capture 核心机制

### 2.1 两阶段 Capture

- **press 帧**：暂定捕获（tentativeId）+ 构建命中栈（hitStack）。
- **超过 drag threshold 后**：方向仲裁确认归属（capturedId）。
- **release 帧**：仅归属控件可触发。
- **hitStack 每帧构建**（每个控件命中时 push），用于 hover 判定；**press 帧构建的那份额外用于方向仲裁**；capture 状态跨帧保留。
- **每个 context 每帧最多一个离散控件触发。**

### 2.2 ID 规则

- GUI 内部每帧维护自增计数器，控件调用时自动分配 ID，帧首重置。
- 调用顺序固定 → ID 天然稳定。
- 数量会变化的动态列表必须放在固定控件之后绘制，否则列表项数变化会整体偏移后续控件 ID，
  正在拖动的 Slider/ScrollPanel 会失去 capture。
- 容器控件位置固定，ID 稳定。
- `GuiPushId / GuiPopId` 不暴露给业务层，为纯内部实现。

### 2.3 方向仲裁（嵌套滚动）

- 控件内部声明拖拽方向（水平 / 垂直 / 可拖拽）。
- 超过 threshold 后，从命中栈顶向下找第一个支持该方向的控件，capture 转移给它。
- 未声明方向则透传给父容器；若没有任何控件支持该方向，保留 press 帧的暂定归属
  （普通 Button 拖动后松开仍会触发，不会被误取消）。
- threshold 之前所有帧不触发任何控件。
- threshold 值通过运行时接口配置（见 3.7）。

### 2.4 各控件行为

| 控件 | 行为 |
|------|------|
| **Button / Toggle / Dropdown** | press 暂定，release 时指针仍在控件范围内才触发；拖出范围后松开视为 Cancel；Dropdown 展开态吃掉外部点击 |
| **TextBox** | press 帧聚焦；输入直接写 buffer；失焦 / 回车 / ESC 均 = 提交。**提交标记在 press 帧产生**（检测到外部点击 / 回车 / ESC），**`if` 返回值在 release 帧交付**。编辑态吃掉外部点击；GUI 不管理原值，回滚交给业务层。编辑焦点由 GUI 内部 `editId` 管理，业务层不传 `editMode` 变量 |
| **ScrollPanel / Slider** | 连续值控件，`if` 通过 = 值变了（拖拽中**值变化时**每帧返回）；**值未变化时 release 帧不再额外返回**；拖动允许在范围外结束，快速点击要求 release 在范围内；ScrollPanel 支持滚轮、ScrollBar 拖动、内容区拖动；虚拟化由业务层算可见范围 + 循环绘制 |
| **Hover 绘制** | 有 capture → 仅 capture 持有者高亮；无 capture → 仅最顶层命中控件（hitStack 栈顶）高亮。hover 权不随鼠标实时漂移 |
| **模态拦截** | TextBox 编辑中、Dropdown 展开中，点击外部仅自身结束，不穿透触发底层控件 |

### 2.5 业务层接口

```c
// 返回当前持有 capture 的控件 ID（press 帧捕获、仲裁确认后的最终归属）
// 无 capture 时返回 0
unsigned int GuiGetCapturedId(void);
```

ID 分配、PushId/PopId、capture 设置、命中栈、方向仲裁、hover 判定 —— 全部为 GUI 内部实现，业务层无感知。

---

## 三、第二阶段：输入抽象 + 多 Context

### 3.1 设计目标

- GUI 不直接依赖 raylib 的 `GetMouse*` API，输入通过抽象结构注入。
- 用 `GuiContext` 替代所有 static 全局状态，支持多 GUI 实例。
- 多指触摸：单 context 只处理第一个 captured 的 touchId；多指并发需求通过拆分多个平级 context 解决。
- context **不允许嵌套**，必须为平级。

### 3.2 输入抽象

```c
typedef struct {
    Vector2 position;     // 当前位置
    bool pressed;         // 本帧刚按下
    bool released;        // 本帧刚松开
    bool down;            // 持续按住
    int touchId;          // 0 = 鼠标 / 第一指, 1 = 第二指...
} GuiInput;
```

- GUI 内部通过 `GuiInput` 获取输入，不直接调用 raylib 输入 API。
- 默认提供从 raylib 适配的实现，但接口不绑定 raylib；业务层可自行喂入输入源（回放、网络等）。

### 3.3 GuiContext

所有第一阶段的 static 状态迁入 context：

```c
typedef struct {
    unsigned int capturedId;      // 仲裁确认后的归属
    unsigned int tentativeId;     // press 帧暂定
    unsigned int hoverId;         // 当前 hover 控件
    HitEntry hitStack[MAX_HIT_STACK];
    int hitStackCount;
    Vector2 startPos;             // press 起始位置，用于 threshold 判定
    int dragDirection;             // 仲裁出的拖拽方向
    bool isConfirmed;              // 是否已过 threshold 确认
    unsigned int editId;           // 当前文本编辑控件 ID（0 = 无编辑）
    GuiInput* inputs;              // 该 context 可见的输入
    int inputCount;
} GuiContext;
```

### 3.4 Context 作用域 API

```c
void GuiBegin(GuiInput* inputs, int inputCount);          // 帧开始，注入所有 touch 输入
void GuiEnd(void);                                         // 帧结束

void GuiPushContext(GuiContext* ctx, Rectangle bounds);   // 进入 context 作用域，bounds 用于路由
void GuiPopContext(void);                                  // 退出，必须配对
```

**约束：**
- `GuiPushContext / GuiPopContext` **不允许嵌套**，context 之间必须平级。
- 单 context 时不需要 Push/Pop，默认使用内置全局 context。
- `GuiPushContext` 时按 bounds 自动决定哪个 touchId 对该 context 可见。

**业务层用法：**

```c
GuiBegin(inputs, 2);              // 两个 touch

GuiPushContext(&guiLeft, leftBounds);
DrawLeftUI();                      // 摇杆等，内部控件自动归属 guiLeft
GuiPopContext();

GuiPushContext(&guiRight, rightBounds);
DrawRightUI();                     // 按钮等，内部控件自动归属 guiRight
GuiPopContext();

GuiEnd();
```

**单 context 场景（PC / 单指）零侵入：**

```c
GuiBegin(inputs, 1);
DrawUI();                          // 不需要 PushContext，使用默认 context
GuiEnd();
```

### 3.5 多指路由规则

| 情况 | 行为 |
|------|------|
| 某指已被某 context capture | 该指只路由给这个 context，不传递给其他 context |
| 某指未被任何 context capture | 按 `GuiPushContext` 调用顺序尝试，命中 bounds 的 context 捕获该指 |
| 单 context | 只处理第一个 `GuiInput`，其余忽略 |
| 多指并发（左摇杆 + 右按钮） | 拆两个平级 context，每个接收独立的 touchId，各自独立走完整 capture 流程 |

**capture 建立后不重新路由该 touchId。** context 是隔离单元，不是并发单元；并发靠数量解决。

### 3.6 Capture 生命周期

| 事件 | 行为 |
|------|------|
| touch down | 建立 capture |
| touch move（移出 bounds / 移出 context 区域） | **仍然持有 capture，不自动释放** |
| touch up（正常松开） | 释放 capture；离散控件要求 release 仍在 bounds 内，否则 Cancel；连续拖动可在范围外结束 |
| 窗口失焦 / context 销毁 / 业务层主动取消 | **Cancel**：静默清零所有状态，不触发任何控件 |
| 业务层主动释放 | `GuiReleaseCapture(ctx)` |

**capture 一旦建立，不因范围变化而自动失效；但“持有 capture”不等于“release 一定触发”，离散控件的 release 范围判定见上表。**

### 3.7 Cancel 机制

```c
void GuiCancel(GuiContext* ctx);              // 静默清零指定 context 所有状态
void GuiSetDragThreshold(float threshold);    // 运行时设置 drag threshold
float GuiGetDragThreshold(void);              // 查询当前 threshold
```

- Cancel 清零 capture / hover / drag / tentative 等所有状态，**不触发任何控件，不产生任何 `if` 通过**。
- TextBox 编辑焦点（`editId`）清零，buffer 保持现状（GUI 不管原值）。
- 与正常 release 严格区分：release 触发，cancel 不触发。
- drag threshold 默认 5px，可在运行时动态调整（适配不同 DPI / 设备手感）。

| 终止方式 | capture | 控件触发 | 文本 editId | buffer |
|---------|---------|---------|-----------------|--------|
| 正常 touch up | 释放 | ✅ 触发 | 0 | 保持（已提交） |
| Cancel（失焦 / 销毁 / 主动） | 清零 | ❌ 不触发 | 0 | 保持（不动） |

---

## 四、核心要素（≤30 行）

1. **核心契约**：`if(GuiXXX())` 通过 = 业务层获得合法修改权，每个 context 每帧最多一个离散控件触发。
2. **两阶段 Capture**：press 帧暂定捕获 + 构建命中栈；超过 drag threshold 后方向仲裁确认归属；release 帧仅归属控件触发。
3. **hitStack 每帧构建**（用于 hover），press 帧那份额外用于仲裁；capture 状态跨帧保留。
4. **ID 自增**：GUI 内部每帧递增计数器自动分配，帧首重置；调用顺序固定则 ID 稳定；PushId/PopId 不暴露。
5. **方向仲裁**：超过 threshold 后，从命中栈顶向下找第一个声明支持该方向的控件，未声明则透传父容器；threshold 前不触发任何控件。
6. **TextBox**：press 帧聚焦，输入直接写 buffer；失焦 / 回车 / ESC 均 = 提交。提交标记在 press 帧产生，`if` 返回值在 release 帧交付；编辑焦点由 GUI 内部 `editId` 管理，单编辑焦点，业务层不传 editMode；GUI 不管原值。
7. **ScrollPanel / Slider**：连续值控件，`if` 通过 = 值变了；值未变化时 release 帧不再额外返回；虚拟化由业务层算可见范围 + 循环绘制。
8. **Hover 判定**：有 capture 时仅 capture 持有者可高亮；无 capture 时仅命中栈顶（最上层）控件可高亮。hover 权不随鼠标实时漂移。
9. **模态拦截**：TextBox 编辑中、Dropdown 展开中，点击外部仅自身结束，不穿透触发底层控件。
10. **唯一查询接口**：`GuiGetCapturedId()` 返回当前 capture 归属 ID，其余全部内部实现。
11. **输入抽象**：`GuiInput` 结构体解耦 raylib，通过 `GuiBegin` 注入；默认提供 raylib 适配器。
12. **Context 隔离**：所有 static 状态迁入 `GuiContext`；context 平级、不允许嵌套；单 context 无需 Push/Pop。
13. **多指路由**：单 context 只处理第一个 captured touchId；多指并发拆多 context；capture 建立后不重新路由。
14. **Capture 生命周期**：touch down 建立 → 持续持有（即使移出范围）→ touch up 释放；移出范围不自动失效，但离散控件 release 在范围外视为 Cancel，连续拖动可在范围外结束。
15. **Cancel 机制**：窗口失焦 / context 销毁 / 主动取消 → 静默清零所有状态，不触发任何控件，与正常 release 严格区分。
16. **Threshold 配置**：`GuiSetDragThreshold / GuiGetDragThreshold` 运行时可调，默认 5px。

---

## 五、待修改内容（落地清单）

本章列出将本设计落地到现有 raygui 代码时，需要实际修改的点。按"改什么 / 怎么改 / 影响范围"组织。

### 5.1 全局 static 状态 → GuiContext

**改什么**：raygui 内部所有 `static` 的捕获 / 交互状态变量。

**怎么改**：
- 新建 `GuiContext` 结构（见 3.3），把下列 static 变量迁入：
  - `capturedId` / `tentativeId` / `hoverId`
  - hitStack 数组及其计数
  - press 起始位置 `startPos`、拖拽方向 `dragDirection`、仲裁确认标记 `isConfirmed`
  - TextBox/ValueBox 的 `editId`（单编辑焦点）、Dropdown 的 `dropdownOpenId`
- 提供一个内置默认 context（单 context 场景使用），由 `guiGetCurrentContext()` 返回当前作用域的 context 指针。
- 所有控件内部把 `capturedId` 等访问改为 `currentCtx->capturedId`。

**影响范围**：所有控件函数内部。控件函数签名不变。

### 5.2 输入源解耦

**改什么**：所有直接调用 `GetMousePosition / GetMouseButtonDown / GetMouseButtonPressed / GetMouseButtonReleased / GetTouch*` 的地方。

**怎么改**：
- 定义 `GuiInput` 结构（见 3.2）。
- 新增 `GuiBegin(GuiInput*, int)` / `GuiEnd()`，在 `GuiBegin` 时把输入存进当前 context。
- 新增默认适配器 `GuiBeginFromRaylib(void)`，内部从 raylib 填充 `GuiInput` 后调用 `GuiBegin`。保留它作为向后兼容的便捷入口。
- 控件内部读取输入改从 `currentCtx->inputs` 取。

**影响范围**：所有控件的输入读取点；新增两个公共 API。

### 5.3 Context 作用域（Push / Pop）

**改什么**：新增 `GuiPushContext / GuiPopContext`，维护一个 context 栈（实际只需支持一层平级切换，用单个 `currentCtx` 指针即可，但要校验不嵌套）。

**怎么改**：
- `GuiPushContext(ctx, bounds)`：校验当前无已激活 context（防止嵌套），将 `currentCtx` 切到 `ctx`，并据 bounds 为该 context 过滤可见 touchId。
- `GuiPopContext()`：恢复上一个 context（单级，即切回默认 context），清空当前 context 的 `inputs` 可见集。
- 内部用断言或错误日志拒绝嵌套调用。

**影响范围**：新增 2 个公共 API；控件无需改动。

### 5.4 两阶段 Capture + 命中栈

**改什么**：每个控件的 press / release / hover 判定逻辑。

**怎么改**：
- 每帧每个控件命中时向 `currentCtx->hitStack` push 自身 ID（用于 hover 取栈顶）。
- press 帧：记录 `tentativeId`、保存 `startPos`、`hitStackCount` 快照用于仲裁。
- 拖拽超过 threshold：按方向从快照栈顶向下找第一个支持该方向的控件，写入 `capturedId`，置 `isConfirmed=true`。
- release 帧：`if` 返回 true 仅当 `currentId == capturedId`。
- `GuiEnd` 时：清空 hitStack（每帧重建）；若已确认且无 down，释放 capture。

**影响范围**：所有离散控件（Button / Toggle / Dropdown）；连续控件（Slider / ScrollPanel）另行适配。

### 5.5 方向仲裁

**改什么**：ScrollPanel / Slider 等可拖拽控件需声明拖拽方向。

**怎么改**：
- 控件内部维护方向标志（水平 / 垂直 / 双向），供仲裁时查询。
- 仲裁函数：遍历 press 帧的 hitStack 快照，返回第一个支持该拖拽方向的控件 ID。

**影响范围**：ScrollPanel、Slider、以及任何可拖拽控件。

### 5.6 Hover 判定统一

**改什么**：当前每个控件各自用 `CheckCollisionPointRec` 决定是否高亮。

**怎么改**：
- 每帧构建 hitStack 后，无 capture 时栈顶为 hover 目标（`currentCtx->hoverId = hitStack[top]`）。
- 有 capture 时 `hoverId = capturedId`。
- 控件绘制高亮时统一判断 `currentId == currentCtx->hoverId`。

**影响范围**：所有控件的绘制分支。

### 5.7 TextBox 时序与内部编辑焦点

**改什么**：TextBox 的提交时机、编辑焦点生命周期、`if` 返回时机。

**怎么改**：
- **已落地（阶段一）**：`GuiTextBox/GuiValueBox/GuiValueBoxFloat/GuiSpinner/GuiDropdownBox`
  不再接收 `editMode` 参数（破坏性变更）。编辑态由 GUI 内部 `editId` 唯一决定，业务层不需要
  准备 `bool` 变量。
- press 帧命中：设置 `editId = 当前控件 ID`，进入编辑；不返回结果。
- 编辑中点击外部：press 帧清 `editId` 并记录提交标记；release 帧返回 `RESULT_PRESSED`，
  底层控件不触发。
- Enter / ESC：当帧提交并返回 `RESULT_PRESSED`（不存在对应的 release 事件）。
- buffer 全程由 GUI 直接写，业务层自己保留原值用于回滚；Cancel 时 GUI 清 `editId`。
- **同一 context 内同一时刻只有一个文本控件拥有编辑态**：点击另一个控件会提交旧焦点并切换；
  键盘输入只送当前编辑控件。键盘输入仍是全局的，多 context 同时编辑文本不在支持范围。
- **Spinner 只是布局辅助**：由两个 Button 和一个内部 ValueBox 组成，三者各自持有 capture ID，
  Spinner 本身不调用 `GuiControlBegin`。编辑中点箭头第一次只提交（模态），第二次才步进。
- **ValueBox / ValueBoxFloat 复用内部 GuiTextBox**：编辑期间保留文本 buffer，提交时才 parse
  数值；非法文本保留原值，整数提交时 clamp。这使它们获得完整的 TextBox 编辑交互。
- **Dropdown 展开态内部保存**（`dropdownOpenId`），也不需要业务 flag。
- 阶段二的 `GuiCancel(ctx)` 会清 `editId` 和展开中的 Dropdown。

**影响范围**：`GuiTextBox` / `GuiTextBoxMulti`、ValueBox 系列、Spinner、DropdownBox 及全部示例。

### 5.8 Slider / ScrollPanel 连续值返回语义

**改什么**：连续值控件 `if` 的返回判定。

**怎么改**：
- 仅在值相对上一帧发生变化时返回 true。
- release 帧若值未变化则不额外返回，避免与拖拽中重复。
- ScrollPanel 的三种滚动源：滚轮、ScrollBar 拖动、内容区拖动。内容区拖动读取
  `GuiContext.dragActive` / `dragDelta`，不保存每面板的上一帧指针。

**影响范围**：`GuiSlider*`、`GuiScrollPanel`。

### 5.9 Cancel 与失焦处理

**改什么**：窗口失焦 / context 销毁时的状态清理。

**怎么改**：
- 新增 `GuiCancel(ctx)`：清零该 context 的 capturedId / tentativeId / hoverId / dragDirection / isConfirmed / editId / dropdownOpenId；不触发任何控件。
- `GuiEnd` 或平台层检测到窗口失焦时调用 `GuiCancel`，而非模拟 touch up。
- context 被真正销毁（不再复用）前，调用方应主动 `GuiCancel(ctx)`；`GuiPopContext` 只退出
  作用域，保留 capture 以便下一帧继续。

**影响范围**：新增 1 个公共 API；平台集成层需挂接失焦事件。

### 5.10 Threshold 运行时配置

**改什么**：当前硬编码的拖拽判定像素值。

**怎么改**：
- 新增全局（或 context 内）`dragThreshold`，默认 5px。
- 提供 `GuiSetDragThreshold / GuiGetDragThreshold` 运行时读写。
- 高 DPI / 触屏设备可在初始化时调大。

**影响范围**：新增 2 个公共 API；仲裁判定处读取该值。

### 5.11 新增公共 API 汇总

**阶段一已落地：**

```c
// 帧 / 输入（宏输入便捷入口，保留）
void GuiBeginFrame(void);
void GuiEndFrame(void);
void GuiBeginFromRaylib(void);
void GuiEndFromRaylib(void);

// 查询
unsigned int GuiGetCapturedId(void);

// 编辑态签名变更（破坏性）
// 编辑/展开状态（阶段一已落地）
bool GuiIsEditing(void);
int GuiTextBox(..., char *text, int textSize);
int GuiValueBox(..., int *value, int minValue, int maxValue);
int GuiValueBoxFloat(..., float *value, float minValue, float maxValue);
int GuiSpinner(..., int *value, int minValue, int maxValue);
int GuiDropdownBox(..., int *active);
```

**阶段二已落地：**

```c
void GuiBegin(GuiInput* inputs, int inputCount);
void GuiBeginFromRaylib(void);     // 保留，改为内部调用 GuiBegin
void GuiEndFromRaylib(void);       // 保留，与 GuiBeginFromRaylib 配对
void GuiEnd(void);
void GuiPushContext(GuiContext* ctx, Rectangle bounds);
void GuiPopContext(void);
unsigned int GuiGetCapturedId(void);
void GuiCancel(GuiContext* ctx);
void GuiReleaseCapture(GuiContext* ctx);
void GuiSetDragThreshold(float threshold);
float GuiGetDragThreshold(void);
```

**已删除（阶段一）**：`GuiSetCapturedId()`、`GuiPushId()`、`GuiPopId()`。ID 由 GUI 内部自动分配。

**控件签名**：TextBox/ValueBox/ValueBoxFloat/Spinner/DropdownBox 移除了 `editMode` 参数，
其余 `GuiButton / GuiToggle / GuiSlider / GuiScrollPanel` 等控件签名不变。
