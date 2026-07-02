# Orator C++20 特性扫描报告

**生成时间:** 2026-06-28
**扫描范围:** `include/` (63 个头文件) + `src/` (56 个源文件)
**优先级排序:** std::span > std::format > std::string_view > std::optional > 其他

> 过渡阶段实验说明：本文是代码现代化候选清单，不是已批准的实施计划。
> 所有建议仍需逐项核对当前代码、遵守宪法的零运行时依赖和精度优先规则，
> 并在对应 spec/plan/tasks 中记录验证方法后才能进入实现。

---

## 1. std::span（最高优先级）

### 1.1 `const std::vector<T>&` 参数 → `std::span<const T>`

| 文件 | 位置 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/feature/mel_spectrogram.h` | L44-45 | `MelSpectrogram(const MelConfig& config, const std::vector<float>& window, const std::vector<float>& filterbank)` | `std::span<const float>` — 构造函数接受外部窗口/滤波器，无需拥有 |
| `include/model/forced_align_decode.h` | L56 | `std::vector<long> FixTimestamps(const std::vector<double>& raw_ms)` | `std::span<const double>` — 纯计算函数，只读输入 |
| `include/model/forced_align_decode.h` | L60 | `std::vector<AlignUnit> PairWordTimestamps(const std::vector<std::string>& words, const std::vector<long>& fixed_ms)` | 两个参数均可改为 `std::span<const ...>` |
| `include/model/forced_align_decode.h` | L50 | `const std::vector<std::vector<int>>& word_token_ids` | 外层可改为 `std::span<const std::vector<int>>` |
| `include/model/qwen3_aligner_lm.h` | L57 | `std::vector<float> Forward(const std::vector<int>& input_ids, ...)` | `std::span<const int>` — 推理入口，高频调用 |
| `include/model/streaming_sortformer.h` | L180 | `std::vector<float> ForwardEncoderDecoder(const std::vector<float>& emb_seq, ...)` | `std::span<const float>` — 推理入口，高频调用 |
| `include/io/bpe_tokenizer.h` | L30 | `std::string Decode(const std::vector<int>& ids, ...)` | `std::span<const int>` — 只读解码 |
| `src/model/streaming_sortformer.cc` | L36 | `void GetLogPredScores(const std::vector<float>& preds, int n, int n_spk, ...)` | `std::span<const float>` — 内部工具函数 |
| `src/model/streaming_sortformer.cc` | L63 | `void DisableLowScores(const std::vector<float>& preds, int n, int n_spk, ...)` | 同上 |
| `src/model/streaming_sortformer.cc` | L109-110 | `void UpdateSilenceProfile(const std::vector<float>& embs, const std::vector<float>& preds, ...)` | 两个参数均可改为 span |
| `src/model/streaming_sortformer.cc` | L140-143 | `void CompressSpkcache(const std::vector<float>& emb_seq, const std::vector<float>& preds, ..., const std::vector<float>& mean_sil_emb)` | 三个参数均可改为 span |
| `src/io/audio_file.cc` | L43 | `std::vector<float> Resample(const std::vector<float>& in, int in_rate, ...)` | `std::span<const float>` |
| `src/core/tensor.cc` | L55 | `static int64_t ProductOfShape(const std::vector<int64_t>& shape)` | `std::span<const int64_t>` — 纯计算 |

**收益:** 消除不必要的拷贝构造，允许传入裸指针+长度或 `std::array`，接口更灵活。

### 1.2 `const T* + size` 参数 → `std::span<const T>`

| 文件 | 位置 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/core/stages.h` | L47 | `virtual DiarizationFrames StreamAudio(const float* samples, int num_samples, ...)` | `std::span<const float>` |
| `include/core/stages.h` | L75 | `virtual bool Enroll(const std::string& speaker_id, const float* embedding)` | 可改为 `std::span<const float>`（但需知道维度） |
| `include/core/stages.h` | L77 | `virtual int Match(const float* embedding, float threshold, ...)` | 同上 |
| `include/core/stages.h` | L117 | `virtual std::string StreamChunk(const float* pcm, int n, ...)` | `std::span<const float>` — ASR 热路径 |
| `include/core/stages.h` | L168 | `virtual void Push(const float* samples, int n) = 0` | `std::span<const float>` — VAD 热路径 |
| `include/core/stages.h` | L201 | `virtual std::vector<AlignUnit> Align(const float* pcm, int n, ...)` | `std::span<const float>` |
| `include/feature/mel_spectrogram.h` | L50 | `std::vector<float> Compute(const float* samples, int num_samples, ...)` | `std::span<const float>` — 特征提取热路径 |
| `include/feature/mel_spectrogram.h` | L59 | `std::vector<float> ComputeStreamFrames(const float* sig, int num_samples, ...)` | `std::span<const float>` — 特征提取热路径 |
| `include/feature/mel_spectrogram.h` | L86 | `std::vector<float> RunStftMel(const float* sig, int num_samples, ...)` | `std::span<const float>` |
| `include/feature/whisper_mel.h` | L49 | `std::vector<float> Compute(const float* samples, int num_samples, ...)` | `std::span<const float>` |
| `include/gpu/buffer.h` | L24 | `size_t Write(const float* samples, size_t num_samples)` | `std::span<const float>` |
| `include/gpu/kernels.h` | L16-26 | `NormalizeVector`, `CosineSimilarity`, `BatchCosineSimilarity` | 均可改为 `std::span<const float>` / `std::span<float>` |

