#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app_controller.h"
#include "audio_adapter.h"
#include "city_catalog.h"
#include "display_controller.h"
#include "display_link.h"
#include "identity.h"
#include "input_service.h"
#include "map_renderer.h"
#include "protocol_v2.h"
#include "radio_link.h"
#include "sd_audio_catalog.h"
#include "source_manager.h"
#include "wifi_service.h"

#include "city_data.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void checkTrue(bool condition, const char* expression, int line) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::printf("FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(condition) checkTrue((condition), #condition, __LINE__)

struct FakePins : radio::PinReader {
  bool a = false;
  bool b = false;
  bool sw = false;
  bool readA() override { return a; }
  bool readB() override { return b; }
  bool readSwitch() override { return sw; }
};

struct FakeAudio : radio::AudioAdapterInterface {
  radio::AudioStartRequest last{};
  int starts = 0;
  int stops = 0;
  bool muted = true;

  void start(const radio::AudioStartRequest& request) override {
    last = request;
    ++starts;
  }
  void stop() override { ++stops; }
  void setMuted(bool value) override { muted = value; }
};

struct FakeLink : radio::DisplayLinkInterface {
  std::vector<radio::Snapshot> snapshots;
  int acks = 0;
  bool degraded = false;
  uint16_t session = 0;

  void setSession(uint16_t value) override { session = value; }
  void sendSnapshot(const radio::Snapshot& value) override {
    snapshots.push_back(value);
  }
  void resendSnapshot() override {}
  void sendAck(uint16_t, uint32_t) override { ++acks; }
  void markDisplayDegraded() override { degraded = true; }
};

radio::CityCatalog makeCatalog() {
  radio::CityCatalog catalog;
  catalog.cities = radio::generated::kCities;
  catalog.count = radio::generated::kCityCount;
  catalog.configId = radio::generated::kConfigId;
  catalog.complete = radio::generated::kComplete;
  catalog.origin = radio::generated::kOrigin;
  return catalog;
}

radio::AppController::Config makeConfig(radio::StandbyPolicy policy,
                                        uint16_t session) {
  radio::AppController::Config config;
  config.standbyPolicy = policy;
  config.initialSession = session;
  config.firstCity = 0;
  return config;
}

struct Harness {
  radio::ManualClock clock;
  FakeAudio audio;
  FakeLink link;
  radio::CityCatalog catalog;
  radio::AppController::Config config;
  radio::AppController controller;

  explicit Harness(radio::StandbyPolicy policy = radio::StandbyPolicy::AfterInitialLoad,
                   uint16_t session = 0x1234, bool wifi = true,
                   uint32_t startTime = 0)
      : catalog(makeCatalog()),
        config(makeConfig(policy, session)),
        controller(clock, catalog, audio, link, config) {
    controller.begin(startTime);
    if (wifi) controller.setWifiConnected(true);
  }

  void at(uint32_t value) {
    clock.set(value);
    controller.run(value);
  }

  void rotate(int8_t delta, uint32_t value) {
    clock.set(value);
    controller.queueRotate(delta);
    controller.run(value);
  }

  void press(uint32_t value) {
    clock.set(value);
    controller.queuePress();
    controller.run(value);
  }

  void progress(uint32_t value, uint32_t request = 0, uint32_t attempt = 0) {
    radio::AudioEvent event;
    event.kind = radio::AudioEvent::Kind::PcmProgress;
    event.session = controller.session();
    event.request = request == 0 ? controller.request() : request;
    event.attempt = attempt == 0 ? controller.attempt() : attempt;
    event.city = controller.city();
    clock.set(value);
    controller.queueAudio(event);
    controller.run(value);
  }

  void done(uint32_t value, uint32_t request = 0, uint8_t city = 0xFF) {
    radio::DisplayDoneEvent event;
    event.session = controller.session();
    event.request = request == 0 ? controller.request() : request;
    event.city = city == 0xFF ? controller.city() : city;
    clock.set(value);
    controller.queueDisplayDone(event);
    controller.run(value);
  }

  void fail(radio::FailureKind kind, uint32_t value, uint32_t request = 0,
            uint32_t attempt = 0) {
    radio::AudioEvent event;
    event.kind = radio::AudioEvent::Kind::Failed;
    event.session = controller.session();
    event.request = request == 0 ? controller.request() : request;
    event.attempt = attempt == 0 ? controller.attempt() : attempt;
    event.city = controller.city();
    event.failure = kind;
    clock.set(value);
    controller.queueAudio(event);
    controller.run(value);
  }

  void ended(uint32_t value, uint32_t request = 0, uint32_t attempt = 0) {
    radio::AudioEvent event;
    event.kind = radio::AudioEvent::Kind::Ended;
    event.session = controller.session();
    event.request = request == 0 ? controller.request() : request;
    event.attempt = attempt == 0 ? controller.attempt() : attempt;
    event.city = controller.city();
    clock.set(value);
    controller.queueAudio(event);
    controller.run(value);
  }

  const radio::Snapshot& last() const { return link.snapshots.back(); }
};

