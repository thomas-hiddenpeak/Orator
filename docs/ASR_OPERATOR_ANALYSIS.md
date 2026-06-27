# Orator ASR 与强制对齐管线 — 算子优化分析（准确版）

**修订日期：** 2026-06-27
**范围：** Qwen3-ASR 推理管线（mel → 音频塔 encoder → 文本 decoder）+ Qwen3 强制对齐 LM
**方法：** 逐行代码核对（非臆测）。本文取代上一版由较弱模型生成的分析——那一版有多处与真实代码不符（见 §2）。
**宪法约束：** 纯 C++20/CUDA 运行时；可用 CUDA 库（cuBLAS/cuFFT/cuBLASLt），**不引入 cuDNN 或其他第三方运行时依赖**（现有代码刻意手写注意力而非 cuDNN，保持该取向）。准确率优先：任何算子改动必须对 PyTorch/NeMo oracle 复验后才算完成。

---

## §1 真实算子栈（已核对）

### 1.1 特征提取
| 算子 | 文件:函数 | 精度 | 实现 |
|---|---|---|---|
| 功率谱 | `whisper_mel.cu:PowerSpectrumKernel` | f32 | **手写直接 DFT**（反射 pad，一块一帧），非 cuFFT |
| Mel+log10 | `whisper_mel.cu:MelLog10Kernel` | f32 | Mel 矩阵乘 + log10 |
| Whisper 归一化 | `whisper_mel.cu:WhisperNormKernel` | f32 | clamp(gmax-8) + (v+4)/4 |

### 1.2 音频塔 Encoder（`asr_audio_tower.cu`）— **24 层纯 Transformer**（非 Conformer）
| 阶段 | 文件:函数 | 精度 | 实现 |
|---|---|---|---|
| Conv 前端 ×3 | `Im2ColKernel` + `asr_gemm::LinearPre` | bf16 权重 / f32 io | im2col(stride2,3×3) + cuBLAS bf16 GEMM；1→480→480→480 |
| Conv 重排+GELU | `ConvOutRearrangeKernel` | f32 | bias + 精确 erf GELU |
| conv_out 投影 | `asr_gemm::Linear` 7680→1024 | bf16 W | 无 bias |
| 位置编码 | `AddPeChunkedKernel` | f32 | 正弦 PE 按块注入 |
| LayerNorm | `asr_audio_tower.cu:LayerNormKernel` | f32 | **LayerNorm**（均值+方差，非 RMSNorm） |
| 注意力 | `WindowAttnKernel` | f32 | 双向、**在线 softmax**、无 [T,T] 矩阵；窗口可选（`ORATOR_ASR_WINDOWED`） |
| FFN | `asr_gemm::Linear`(fc1,act=GELU) + `Linear`(fc2) | bf16 W | GELU 在 GEMM 外（`BiasActKernel`） |
| 末端投影 | ln_post + proj1(GELU) + proj2 → 2048 | bf16 W | — |

同步点：CNN 后、drop-pad 后、conv_out 后、24 层后、输出 DtoH —— **每次 Forward 5 个 `cudaStreamSynchronize`**。

### 1.3 共享算子库（`asr_ops.cu`）—— ASR decoder 与对齐 LM 共用
| 算子 | 函数 | 精度 | 实现 |
|---|---|---|---|
| RMSNorm | `RmsNormKernel` | f32 | 一块一行，shared reduce |
| RoPE（interleaved / rotate-half） | `RopeKernel` / `RopeHalfKernel` | f32 精确 sin/cos | HF Qwen3 形式 |
| SwiGLU | `SwiGLUKernel` | f32 精确 expf | SiLU(gate)×up |
| GQA 注意力 | `GqaAttnWarpKernel<ELEMS>` | f32 | **一 warp 一 (token,head)**，寄存器持 query，**在线 softmax**，causal 可选；无 [T,T] |
| Argmax(禁词) | `ArgmaxBannedKernel` | f32→int | 单块 reduce，支持 ~152k 词表 |

### 1.4 文本 Decoder（`asr_text_decoder.cu`）
| 算子 | 函数 | 精度 | 实现 |
|---|---|---|---|
| 嵌入查表 | `EmbedGatherKernel` | bf16 表→f32 | — |
| KV 写缓存 | `WriteKvCacheKernel` | f32→bf16 | `[seq,Hkv,Dh]` 行主序，pos0 从 device 标量读（graph 兼容） |
| Prefill GQA | `GqaCacheAttnKernel` | f32 Q / bf16 KV | 一 warp 一 (token,head)，causal |
| Decode GQA | `GqaDecodeAttnKernel` | f32 Q / bf16 KV | **一块一 query head**，两遍（score/context），shared scores `[2048]` |
| 已有 CUDA Graph | `graph_exec_` | — | **仅 decode 步**已用图捕获 |

GQA 配置：Hq=16, Hkv=8（2:1），Dh=128。KV cache bf16（半内存）。