**收益:** 接口更简洁（一个参数代替两个），类型安全（编译器追踪长度），零运行时开销。

### 1.3 返回值中的 `const std::vector<T>&` → `std::span<const T>`

| 文件 | 位置 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/core/tensor.h` | L44 | `const std::vector<int64_t>& shape() const` | `std::span<const int64_t>` — 更通用的只读视图 |
| `include/protocol/topic.h` | L26 | `const std::vector<std::string>& levels() const` | `std::span<const std::string>` |
| `include/io/safetensor.h` | L37 | `const std::vector<std::string>& GetWeightNames() const` | `std::span<const std::string>` |
| `include/io/sharded_safetensor.h` | L33 | `const std::vector<std::string>& names() const` | `std::span<const std::string>` |

**收益:** 返回值语义更准确（只读视图），不暗示拥有权或 `std::vector` 实现细节。

---

## 2. std::format（高频字符串构建）

### 2.1 `snprintf` / `sprintf` 调用

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/pipeline/json_util.h` | L41 | `std::snprintf(buf, sizeof(buf), "\\u%04x", c)` | `std::format("\\u{:04x}", c)` |
| `src/pipeline/json_util.cc` | L69 | `std::snprintf(buf, sizeof(buf), "{\"id\":{},\"start\":{:.3f},\"end\":{:.3f},\"text\":\"")` | `std::format` |
| `src/pipeline/json_util.cc` | L85 | `std::snprintf(buf, sizeof(buf), ...)` | `std::format` |
| `src/pipeline/asr_worker.cc` | L242 | `std::snprintf(buf, sizeof(buf), "{\"id\":{},\"start\":{:.3f},\"end\":{:.3f},\"text\":\"")` | `std::format` |
| `src/pipeline/asr_worker.cc` | L258 | 同上模式 | `std::format` |
| `src/pipeline/auditory_stream.cc` | L547 | `std::snprintf(session_id_buf, sizeof(session_id_buf), "%08x%08x", ...)` | `std::format("{:08x}{:08x}", ...)` |
| `src/pipeline/auditory_stream_subscriptions.cc` | L215 | `std::snprintf(b, sizeof(b), ...)` | `std::format` |
| `src/pipeline/auditory_stream_subscriptions.cc` | L282 | `std::snprintf(b, sizeof(b), "{\"text\":\"%s\",\"start\":%.3f,\"end\":%.3f}")` | `std::format` |
| `src/pipeline/auditory_stream_subscriptions.cc` | L329 | `std::snprintf(buf, sizeof(buf), ...)` | `std::format` |
| `src/pipeline/auditory_stream_subscriptions.cc` | L350 | `std::snprintf(buf, sizeof(buf), "{\"horizon\":%.3f}", horizon_sec)` | `std::format` |
| `src/pipeline/auditory_stream_serialize.cc` | L44, L58, L128, L137, L144 | 多处 `std::snprintf` | `std::format` |
| `src/net/auditory_ws_handler.cc` | L43 | `snprintf(buf, sizeof(buf), "\\u%04x", ...)` | `std::format` |

**收益:** 类型安全（编译期格式字符串检查），无需缓冲区大小管理，可读性更好。

### 2.2 字符串拼接（`+` 运算符）

