# 物理机器人建造游戏 — 实施计划

> 工程：`Robot.uproject`（UE 5.8，C++ 模块 `Robot`）
> 模式：第三人称沙盒建造 + 物理 + 驾驶
> 日期：2026-09-08（v2：取消 Variant 清理，全部保留）

---

## 1. 游戏设计目标

- 通过 **UI 选择部件**（核心 / 轮子 / 推进器 / 护甲），在 **摄像机 trace 命中点** 自由摆放生成
- 部件**支持旋转**（待放置预览 + 滚轮 yaw + 按键 pitch）
- **拆除模式**：移除当前 trace 命中的部件
- 每个部件有 **本体耐久（HP）**，受碰撞 / 攻击降低
- **连接（接头）有独立耐久（JointHP）**，单次冲量超过阈值 → 直接被撞掉
- 部件脱落 / 拆除后：**图连通性判定（BFS）**，与核心失去连通的子树整体脱落
- **重心随部件增删实时变化**（物理自动涌现 + 暴露 COM 查询 / 调试绘制）
- 建造完成后可 **驾驶**：WASD 轮子扭矩（坦克转向）、Space 推进器推力

## 2. 核心架构 — 图论链接

```
        ┌─────────── ARobot（图管理器）───────────┐
        │  Nodes: ARobotPart（部件 actor）         │
        │  Edges: FRobotLink（物理约束 + JointHP） │
        │  Adjacency + BFS from CorePart           │
        └──────────────────────────────────────────┘

  Node(部件) ──Edge(UP锚定接触点)──> Node(父部件)

  破坏规则：
    - 部件本体 HP = 0   → 打断它参与的所有边 → 部件变残骸
    - 边 JointHP = 0    → 打算连通性
    - 任一节点失去到 Core 的路径 → 该节点及其子树整体脱落（残骸仍受物理驱动）
    - Core 死亡/被拆 → 全机器人散架，ARobot 自毁
```

**双血量设计**

| 血量池 | 归属 | 归零效果 |
|--------|------|----------|
| 本体 HP | `ARobotPart` | 部件坏死，断开所有连接，脱落成残骸 |
| 连接 JointHP | `FRobotLink`（边） | 该约束被打断，触发连通性重算 |

**伤害来源**
- 碰撞：`OnComponentHit` 的 `NormalImpulse`，超过 `MinImpactImpulse` 按 `(冲量-阈值)*系数` 同时伤本体与接头；超过 `BreakImpulse` 直接断边（"被撞掉"）
- 攻击：右键对 trace 命中部件调 `TakeDamage`（标准伤害通道），同时伤本体与接头
- 同一机器人内部自碰撞不计伤害

**重心**
- 每个部件独立刚体（物理模拟），约束连接 → 增删部件时整体行为自动变化
- `ARobot::GetCenterOfMassWorld()` = Σ(m·COM)/Σm，仅统计连通部件；用于调试绘制与后续玩法

## 3. 配置调整（不删除任何现有内容）

| 操作 | 对象 | 内容 |
|------|------|------|
| 修改 | `Robot.Build.cs` | 仅**新增**：include 路径 `Robot/RobotBuilder`、依赖 `PhysicsCore`（variant 路径与依赖全部保留） |
| 修改 | `Config/DefaultEngine.ini` | `GlobalDefaultGameMode=/Script/Robot.RobotBuilderGameMode`（一行，让默认地图直接进建造玩法；不想动默认可改为在地图 World Settings 里覆盖 GameMode） |
| 保留 | Variant 代码与资源 | 不做任何清理 |

## 4. 阶段 1 — 核心系统实现

### 4.1 新增文件（`Source/Robot/RobotBuilder/`）

| 文件 | 类 | 职责 |
|------|----|------|
| `RobotTypes.h` | `ERobotPartCategory`（Core/Wheel/Thruster/Armor）、`FRobotLink` | 图的边结构与类别枚举 |
| `RobotPartDefinition.h/.cpp` | `URobotPartDefinition : UPrimaryDataAsset` | 部件类型配置：网格、质量、本体HP、接头HP、断开冲量、冲量伤害系数、轮子扭矩/转速上限、推进器推力 |
| `RobotPart.h/.cpp` | `ARobotPart : AActor` | 部件节点：StaticMesh 物理模拟、本体HP、碰撞冲量伤害、死亡上报、残骸标记 |
| `Robot.h/.cpp` | `ARobot : AActor` | 图管理器：AddPart/RemovePart/BreakLink、BFS 连通性、质心、驾驶 tick、部件与机器人事件委托 |
| `RobotBuilderComponent.h/.cpp` | `URobotBuilderComponent : UActorComponent` | 建造组件（挂 PlayerController）：Build/Demolish/Drive 三模式、trace、放置预览、旋转、攻击、驾驶输入转发 |
| `RobotBuilderWidget.h/.cpp` | `URobotBuilderWidget : UUserWidget` | 纯 C++ 构建 UMG：部件按钮列表 + 模式切换 + 状态文本（当前模式/选中部件/驾驶目标血量） |
| `RobotBuilderCharacter.h/.cpp` | `ARobotBuilderCharacter : ARobotCharacter` | 具体 Pawn（基类 abstract） |
| `RobotBuilderPlayerController.h/.cpp` | `ARobotBuilderPlayerController : ARobotPlayerController` | 具体 PC：持有 Builder 组件、暴露输入资产引用、绑定 EnhancedInput |
| `RobotBuilderGameMode.h/.cpp` | `ARobotBuilderGameMode : ARobotGameMode` | DefaultPawn = RobotBuilderCharacter，PC = RobotBuilderPlayerController |