void testEncoder() {
  std::printf("- encoder\n");
  FakePins pins;
  radio::InputService input(pins);
  input.begin(0);
  radio::InputEvent event;

  const bool forward[4][2] = {{false, true}, {true, true}, {true, false}, {false, false}};
  for (int cycle = 0; cycle < 50; ++cycle) {
    for (int step = 0; step < 4; ++step) {
      pins.a = forward[step][0];
      pins.b = forward[step][1];
      const bool got = input.update(static_cast<uint32_t>(cycle * 4 + step), event);
      if (step < 3) {
        CHECK(!got);
      } else {
        CHECK(got);
        CHECK(event.kind == radio::InputEventKind::Rotate);
        CHECK(event.delta == 1);
      }
    }
  }
  CHECK(input.update(1000, event) == false);

  const bool reverse[4][2] = {{true, false}, {true, true}, {false, true}, {false, false}};
  for (int step = 0; step < 4; ++step) {
    pins.a = reverse[step][0];
    pins.b = reverse[step][1];
    const bool got = input.update(2000 + step, event);
    if (step < 3) {
      CHECK(!got);
    } else {
      CHECK(got);
      CHECK(event.delta == -1);
    }
  }

  // Bounce between two adjacent states must not produce a detent.
  for (int i = 0; i < 10; ++i) {
    pins.a = true;
    pins.b = false;
    CHECK(!input.update(3000 + i * 2, event));
    pins.a = false;
    pins.b = false;
    CHECK(!input.update(3001 + i * 2, event));
  }

  radio::InputService pressInput(pins);
  pressInput.begin(4000);
  pins.sw = true;
  CHECK(!pressInput.update(4000, event));
  CHECK(!pressInput.update(4050, event));
  pins.sw = false;
  CHECK(!pressInput.update(4100, event));
  CHECK(pressInput.update(4150, event));
  CHECK(event.kind == radio::InputEventKind::EncoderPress);

  // A press shorter than the debounce window is discarded.
  radio::InputService shortInput(pins);
  shortInput.begin(5000);
  pins.sw = true;
  CHECK(!shortInput.update(5000, event));
  pins.sw = false;
  CHECK(!shortInput.update(5010, event));
  CHECK(!shortInput.update(5060, event));
}

void testEncoderCalibration() {
  std::printf("- encoder calibration\n");
  FakePins pins;
  radio::InputService input(pins);
  input.configure({2, 20});
  input.begin(0);
  radio::InputEvent event;

  const bool forward[4][2] = {{false, true}, {true, true}, {true, false}, {false, false}};
  int detents = 0;
  for (int step = 0; step < 4; ++step) {
    pins.a = forward[step][0];
    pins.b = forward[step][1];
    if (input.update(static_cast<uint32_t>(step), event)) {
      CHECK(event.kind == radio::InputEventKind::Rotate);
      CHECK(event.delta == 1);
      ++detents;
    }
  }
  CHECK(detents == 2);

  pins.sw = true;
  CHECK(!input.update(100, event));
  CHECK(!input.update(120, event));
  pins.sw = false;
  CHECK(!input.update(130, event));
  CHECK(input.update(150, event));
  CHECK(event.kind == radio::InputEventKind::EncoderPress);
}

void testIdentity() {
  std::printf("- identity\n");
  radio::Identity identity;
  identity.reset(0x1234);
  auto first = identity.newRequest();
  auto second = identity.newRequest();
  CHECK(first.session == 0x1234);
  CHECK(first.request == 1);
  CHECK(second.request == 2);
  CHECK(identity.nextAttempt() == 1);
  CHECK(identity.nextAttempt() == 2);

  identity.setNextRequestForTest(0xFFFFFFF0u);
  const auto wrapped = identity.newRequest();
  CHECK(wrapped.session != 0x1234);
  CHECK(wrapped.request == 1);
}

void testCityCatalog() {
  std::printf("- city catalog\n");
  const radio::CityCatalog catalog = makeCatalog();
  CHECK(catalog.complete);
  CHECK(catalog.count == 8);
  CHECK(catalog.validate());
  CHECK(catalog.byId(0) != nullptr);
  CHECK(std::strcmp(catalog.byId(0)->code, "FIX_A") == 0);
  CHECK(catalog.byDisplayOrder(7) != nullptr);
  CHECK(catalog.byDisplayOrder(7)->id == 7);
  CHECK(catalog.byId(8) == nullptr);

  radio::City cities[8];
  for (size_t i = 0; i < 8; ++i) cities[i] = radio::generated::kCities[i];
  cities[0].timezoneRule = "";
  radio::CityCatalog invalid;
  invalid.cities = cities;
  invalid.count = 8;
  invalid.configId = radio::generated::kConfigId;
  invalid.complete = true;
  CHECK(!invalid.validate());
}

void testSourceManager() {
  std::printf("- source manager\n");
  radio::SourceManager manager;
  manager.begin(true);
  CHECK(manager.stage() == radio::SourceStage::Main);
  CHECK(manager.advance(radio::FailureKind::ConnectRefused) == radio::SourceStage::Backup);
  CHECK(manager.advance(radio::FailureKind::ConnectRefused) == radio::SourceStage::Local);
  CHECK(manager.advance(radio::FailureKind::FileMissing) == radio::SourceStage::Error);
  CHECK(manager.failed());

  radio::SourceManager offline;
  offline.begin(false);
  CHECK(offline.stage() == radio::SourceStage::Local);
  CHECK(offline.forceLocal() == radio::SourceStage::Local);
}

