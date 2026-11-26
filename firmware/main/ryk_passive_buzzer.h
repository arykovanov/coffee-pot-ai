#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>

#include <cstdint>
#include <span>

namespace Ryk
{
  class PassiveBuzzer
  {
  public:
    struct Tone
    {
      std::uint32_t frequencyHz;
      std::uint32_t durationMs;
    };

    // The pin, timer, and channel must be dedicated to this instance.
    // Hardware is initialised on the first playback, allowing static instances.
    explicit PassiveBuzzer(gpio_num_t pin,
                           ledc_timer_t timer = LEDC_TIMER_0,
                           ledc_channel_t channel = LEDC_CHANNEL_0) noexcept;
    ~PassiveBuzzer();

    PassiveBuzzer(const PassiveBuzzer&) = delete;
    PassiveBuzzer& operator=(const PassiveBuzzer&) = delete;
    PassiveBuzzer(PassiveBuzzer&&) = delete;
    PassiveBuzzer& operator=(PassiveBuzzer&&) = delete;

    // Both methods block the calling task; use a worker task for HTTP control.
    // Calls and destruction must be serialised by the caller, outside ISRs.
    // Durations are 1..60000 ms, subject to FreeRTOS tick granularity.
    // An empty sequence plays the built-in melody. No sequence is retained.
    esp_err_t playMelody(std::span<const Tone> melody = {});

    // Frequencies are 100..20000 Hz; 0 Hz represents a silent rest.
    esp_err_t playTone(std::uint32_t frequencyHz, std::uint32_t durationMs);

  private:
    esp_err_t Init();

    gpio_num_t Pin;
    ledc_timer_t Timer;
    ledc_channel_t Channel;
    bool TimerInitialised = false;
    bool ChannelInitialised = false;
  };
}
