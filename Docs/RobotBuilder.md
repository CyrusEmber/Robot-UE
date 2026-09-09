# 物理机器人建造系统 — 功能文档

> 模块：`Source/Robot/RobotBuilder/`
> 版本：v1（2026-09-09）
> 关联文档：`PLAN_RobotBuilder.md`（初始设计）

---

## 1. 概述

第三人称沙盒：玩家用 UI 选择部件，在摄像机 trace 命中点自由摆放建造物理机器人；部件由**图论结构**（节点=部件、边=物理约束）连接，带**双血量**（本体+接头）；碰撞/攻击掉血，断连子树**整块脱落**；建造完成后可驾驶（轮子扭矩 + 推进器推力）。

## 2. 操作说明

| 输入 | 行为 |
|------|------|
| 左键 | Build：放核心（指地面）/ 挂接部件（指部件）；Demolish：拆除命中部件；Drive：选中驾驶目标 |
| 右键 | 对命中部件造成攻击伤害（同时伤本体与接头） |
| Tab | 循环切换 Build → Demolish → Drive |
| 滚轮 | 待放置部件 yaw 旋转（步进 15°） |
| F | 待放置部件 pitch 旋转（步进 45°） |
| WASD | Drive 模式：轮子驱动 + 坦克转向（进出 Drive 模式时与移动 IMC 互斥交换，驾驶时角色不走动） |
| Space | Drive 模式：推进器推力（按住） |

放置规则：命中已有部件 → 挂接（建边）；命中地面 → 只有 Core 类部件能新建机器人；部件放置前有 ghost 预览。

## 3. 架构

### 3.1 图论模型

```
节点 Node      = ARobotPart        每部件一个 actor，独立刚体（物理模拟）
边   Edge      = FRobotLink        UPhysicsConstraintComponent + 接头血量 JointHP
图   Graph     = ARobot            节点表 Parts + 边表 Links + 邻接表（按需重建）
连通域 Cluster = 从 CorePart 可 BFS 到达的所有节点
```

- **建边**：`ARobot::AddPart` —— 约束锚定在 trace 命中点（Frame1/Frame2 同锚），位置全锁定；轮子部件额外释放 Twist 自由度（铰链，轴 = 部件局部 X）
- **切分**：`ARobot::RunConnectivityPass` —— BFS 从核心算可达集，**只切断"跨界边"**（一侧可达一侧不可达），掉落子树内部约束保留 → **整块脱落**（cluster 语义：脱落块仍是一个整体刚体簇，可推动、可继续拆）
- **拆除**：`RemovePart` 销毁节点 + 断开引用它的所有边（含残骸块内部），再跑连通性 → 在断点分裂
- **残骸**：部件保留 robot 归属（`MarkAsDebris` 只置 `bDead`），图仍能对残骸块做清理；`DebrisLifetime=0` 永久，>0 到期走 `RemovePart` 安全消失

### 3.2 双血量

| 血量池 | 归属 | 归零效果 |
|--------|------|----------|
| 本体 HP | `ARobotPart.CurrentHP` | 部件坏死 → 断开所有边 → 变残骸 |
| 接头 JointHP | `FRobotLink.JointHP` | 边断裂 → 连通性重算 → 子树整块脱落 |

伤害来源：
- **碰撞**：`OnComponentHit` 的 `NormalImpulse`；`≥ MinImpactImpulse` 才结算，伤害 = `(冲量-阈值)×ImpulseDamageScale`，同时扣本体与接头；`≥ BreakImpulse` 直接断边（"被撞掉"）；同机器人自碰撞不结算；0.1s 冷却防连帧刷伤害
- **攻击**：右键 → `ApplyAttackDamage`（走定义的 `AttackDamage`，同样双扣）

### 3.3 重心

每个部件独立刚体，增删自动涌现。`ARobot::GetCenterOfMassWorld()` = Σ(m·COM)/Σm（只算连通域）；Drive 模式下画面画红点+黄箭头（`bDrawCenterOfMass`）。

### 3.4 驾驶

`ARobot::ApplyDriveForces`（每帧 Tick）：
- 轮子：`AddTorqueInRadians(局部X × 扭矩 × 输入)`，转速封顶 `MaxWheelSpinRate`；松开时按 `WheelBrakeFactor` 比例反向扭矩刹车
- 转向：坦克式——轮子按"相对核心的左右侧"取反输入
- 推进器：`AddForceAtLocation(局部+X × 推力 × 输入, 部件位置)`

### 3.5 代码地图

| 文件 | 职责 |
|------|------|
| `RobotTypes.h` | 部件类别枚举 + `FRobotLink` 边结构 |
| `RobotPartDefinition.h` | 部件类型 DataAsset（全部参数） |
| `RobotPart.h/.cpp` | 图节点：刚体、HP、碰撞伤害、死亡上报 |
| `RobotAssembly.h/.cpp` | 图管理：AddPart/RemovePart/BFS/质心/驾驶/散架 |
| `RobotBuilderComponent.h/.cpp` | 交互：三模式、trace、ghost 预览、旋转、攻击、驾驶转发（挂在 PC 上） |
| `RobotBuilderWidget.h/.cpp` | 纯 C++ UMG：部件/模式按钮 + 状态栏 |
| `RobotBuilderFunctionLibrary.h/.cpp` | 编辑器工具：`CreateBuilderAssets()` 一键建 13 个资产、`MapKeyByName` |
| `RobotBuilderCharacter/PlayerController/GameMode` | Pawn / PC（输入绑定 + 资产自动接线）/ GM |