void testProtocolParser() {
  std::printf("- protocol parser\n");
  radio::protocol::FrameParser parser;
  radio::protocol::Frame frame;
  const std::string hello = "HELLO:2:A:1234:5CC48362\n";
  for (size_t i = 0; i < hello.size(); ++i) {
    const auto status = parser.feed(static_cast<uint8_t>(hello[i]), frame);
    if (i + 1 < hello.size()) {
      CHECK(status == radio::protocol::ParseStatus::NeedMore);
    } else {
      CHECK(status == radio::protocol::ParseStatus::Ok);
    }
  }
  CHECK(frame.kind == radio::protocol::FrameKind::Hello);
  CHECK(frame.board == 0);
  CHECK(frame.boot == 0x1234);
  CHECK(frame.config == 0x5CC48362);

  const std::string st = "ST:2:1234:7:7:MAP:RUNNING:READY:BACKUP\r\n";
  for (char c : st) parser.feed(static_cast<uint8_t>(c), frame);
  CHECK(frame.kind == radio::protocol::FrameKind::Snapshot);
  CHECK(frame.session == 0x1234);
  CHECK(frame.request == 7);
  CHECK(frame.city == 7);
  CHECK(frame.view == radio::View::Map);
  CHECK(frame.anim == radio::Anim::Running);
  CHECK(frame.audio == radio::AudioState::ReadyMuted);
  CHECK(frame.source == radio::Source::Backup);

  for (char c : std::string("DONE:2:1234:9:3\n")) {
    parser.feed(static_cast<uint8_t>(c), frame);
  }
  CHECK(frame.kind == radio::protocol::FrameKind::Done);
  CHECK(frame.request == 9);
  CHECK(frame.city == 3);

  for (char c : std::string("ACK:2:1234:9:DONE\n")) {
    parser.feed(static_cast<uint8_t>(c), frame);
  }
  CHECK(frame.kind == radio::protocol::FrameKind::Ack);
  CHECK(frame.ackDone);

  const std::string coalesced = "DONE:2:1234:1:0\nDONE:2:1234:2:1\n";
  int completed = 0;
  for (char c : coalesced) {
    if (parser.feed(static_cast<uint8_t>(c), frame) == radio::protocol::ParseStatus::Ok) {
      ++completed;
    }
  }
  CHECK(completed == 2);
  CHECK(frame.request == 2);

  const size_t droppedBefore = parser.droppedFrames();
  for (int i = 0; i < 200; ++i) {
    parser.feed('A', frame);
  }
  CHECK(parser.feed('\n', frame) == radio::protocol::ParseStatus::Oversize);
  CHECK(parser.droppedFrames() == droppedBefore + 1);
  for (char c : std::string("DONE:2:1234:3:2\n")) {
    parser.feed(static_cast<uint8_t>(c), frame);
  }
  CHECK(frame.request == 3);

  const char* invalid[] = {
      "ST:2:1234:1:8:MAP:RUNNING:READY:BACKUP",
      "ST:2:1234:1:7:map:RUNNING:READY:BACKUP",
      "ST:2:1234:1:7:MAP:RUNNING:READY:BACKUP:EXTRA",
      "ST:2:0000:1:7:MAP:RUNNING:READY:BACKUP",
      "ST:2:1234:-1:7:MAP:RUNNING:READY:BACKUP",
      "HELLO:1:A:1234:5CC48362",
      "HELLO:2:C:1234:5CC48362",
      "UNKNOWN:2:1234:1:0",
  };
  for (const char* line : invalid) {
    radio::protocol::Frame invalidFrame;
    CHECK(!radio::protocol::parseLine(line, std::strlen(line), invalidFrame));
  }

  char buffer[radio::protocol::kMaxFrameBytes];
  const radio::Snapshot snapshot{0x1234, 4294967295u, 7, radio::View::Map,
                                 radio::Anim::Running, radio::AudioState::Connecting,
                                 radio::Source::Backup};
  const size_t encoded = radio::protocol::encodeSnapshot(buffer, sizeof(buffer), snapshot);
  CHECK(encoded > 0);
  radio::protocol::Frame roundTrip;
  CHECK(radio::protocol::parseLine(buffer, encoded, roundTrip));
  CHECK(roundTrip.request == 4294967295u);
  const radio::Snapshot playing{0x1234, 6, 2, radio::View::Map,
                                radio::Anim::Done, radio::AudioState::Playing,
                                radio::Source::Local};
  const size_t playingEncoded =
      radio::protocol::encodeSnapshot(buffer, sizeof(buffer), playing);
  CHECK(playingEncoded > 0);
  radio::protocol::Frame playingTrip;
  CHECK(radio::protocol::parseLine(buffer, playingEncoded, playingTrip));
  CHECK(playingTrip.audio == radio::AudioState::Playing);
  CHECK(std::string(buffer, playingEncoded).find("PLAYING") != std::string::npos);
  CHECK(radio::protocol::encodeSnapshot(buffer, 4, snapshot) == 0);
}

struct BufferTransport : radio::LinkTransport {
  std::string data;
  void write(const uint8_t* bytes, size_t length) override {
    data.append(reinterpret_cast<const char*>(bytes), length);
  }
  bool contains(const std::string& text) const { return data.find(text) != std::string::npos; }
};

void testRadioLink() {
  std::printf("- radio link\n");
  BufferTransport transport;
  radio::RadioLink::Config config;
  config.configId = "5CC48362";
  config.bootId = 0x1234;
  radio::RadioLink link(transport, config);
  link.begin(0x1234, 0);
  CHECK(transport.contains("HELLO:2:A:1234:5CC48362\n"));

  link.handleLine("HELLO:2:B:ABCD:5CC48362", std::strlen("HELLO:2:B:ABCD:5CC48362"), 0);
  CHECK(link.peerHello());
  CHECK(!link.configMismatch());

  const radio::Snapshot snapshot{0x1234, 5, 3, radio::View::Map,
                                 radio::Anim::Running, radio::AudioState::Connecting,
                                 radio::Source::Main};
  link.sendSnapshot(snapshot);
  CHECK(transport.contains("ST:2:1234:5:3:MAP:RUNNING:CONNECTING:MAIN\n"));

  link.handleLine("DONE:2:1234:5:3", std::strlen("DONE:2:1234:5:3"), 10);
  radio::LinkEvent event;
  CHECK(link.pollEvent(event));
  CHECK(event.kind == radio::LinkEvent::Kind::Done);
  CHECK(event.request == 5);
  CHECK(event.city == 3);

  link.sendAck(0x1234, 5);
  CHECK(transport.contains("ACK:2:1234:5:DONE\n"));

  BufferTransport mismatchTransport;
  radio::RadioLink mismatch(mismatchTransport, config);
  mismatch.begin(0x1234, 0);
  mismatch.handleLine("HELLO:2:B:ABCD:00000000", std::strlen("HELLO:2:B:ABCD:00000000"), 0);
  CHECK(mismatch.configMismatch());

  BufferTransport rebootTransport;
  radio::RadioLink reboot(rebootTransport, config);
  reboot.begin(0x1234, 0);
  reboot.handleLine("HELLO:2:B:AAAA:5CC48362", std::strlen("HELLO:2:B:AAAA:5CC48362"), 0);
  reboot.handleLine("HELLO:2:B:BBBB:5CC48362", std::strlen("HELLO:2:B:BBBB:5CC48362"), 0);
  bool sawReboot = false;
  radio::LinkEvent rebootEvent;
  while (reboot.pollEvent(rebootEvent)) {
    if (rebootEvent.kind == radio::LinkEvent::Kind::DisplayRebooted) sawReboot = true;
  }
  CHECK(sawReboot);
}

