#pragma once

#include <cstdint>
#include <atomic>

#include "L1_Peripheral/hardware_counter.hpp"
#include "L2_HAL/sensors/battery/coulomb_counter.hpp"
#include "utility/units.hpp"

namespace sjsu
{
/// LTC4150 control driver for the LPC40xx and LPC17xx microcontrollers. It
/// keeps track of a connected battery's current milliamp hours.
class Ltc4150 : public CoulombCounter
{
 public:
  /// Represents the voltage to frequency gain in millivolts.
  static constexpr units::voltage::millivolt_t kGvf = 32.55_mV;
  /// 3600 is the number of coulombs per amp hour
  static constexpr float kCoulombsPerAh = 3600.0f;

  /// Defines what state the Polarity pin can be in.
  enum class Polarity : uint8_t
  {
    kDischarging,
    kCharging,
  };

  /// @param counter - a hardware counter that must count on rising edge pulses.
  /// @param pol - gpio pin to determine the polarity output of the LTC4150.
  /// @param resistance - value of impedance represented in ohms for
  ///        calculating battery charge
  explicit constexpr Ltc4150(sjsu::HardwareCounter & counter,
                             sjsu::Gpio & pol,
                             units::impedance::ohm_t resistance)
      : counter_(counter), pol_pin_(pol), resistance_(resistance)
  {
  }

  /// Initialize hardware, setting pins as inputs and attaching ISR handlers to
  /// the interrupt pin.
  void Initialize() override
  {
    auto polarity_interrupt = [this]() {
      if (pol_pin_.Read())
      {
        counter_.SetDirection(sjsu::HardwareCounter::Direction::kUp);
      }
      else
      {
        counter_.SetDirection(sjsu::HardwareCounter::Direction::kDown);
      }
    };

    // Initialize the hardware counter to allow counting of signal transitions.
    counter_.Initialize();

    // Set the polarity pin as an input as we need to use it to read the state
    // polarity signal.
    pol_pin_.SetAsInput();

    // Sets the initial state of charging (counting down) or discharging
    // (counting up)
    polarity_interrupt();

    // Every time the polarity changes, this interrupt to be called to determine
    // the direction of the counter.
    pol_pin_.OnChange(polarity_interrupt);

    // Start counting
    counter_.Enable();
  }

  /// @return the calculated mAh
  units::charge::milliampere_hour_t GetCharge() const override
  {
    /// We cast the pulses to scalar so we can calculate mAh
    float pulses = static_cast<float>(counter_.GetCount());
    units::charge::milliampere_hour_t charge(
        pulses / (kCoulombsPerAh * kGvf.to<float>() * resistance_.to<float>()));
    return charge;
  }

  /// Destructor of this object will detach the interrupt from the polarity
  /// GPIO pin. The counter_ class member variable automatically detaches the
  /// interrupt from the tick pin on destruction.
  ~Ltc4150()
  {
    pol_pin_.DetachInterrupt();
  }

 private:
  sjsu::HardwareCounter & counter_;
  sjsu::Gpio & pol_pin_;
  units::impedance::ohm_t resistance_;
};
}  // namespace sjsu