### 1.5 对齐 LM（`qwen3_aligner_lm.cu`）—— 28 层因果 NAR
- **100% 复用** `asr_ops::{RmsNorm,RopeHalf,GqaAttention}` + `asr_gemm::Linear`；唯一自写 kernel 是 `AddInPlaceKernel`（残差）。
- 运行在**默认流 s=0**（`qwen3_aligner_lm.cu:108`），结尾 `cudaStreamSynchronize(s)`。全 DeviceBuffer。

### 1.6 GEMM（`asr_gemm.cu`）—— 性能核心
- `Linear`：f32 输入 → **`F32ToBf16` 转 bf16** → `cublasGemmEx(bf16,bf16→f32, FP32 累加, GEMM_DEFAULT)` → 可选 `BiasActKernel`（bias+GELU/ReLU，**在 GEMM 外**）。
- `LinearPre`：输入已是 bf16，跳过转换（q/k/v 共享输入时用）。
- **M=1 解码**：手写 `GemvBf16Kernel`（一 warp 一行、coalesced bf16 权重读、half2、x 暂存 shared）——已是高级优化，优于 cuBLAS 的 M=1 转置路径。
- `g_scratch`：**文件级全局可变** bf16 暂存（`asr_gemm.cu:23`），容量增长时 `cudaFree`+`cudaMalloc`。

---

## §2 上一版（弱模型）分析的错误更正
| 弱版声称 | 真实代码 | 结论 |
|---|---|---|
| Conformer **30 层** | `encoder_layers=24`（`asr_audio_tower.h:37`）纯 Transformer | ❌ 架构+层数错误 |
| cuDNN **flash attention** | 手写 `GqaAttnWarpKernel`/`WindowAttnKernel`，无 cuDNN | ❌ 库错误 |
| **cublasSgemm**（f32） | `cublasGemmEx` **bf16** Tensor Core，FP32 累加 | ❌ 精度/路径错误 |
| **GeluApprox**（不精确） | 精确 `erff` 式 GELU | ❌ 方向反了 |
| Conv1d 手写逐滤波器 | **im2col + cuBLAS GEMM** | ❌ 实现错误 |
| LayerNorm 全程 | encoder=LayerNorm，decoder/aligner=**RMSNorm**（混用） | ⚠️ 半对 |
| 漏掉手写 GEMV、漏掉 g_scratch、漏掉 decode 已有 CUDA Graph | — | ⚠️ 重大遗漏 |
| 无内存池 / encoder 无 CUDA Graph / bias+act 未融合 | 属实 | ✅ 正确 |

---

## §3 性能 ↔ 准确率「共享路径」（你的判断成立）

许多改动**同时**降低耗时与提升/保护数值正确性，因为：每个算子重写都要对 oracle 复验（Art. II），同一套验证既挡性能回归也量化精度；而「融合」既减少 kernel 启动又减少中间 f32 写回（更少舍入）。

| 共享项 | 性能收益 | 准确率/正确性收益 |
|---|---|---|
| **per-stream/per-context GEMM scratch**（替换 `g_scratch`） | 解除 ASR↔aligner 串行，可真并发 | **消除跨流共享暂存的数据竞态**（aligner 用 s=0、ASR 用 asr_stream，并发调 `Linear` 时 `g_scratch` 被同时写/realloc） |
| **bias+激活 epilogue 融合**（cuBLASLt `GELU_BIAS`） | 去掉 `BiasActKernel` 整趟读写 | epilogue 用 FP32，不增舍入；激活路径与 oracle 对齐更可控 |
| **q/k/v（及 gate/up）投影合并为一个 GEMM** | 少 2 次 kernel 启动 + 少 2 次 f32→bf16 转换 | 单次 cast 减少重复量化噪声 |
| **激活量化精度可调**（bf16↔fp16/tf32 路径开关，按层验证） | 维持 Tensor Core 速度 | 对精度敏感层（score head、proj2）可换更高精度，**直接抬高对齐/转写准确率** |
| **per-stage 计时探针 + oracle 对拍一体化** | 指出真正热点，避免盲优化 | 同一探针回归测精度容差 |

---

## §4 优先级路线图（先测后改）

### P0 — 测量基线（先做，零风险）
- 在真实 WS 流式路径上加 per-stage 计时（mel / conv 前端 / 24 层 encoder / decoder prefill / decode / aligner），输出每阶段 ms 与占比。**没有分阶段数据就不要动算子。**
- 现有 `compute_sec` 仅到 worker 粒度；需细化到 encoder/decoder/aligner 子阶段。

**已测（2026-06-27，真实 WS rate=0，120s，39 段）：**
- **对齐管线分阶段**（`ORATOR_ALIGN_PROFILE=1`，`qwen3_forced_aligner.cc:Align`）：
  mel 46ms / **tower 3262ms (58%)** / asm 9ms / **lm 2271ms (41%)** / decode 14ms。
  → **音频塔 encoder 是首要热点**，LM 次之；mel/装配/解码可忽略。