### 4.2 自由摆放 + 旋转

- 选中部件后，每帧从摄像机 trace：
  - 命中**已有部件** → 预览吸附在命中点（沿命中法线偏移该部件包围盒半尺寸），点击挂接（建边）
  - 命中**世界**（地面等）且当前部件类别为 **Core** → 预览 + 点击生成新机器人（新 ARobot + 核心节点）
  - 命中世界且非 Core → 预览变红 / 不放置（提示：需要先命中机器人或选择核心）
- **旋转**：滚轮 = yaw 步进（默认 15°，可配置）；`F` 键 = pitch 步进 45°；预览实时反映
- 轮子默认朝向自动对齐（轴向垂直于视线水平方向），再叠加用户旋转

### 4.3 驾驶

- Drive 模式下点击机器人 → 绑定为驾驶目标
- WASD（IA Axis2D）→ `ARobot::SetDriveInput(FVector2D)`
  - 轮子：`AddTorqueInRadians(轴向 * 扭矩 * 输入)`，转速封顶；左右轮按相对核心的侧向偏移取反 → 坦克转向；松手反向扭矩刹车
  - 转向参考系：核心部件朝向
- Space 按住 → `SetThrustInput(1)`：每个推进器沿自身局部 +X `AddForceAtLocation`
- 驾驶目标死亡 → 委托清空选择

### 4.4 残骸

- 脱落部件 = 无约束的独立模拟刚体，保留在世界（`DebrisLifetime` 可配，0=永久）
- 残骸可继续被物理推动、可被点击拆除（直接销毁）

## 5. 输入设计（蓝图资产，C++ 只做绑定）

> IMC / IA 全部为蓝图资产，由你在编辑器创建后填入 `BP_RobotBuilderPlayerController`（或 `ARobotBuilderPlayerController` CDO）的 EditAnywhere 属性。

### 5.1 需创建的资产（建议放 `Content/Input/`）

**Input Action（`Content/Input/Actions/`）**

| 资产名 | Value Type | 说明 |
|--------|-----------|------|
| `IA_RobotClick` | Digital (bool) | 左键：放置 / 拆除 / 选中驾驶目标（随模式） |
| `IA_RobotAttack` | Digital (bool) | 右键：对瞄准部件造成伤害 |
| `IA_RobotCycleMode` | Digital (bool) | Tab：切换 Build → Demolish → Drive |
| `IA_RobotRotateYaw` | Axis1D (float) | 滚轮：待放置部件 yaw 旋转 |
| `IA_RobotRotatePitch` | Digital (bool) | F：pitch 步进 |
| `IA_RobotDriveMove` | Axis2D (Vector2D) | WASD：驾驶输入（仅 Drive 模式生效） |
| `IA_RobotThrust` | Digital (bool) | Space：推进器推力 |

**Input Mapping Context**

| 资产名 | 映射 | 优先级 |
|--------|------|--------|
| `IMC_Builder` | LMB→IA_RobotClick，RMB→IA_RobotAttack，Tab→IA_RobotCycleMode，Wheel→IA_RobotRotateYaw，F→IA_RobotRotatePitch | 0（常驻） |
| `IMC_Drive` | W/S→IA_RobotDriveMove(Y ±)，A/D→IA_RobotDriveMove(X ±, 负向加 Negate)，Space→IA_RobotThrust | 1（仅 Drive 模式叠加，屏蔽角色移动 WASD） |

### 5.2 C++ 侧约定

- `ARobotBuilderPlayerController` 暴露以上资产引用（`EditAnywhere`），`SetupInputComponent` 中空指针安全绑定
- 模式切换时由 `URobotBuilderComponent` 增删 `IMC_Drive`（EnhancedInput LocalPlayer Subsystem）
- 模板自带 `IMC_Default`（角色移动）保留不动

## 6. 阶段 2 — 验证与交付

1. 编译 `RobotEditor` target（UBT），修复错误至零警告通过
2. 编辑器内操作指引（交付时给出详细步骤）：
   - 创建 4 个 `RobotPartDefinition` 资产（Core/Wheel/Thruster/Armor 示例参数 + 引擎基础形状网格）
   - 创建 5.1 的 IMC/IA 资产并配置按键
   - 填入 PlayerController CDO / BP
   - PIE 开玩：放核心 → 挂轮子/装甲/推进器 → 撞墙掉血 → 大冲击撞掉部件 → 断连子树脱落 → 拆除模式拆件 → Drive 模式驾驶

## 7. 风险与备忘

- 物理约束运行时创建：`SetWorldLocation/SetWorldRotation → SetConstrainedComponents`，Frame1/Frame2 双锚定接触点；轮子用 Twist Free + Swing Locked 铰链（轴 = 局部 X）
- UE 力单位为 cm 制（kg·cm/s²），默认参数按 50kg 部件量级给出，全部可调
- UMG 纯 C++ 构建（VerticalBox + Button + TextBlock），不依赖 Widget 蓝图
- 预览材质默认用引擎 `/Engine/BasicShapes/BasicShapeMaterial`，可用 `PreviewMaterial` 属性覆盖
- Variant 代码保留：新系统全部位于 `RobotBuilder/` 子目录，互不影响

## 8. 执行顺序

```
① 改 Build.cs（新增路径 + PhysicsCore）与 ini GameMode → ② 写 16 个新文件
→ ③ 编译修复 → ④ 交付使用说明（资产创建清单）
```
