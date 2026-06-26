#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "protocol/disk_backend.h"
#include "protocol/memory_backend.h"
#include "protocol/storage.h"
#include "protocol/time_index.h"

using orator::protocol::Backend;
using orator::protocol::DiskBackend;
using orator::protocol::IndexedMessage;
using orator::protocol::MemoryBackend;
using orator::protocol::Message;
using orator::protocol::StorageManager;
using orator::protocol::StorageRef;
using orator::protocol::TimeIndex;
using orator::protocol::TopicRetention;

static int g_fail = 0;
#define CHECK(cond, msg)                \
  do {                                  \
    if (!(cond)) {                      \
      std::printf("FAIL: %s\n", msg);   \
      ++g_fail;                         \
    }                                   \
  } while (0)

// ---------------------------------------------------------------------------
// MemoryBackend tests
// ---------------------------------------------------------------------------

static void test_memory_backend_write_read_small() {
  std::printf("  MemoryBackend: write/read small data... ");
  MemoryBackend mem(1024);

  const uint8_t data[] = {1, 2, 3, 4, 5};
  uint64_t offset = mem.Write(data, 5);
  CHECK(offset == 0, "first write returns offset 0");

  uint8_t buf[5] = {0};
  uint32_t read = mem.Read(offset, 5, buf);
  CHECK(read == 5, "reads 5 bytes");
  CHECK(std::memcmp(buf, data, 5) == 0, "data matches");

  std::printf("PASS\n");
}

static void test_memory_backend_write_read_large() {
  std::printf("  MemoryBackend: write/read large data... ");
  MemoryBackend mem(65536);

  // Write a 4KB block.
  std::vector<uint8_t> data(4096);
  for (int i = 0; i < 4096; ++i) {
    data[static_cast<size_t>(i)] = static_cast<uint8_t>(i % 256);
  }
  uint64_t offset = mem.Write(data.data(), 4096);

  std::vector<uint8_t> buf(4096);
  uint32_t read = mem.Read(offset, 4096, buf.data());
  CHECK(read == 4096, "reads full 4KB");
  CHECK(std::memcmp(buf.data(), data.data(), 4096) == 0, "4KB data matches");

  std::printf("PASS\n");
}

