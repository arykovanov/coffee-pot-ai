# Coffee Pot Alarm Eight Week Roadmap

Goal: a job-application portfolio release within two months of starting, with
full-time availability. Use eight implementation weeks and keep any remaining
calendar days as contingency. Dates below are relative to the actual start.

Status: proposed implementation outline; hardware and architecture still need
review. No functional task is marked complete. Each task gives its title and
required outcome; refine its design and tests when starting it.

## Scope and sequencing

- **Core release:** mounted image capture, versioned real datasets, Kubernetes-hosted experiment tracking, a reproducible INT8 model, an offline alarm, runtime measurements, one focused kernel experiment, tests, and a documented demonstration.
- **Capture first:** verify usable frames and bounded continuous capture in Week 1 before implementing ingestion and model/MLOps work.
- **Model and MLOps together:** build the minimal model and dataset/tracking pipeline alongside each other; record every meaningful experiment from the first integrated run.
- **Continuous collection:** collect and annotate supervised brewing sessions from Week 2 onward; reserve independent test sessions before model selection.
- **Offline operation:** network failure must not block capture, inference, or local controls. Recording gaps are acceptable if reported; microSD is deferred.
- **Schedule control:** review actual effort and risks weekly. Reforecast after Week 1, freeze core functionality by the end of Week 7, and protect Week 8 for validation and portfolio delivery.

## M1 Hardware and Continuous Capture — Week 1

Deliver usable camera frames and a repeatable firmware build.

- **W1-01 Confirm hardware and architecture** — Verify the exact N16R8 board, OV7670/FIFO variant, HW-508 circuit, power requirements, safe GPIO map, and initial software boundaries.
- **W1-02 Establish the firmware baseline** — Pin ESP-IDF/components and the C++ dialect; confirm physical boot, 16 MB flash, 8 MB PSRAM, clock settings, and reproducible builds.
- **W1-03 Add peripheral diagnostics** — Verify button debounce, status indications, manually controlled buzzer, and steady illumination through appropriate drivers.
- **W1-04 Validate camera output** — Trial OV7670 RGB565 at 160×120 then 320×240; verify layout, colour order, orientation, and clear views of the pot rim and surface.
- **W1-05 Validate bounded capture** — Implement explicit frame leases and a fixed pool; run continuous acquisition with a slow consumer and measure frame rate, drops, timestamps, and memory use.
- **W1-06 Add build and boot CI** — Run the firmware under pinned Espressif QEMU in GitHub Actions with explicit pass/fail, timeout, and retained logs.

Exit: decoded, useful frames and bounded continuous capture on physical hardware.
If camera suitability is unresolved, time-box the next experiment and revise the
schedule before starting dependent pipeline work.

## M2 Mounted Acquisition and Initial Ingestion — Week 2

Deliver a printed assembly and stored pilot brewing sessions.

- **W2-01 Define acquisition records** — Specify versioned session/device/boot IDs, capture timestamps and layout, hashes, camera/mount settings, and bounded gap/fault summaries.
- **W2-02 Implement raw ingestion** — Store authenticated, validated frame/session uploads durably; verify TLS, idempotent retries, quotas, hashes, and incomplete-write recovery.
- **W2-03 Connect acquisition to ingestion** — Upload selected frames outside the capture task; verify bounded retries/expiry, Wi-Fi/server outage handling, and interrupted sessions after reboot.
- **W2-04 Design and print the mount/enclosure** — Measure components, prototype an adjustable camera bracket, create editable CAD, print the assembly, and verify fit, wiring, access, stability, and temperatures.
- **W2-05 Start supervised collection** — Document setup and annotation/event rules; record at least three pilot brews across lighting conditions plus no-pot/other-cookware clips, with explicit gaps and mount revisions.

## M3 Dataset Pipeline and First Tracked Model — Week 3

Deliver one traceable training run using the Kubernetes tracking stack.
Model work W3-04 proceeds alongside pipeline work W3-02/W3-03; join at W3-05.

