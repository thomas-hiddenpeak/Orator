# Orator Tools

This directory contains various tools for verification, testing, conversion, and evaluation.

## Directory Structure

```
tools/
├── verify/                 # Verification tools
│   ├── cc/                 # C++ verification tools
│   │   ├── verify_streaming.cc
│   │   ├── verify_streaming_incremental.cc
│   │   ├── verify_conformer.cc
│   │   ├── verify_conformer_full.cc
│   │   ├── verify_decoder.cc
│   │   ├── verify_forward.cc
│   │   ├── verify_mel.cc
│   │   ├── verify_pipeline.cc
│   │   ├── verify_preencode.cc
│   │   ├── asr_stream_test.cc
│   │   ├── asr_testmp3.cc
│   │   └── run_testmp3.cc
│   └── py/                 # Python verification tools
│       ├── ws_stream_client.py
│       ├── ws_ui_integration_test.py
│       ├── ws_dump_segments.py
│       ├── asr_strategy_autotest.py
│       ├── param_tune.py
│       └── sweep_diar_params.py
├── probes/                 # Probe tools for debugging and analysis
│   ├── asr_encoder_chunk_probe.cc
│   ├── asr_stream_incremental_probe.cc
│   └── asr_stream_window_probe.cc
├── convert/                # Data conversion tools
│   ├── convert_nemo_to_safetensors.py
│   ├── convert_silero_vad_to_orator_safetensors.py
│   └── download_and_convert_asr_preproc_models.py
├── dump/                   # Dump tools for activations, operations, etc.
│   ├── dump_activations.py
│   ├── dump_asr_ops.py
│   ├── dump_pcm.cc
│   └── dump_streaming.py
├── eval/                   # Evaluation tools
│   ├── eval_speaker_acc.py
│   └── cer.py
├── reference/              # Reference and validation tools
│   ├── asr_oracle.py
│   ├── reference_infer.py
│   └── validate_weights.cc
├── bench/                  # Benchmarking tools
│   ├── gemm_bench.cu
│   └── decode_audio.cc
├── asr_preproc_infer.py    # ASR preprocessor inference tool
└── torchenv.sh             # PyTorch environment setup script
```

## Usage

### C++ Verification Tools
Build with CMake and run from the build directory or tools directory as needed.

### Python Verification Tools
Run directly with Python 3:
```bash
python3 tools/verify/py/ws_stream_client.py --pcm /path/to/audio.pcm
python3 tools/verify/py/ws_ui_integration_test.py
```

### Conversion Tools
Use to convert models between different formats:
```bash
python3 tools/convert/convert_nemo_to_safetensors.py
```

### Dump Tools
Use to dump activations, operations, or streaming data for analysis:
```bash
python3 tools/dump/dump_activations.py
```

### Evaluation Tools
Use to evaluate speaker accuracy or CER:
```bash
python3 tools/eval/eval_speaker_acc.py
```

### Reference Tools
Use for reference inference and weight validation:
```bash
python3 tools/reference/reference_infer.py
python3 tools/reference/validate_weights.cc
```

### Benchmark Tools
Use for performance benchmarking:
```bash
./tools/bench/gemm_bench.cu
```