void testDisplayController() {
  std::printf("- display controller\n");
  radio::DisplayController controller;
  controller.begin(0xBEEF, "5CC48362", 0);
  controller.handleHello(0, 0x1234, 0x5CC48362, 0);
  CHECK(!controller.configMismatch());

  const radio::Snapshot snapshot{0x1234, 1, 2, radio::View::Map,
                                 radio::Anim::Running, radio::AudioState::Connecting,
                                 radio::Source::Main};
  controller.handleSnapshot(snapshot, 0);
  CHECK(controller.state().anim == radio::Anim::Running);
  controller.handleSnapshot(snapshot, 500);
  CHECK(controller.state().request == 1);
  CHECK(controller.tick(500) == false);
  CHECK(std::fabs(controller.state().arcProgress - (500.0f / 1500.0f)) < 0.01f);
  CHECK(!controller.tick(1499));
  CHECK(controller.tick(1500));
  radio::DisplayDoneEvent done;
  CHECK(controller.takeDone(done));
  CHECK(done.session == 0x1234);
  CHECK(done.request == 1);
  CHECK(done.city == 2);
  CHECK(controller.state().anim == radio::Anim::Done);

  CHECK(controller.tick(1749) == false);
  CHECK(controller.tick(1750));
  CHECK(controller.takeDone(done));
  CHECK(controller.doneRetryCount() == 1);
  controller.handleAck(0x1234, 1);
  CHECK(!controller.state().awaitingAck);
  CHECK(controller.tick(2000) == false);

  const radio::Snapshot standbySnapshot{0x1234, 1, 2, radio::View::Standby,
                                        radio::Anim::None, radio::AudioState::Idle,
                                        radio::Source::None};
  controller.handleSnapshot(standbySnapshot, 2100);
  CHECK(controller.state().view == radio::View::Standby);
  CHECK(controller.tick(2200) == false);

  radio::DisplayController retry;
  retry.begin(0xBEEF, "5CC48362", 0);
  retry.handleHello(0, 0x1234, 0x5CC48362, 0);
  retry.handleSnapshot(snapshot, 0);
  CHECK(retry.tick(1500));
  CHECK(retry.takeDone(done));
  int retries = 0;
  for (int i = 1; i <= 20; ++i) {
    if (retry.tick(1500 + i * 250)) {
      CHECK(retry.takeDone(done));
      ++retries;
    }
  }
  CHECK(retries == 8);
  CHECK(!retry.state().awaitingAck);

  radio::DisplayController stale;
  stale.begin(0xBEEF, "5CC48362", 0);
  stale.handleHello(0, 0x1234, 0x5CC48362, 0);
  const radio::Snapshot newer{0x1234, 5, 3, radio::View::Map,
                              radio::Anim::Running, radio::AudioState::Connecting,
                              radio::Source::Main};
  stale.handleSnapshot(newer, 0);
  const radio::Snapshot older{0x1234, 4, 1, radio::View::Map,
                              radio::Anim::Running, radio::AudioState::Connecting,
                              radio::Source::Main};
  stale.handleSnapshot(older, 100);
  CHECK(stale.state().request == 5);
  CHECK(stale.state().city == 3);

  radio::DisplayController mismatch;
  mismatch.begin(0xBEEF, "5CC48362", 0);
  mismatch.handleHello(0, 0x1234, 0xDEADBEEF, 0);
  CHECK(mismatch.configMismatch());
  CHECK(mismatch.state().view == radio::View::Standby);
}

void testDisplayLink() {
  std::printf("- display link\n");
  BufferTransport transport;
  radio::DisplayController controller;
  radio::DisplayLink::Config config;
  config.configId = "5CC48362";
  config.bootId = 0xBEEF;
  radio::DisplayLink link(transport, controller, config);
  controller.begin(0xBEEF, "5CC48362", 0);
  link.begin(0);
  CHECK(transport.contains("HELLO:2:B:BEEF:5CC48362\n"));
  link.handleLine("HELLO:2:A:1234:5CC48362", std::strlen("HELLO:2:A:1234:5CC48362"), 0);
  link.handleLine("ST:2:1234:1:0:MAP:RUNNING:CONNECTING:MAIN", std::strlen("ST:2:1234:1:0:MAP:RUNNING:CONNECTING:MAIN"), 0);
  link.tick(1500);
  CHECK(transport.contains("DONE:2:1234:1:0\n"));
  link.handleLine("ACK:2:1234:1:DONE", std::strlen("ACK:2:1234:1:DONE"), 1500);
  CHECK(!controller.state().awaitingAck);
}

void testMapRenderer() {
  std::printf("- map renderer\n");
  radio::MapRenderer renderer;
  const radio::MapPoint shanghai = renderer.project({121474, 31230});
  CHECK(std::fabs(shanghai.x - 837.4f) < 1.0f);
  CHECK(std::fabs(shanghai.y - 163.2f) < 1.0f);
  CHECK(renderer.crossesDateline({170000, 0}, {-170000, 0}));
  CHECK(!renderer.crossesDateline({100000, 0}, {120000, 0}));
  const std::vector<radio::ArcPoint> arc =
      renderer.buildArc({170000, 0}, {-170000, 0}, 48);
  CHECK(arc.size() == 49);
  bool hasBreak = false;
  for (const auto& point : arc) hasBreak = hasBreak || point.breakBefore;
  CHECK(hasBreak);
  const radio::MapPoint start = renderer.pointAt({121474, 31230}, {139692, 35690}, 0.0f);
  const radio::MapPoint end = renderer.pointAt({121474, 31230}, {139692, 35690}, 1.0f);
  CHECK(std::fabs(start.x - shanghai.x) < 0.5f);
  CHECK(std::fabs(end.x - renderer.project({139692, 35690}).x) < 0.5f);
  CHECK(radio::MapRenderer::easeOutCubic(-1.0f) == 0.0f);
  CHECK(radio::MapRenderer::easeOutCubic(2.0f) == 1.0f);
}