- **W3-01 Define shared data/model contracts** — Specify immutable dataset manifests, annotations, preprocessing, label order, model inputs/outputs, source hashes, and session-safe partitions; reject leakage, duplicates, and corrupt data.
- **W3-02 Establish Kubernetes tracking** — Deploy pinned MLflow, PostgreSQL, and MinIO using Helm/configuration; verify persistent storage and parameter/metric/artifact retrieval after restart.
- **W3-03 Publish a dataset snapshot** — Extend ingestion to MinIO, configure annotation tooling, and publish validated raw-session references with versioned labels and partitions before supervised training.
- **W3-04 Build minimal CoffeeNet** — Implement a small CPU-only TensorFlow/Keras CNN with configuration-driven training and save/reload tests; label synthetic plumbing fixtures explicitly.
- **W3-05 Run the joint pipeline** — Execute a Kubernetes training/evaluation Job through the local CLI entry point; log data/source/image versions, seed, parameters, metrics, artifacts, and failures, then retrieve and reload the model.

## M4 Model Evaluation and INT8 Export — Week 4

Deliver an evaluated model candidate through the established tracked pipeline.

- **W4-01 Establish a CV baseline** — Evaluate a simple OpenCV approach on versioned sessions; record metrics and failure cases in MLflow.
- **W4-02 Compare small CNN candidates** — Compare custom/depthwise CoffeeNet variants and selected RGB/grayscale resolutions; retain supported operators and justify choices using measured quality and resource estimates.
- **W4-03 Evaluate session behaviour** — Report frame metrics, critical-event detection, false alarms, and timing on validation sessions; derive initial confidence/temporal thresholds without tuning on held-out tests.
- **W4-04 Export and validate INT8** — Use representative training-only calibration, compare FP32/INT8 quality and outputs, and version model/preprocessing/label metadata; add QAT only if justified.

## M5 Offline Embedded AI Alarm — Week 5

Deliver target inference, deterministic decisions, and an audible offline alarm.

- **W5-01 Integrate TFLite Micro** — Add minimal inference/mock boundaries, operator resolution, tensor validation, allocation diagnostics, and model invocation on ESP32-S3.
- **W5-02 Match preprocessing** — Implement ROI, resize, colour conversion, and quantisation; compare embedded inputs/outputs with host-generated golden tensors.
- **W5-03 Implement temporal decisions** — Test monitoring, pre-alarm, alarm, acknowledgement/rearming, and fault transitions using confidence, hysteresis, and recorded/synthetic sequences.
- **W5-04 Integrate health and offline behaviour** — Verify camera/stale-frame failures, distinct local warnings, watchdog recovery, and responsive alarms without networking.
- **W5-05 Add inference CI** — Run fixed models/inputs in QEMU, verify supported kernel paths and numerical tolerances, and fail on allocation errors, inference failures, or timeouts.

## M6 Runtime Analysis and Benchmarking — Week 6

Deliver a reproducible report explaining runtime memory and performance.

- **W6-01 Inspect model graphs** — Report operator versions, tensor shapes/types, quantisation, dependencies, constants, estimated MACs, and runtime compatibility.
- **W6-02 Analyse tensor memory** — Use allocation recording to explain persistent/scratch allocations, lifetimes, reuse, alignment, arena sizing, SRAM/PSRAM placement, and failure cases.
- **W6-03 Profile physical execution** — Measure camera/preprocessing/inference/decision/alarm stages, per-operator timing, CPU, memory, and throughput with warm-up and at least 1,000 samples where practical.
- **W6-04 Compare kernel configurations** — Compare reference and ESP-NN kernels and memory placement; track numerical checks, model/configuration hashes, statistics, and benchmark artifacts in MLflow.

## M7 Focused Kernel Experiment and Integration — Week 7

Deliver one explained optimisation experiment and freeze the core release.

- **W7-01 Implement a focused INT8 experiment** — Select a bounded kernel operation from Conv2D/DepthwiseConv2D; implement and test accumulation, scales/zero points, requantisation, and saturation against a reference.
- **W7-02 Measure and explain the result** — Report numerical agreement, supported cases, latency/resource costs, and limitations; treat an unsuccessful optimisation as a measured result.
- **W7-03 Resolve core integration issues** — Exercise the mounted capture-to-alarm path and tracked lifecycle; fix leaks, stale-frame handling, resets, preprocessing mismatches, and actionable performance regressions.
- **W7-04 Freeze scope and prepare release evidence** — Finish core tests and reproduction commands, record remaining issues, and move unfinished non-core features to the stretch backlog.

