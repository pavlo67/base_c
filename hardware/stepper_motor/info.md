# Stepper motor pulse planning

`stepper_motor.h` provides platform-independent pulse planning, evaluation, and execution. Angles and speeds use degrees, time uses seconds, 
and `degreesPerPulse` depends on the motor step angle and driver microstep configuration. Requested angles are quantized to whole pulses. 
A move that begins in the opposite direction is rejected; braking through zero and reversing must be planned separately.

`pulse_series_t` supports two selectable interval algorithms:

- `constantAcceleration` is the default and derives each interval from exact constant-acceleration kinematics using `2 * degreesPerPulse / (speedBefore + speedAfter)`.
- `linearIntervalFallback` uses `initialIntervalSec + intervalChangePerPulse * pulseIndex` for comparison, diagnostics, or fallback behavior.

`calculateAction()`, `getFastestSeries()`, and `getAcceleratedSequence()` accept the algorithm as an optional final argument. 
The fallback fields remain populated for both algorithms. `getAcceleratedSequence()` fills the remaining quantized displacement with 
symmetric acceleration, cruise, and deceleration sections starting and ending at the supplied speed.

`evaluateSeries()` calculates duration, displacement, and final speed without emitting pulses. `action()` executes a series through 
a hardware callback that emits one pulse and receives direction `-1` or `+1`.