void testAppControllerBasic() {
  std::printf("- app controller basic\n");
  Harness harness;
  harness.rotate(1, 0);
  CHECK(harness.controller.city() == 1);
  CHECK(harness.controller.view() == radio::View::Map);
  CHECK(harness.controller.anim() == radio::Anim::Running);
  CHECK(harness.controller.source() == radio::Source::Main);
  CHECK(harness.controller.audioState() == radio::AudioState::Connecting);
  CHECK(harness.audio.muted);

  harness.progress(100);
  harness.progress(300);
  CHECK(harness.controller.audioReady());
  CHECK(harness.controller.audioState() == radio::AudioState::ReadyMuted);
  CHECK(!harness.controller.soundOpened());
  CHECK(harness.audio.muted);

  harness.done(1500);
  CHECK(harness.controller.soundOpened());
  CHECK(!harness.audio.muted);
  CHECK(harness.controller.audioState() == radio::AudioState::Playing);
  CHECK(harness.controller.firstSoundAt() == 1500);

  const auto beforePress = harness.controller.snapshot();
  harness.press(2000);
  CHECK(harness.controller.view() == beforePress.view);
  CHECK(harness.controller.source() == beforePress.source);
  CHECK(harness.controller.city() == beforePress.city);

  Harness standbyHarness;
  standbyHarness.press(0);
  CHECK(!standbyHarness.controller.active());
  CHECK(standbyHarness.controller.view() == radio::View::Standby);
}

void testCityWrap() {
  std::printf("- city wrap\n");
  Harness harness;
  harness.rotate(1, 0);
  CHECK(harness.controller.city() == 1);
  harness.rotate(-1, 1);
  CHECK(harness.controller.city() == 0);
  harness.rotate(-1, 2);
  CHECK(harness.controller.city() == 7);
  harness.rotate(1, 3);
  CHECK(harness.controller.city() == 0);
  harness.rotate(1, 4);
  CHECK(harness.controller.city() == 1);
}

void testReadyOrders() {
  std::printf("- ready orders\n");
  Harness animationFirst;
  animationFirst.rotate(1, 0);
  animationFirst.done(1500);
  CHECK(!animationFirst.controller.soundOpened());
  animationFirst.progress(1600);
  animationFirst.progress(1800);
  CHECK(animationFirst.controller.soundOpened());
  CHECK(animationFirst.controller.firstSoundAt() == 1800);

  Harness audioFirst;
  audioFirst.rotate(1, 0);
  audioFirst.progress(300);
  audioFirst.progress(500);
  CHECK(!audioFirst.controller.soundOpened());
  CHECK(audioFirst.audio.muted);
  audioFirst.done(1500);
  CHECK(audioFirst.controller.soundOpened());
  CHECK(audioFirst.controller.firstSoundAt() == 1500);
}

void testDisplayDegradedAndDuplicateDone() {
  std::printf("- display degraded and duplicate done\n");
  Harness degraded;
  degraded.rotate(1, 0);
  degraded.progress(300);
  degraded.progress(500);
  degraded.at(3499);
  CHECK(!degraded.controller.displayDegraded());
  CHECK(!degraded.controller.soundOpened());
  degraded.at(3500);
  CHECK(degraded.controller.displayDegraded());
  CHECK(degraded.controller.soundOpened());
  CHECK(degraded.controller.firstSoundAt() == 3500);
  const int acksBefore = degraded.link.acks;
  degraded.done(3600);
  CHECK(degraded.link.acks == acksBefore + 1);
  CHECK(!degraded.controller.displayDegraded());
  CHECK(degraded.controller.anim() == radio::Anim::Done);
  CHECK(degraded.controller.soundOpened());

  Harness duplicate;
  duplicate.rotate(1, 0);
  duplicate.progress(300);
  duplicate.progress(500);
  duplicate.done(1500);
  const int firstAcks = duplicate.link.acks;
  duplicate.done(1600);
  CHECK(duplicate.link.acks == firstAcks + 1);
  CHECK(duplicate.controller.soundOpened());
  CHECK(duplicate.controller.anim() == radio::Anim::Done);
}

void testBoardRebootRecovery() {
  std::printf("- board reboot recovery\n");
  radio::DisplayController display;
  display.begin(0xBEEF, "5CC48362", 0);
  display.handleHello(0, 0x1234, 0x5CC48362, 0);
  const radio::Snapshot doneSnapshot{0x1234, 5, 3, radio::View::Map,
                                     radio::Anim::Done, radio::AudioState::Playing,
                                     radio::Source::Main};
  display.handleSnapshot(doneSnapshot, 0);
  CHECK(display.state().anim == radio::Anim::Done);
  CHECK(display.state().awaitingAck);

  radio::DisplayController rebooted;
  rebooted.begin(0xBEEF, "5CC48362", 0);
  rebooted.handleHello(0, 0x1234, 0x5CC48362, 0);
  rebooted.handleSnapshot(doneSnapshot, 0);
  CHECK(rebooted.state().request == 5);
  CHECK(rebooted.state().anim == radio::Anim::Done);
  radio::DisplayDoneEvent done;
  CHECK(rebooted.tick(0));
  CHECK(rebooted.takeDone(done));
  CHECK(done.request == 5);

  radio::DisplayController newSession;
  newSession.begin(0xBEEF, "5CC48362", 0);
  newSession.handleHello(0, 0x1234, 0x5CC48362, 0);
  newSession.handleHello(0, 0x5678, 0x5CC48362, 0);
  const radio::Snapshot oldSession{0x1234, 9, 7, radio::View::Map,
                                   radio::Anim::Running, radio::AudioState::ReadyMuted,
                                   radio::Source::Local};
  newSession.handleSnapshot(oldSession, 0);
  CHECK(newSession.state().request == 0);
  const radio::Snapshot current{0x5678, 1, 2, radio::View::Map,
                                radio::Anim::Running, radio::AudioState::Connecting,
                                radio::Source::Main};
  newSession.handleSnapshot(current, 0);
  CHECK(newSession.state().request == 1);
  CHECK(newSession.state().anim == radio::Anim::Running);
}