## M8 Independent Validation and Portfolio Delivery — Week 8

Deliver a demonstrable portfolio release with honest evidence and limitations.

- **W8-01 Evaluate independent sessions** — Evaluate held-out brews and difficult scenes; report precision/recall/F1, confusion/PR results, missed events, false alarms/session, warning lead time, and statistical uncertainty.
- **W8-02 Complete stability and fault testing** — Run at least 24 hours and test offline operation, obstruction, lighting changes, acknowledgement, stale frames, hardware failures, and recovery; document regressions and limits.
- **W8-03 Complete technical documentation** — Publish setup, architecture/ADRs, CAD/wiring/print files, dataset/model cards, benchmark methods/results, and reproduction instructions without private data or credentials.
- **W8-04 Prepare the portfolio demonstration** — Record the mounted alarm and tracked model lifecycle; explain C++ ownership, runtime allocation, kernel results, trade-offs, and personal contributions for interviews.
- **W8-05 Prepare the release and application material** — Verify the core release checklist, document unresolved limitations, and prepare a tagged release and concise project description for explicit publication approval.

## Stretch Backlog

Start only after core deliverables are secure; these are not required for the
eight-week portfolio release.

- **Lifecycle Automation and Model Deployment** — Add automated validation/registration gates, explicit promotion, signed compatible manifests, independent A/B model OTA, candidate self-tests, atomic activation outside brewing, and interruption-safe rollback.
- **Broader runtime integration** — Add capability-based kernel dispatch with reference fallback, an ONNX Runtime host backend, or an experimental lifetime-based memory planner.
- **Production telemetry and feedback** — Add Prometheus/Grafana monitoring and bounded event/low-confidence collection with human-reviewed retraining data.
- **Optional AI tools** — Evaluate VLM-assisted annotation or read-only diagnostic MCP tools; keep agents outside the alarm path and defer foundation-model fine-tuning.

## Experimental Targets

| Area | Target |
| --- | --- |
| Model and arena | Model ≤ 250 KB; tensor arena ≤ 1 MB |
| Inference | Median ≤ 100 ms; P95 ≤ 150 ms |
| Processing | P95 end-to-end ≤ 300 ms; sustained ≥ 5 FPS |
| Event detection | Critical-event recall ≥ 95%; false alarms in ≤ 5% of normal sessions |
| Warning and operation | Target ≥ 3 seconds warning; offline alarms required |
| Stability | At least 24 hours |
| Dataset | Aim for 60–100 independent brews, 30,000–60,000 raw frames, 3,000–6,000 annotated images, and at least 15 held-out sessions |

Targets are hypotheses, not guaranteed release results. Report measured shortfalls
and dataset limitations; do not tune on held-out tests or infer physical timing
from QEMU. Collect data across lighting, pots, recipes, heating rates, steam,
occlusion, glare, other cookware, hands/utensils, and unsupported scenes.

Core release completion requires a demonstrated offline alarm, traceable model
training, physical runtime measurements, numerical tests, one documented kernel
experiment, and reproducible portfolio artifacts. Optional model OTA still
requires signatures, compatibility checks, a known-good model, and verified
rollback before it can be described as complete.

## Decisions Before Dependent Work

- **Hardware:** confirm module identities, safe wiring, camera suitability, mounting position, printer/material, and thermal conditions; evaluate the available OV7670 before considering OV2640/OV5640 replacement.
- **Build and buffers:** confirm firmware location, SDK/C++ dialect (ESP-IDF 6.1/C++20 proposed), RGB565 resolution/rate, explicit ownership, and bounded driver/application pool sizes.
- **Pipeline:** choose the development Kubernetes cluster, persistent storage, provisioning, and TLS setup; local ingestion bring-up may precede MinIO, but integrated experiments use Kubernetes-hosted tracking.
- **Data split:** reconcile 70/15/15 with at least 15 independent held-out sessions; 100 sessions satisfies both, while smaller totals need explicitly reviewed proportions.