| 文件 | 行号 | 当前模式 | 建议 |
|------|------|----------|------|
| `src/pipeline/auditory_stream.cc` | L44 | `"session_" + std::to_string(...)` | `std::format("session_{0}", ...)` |
| `src/pipeline/auditory_stream.cc` | L448-450, L465 | 多处 `json +=` 拼接 | `std::format` 一次性构建 |
| `src/pipeline/auditory_stream_subscriptions.cc` | L212, L260-262, L289-291 | `"{"id\":" + std::to_string(id) + ...` | `std::format` |
| `src/net/auditory_ws_handler.cc` | L125, L163, L239-241 | 同上模式 | `std::format` |
| `src/model/sortformer_decoder.cu` | L203, L271 | `"transformer_encoder.layers." + std::to_string(l) + "."` | `std::format` |
| `src/model/qwen3_aligner_lm.cu` | L79 | `M + "layers." + std::to_string(i) + "."` | `std::format` |
| `src/model/asr_audio_tower.cu` | L337 | `A + "layers." + std::to_string(i) + "."` | `std::format` |
| `src/model/asr_text_decoder.cu` | L284 | 同上模式 | `std::format` |
| `src/model/streaming_sortformer.cc` | L375 | `"encoder.layers." + std::to_string(l)` | `std::format` |
| `src/pipeline/json_util.cc` | L13, L27, L50 | `"\"" + std::string(key) + "\":"` | `std::format` |
| `src/gpu/scheduler.cc` | L14 | `std::string("GpuScheduler: ") + what + ":"` | `std::format` |
| `src/gpu/memory.cc` | L81, L91, L102 | `std::string("Cannot open file: " + filepath)` | `std::format` |
| `src/pipeline/asr_preprocessor.cc` | L86, L163 | `std::to_string(...)` 拼接命令 | `std::format` |

**收益:** 消除 N 次临时 `std::string` 分配（拼接链产生多个临时对象），`std::format` 一次性计算长度后分配。

### 2.3 `std::ostringstream` 可替换场景

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `src/pipeline/asr_preprocessor.cc` | L86 | `oss << "orator_preproc_" << std::to_string(...)` | `std::format` |
| `src/gpu/memory.cc` | L16, L113 | `std::ostringstream oss` 构造异常消息 | `std::format` |
| `src/protocol/protocol_timeline.cc` | L92, L127 | `std::ostringstream` 构建消息 | `std::format` |
| `src/io/json_sink.cc` | L38, L48 | `std::ostringstream oss` 构建 JSON | `std::format` |
| `src/io/safetensor.cc` | L191 | `std::ostringstream header` | `std::format` |
| `src/net/http_static_server.cc` | L158, L185 | `std::ostringstream` 构建 HTTP 响应 | `std::format` |

**收益:** `std::format` 比 `ostringstream` 更简洁，且无 iomanip 开销（虽然都是零运行时依赖，但 format 是 C++20 标准库）。

---

## 3. std::string_view（只读参数）