- **关键共享点**：`AsrAudioTower::Forward` 同时是 ASR encoder 与 aligner encoder（同类、不同权重/`output_dim`）。优化它**一次利好两条管线**。
- ASR 流式路径（StreamChunk/StreamFinalize）当前**不经被插桩的 `BuildAndRun`**，故无 `[asr-profile]` 输出；下一步需在流式入口补计时以拿到 ASR encoder/decode 分布。
- `AsrAudioTower::Forward` 每次 **8 个 `cudaStreamSynchronize`**（`asr_audio_tower.cu:370,383,405,418,432,522,538,547`）——比初估的 5 个更多，每个都是 GPU 停顿。

### P1 — 高收益 / 低风险（性能优先）
1. **per-context GEMM scratch**（替换全局 `g_scratch`）——同时修竞态，解并发。✅ **已完成（thread_local，见 §5）**
2. **encoder 端去除多余 `cudaStreamSynchronize`**（8→必要的少数），用事件替代；为定形的 encoder Forward 加 **CUDA Graph**（decode 已有先例）。
3. **conv 前端 im2col 临时缓冲预分配/复用**（去掉每次 Forward 的 UnifiedBuffer 分配；UnifiedBuffer→DeviceBuffer 避免 Tegra managed 迁移）。

### P2 — 中收益 / 中风险（性能，需复验）
4. **cuBLASLt epilogue 融合** bias+GELU（覆盖 encoder FFN/proj + decoder/aligner Linear）。
5. **q/k/v 与 gate/up 投影合并 GEMM**（encoder + decoder + aligner LM 三处同构）。
6. **KV cache 预分配**（按 `max_audio_tokens` 一次性分配，去掉 prefill 期重分配）。

### P3 — 准确率专项（与上面共用验证）
7. **GELU `erff` vs PyTorch exact erf 偏差量化**；必要时换更精确式。
8. **精度敏感层激活路径**（score head 5000、proj2）评估 fp16/tf32 vs bf16 对 CER / 对齐误差的影响，逐层开关。

### P4 — 探索 / 高成本
9. encoder 增量编码（若实测 encoder 为热点且存在重复编码）。
10. 全 encoder 层融合（最大化减启动，工程量大）。

---

## §5 关键正确性发现（建议 P1 优先处理）
- **`g_scratch` 跨流竞态**：~~`asr_gemm.cu:23` 的全局 bf16 暂存被所有 `Linear` 共用~~ —— **已修复（2026-06-27）**。核实结论：生产默认 `kAsrOnly` 模式下 ASR（own_stream）无锁、aligner **完全不持 `DeviceGuard`** 跑默认流，二者并发调 `asr_gemm::Linear` 时共享 `g_scratch` 与 `g_handle`（`cublasSetStream` 改 handle 状态、`Scratch()` 增长时 `cudaFree`+`cudaMalloc` 可能释放另一线程排队 GEMM 正读的显存——疑为先前 WS 验证一次神秘崩溃的根因）。已将 `g_handle`/`g_scratch`/`g_scratch_cap` 改为 **`thread_local`**（每管线线程独立），消除竞态并允许真并发。`ctest` 45/45、WS 路径稳定。

## §6 验证方法（每项改动都走）
1. 构建零告警（`-Wall -Wextra`）+ `ctest` 全绿（现 45/45）。
2. 算子级对 oracle 复验：`test_asr_encoder`/`_decoder`/`test_aligner_*` 容差不退化（bf16 ~3e-3）。
3. 真实 WS 路径（rate=0 与 rate=1）测 per-stage 计时 + 转写 CER / 对齐误差，对比改动前后。
4. 同步更新 `specs/PROJECT_STATE.md`（Art. VIII）。

## §7 建议的第一步
先做 **P0 计时探针** + **P1.1 per-context scratch（含竞态确认）**：前者给出真实热点分布以指导后续，后者是低风险、同时利好正确性与并发的「共享路径」首选。两者落地后再按实测数据推进 P1.2/P1.3。

## §8 进度（2026-06-27）
- ✅ **P1.1 完成**：`asr_gemm` 的 handle+scratch 改 `thread_local`，消除 ASR↔aligner 竞态（§5）。
- ✅ **P0 部分完成**：aligner 分阶段计时落地并实测——**tower 58% / lm 41%**，音频塔为首要共享热点（§4 P0）。
- ⏭ **下一步**：(a) 给 ASR 流式入口补计时拿到 encoder/decode 分布；(b) 塔内细分（conv 前端 vs 24 层 vs 8 个 sync），再据此攻 P1.2（去 sync + CUDA Graph）/ P1.3（im2col 预分配）。攻 encoder 时每步对 `test_asr_encoder` / `test_aligner_*` oracle 复验，容差不退化。
