#include "ryk_passive_buzzer.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{
  constexpr ledc_mode_t SpeedMode = LEDC_LOW_SPEED_MODE;
  constexpr std::uint32_t HalfDuty = 512; // 50% at 10-bit resolution.

  constexpr Ryk::PassiveBuzzer::Tone DefaultMelody[] = {
    {523, 150}, // C5
    {0, 50},
    {659, 150}, // E5
    {0, 50},
    {784, 150}, // G5
    {0, 50},
    {1047, 300} // C6
  };

  constexpr bool IsValidTone(std::uint32_t frequencyHz, std::uint32_t durationMs)
  {
    return (frequencyHz == 0 || (frequencyHz >= 100 && frequencyHz <= 20000))
           && durationMs >= 1 && durationMs <= 60000;
  }
}

namespace Ryk
{
  PassiveBuzzer::PassiveBuzzer(gpio_num_t pin, ledc_timer_t timer,
                             ledc_channel_t channel) noexcept
    : Pin(pin), Timer(timer), Channel(channel)
  {
  }

  PassiveBuzzer::~PassiveBuzzer()
  {
    if (ChannelInitialised)
    {
      (void)ledc_stop(SpeedMode, Channel, 0);
      ledc_channel_config_t config = {};
      config.speed_mode = SpeedMode;
      config.channel = Channel;
      config.deconfigure = true;
      (void)ledc_channel_config(&config);
    }

    if (TimerInitialised)
    {
      (void)ledc_timer_pause(SpeedMode, Timer);
      ledc_timer_config_t config = {};
      config.speed_mode = SpeedMode;
      config.timer_num = Timer;
      config.deconfigure = true;
      (void)ledc_timer_config(&config);
    }
  }

  esp_err_t PassiveBuzzer::Init()
  {
    if (ChannelInitialised)
      return ESP_OK;

    if (!GPIO_IS_VALID_OUTPUT_GPIO(Pin)
        || static_cast<unsigned>(Timer) >= LEDC_TIMER_MAX
        || static_cast<unsigned>(Channel) >= LEDC_CHANNEL_MAX)
      return ESP_ERR_INVALID_ARG;

    if (!TimerInitialised)
    {
      ledc_timer_config_t config = {};
      config.speed_mode = SpeedMode;
      config.duty_resolution = LEDC_TIMER_10_BIT;
      config.timer_num = Timer;
      config.freq_hz = 1000;
      config.clk_cfg = LEDC_AUTO_CLK;
      const esp_err_t err = ledc_timer_config(&config);
      if (err != ESP_OK)
        return err;
      TimerInitialised = true;
    }

    ledc_channel_config_t config = {};
    config.gpio_num = Pin;
    config.speed_mode = SpeedMode;
    config.channel = Channel;
    config.timer_sel = Timer;
    config.duty = 0;
    config.hpoint = 0;
    const esp_err_t err = ledc_channel_config(&config);
    if (err != ESP_OK)
      return err;

    ChannelInitialised = true;
    return ESP_OK;
  }

  esp_err_t PassiveBuzzer::playTone(std::uint32_t frequencyHz,
                                   std::uint32_t durationMs)
  {
    if (!IsValidTone(frequencyHz, durationMs))
      return ESP_ERR_INVALID_ARG;

    esp_err_t err = Init();
    if (err != ESP_OK)
      return err;

    if (frequencyHz == 0)
    {
      err = ledc_stop(SpeedMode, Channel, 0);
    }
    else
    {
      err = ledc_set_freq(SpeedMode, Timer, frequencyHz);
      if (err == ESP_OK)
        err = ledc_set_duty(SpeedMode, Channel, HalfDuty);
      if (err == ESP_OK)
        err = ledc_update_duty(SpeedMode, Channel);
    }

    if (err != ESP_OK)
    {
      (void)ledc_stop(SpeedMode, Channel, 0);
      return err;
    }

    // Round up to a whole tick so short tones never become a zero-tick delay.
    const auto ticks = static_cast<TickType_t>(
      (static_cast<std::uint64_t>(durationMs) * configTICK_RATE_HZ + 999) / 1000);
    vTaskDelay(ticks);
    return ledc_stop(SpeedMode, Channel, 0);
  }

  esp_err_t PassiveBuzzer::playMelody(std::span<const Tone> melody)
  {
    if (melody.empty())
      melody = DefaultMelody;

    // Reject invalid sequences before producing any sound.
    for (const Tone& tone : melody)
    {
      if (!IsValidTone(tone.frequencyHz, tone.durationMs))
        return ESP_ERR_INVALID_ARG;
    }

    for (const Tone& tone : melody)
    {
      const esp_err_t err = playTone(tone.frequencyHz, tone.durationMs);
      if (err != ESP_OK)
        return err;
    }

    return ESP_OK;
  }
}
