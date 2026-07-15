import assert from "node:assert/strict";
import test from "node:test";

import { Model } from "../../web/js/model.js";
import { copyPcmFrame } from "../../web/js/audio.js";
import { fmtWallTime } from "../../web/js/format.js";
import { OratorWs, unwrapEnvelope } from "../../web/js/ws.js";

globalThis.window = {
  location: { hostname: "127.0.0.1", port: "8766" },
};

test("protocol envelopes infer typed payloads and raw errors", () => {
  const diar = unwrapEnvelope(JSON.stringify({
    topic: "diar/speaker_segment",
    data: JSON.stringify({ source: "sortformer", segments: [] }),
  }));
  assert.equal(diar.type, "diar");

  const align = unwrapEnvelope(JSON.stringify({
    topic: "align/units",
    data: { text_id: 4, units: [] },
  }));
  assert.deepEqual(align, { type: "align", text_id: 4, units: [] });

  const error = unwrapEnvelope(JSON.stringify({ error: "session not found" }));
  assert.equal(error.type, "error");
  assert.equal(error.error, "session not found");
});

test("file-stream frames cover the PCM buffer exactly once", () => {
  const pcm = new Int16Array(2000);
  for (let index = 0; index < pcm.length; index++) pcm[index] = index;
  const frames = [];
  let offset = 0;
  while (offset < pcm.byteLength) {
    const frame = copyPcmFrame(pcm, offset, 1920);
    frames.push(frame.data);
    offset = frame.nextOffset;
  }

  assert.deepEqual(frames.map((frame) => frame.byteLength), [1920, 1920, 160]);
  assert.equal(frames.reduce((total, frame) => total + frame.byteLength, 0),
               pcm.byteLength);
  assert.equal(new Int16Array(frames[1])[0], 960);
});

test("saved-session wall clocks render as calendar timestamps", () => {
  assert.match(fmtWallTime(1783958458.958),
               /^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/);
  assert.equal(fmtWallTime(0), "-");
});

test("revision before ASR final converges on one stable text ID", () => {
  const model = new Model();
  model.beginSession({ sample_rate: 16000, asr: true });
  model.applyRevision({
    entries: [
      {
        text_id: 0,
        start: 1.0,
        end: 1.5,
        speaker: 2,
        speaker_id: "spk_2",
        speaker_decision: {
          speaker_source: "sortformer_diarization",
          text_projection_source: "forced_alignment",
          reason: "competing_diar_interval_policy",
          overlap_margin_sec: 0,
          confidence_margin: 0.2,
          candidates: [
            { speaker: 2, speaker_id: "spk_2", selected: true },
            { speaker: 1, speaker_id: "spk_1", selected: false },
          ],
        },
        text: "hello",
      },
      {
        text_id: 0,
        start: 1.5,
        end: 2.0,
        speaker: 2,
        speaker_id: "spk_2",
        text: " world",
      },
    ],
  });

  assert.equal(model.asr.size, 0);
  assert.equal(model.turns.size, 2);
  model.applyAsr({ text_id: 0, start: 1.0, end: 2.0, text: "hel" }, false);
  model.applyAsr(
    { text_id: 0, start: 1.0, end: 2.0, text: "hello world" }, true);

  assert.deepEqual(model.asrOrder, [0]);
  assert.equal(model.asr.get(0).speaker_id, "spk_2");
  assert.equal(model.asr.get(0).speakerSegments.length, 2);
  assert.equal(model.asr.get(0).speakerSegments[0].speaker_decision.reason,
               "competing_diar_interval_policy");
  assert.equal(model.asr.get(0).speakerSegments[0]
    .speaker_decision.candidates[1].selected, false);
  assert.equal(model.asr.get(0).status, "confirmed");
  assert.equal(model.draft, null);
  assert.deepEqual(model.tracks.asr, [
    { text_id: 0, start: 1.0, end: 2.0, text: "hello world" },
  ]);
  assert.equal(model.applyAsr({ text: "missing id" }, true), null);
});