void testTimeouts() {
  std::printf("- timeouts\n");
  Harness slow;
  slow.rotate(1, 0);
  slow.at(14999);
  CHECK(slow.controller.source() == radio::Source::Main);
  slow.at(15000);
  CHECK(slow.controller.source() == radio::Source::Backup);
  slow.at(29999);
  CHECK(slow.controller.source() == radio::Source::Backup);
  slow.at(30000);
  CHECK(slow.controller.source() == radio::Source::Local);
  slow.at(31999);
  CHECK(slow.controller.source() == radio::Source::Local);
  slow.at(32000);
  CHECK(slow.controller.hasError());
  CHECK(slow.controller.audioState() == radio::AudioState::Error);

  Harness hard;
  hard.rotate(1, 0);
  hard.fail(radio::FailureKind::ConnectRefused, 1000);
  CHECK(hard.controller.source() == radio::Source::Backup);
  hard.fail(radio::FailureKind::UnsupportedCodec, 2000);
  CHECK(hard.controller.source() == radio::Source::Local);
  hard.fail(radio::FailureKind::FileMissing, 3000);
  CHECK(hard.controller.hasError());

  Harness wifi;
  wifi.rotate(1, 0);
  wifi.controller.setWifiConnected(false);
  wifi.at(5000);
  CHECK(wifi.controller.source() == radio::Source::Local);

  Harness stall;
  stall.rotate(1, 0);
  stall.progress(100);
  stall.progress(300);
  stall.at(10299);
  CHECK(stall.controller.source() == radio::Source::Main);
  stall.at(10300);
  CHECK(stall.controller.source() == radio::Source::Backup);

  Harness silence;
  silence.rotate(1, 0);
  silence.progress(100);
  silence.progress(300);
  for (uint32_t t = 1000; t <= 12000; t += 1000) silence.progress(t);
  CHECK(silence.controller.source() == radio::Source::Main);
  CHECK(!silence.controller.hasError());

  Harness local;
  local.controller.setWifiConnected(false);
  local.rotate(1, 0);
  CHECK(local.controller.source() == radio::Source::Local);
  local.progress(100);
  local.progress(300);
  local.done(1500);
  const uint32_t attemptBefore = local.controller.attempt();
  local.ended(2000);
  CHECK(local.controller.source() == radio::Source::Local);
  CHECK(local.controller.attempt() == attemptBefore + 1);
  CHECK(!local.controller.hasError());

  Harness localTimeout;
  localTimeout.controller.setWifiConnected(false);
  localTimeout.rotate(1, 0);
  localTimeout.at(1999);
  CHECK(localTimeout.controller.source() == radio::Source::Local);
  localTimeout.at(2000);
  CHECK(localTimeout.controller.hasError());

  Harness localFileMissing;
  localFileMissing.controller.setWifiConnected(false);
  localFileMissing.rotate(1, 0);
  const uint32_t missingAttempt = localFileMissing.controller.attempt();
  localFileMissing.fail(radio::FailureKind::FileMissing, 1000);
  CHECK(localFileMissing.controller.hasError());
  CHECK(localFileMissing.controller.attempt() == missingAttempt);

  Harness localDecoderFailure;
  localDecoderFailure.controller.setWifiConnected(false);
  localDecoderFailure.rotate(1, 0);
  CHECK(localDecoderFailure.controller.source() == radio::Source::Local);
  const uint32_t decoderAttempt = localDecoderFailure.controller.attempt();
  localDecoderFailure.fail(radio::FailureKind::DecoderFatal, 1000);
  CHECK(localDecoderFailure.controller.hasError());
  CHECK(localDecoderFailure.controller.source() == radio::Source::Local);
  CHECK(localDecoderFailure.controller.attempt() == decoderAttempt);
}

void testStandby() {
  std::printf("- standby\n");
  Harness initialLoad;
  initialLoad.rotate(1, 0);
  initialLoad.at(20000);
  CHECK(initialLoad.controller.active());
  initialLoad.at(31999);
  CHECK(initialLoad.controller.active());
  initialLoad.at(32000);
  CHECK(initialLoad.controller.hasError());
  initialLoad.at(51999);
  CHECK(initialLoad.controller.active());
  initialLoad.at(52000);
  CHECK(!initialLoad.controller.active());
  CHECK(initialLoad.controller.view() == radio::View::Standby);
  CHECK(initialLoad.controller.source() == radio::Source::None);
  CHECK(initialLoad.controller.audioState() == radio::AudioState::Idle);
  CHECK(initialLoad.audio.muted);

  Harness afterSound;
  afterSound.rotate(1, 0);
  afterSound.progress(300);
  afterSound.done(1500);
  afterSound.progress(20000);
  afterSound.at(21499);
  CHECK(afterSound.controller.active());
  afterSound.at(21500);
  CHECK(!afterSound.controller.active());

  Harness autoSwitch;
  autoSwitch.rotate(1, 0);
  autoSwitch.progress(300);
  autoSwitch.done(1500);
  autoSwitch.fail(radio::FailureKind::ConnectRefused, 5000);
  CHECK(autoSwitch.controller.source() == radio::Source::Backup);
  autoSwitch.progress(20000);
  autoSwitch.progress(20200);
  autoSwitch.at(21499);
  CHECK(autoSwitch.controller.active());
  autoSwitch.at(21500);
  CHECK(!autoSwitch.controller.active());

  Harness pressExtends;
  pressExtends.rotate(1, 0);
  pressExtends.progress(300);
  pressExtends.done(1500);
  pressExtends.press(10000);
  pressExtends.progress(20000);
  pressExtends.progress(29000);
  pressExtends.at(29999);
  CHECK(pressExtends.controller.active());
  pressExtends.at(30000);
  CHECK(!pressExtends.controller.active());

  Harness lastInput(radio::StandbyPolicy::LastUserInput);
  lastInput.rotate(1, 0);
  lastInput.at(19999);
  CHECK(lastInput.controller.active());
  lastInput.at(20000);
  CHECK(!lastInput.controller.active());

  Harness errorStandby;
  errorStandby.rotate(1, 0);
  errorStandby.at(32000);
  CHECK(errorStandby.controller.hasError());
  errorStandby.at(51999);
  CHECK(errorStandby.controller.active());
  errorStandby.at(52000);
  CHECK(!errorStandby.controller.active());

  Harness wake(radio::StandbyPolicy::LastUserInput);
  wake.rotate(1, 0);
  wake.at(20000);
  CHECK(!wake.controller.active());
  wake.rotate(1, 21000);
  CHECK(wake.controller.active());
  CHECK(wake.controller.city() == 2);
  CHECK(wake.controller.view() == radio::View::Map);
  CHECK(wake.controller.source() == radio::Source::Main);
}

