# Coffee Pot Alarm

An embedded vision alarm for rising coffee foam, with an offline ESP32-S3 device
and a reproducible ML lifecycle. This is a learning project focused on Embedded
AI, modern C++, computer vision, runtime internals, and MLOps.

Read [PLAN.md](PLAN.md) for all milestones, tasks, sequencing, decisions, and
acceptance targets. Keep roadmap information there.

## Collaboration

- Act as a consultant: explain mechanisms and trade-offs, help debug, and review
  requested work. The maintainer implements the learning tasks.
- Write code/examples, modify files, or install dependencies only when explicitly
  requested. Reviewing or approving a plan alone does not authorise implementation.
- Work incrementally within the requested scope; preserve unrelated user changes.
- Do not commit, push, or modify GitHub issues, milestones, or boards without
  explicit permission. Never fabricate results or claim unexecuted checks passed.
- Use English throughout; offer brief, supportive language corrections.

## Hardware and stack

Target: ESP32-S3 N16R8, 16 MB flash, 8 MB PSRAM, camera, white illumination,
buzzer, acknowledgement button, status LED, and a mountable enclosure. Verify
actual board/module specifications and GPIO assignments before wiring.

| Area | Technology |
| --- | --- |
| Firmware | Modern C++, ESP-IDF, FreeRTOS |
| Embedded inference | TensorFlow Lite Micro, ESP-NN |
| Host runtime experiments | ONNX Runtime |
| ML and vision | Python, TensorFlow/Keras, OpenCV |
| Annotation | Label Studio or CVAT |
| Backend and storage | FastAPI, MinIO, PostgreSQL |
| Tracking and registry | MLflow |
| Infrastructure | Docker, Kubernetes Jobs, Helm |
| CI and emulation | GitHub Actions, Espressif QEMU |
| Monitoring | Prometheus, Grafana |
| Testing | pytest; GoogleTest and ESP-IDF Unity |

Pin dependencies and verify APIs against selected versions. Keep local commands
available alongside Kubernetes Jobs. Do not use Edge Impulse or introduce extra
orchestration infrastructure without a concrete requirement.

## Coding and architecture

- Use idiomatic C++, RAII, explicit ownership, const correctness, and bounded,
  preallocated buffers. Minimise copies and steady-state dynamic allocation.
- Separate portable application logic from hardware and runtime adapters. Add
  interfaces only when they provide a concrete architectural or testing benefit.
- Keep network I/O outside capture/inference/alarm paths. Use bounded queues,
  stale-frame dropping, watchdogs, and explicit error/recovery behaviour.
- FreeRTOS queues copy bytes: pass simple ownership tokens, not smart pointers
  or objects requiring destructors. Return borrowed camera buffers exactly once.
- Use typed Python, reusable libraries with separate CLI entry points, validated
  inputs, configuration-driven experiments, and reproducible seeds/environments.
- Name the CNN family CoffeeNet. Preserve label order: `NO_POT`, `CALM`,
  `FOAM_FORMING`, `FOAM_RISING`, `UNKNOWN`. Separate predictions from alarm logic.
- Explain allocation, operator execution, numerical behaviour, and performance;
  do not hide runtime internals behind unnecessary wrappers. An experimental
  dispatcher is not a complete LiteRT delegate or ONNX Execution Provider.

## Data, validation, and security

- Preserve raw data and immutable, hashed manifests; split by brewing session.
  Augment training data only. Track meaningful experiments and artifact lineage.
- Use appropriate unit, integration, golden-tensor, replay, numerical, and hardware
  tests. Distinguish QEMU correctness checks from physical-device benchmarks.
- Measure model quality and session behaviour as well as latency and memory;
  report dataset size, uncertainty, configuration, failures, and limitations.
- Keep datasets, credentials, and private keys outside Git. Authenticate device
  traffic, verify TLS certificates and signed models, and use least privilege.
- Keep alarms operational offline. Preserve a known-good model and explicit
  promotion/rollback behaviour; acknowledgement must not suppress danger forever.
- Keep electronics and printed parts away from flames and excessive heat. This
  is a supplementary warning device; gas control and unsupervised overflow tests
  are outside scope. Keep LLMs/agents outside the alarm control path.