static void test_memory_backend_capacity_eviction() {
  std::printf("  MemoryBackend: capacity eviction... ");
  MemoryBackend mem(64);  // 64 bytes capacity

  // Write 3 x 20 bytes = 60 bytes.
  uint8_t a[] = "AAAAAAAAAAAAAAAAAAAA";
  uint8_t b[] = "BBBBBBBBBBBBBBBBBBBB";
  uint8_t c[] = "CCCCCCCCCCCCCCCCCCCC";

  uint64_t off_a = mem.Write(a, 20);
  uint64_t off_b = mem.Write(b, 20);
  uint64_t off_c = mem.Write(c, 20);

  CHECK(mem.used() == 60, "60 bytes used after 3 writes");

  // Write 10 more bytes. 60 + 10 = 70 > 64, so eviction should occur.
  uint8_t d[] = "DDDDDDDDDD";
  uint64_t off_d = mem.Write(d, 10);

  // After eviction of oldest (a), we should have b + c + d = 50 bytes.
  CHECK(mem.used() <= 64, "used <= capacity after eviction");

  // 'a' should be evicted.
  uint8_t buf[20] = {0};
  uint32_t read_a = mem.Read(off_a, 20, buf);
  CHECK(read_a == 0, "evicted data returns 0 bytes");

  // 'b' should still be readable.
  uint32_t read_b = mem.Read(off_b, 20, buf);
  CHECK(read_b == 20, "non-evicted data 'b' is readable");
  CHECK(std::memcmp(buf, b, 20) == 0, "'b' data matches");

  // 'd' should be readable.
  uint32_t read_d = mem.Read(off_d, 10, buf);
  CHECK(read_d == 10, "'d' is readable");
  CHECK(std::memcmp(buf, d, 10) == 0, "'d' data matches");

  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// DiskBackend tests
// ---------------------------------------------------------------------------

static void test_disk_backend_write_read() {
  std::printf("  DiskBackend: write/read data... ");
  DiskBackend disk("/tmp/orator_test_storage", "test1");

  const uint8_t data[] = {10, 20, 30, 40, 50};
  uint64_t offset = disk.Write(data, 5);
  CHECK(offset == 0, "first write at offset 0");
  CHECK(disk.total_bytes() == 5, "total_bytes = 5");

  uint8_t buf[5] = {0};
  uint32_t read = disk.Read(offset, 5, buf);
  CHECK(read == 5, "reads 5 bytes");
  CHECK(std::memcmp(buf, data, 5) == 0, "data matches");

  std::printf("PASS\n");
}

static void test_disk_backend_multiple_writes() {
  std::printf("  DiskBackend: multiple writes... ");
  DiskBackend disk("/tmp/orator_test_storage", "test2");

  const uint8_t a[] = "HELLO";
  const uint8_t b[] = "WORLD";

  uint64_t off_a = disk.Write(a, 5);
  uint64_t off_b = disk.Write(b, 5);

  CHECK(off_a == 0, "first write at offset 0");
  CHECK(off_b == 5, "second write at offset 5");
  CHECK(disk.total_bytes() == 10, "total_bytes = 10");

  uint8_t buf[5] = {0};
  uint32_t ra = disk.Read(off_a, 5, buf);
  CHECK(ra == 5 && std::memcmp(buf, a, 5) == 0, "first write readable");

  uint32_t rb = disk.Read(off_b, 5, buf);
  CHECK(rb == 5 && std::memcmp(buf, b, 5) == 0, "second write readable");

  std::printf("PASS\n");
}

static void test_disk_backend_persistence() {
  std::printf("  DiskBackend: file persistence... ");

  // Write data, then let the DiskBackend destructor close the file.
  {
    DiskBackend disk("/tmp/orator_test_storage", "test3");
    const uint8_t data[] = "PERSISTENT";
    disk.Write(data, 10);
  }

  {
    std::string path = "/tmp/orator_test_storage/test3.dat";
    int fd = open(path.c_str(), O_RDONLY);
    CHECK(fd >= 0, "file exists after DiskBackend destruction");

    uint8_t buf[10] = {0};
    ssize_t n = read(fd, buf, 10);
    CHECK(n == 10, "reads 10 bytes from persisted file");
    CHECK(std::memcmp(buf, "PERSISTENT", 10) == 0, "data persists on disk");

    close(fd);
  }

  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// Message serialization roundtrip
// ---------------------------------------------------------------------------

static void test_message_serialization() {
  std::printf("  Message: serialization roundtrip... ");

  Message msg;
  msg.msg_id = 42;
  msg.topic = "audio/raw";
  msg.pipeline = "asr_pipeline";
  msg.pipeline_version = "1.2.3";
  msg.timestamp_sec = 1234.5678;
  msg.wall_clock_sec = 9876.5432;
  msg.qos = 2;
  msg.schema_version = 5;
  msg.data = "{\"text\":\"hello world\"}";

  auto mem = std::make_unique<MemoryBackend>();
  auto disk = std::make_unique<DiskBackend>("/tmp/orator_test_storage", "test4");
  StorageManager manager(std::move(mem), std::move(disk));

  StorageRef ref = manager.Write("audio/raw", msg);
  CHECK(ref.backend == Backend::MEMORY, "default backend is MEMORY");
  CHECK(ref.size > 0, "stored data has size > 0");

  Message read = manager.Read(ref);
  CHECK(read.msg_id == msg.msg_id, "msg_id matches");
  CHECK(read.topic == msg.topic, "topic matches");
  CHECK(read.pipeline == msg.pipeline, "pipeline matches");
  CHECK(read.pipeline_version == msg.pipeline_version, "pipeline_version matches");
  CHECK(read.timestamp_sec == msg.timestamp_sec, "timestamp_sec matches");
  CHECK(read.wall_clock_sec == msg.wall_clock_sec, "wall_clock_sec matches");
  CHECK(read.qos == msg.qos, "qos matches");
  CHECK(read.schema_version == msg.schema_version, "schema_version matches");
  CHECK(read.data == msg.data, "data matches");

  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// StorageManager routing tests
// ---------------------------------------------------------------------------

static void test_storage_manager_default_memory() {
  std::printf("  StorageManager: default MEMORY routing... ");

  auto mem = std::make_unique<MemoryBackend>();
  auto disk = std::make_unique<DiskBackend>("/tmp/orator_test_storage", "test5");
  StorageManager manager(std::move(mem), std::move(disk));

  Message msg;
  msg.msg_id = 1;
  msg.data = "test payload";

  StorageRef ref = manager.Write("audio/raw", msg);
  CHECK(ref.backend == Backend::MEMORY, "default backend is MEMORY");

  TopicRetention ret = manager.GetTopicRetention("audio/raw");
  CHECK(ret.backend == Backend::MEMORY, "default retention is MEMORY");
  CHECK(ret.retention_sec == 120.0, "default retention is 120s");

  std::printf("PASS\n");
}

static void test_storage_manager_disk_routing() {
  std::printf("  StorageManager: DISK routing when configured... ");

  auto mem = std::make_unique<MemoryBackend>();
  auto disk = std::make_unique<DiskBackend>("/tmp/orator_test_storage", "test6");
  StorageManager manager(std::move(mem), std::move(disk));

  // Configure topic for DISK backend.
  TopicRetention config;
  config.backend = Backend::DISK;
  config.retention_sec = 300.0;
  manager.SetTopicBackend("system/events", config);

  TopicRetention ret = manager.GetTopicRetention("system/events");
  CHECK(ret.backend == Backend::DISK, "topic configured for DISK");
  CHECK(ret.retention_sec == 300.0, "retention_sec = 300");

  Message msg;
  msg.msg_id = 100;
  msg.data = "disk event";

  StorageRef ref = manager.Write("system/events", msg);
  CHECK(ref.backend == Backend::DISK, "message routed to DISK");

  // Read back and verify.
  Message read = manager.Read(ref);
  CHECK(read.msg_id == msg.msg_id, "msg_id matches after DISK roundtrip");
  CHECK(read.data == msg.data, "data matches after DISK roundtrip");

  std::printf("PASS\n");
}

static void test_storage_manager_read_back_roundtrip() {
  std::printf("  StorageManager: full read-back roundtrip... ");

  auto mem = std::make_unique<MemoryBackend>();
  auto disk = std::make_unique<DiskBackend>("/tmp/orator_test_storage", "test7");
  StorageManager manager(std::move(mem), std::move(disk));

  // Write to MEMORY.
  Message msg1;
  msg1.msg_id = 1;
  msg1.topic = "audio/raw";
  msg1.data = "{\"samples\":16000}";
  StorageRef ref1 = manager.Write("audio/raw", msg1);

  // Write to DISK.
  TopicRetention config;
  config.backend = Backend::DISK;
  manager.SetTopicBackend("system/log", config);

  Message msg2;
  msg2.msg_id = 2;
  msg2.topic = "system/log";
  msg2.data = "{\"level\":\"info\"}";
  StorageRef ref2 = manager.Write("system/log", msg2);

  // Read both back.
  Message r1 = manager.Read(ref1);
  CHECK(r1.msg_id == 1 && r1.data == msg1.data, "MEMORY roundtrip OK");

  Message r2 = manager.Read(ref2);
  CHECK(r2.msg_id == 2 && r2.data == msg2.data, "DISK roundtrip OK");

  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// TimeIndex tests
// ---------------------------------------------------------------------------

static void test_time_index_append_in_order() {
  std::printf("  TimeIndex: append in order... ");
  TimeIndex idx;

  StorageRef ref1{Backend::MEMORY, 0, 10};
  StorageRef ref2{Backend::MEMORY, 10, 20};
  StorageRef ref3{Backend::MEMORY, 30, 30};

  bool ooo1 = idx.Append("audio/raw", 1.0, ref1, 1);
  bool ooo2 = idx.Append("audio/raw", 2.0, ref2, 2);
  bool ooo3 = idx.Append("audio/raw", 3.0, ref3, 3);

  CHECK(!ooo1, "first append is in order");
  CHECK(!ooo2, "second append is in order");
  CHECK(!ooo3, "third append is in order");

  auto all = idx.GetAll("audio/raw");
  CHECK(all.size() == 3, "3 entries in index");
  CHECK(all[0].timestamp_sec == 1.0, "first entry at 1.0");
  CHECK(all[1].timestamp_sec == 2.0, "second entry at 2.0");
  CHECK(all[2].timestamp_sec == 3.0, "third entry at 3.0");

  std::printf("PASS\n");
}

static void test_time_index_append_out_of_order() {
  std::printf("  TimeIndex: append out-of-order... ");
  TimeIndex idx;

  StorageRef ref1{Backend::MEMORY, 0, 10};
  StorageRef ref2{Backend::MEMORY, 10, 20};

  idx.Append("audio/raw", 3.0, ref1, 1);
  bool ooo = idx.Append("audio/raw", 1.5, ref2, 2);

  CHECK(ooo, "out-of-order append returns true");

  // Verify entries are sorted by timestamp.
  auto all = idx.GetAll("audio/raw");
  CHECK(all.size() == 2, "2 entries in index");
  CHECK(all[0].timestamp_sec == 1.5, "first entry is 1.5 (sorted)");
  CHECK(all[1].timestamp_sec == 3.0, "second entry is 3.0 (sorted)");

  std::printf("PASS\n");
}

static void test_time_index_replay() {
  std::printf("  TimeIndex: replay from timestamp... ");
  TimeIndex idx;

  for (int i = 0; i < 5; ++i) {
    StorageRef ref{Backend::MEMORY, static_cast<uint64_t>(i) * 10, 10};
    idx.Append("audio/raw", static_cast<double>(i) * 0.5, ref, static_cast<uint64_t>(i));
  }

  // Replay from 1.0s (should get timestamps 1.0, 1.5, 2.0 = indices 2,3,4).
  auto replay = idx.Replay("audio/raw", 1.0);
  CHECK(replay.size() == 3, "replay returns 3 entries from 1.0s");
  CHECK(replay[0].timestamp_sec == 1.0, "first replayed at 1.0");
  CHECK(replay[1].timestamp_sec == 1.5, "second replayed at 1.5");
  CHECK(replay[2].timestamp_sec == 2.0, "third replayed at 2.0");

  // Replay from a nonexistent topic.
  auto empty = idx.Replay("nonexistent", 0.0);
  CHECK(empty.empty(), "replay of nonexistent topic is empty");

  std::printf("PASS\n");
}

static void test_time_index_last() {
  std::printf("  TimeIndex: last message... ");
  TimeIndex idx;

  StorageRef ref{Backend::MEMORY, 0, 10};
  idx.Append("audio/raw", 1.0, ref, 1);

  IndexedMessage const* last = idx.Last("audio/raw");
  CHECK(last != nullptr, "Last returns non-null for existing topic");
  CHECK(last->timestamp_sec == 1.0, "last timestamp is 1.0");
  CHECK(last->msg_id == 1, "last msg_id is 1");

  // Nonexistent topic.
  IndexedMessage const* none = idx.Last("nonexistent");
  CHECK(none == nullptr, "Last returns nullptr for nonexistent topic");

  std::printf("PASS\n");
}

static void test_time_index_retain() {
  std::printf("  TimeIndex: retain (eviction)... ");
  TimeIndex idx;

  for (int i = 0; i < 10; ++i) {
    StorageRef ref{Backend::MEMORY, static_cast<uint64_t>(i) * 10, 10};
    idx.Append("audio/raw", static_cast<double>(i), ref, static_cast<uint64_t>(i));
  }

  CHECK(idx.GetAll("audio/raw").size() == 10, "10 entries before retain");

  // Retain messages from 5.0s onward.
  idx.Retain("audio/raw", 5.0);

  auto remaining = idx.GetAll("audio/raw");
  CHECK(remaining.size() == 5, "5 entries after retain (5.0, 6.0, 7.0, 8.0, 9.0)");
  CHECK(remaining[0].timestamp_sec == 5.0, "oldest retained is 5.0");
  CHECK(remaining[4].timestamp_sec == 9.0, "newest retained is 9.0");

  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  std::printf("Testing protocol storage layer (Spec 004 Phase 10)...\n\n");

  std::printf("MemoryBackend:\n");
  test_memory_backend_write_read_small();
  test_memory_backend_write_read_large();
  test_memory_backend_capacity_eviction();

  std::printf("\nDiskBackend:\n");
  test_disk_backend_write_read();
  test_disk_backend_multiple_writes();
  test_disk_backend_persistence();

  std::printf("\nMessage serialization:\n");
  test_message_serialization();

  std::printf("\nStorageManager:\n");
  test_storage_manager_default_memory();
  test_storage_manager_disk_routing();
  test_storage_manager_read_back_roundtrip();

  std::printf("\nTimeIndex:\n");
  test_time_index_append_in_order();
  test_time_index_append_out_of_order();
  test_time_index_replay();
  test_time_index_last();
  test_time_index_retain();

  if (g_fail == 0) {
    std::printf("\nAll storage tests PASSED\n");
    return 0;
  }
  std::printf("\nStorage tests FAILED (%d checks)\n", g_fail);
  return 1;
}