void testStandbyWrap() {
  std::printf("- standby wrap\n");
  Harness harness;
  harness.clock.set(0xFFFFF000u);
  harness.controller.begin(0xFFFFF000u);
  harness.controller.setWifiConnected(true);
  harness.rotate(1, 0xFFFFF000u);
  harness.progress(0xFFFFF100u);
  harness.progress(0xFFFFF300u);
  harness.done(0x00000100u);
  CHECK(harness.controller.soundOpened());
  harness.progress(0x00001400u);
  harness.progress(0x00002800u);
  harness.progress(0x00003C00u);
  harness.at(0x00004F1Fu);
  CHECK(harness.controller.active());
  harness.at(0x00004F20u);
  CHECK(!harness.controller.active());
  CHECK(harness.controller.view() == radio::View::Standby);
}

void testPriorityAndStale() {
  std::printf("- priority and stale events\n");
  Harness rotateWins;
  rotateWins.rotate(1, 0);
  rotateWins.rotate(1, 15000);
  CHECK(rotateWins.controller.city() == 2);
  CHECK(rotateWins.controller.source() == radio::Source::Main);
  CHECK(!rotateWins.controller.hasError());

  Harness rotateAndWifi;
  rotateAndWifi.rotate(1, 0);
  rotateAndWifi.controller.queueRotate(1);
  rotateAndWifi.controller.setWifiConnected(false);
  rotateAndWifi.at(5000);
  CHECK(rotateAndWifi.controller.city() == 2);
  CHECK(rotateAndWifi.controller.source() == radio::Source::Local);

  Harness roundTrip;
  roundTrip.rotate(1, 0);
  const uint32_t firstRequest = roundTrip.controller.request();
  roundTrip.rotate(1, 1000);
  const uint32_t secondRequest = roundTrip.controller.request();
  CHECK(secondRequest != firstRequest);
  roundTrip.done(1500, firstRequest);
  CHECK(roundTrip.controller.anim() == radio::Anim::Running);
  roundTrip.done(1600, secondRequest);
  CHECK(roundTrip.controller.anim() == radio::Anim::Done);

  Harness staleAudio;
  staleAudio.rotate(1, 0);
  const uint32_t oldRequest = staleAudio.controller.request();
  const uint32_t oldAttempt = staleAudio.controller.attempt();
  staleAudio.rotate(1, 1000);
  staleAudio.progress(1100, oldRequest, oldAttempt);
  CHECK(!staleAudio.controller.audioReady());
  staleAudio.progress(1200);
  staleAudio.progress(1400);
  CHECK(staleAudio.controller.audioReady());

  Harness staleFailure;
  staleFailure.rotate(1, 0);
  const uint32_t staleRequest = staleFailure.controller.request();
  const uint32_t staleAttempt = staleFailure.controller.attempt();
  staleFailure.rotate(1, 1000);
  staleFailure.fail(radio::FailureKind::ConnectRefused, 1100, staleRequest, staleAttempt);
  CHECK(staleFailure.controller.source() == radio::Source::Main);

  Harness staleEnded;
  staleEnded.rotate(1, 0);
  const uint32_t endedRequest = staleEnded.controller.request();
  const uint32_t endedAttempt = staleEnded.controller.attempt();
  staleEnded.rotate(1, 1000);
  staleEnded.ended(1100, endedRequest, endedAttempt);
  CHECK(staleEnded.controller.source() == radio::Source::Main);
  CHECK(!staleEnded.controller.hasError());
  CHECK(staleEnded.controller.anim() == radio::Anim::Running);

  Harness rapid;
  rapid.rotate(1, 0);
  for (int i = 0; i < 20; ++i) rapid.controller.queueRotate(1);
  rapid.at(1000);
  CHECK(rapid.controller.city() == 5);
  CHECK(rapid.controller.request() == 21);
  CHECK(rapid.controller.source() == radio::Source::Main);
  CHECK(rapid.last().city == 5);
}

struct FakeBackend : radio::AudioBackend {
  int starts = 0;
  int stops = 0;
  bool muted = true;
  bool stopped = true;
  uint32_t progress = 0;
  radio::FailureKind failure = radio::FailureKind::None;
  bool ended = false;
  std::string lastUrl;
  std::string lastPath;

  void startOnline(const char* url) override {
    lastUrl = url;
    ++starts;
    stopped = false;
  }
  void startFile(const char* path) override {
    lastPath = path;
    ++starts;
    stopped = false;
  }
  void stop() override {
    ++stops;
    stopped = true;
  }
  void setMuted(bool value) override { muted = value; }
  bool isStopped() override { return stopped; }
  uint32_t pcmProgressCount() override { return progress; }
  bool takeFailure(radio::FailureKind& kind) override {
    if (failure == radio::FailureKind::None) return false;
    kind = failure;
    failure = radio::FailureKind::None;
    return true;
  }
  bool takeEnded() override {
    if (!ended) return false;
    ended = false;
    return true;
  }
  void loop() override {}
};