test("a VAD rejection retracts only the matching provisional ASR row", () => {
  const model = new Model();
  model.applyAsr({ text_id: 3, start: 0, end: 1, text: "draft" }, false);
  model.retractAsr({ text_id: 3 });
  assert.equal(model.asr.has(3), false);
  assert.equal(model.draft, null);

  model.applyAsr({ text_id: 4, start: 1, end: 2, text: "final" }, true);
  model.retractAsr({ text_id: 4 });
  assert.equal(model.asr.get(4).text, "final");
});

test("live diarization, VAD, alignment, and progress update typed tracks", () => {
  const model = new Model();
  model.applyDiar({
    segments: [
      {
        start: 0.1,
        end: 0.8,
        speaker: "speaker_3",
        speaker_id: "spk_3",
        confidence: 0.9,
      },
    ],
  });
  model.applyVad({ start: 0.2, end: 0.7 });
  model.applyVad({ start: 0.2, end: 0.7 });
  model.applyAlign({
    text_id: 5,
    start: 0.2,
    end: 0.7,
    units: [{ start: 0.2, end: 0.7, text: "a" }],
  });
  model.applyVadProgress({ horizon: 0.9 });

  assert.equal(model.tracks.diarization[0].speaker, 3);
  assert.equal(model.tracks.diarization[0].speaker_id, "spk_3");
  assert.deepEqual(model.tracks.vad, [{ start: 0.2, end: 0.7 }]);
  assert.equal(model.tracks.align[0].text_id, 5);
  assert.equal(model.alignUnits.get(5).length, 1);
  assert.equal(model.vadHorizon, 0.9);
});

test("terminal timeline authoritatively removes stale live state", () => {
  const model = new Model();
  model.applyAsr({ text_id: 99, start: 0, end: 1, text: "stale" }, false);
  model.applyAlign({ text_id: 99, start: 0, end: 1, units: [] });
  model.applyDiar({
    segments: [{ start: 0, end: 1, speaker: "speaker_0" }],
  });

  const timeline = {
    type: "timeline",
    sample_rate: 16000,
    audio_sec: 3.0,
    tracks: [
      {
        kind: "diarization",
        entries: [{ start: 1, end: 3, speaker: 1, speaker_id: "spk_1" }],
      },
      {
        kind: "asr",
        entries: [{ text_id: 7, start: 1, end: 3, text: "final" }],
      },
      { kind: "vad", entries: [{ start: 1.1, end: 2.9 }] },
      {
        kind: "align",
        entries: [{
          text_id: 7,
          start: 1,
          end: 3,
          units: [{ start: 1, end: 3, text: "final" }],
        }],
      },
      {
        kind: "business_speaker",
        entries: [{
          text_id: 7,
          start: 1,
          end: 3,
          speaker: 1,
          speaker_id: "spk_1",
          text: "final",
        }],
      },
    ],
    comprehensive: [{
      text_id: 7,
      start: 1,
      end: 3,
      speaker: 1,
      speaker_id: "spk_1",
      text: "final",
    }],
  };

  model.applyTimeline(timeline);
  assert.strictEqual(model.timeline, timeline);
  assert.deepEqual(model.asrOrder, [7]);
  assert.equal(model.asr.has(99), false);
  assert.equal(model.alignUnits.has(99), false);
  assert.equal(model.asr.get(7).text, "final");
  assert.equal(model.asr.get(7).speaker_id, "spk_1");
  assert.equal(model.alignUnits.get(7).length, 1);
  assert.equal(model.turns.size, 1);
  assert.deepEqual(model.tracks.diarization, timeline.tracks[0].entries);
  assert.equal(model.draft, null);

  model.beginSession({ sample_rate: 16000, asr: true });
  assert.equal(model.connectionGeneration, 1);
  assert.equal(model.asr.size, 0);
  assert.equal(model.turns.size, 0);
  assert.equal(model.timeline, null);
});

test("WebSocket close schedules reconnect even after an error event", () => {
  class FakeWebSocket {
    static OPEN = 1;
    static CONNECTING = 0;

    constructor() {
      this.readyState = FakeWebSocket.CONNECTING;
      FakeWebSocket.last = this;
    }
  }
  globalThis.WebSocket = FakeWebSocket;

  const client = new OratorWs({});
  let reconnects = 0;
  client._scheduleReconnect = () => { reconnects += 1; };
  client.connect();
  FakeWebSocket.last.onerror({});
  FakeWebSocket.last.onclose({});
  assert.equal(reconnects, 1);
});