### 3.1 接口参数 `const std::string&` → `std::string_view`

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/core/stages.h` | L38 | `virtual void LoadWeights(const std::string& path) = 0` | `std::string_view` — 路径只读 |
| `include/core/stages.h` | L61, L74, L103, L161, L197 | 同上（所有 `LoadWeights`） | `std::string_view` |
| `include/core/stages.h` | L202-203 | `virtual std::vector<AlignUnit> Align(..., const std::string& transcript, const std::string& language)` | 两个参数均可改为 `std::string_view` |
| `include/model/qwen3_aligner_lm.h` | L57 | `std::vector<float> Forward(const std::vector<int>& input_ids, const std::vector<float>& position_ids, const std::vector<float>& logit_bias)` | 已用 span 更好，但 `position_ids` 和 `logit_bias` 可改为 span |
| `include/model/qwen3_asr.h` | L65 | `std::string BuildAndRun(const std::vector<float>& encoder_out, const std::vector<int>& input_ids)` | 可改为 span |
| `include/io/bpe_tokenizer.h` | L30 | `std::string Decode(const std::vector<int>& ids, const std::string& text)` | `text` 可改为 `std::string_view` |
| `include/io/safetensor.h` | L76 | `static bool Write(const std::string& path, const std::vector<Entry>& entries)` | `path` 可改为 `std::string_view` |
| `include/io/config_reader.h` | 构造函数 | `const std::string&` 路径参数 | `std::string_view` |
| `include/pipeline/align_worker.h` | L47 | `const std::vector<core::AlignUnit>& units` 回调参数 | 可改为 `std::span<const core::AlignUnit>` |

**收益:** 避免 `std::string` 拷贝构造（虽然是小字符串优化，但接口语义更准确）。

---

## 4. std::optional（返回值优化）

### 4.1 `nullptr` / 特殊值返回

| 文件 | 行号 | 当前模式 | 建议 |
|------|------|----------|------|
| `include/gpu/device_scratch.h` | L31 | `void* Get(int slot, size_t bytes)` 返回 `nullptr` | `std::optional<void*>`（但 slot 场景返回指针合理） |
| `include/core/tensor.h` | L57-61 | `data()` 返回裸指针 | 可保留（RAII 语义），但 `data_as<T>()` 可返回 `std::span<T>` |
| `include/model/speaker_database.h` | L41-43 | `static_cast<const float*>(embeddings_.data())` | 可改为 `std::span<const float>` |
| `src/pipeline/comprehensive_timeline.cc` | L277 | `std::find_if` 找迭代器然后解引用 | 可返回 `std::optional<const Entry*>` |

### 4.2 `bool` + out 参数模式

| 文件 | 行号 | 当前模式 | 建议 |
|------|------|----------|------|
| `include/feature/mel_spectrogram.h` | L50 | `std::vector<float> Compute(..., int* out_num_frames)` | 可返回 `std::pair<std::vector<float>, int>` 或 structured bindings |
| `include/feature/whisper_mel.h` | L49 | `std::vector<float> Compute(..., int* out_num_frames)` | 同上 |
| `include/core/stages.h` | L77 | `virtual int Match(..., float* out_score) const` | 可返回 `std::optional<float>`（-1 表示未匹配，分数为结果） |
| `include/core/stages.h` | L77 | 或改为 `std::pair<int, float>` 返回 (index, score) | 更清晰 |

---

## 5. std::bit_cast / std::bytes

### 5.1 `reinterpret_cast` 类型转换

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `src/io/safetensor.cc` | L32 | `static_cast<const char*>(mmap_->get_mapped_ptr())` | 指针偏移场景，`reinterpret_cast` 合理 |
| `src/io/safetensor.cc` | L34 | `std::memcpy(&header_len, base, 8)` | `std::bit_cast<uint64_t>(...)`（需 C 数组 → 可改为 `std::array`） |
| `src/io/safetensor.cc` | L228 | `out.write(reinterpret_cast<const char*>(&header_len), 8)` | 可保留（二进制 I/O 场景） |
| `src/io/audio_file.cc` | L105-126 | 多处 `reinterpret_cast<char*>` 读 WAV 头 | 可保留（二进制格式解析） |
| `src/net/websocket_server.cc` | L155-L163 | `reinterpret_cast<const uint8_t*>` SHA 计算 | 可保留（哈希输入） |
| `src/net/auditory_ws_handler.cc` | L179 | `reinterpret_cast<const float*>(data)` | 可改为 `std::span<const float>` |

**收益:** `std::bit_cast` 无 UB 保证（源和目标大小相同），比 `reinterpret_cast` + `memcpy` 更明确意图。

---

## 6. 构造函数简化 & constinit / consteval

### 6.1 可改为 `consteval` 的纯计算函数

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/core/tensor.h` | L28-31 | `size_t DTypeSize(DType dtype)`, `DType DTypeFromString(const char* name)` | `DTypeSize` 可改为 `consteval`（枚举→大小的编译期映射） |
| `include/core/time_base.h` | L46-58 | `static_cast<double>` 转换 | 已在内联函数中，可改为 `constexpr` |
| `include/core/types.h` | L24 | `DurationSec()` | 可加 `constexpr`（纯计算） |
| `include/feature/whisper_mel.cu` | L18 | `constexpr float kPi` | 已是 constexpr，可改为 `consteval` 若用于编译期计算 |