void testAudioAdapter() {
  std::printf("- audio adapter\n");
  radio::ManualClock clock;
  FakeBackend backend;
  radio::AudioAdapter adapter(backend, clock, {50, false});
  adapter.begin(0);

  radio::AudioStartRequest request;
  request.session = 0x1234;
  request.request = 1;
  request.attempt = 1;
  request.city = 0;
  request.source = radio::Source::Main;
  request.url = "https://example.invalid/a.mp3";
  adapter.start(request);
  CHECK(backend.starts == 1);
  CHECK(backend.muted);
  radio::AudioEvent event;
  CHECK(adapter.pollEvent(event));
  CHECK(event.kind == radio::AudioEvent::Kind::Started);
  CHECK(event.attempt == 1);

  backend.progress = 100;
  clock.set(100);
  CHECK(adapter.pollEvent(event));
  CHECK(event.kind == radio::AudioEvent::Kind::PcmProgress);
  CHECK(event.sampleCount == 100);

  adapter.stop();
  backend.progress = 200;
  clock.set(110);
  CHECK(!adapter.pollEvent(event));
  CHECK(backend.stopped);

  backend.failure = radio::FailureKind::ConnectRefused;
  radio::AudioStartRequest second = request;
  second.request = 2;
  second.attempt = 2;
  adapter.start(second);
  CHECK(backend.starts == 1);  // queued until quiescence
  backend.stopped = true;
  clock.set(160);
  CHECK(adapter.pollEvent(event));
  CHECK(event.kind == radio::AudioEvent::Kind::Started);
  CHECK(event.attempt == 2);
  CHECK(backend.starts == 2);
  CHECK(!adapter.pollEvent(event));  // stale failure was drained, not relabeled

  backend.failure = radio::FailureKind::ConnectRefused;
  clock.set(200);
  CHECK(adapter.pollEvent(event));
  CHECK(event.kind == radio::AudioEvent::Kind::Failed);
  CHECK(event.failure == radio::FailureKind::ConnectRefused);

  backend.ended = true;
  clock.set(250);
  CHECK(adapter.pollEvent(event));
  CHECK(event.kind == radio::AudioEvent::Kind::Ended);

  adapter.setMuted(false);
  CHECK(!backend.muted);
}

void testWifiAndSd() {
  std::printf("- wifi and sd\n");
  radio::WifiService wifi;
  wifi.begin(0, true);
  CHECK(wifi.connected());
  wifi.update(false, 10);
  CHECK(!wifi.connected());
  CHECK(wifi.takeLostEvent());
  CHECK(!wifi.takeLostEvent());
  CHECK(wifi.initialTimedOut(15000));
  CHECK(wifi.shouldAttemptReconnect(5000));
  wifi.notifyReconnectStarted(5000);
  CHECK(!wifi.shouldAttemptReconnect(9999));
  CHECK(wifi.shouldAttemptReconnect(10000));

  radio::SdAudioCatalog sd;
  radio::City city = radio::generated::kCities[0];
  CHECK(sd.check(city) == radio::SdAudioCatalog::Status::NotMounted);
  sd.setMounted(true);
  CHECK(sd.check(city) == radio::SdAudioCatalog::Status::MissingFile);
  sd.setFileState(city.offlinePath, false, false);
  CHECK(sd.check(city) == radio::SdAudioCatalog::Status::MissingFile);
  sd.setFileState(city.offlinePath, true, true);
  CHECK(sd.check(city) == radio::SdAudioCatalog::Status::CorruptFile);
  sd.setFileState(city.offlinePath, true, false);
  CHECK(sd.check(city) == radio::SdAudioCatalog::Status::Ok);
}

void testClockWrap() {
  std::printf("- clock wrap\n");
  Harness wrap;
  wrap.clock.set(0xFFFFF000u);
  wrap.controller.begin(0xFFFFF000u);
  wrap.controller.setWifiConnected(true);
  wrap.rotate(1, 0xFFFFF000u);
  CHECK(wrap.controller.session() != 0);
  wrap.at(0xFFFFF000u + 15000u);
  CHECK(wrap.controller.source() == radio::Source::Backup);
  wrap.at(0xFFFFF000u + 30000u);
  CHECK(wrap.controller.source() == radio::Source::Local);
  wrap.at(0xFFFFF000u + 32000u);
  CHECK(wrap.controller.hasError());
}

void testSimulatedLongRun() {
  std::printf("- simulated two-hour active run\n");
  Harness active;
  uint32_t now = 0;
  int rotations = 0;
  for (int minute = 0; minute < 120; ++minute) {
    for (int slot = 0; slot < 6; ++slot) {
      now += 10000;
      if (rotations < 200) {
        active.rotate(rotations % 2 == 0 ? 1 : -1, now);
        active.progress(now + 100);
        active.progress(now + 300);
        active.done(now + 500);
        ++rotations;
      } else {
        active.progress(now);
        active.press(now);
      }
    }
  }
  CHECK(rotations == 200);
  CHECK(active.controller.active());
  CHECK(!active.controller.hasError());
  CHECK(active.controller.soundOpened());
}

void testLongStandbyWake() {
  std::printf("- long standby wake\n");
  Harness standby(radio::StandbyPolicy::LastUserInput);
  standby.rotate(1, 0);
  standby.at(20000);
  CHECK(!standby.controller.active());
  standby.at(3600000);
  CHECK(!standby.controller.active());
  standby.rotate(1, 3600000);
  CHECK(standby.controller.active());
  CHECK(standby.controller.city() == 2);
  CHECK(standby.controller.view() == radio::View::Map);
}

}  // namespace

int main() {
  std::printf("radio-project host logic tests\n");
  testEncoder();
  testEncoderCalibration();
  testIdentity();
  testCityCatalog();
  testSourceManager();
  testProtocolParser();
  testRadioLink();
  testDisplayController();
  testDisplayLink();
  testMapRenderer();
  testAppControllerBasic();
  testCityWrap();
  testReadyOrders();
  testDisplayDegradedAndDuplicateDone();
  testBoardRebootRecovery();
  testTimeouts();
  testStandby();
  testStandbyWrap();
  testPriorityAndStale();
  testAudioAdapter();
  testWifiAndSd();
  testClockWrap();
  testSimulatedLongRun();
  testLongStandbyWake();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