## 4. 资产规范

### 4.1 目录约定

```
Content/
  Input/
    Actions/IA_*          输入动作
    IMC_Builder / IMC_Drive
  RobotParts/DA_*         部件定义（DataAsset）
  ThirdPerson/            模板地图（启动图）
Source/Robot/RobotBuilder/  本功能全部代码
Tools/create_robotbuilder_assets.py  一键重建资产脚本（幂等，重跑覆盖值）
```

### 4.2 RobotPartDefinition 字段

| 字段 | 说明 | 默认 |
|------|------|------|
| PartName / Category | UI 显示名 / Core·Wheel·Thruster·Armor | — |
| Mesh | 部件网格（轮子建议轴向为局部 X） | — |
| Mass (kg) | 0 = 用网格默认质量 | 50 |
| MaxHP / MaxJointHP | 本体 / 接头血量 | 100/100 |
| BreakImpulse | 单次冲量 ≥ 此值直接撞断接头 | 60000 |
| MinImpactImpulse / ImpulseDamageScale | 冲量伤害门槛 / 系数 | 3000 / 0.004 |
| AttackDamage | 右键单次伤害 | 25 |
| WheelTorque / MaxWheelSpinRate / WheelBrakeFactor | 驱动参数（cm 制：kg·cm²/s²） | 4e5 / 40 / 0.35 |
| ThrustForce | 推力（kg·cm/s²），沿局部 +X | 80000 |
| DriveTorqueBudget / ThrustForceBudget | **核心动力预算**：全机器人轮子扭矩 / 推进器推力需求超过预算时等比摊薄 → 质量大、轮子多只是加速变慢（物理涌现，不会趴窝） | 1e6 / 1.6e5 |

**新增部件类型 = 建一个 DA 资产**（右键 → Miscellaneous → RobotPartDefinition），填参数即可出现在 UI；无需改代码。

### 4.3 输入资产

全部由 `Tools/create_robotbuilder_assets.py`（或 C++ `CreateBuilderAssets`）创建，路径固定，PC 构造函数 `ConstructorHelpers` 自动接线——**改键位直接改 IMC 资产，不动代码**。

## 5. 项目管理约定

### 5.1 Git 工作流

- `main` 保持可编译：每次提交前先跑一遍 `Build.bat RobotEditor Win64 Development -project=...`
- 一个功能一个 commit，消息格式：`<动词> <内容>`（如 `Add ...`、`Fix ...`），中文亦可
- `.uasset` 是二进制：**避免多人同时改同一资产**（单人项目无碍）；资产与引用它的代码尽量同一 commit
- 大文件（>50MB）禁止直接入 Git；未来加角色/大贴图时考虑 Git LFS
- 生成物已忽略：`Binaries/ Intermediate/ Saved/ DerivedDataCache/ Backup/ *.sln*`

### 5.2 内容与代码的边界

| 层 | 放什么 |
|----|--------|
| C++ | 系统、机制、图逻辑、输入绑定 |
| DataAsset（DA_*） | 数值、外观、部件种类 —— **纯数据扩展不动代码** |
| Blueprint | 只做轻量外观/特效子类，逻辑尽量不上蓝图 |

### 5.3 文档

- `PLAN_RobotBuilder.md`：历史设计（不再更新）
- `Docs/RobotBuilder.md`：本文件，**功能现状的活文档**，改架构必须同步改这里
- 新功能建议各建一个 `Docs/<功能名>.md`

### 5.4 回归测试清单（改图逻辑后必测）

1. 放核心 → 挂轮/装甲/推进器，各能挂上且带动
2. 攻击掉血 → 血量归零部件坏死脱落
3. 大冲量撞 → 直接撞掉
4. 链条中间打断 → 上段子树**整块**掉落（内部不散架）
5. 拆除块内部件 → 块在断点分裂
6. Drive：WASD 走、Space 飞、松开刹车
7. 拆核心 → 整机散架，驾驶目标自动清空

## 6. 已知边界 & 扩展路线

**当前 残骸块不能重新挂回机器人（方案 B 待做）
- 两个机器人不能拼合（方案 C 待做）
- 部件无朝向吸附辅助、无镜像摆放
- UI 为代码生成的简易面板，无血条显示

**路线图**
| 优先级 | 项 | 说明 |
|--------|-----|------|
| 高 | 部件血条 | 命中部件时头顶/角标显示 HP 与接头 HP |
| 高 | 碎片重挂接（B） | 残骸块整体吸附回机器人，合并邻接表 |
| 中 | 机器人合体（C） | 两 ARobot 拼一个，核心重定向 |
| 中 | 武器部件 | 新类别 + 射击走 TakeDamage 通道 |
| 中 | 存档 | 图序列化（节点 transform + 定义引用 + HP） |
| 低 | 镜像/吸附辅助、音效、可破坏特效 |