### 6.2 可改为 `constinit` 的静态变量

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/protocol/topic.h` | L54-61 | `inline Topic kAudioRaw{"audio/raw"}` 等 | 可改为 `constinit inline Topic` 确保编译期初始化 |
| `include/model/qwen3_asr.h` | L160-L170 | `static constexpr int kImStart = 151644` 等 | 已是 constexpr，无需改动 |

---

## 7. 范围算法增强（std::ranges）

### 7.1 手动循环可改为 ranges 管道

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `src/io/bpe_tokenizer.cc` | L65 | `std::find(bs.begin(), bs.end(), b) == bs.end()` | `std::ranges::find(bs, b) == bs.end()` 或 `!std::ranges::contains(bs, b)` (C++26) |
| `src/io/audio_file.cc` | L21 | `std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower)` | `std::ranges::transform(tail, tail.begin(), ::tolower)` |
| `src/pipeline/comprehensive_timeline.cc` | L277 | `std::find_if(texts_.begin(), texts_.end(), ...)` | `std::ranges::find_if(texts_, ...)` |
| `src/pipeline/asr_preprocessor.cc` | L99 | `std::transform(m.begin(), m.end(), m.begin(), ...)` | `std::ranges::transform(m, m.begin(), ...)` |

**收益:** 更简洁的管道语法，编译器可优化范围适配器链。

---

## 8. structured bindings 缺失

### 8.1 `std::pair` / `std::tuple` 使用 .first/.second

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `src/pipeline/auditory_stream.cc` | L437 | `const std::pair<const char*, PipelineAudioCache*> caches[]` | 迭代时可用 structured bindings |
| `src/pipeline/auditory_stream.cc` | L448 | `c.first`, `c.second` | `for (const auto& [name, cache] : caches)` |
| `src/pipeline/auditory_stream.cc` | L465 | `c.first`, `c.second` | 同上 |
| `src/protocol/protocol_timeline.cc` | L67-68 | `std::vector<std::pair<long, Message>> pending_callbacks` | 迭代时可用 structured bindings |
| `src/pipeline/asr_worker.cc` | L99 | `std::vector<std::pair<double, double>>{}` | 可保留（类型表达清晰） |

---

## 9. std::move 冗余检查

### 9.1 已正确使用 std::move 的场景（无需改动）

| 文件 | 行号 | 场景 |
|------|------|------|
| `include/core/registry.h` | L31, L63 | `factories_[key] = std::move(factory)` — 正确 |
| `include/pipeline/align_worker.h` | L56 | `sink_ = std::move(sink)` — 正确 |
| `include/pipeline/asr_worker.h` | L82 | `text_sink_ = std::move(sink)` — 正确 |
| `include/pipeline/diarization_worker.h` | L60 | `speaker_sink_ = std::move(sink)` — 正确 |
| `src/pipeline/auditory_stream.cc` | L50-82 | 多处 `std::move(desc)` 注册 PipelineHandle — 正确 |

**发现:** `std::move` 使用总体合理，无明显冗余。

---

## 10. 空格分隔数字

### 10.1 魔法数字未使用 `'` 分隔

| 文件 | 行号 | 当前代码 | 建议 |
|------|------|----------|------|
| `include/protocol/memory_backend.h` | L20 | `128 * 1024 * 1024` | `128 * 1024 * 1024` 已是表达式，但可改为 `128_MiB` 常量 |
| `include/pipeline/asr_worker.h` | L137 | `8000` | `8'000` |
| `include/model/qwen3_asr.h` | L70 | `kStreamWindowMel` | 查看实际值 |
| `include/model/qwen3_asr.h` | L160-L170 | `151644`, `151645`, `151669`, `151670`, `151676`, `151704`, `151705` | `151'644`, `151'645`, `151'669` 等 |
| `include/model/qwen3_forced_aligner.h` | L45-L48 | 同上 | `151'669` 等 |
| `include/model/qwen3_asr.h` | L167-L169 | `8948`, `872`, `77091` | `8'948`, `872`, `77'091` |

---

## 汇总统计

| 特性 | 发现数量 | 影响范围 | 优先级 |
|------|----------|----------|--------|
| `std::span` (vector 参数) | ~15 | 模型推理、特征提取、协议层 | ⭐⭐⭐ |
| `std::span` (指针+size 参数) | ~12 | 阶段接口、GPU 内核、热路径 | ⭐⭐⭐ |
| `std::span` (返回值) | 4 | 接口只读视图 | ⭐⭐ |
| `std::format` (snprintf) | ~12 | JSON 序列化、日志 | ⭐⭐⭐ |
| `std::format` (字符串拼接) | ~20 | JSON 构建、权重路径 | ⭐⭐⭐ |
| `std::format` (ostringstream) | 6 | 异常消息、HTTP 响应 | ⭐⭐ |
| `std::string_view` | ~8 | 接口参数 | ⭐⭐ |
| `std::optional` | 4 | 返回值语义 | ⭐⭐ |
| `std::ranges` | 4 | 算法调用 | ⭐ |
| `consteval`/`constinit` | ~5 | 编译期优化 | ⭐ |
| 空格分隔数字 | ~10 | 可读性 | ⭐ |
| structured bindings | ~4 | 代码简洁性 | ⭐ |

**总计:** ~67 处可改进点

---

## 推荐实施顺序

1. **第一阶段（热路径优化）:** `std::span` 替换 `const float* + int` 参数（ASR/VAD/特征提取接口）
2. **第二阶段（JSON 构建）:** `std::format` 替换 `snprintf` + 字符串拼接（序列化热路径）
3. **第三阶段（接口现代化）:** `std::string_view` 替换 `const std::string&`（只读参数）
4. **第四阶段（返回值语义）:** `std::optional` 替换 `nullptr`/特殊值/出参模式
5. **第五阶段（代码质量）:** `std::ranges`、`consteval`、空格分隔数字、structured bindings
