// audio.js — microphone + file capture → int16LE mono 16k frames.
// Reuses the MVP's proven Web Audio capture/decode/downsample path.

function f32ToInt16(buf) {
  const out = new Int16Array(buf.length);
  for (let i = 0; i < buf.length; i++) {
    const s = Math.max(-1, Math.min(1, buf[i]));
    out[i] = s < 0 ? Math.round(s * 32768) : Math.round(s * 32767);
  }
  return out.buffer;
}

function downsample(input, srcRate, dstRate) {
  if (srcRate === dstRate) return input;
  const ratio = srcRate / dstRate;
  const len = Math.max(1, Math.floor(input.length / ratio));
  const out = new Float32Array(len);
  let pos = 0;
  for (let i = 0; i < len; i++) {
    const end = Math.min(input.length, Math.floor((i + 1) * ratio));
    const start = Math.floor(pos);
    let sum = 0;
    for (let j = start; j < end; j++) sum += input[j];
    out[i] = sum / Math.max(1, end - start);
    pos = end;
  }
  return out;
}

export class MicCapture {
  constructor(sendBinary, targetRate) {
    this.send = sendBinary;
    this.rate = targetRate || 16000;
    this.ctx = null; this.stream = null; this.src = null; this.proc = null;
    this.running = false;
  }

  async start() {
    this.stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    this.ctx = new (window.AudioContext || window.webkitAudioContext)();
    this.src = this.ctx.createMediaStreamSource(this.stream);
    this.proc = this.ctx.createScriptProcessor(4096, 1, 1);
    this.proc.onaudioprocess = (ev) => {
      const ch = ev.inputBuffer.getChannelData(0);
      const down = downsample(ch, this.ctx.sampleRate, this.rate);
      if (down.length) this.send(f32ToInt16(down));
    };
    this.src.connect(this.proc);
    this.proc.connect(this.ctx.destination);
    this.running = true;
  }

  stop() {
    if (this.proc) { this.proc.disconnect(); this.proc.onaudioprocess = null; this.proc = null; }
    if (this.src) { this.src.disconnect(); this.src = null; }
    if (this.stream) { this.stream.getTracks().forEach((t) => t.stop()); this.stream = null; }
    if (this.ctx) { this.ctx.close(); this.ctx = null; }
    this.running = false;
  }
}

// Decode a file to mono 16k int16 and stream it in ~60 ms chunks at wall-time
// pacing. onProgress(frac, durationSec); onDone() called after the last chunk.
export async function streamFile(file, sendBinary, targetRate, onProgress, onDone) {
  const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  const arrayBuf = await file.arrayBuffer();
  const audioBuf = await audioCtx.decodeAudioData(arrayBuf);

  let channelData;
  if (audioBuf.numberOfChannels === 1) {
    channelData = audioBuf.getChannelData(0);
  } else {
    const len = audioBuf.length;
    channelData = new Float32Array(len);
    for (let ch = 0; ch < audioBuf.numberOfChannels; ch++) {
      const cd = audioBuf.getChannelData(ch);
      for (let i = 0; i < len; i++) channelData[i] += cd[i];
    }
    for (let i = 0; i < len; i++) channelData[i] /= audioBuf.numberOfChannels;
  }

  const resampled = downsample(channelData, audioBuf.sampleRate, targetRate);
  const int16 = new Int16Array(resampled.length);
  for (let i = 0; i < resampled.length; i++) {
    const s = Math.max(-1, Math.min(1, resampled[i]));
    int16[i] = s < 0 ? Math.round(s * 32768) : Math.round(s * 32767);
  }
  audioCtx.close();

  const durationSec = int16.length / targetRate;
  const bytesPerChunk = Math.floor(targetRate * 0.06 * 2);
  let offset = 0;
  let cancelled = false;

  function sendChunk() {
    if (cancelled) return;
    const end = Math.min(offset + bytesPerChunk, int16.byteLength);
    const slice = new Int16Array(int16.buffer, offset, (end - offset) / 2);
    sendBinary(slice.buffer);
    offset = end;
    if (onProgress) onProgress(offset / int16.byteLength, durationSec);
    if (offset < int16.byteLength) setTimeout(sendChunk, 60);
    else if (onDone) onDone();
  }
  sendChunk();
  return { cancel() { cancelled = true; } };
}
