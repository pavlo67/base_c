# StepperMotorSeries: timer-driven interval calculation

## Units and state

The public API lives in stepper_motor.h. `moment` and `duration` are uint64_t nanoseconds from lib/timelib.h; angles are degrees. Simulations use a synthetic clock starting at zero. Real callers should provide monotonically increasing moments from one clock.

`intervalSec(moment at, const stepper_motor_options_t&)` mutates execution state. Its first call starts the first interval at `at`; the first pulse occurs when that interval completes. A call before the previously selected interval has completed leaves the selection unchanged. When recalculation is allowed, the interval algorithm advances by one step, irrespective of the number of repeated PWM pulses since the previous call. All repeated pulses contribute to actual displacement. This distinction makes a delayed series longer than its ideal counterpart.

A new period selected inside an active PWM interval is pending until the end of that interval. The current interval is never shortened or restarted. A selection made exactly at a pulse boundary applies immediately to the following interval. The next calculation becomes eligible after one full interval with that selected period has completed. Duplicate or older moments do not advance execution. Zero means the series has finished (or has no valid interval). On the final timer call, all pulses up to that call are counted before stopping; PWM does not stop retroactively at the nominal final pulse.

`expectedPulsesCount_` describes the ideal, undelayed plan; `intervalIndex_` tracks algorithm progress. `pulsesCount_` counts actual simulated PWM pulses. `firstPulseAt_` and `lastPulseAt_` are actual pulse timestamps; `startedAt_` is the start of the first interval. `totalSec()` (also the compatibility overload taking options) measures time from start through the most recent accepted observation, including the initial waiting interval and the final timer delay. `totalRotationDeg()` uses actual pulses. Before execution these actual metrics are zero.

`finalSpeed()` is signed degrees per last actual pulse interval, including the first interval when only one pulse exists. It is an interval-average speed, not the instantaneous endpoint velocity: a kinematic stop has a nonzero last interval-average speed. `idealFinalSpeed()`, `idealIntervalSec()`, `idealTotalSec()` and `expectedRotationDeg()` retain the old analytical metrics. `reset()` clears execution state; `limitWithDeg()` also resets state before changing the planned count.

## Simulation and sequence construction

`evaluateSeries(series, expecterInterval, options, startedAt, stopAfterPulses, onPulse)` evaluates a fresh copy and returns its completed or interrupted state. Zero timer means calculation exactly on every pulse boundary. A nonzero timer visits a grid anchored at zero, including across adjacent series. An optional pulse-count threshold is checked at timer ticks, so a coarse timer can overshoot it. With a threshold, the last period continues repeating if the interval algorithm has already reached its end. The optional callback receives every simulated pulse timestamp; evaluation does not itself wait or access GPIO.

`addAcceleratedSeries()` and `getSeriesSequence()` take the required `duration expecterInterval` immediately before motor options. Returned sections contain evaluated state, suitable for direct actual-metric inspection. `getFastestSeries()` still returns an unevaluated plan. The public `StepperMotorSeriesSequence` contains `seq`, `error`, and the const `log(options, label)` method.

With timer zero the existing acceleration/cruise/deceleration geometry is preserved. In timed accelerated motion, the acceleration section runs until the first timer tick at or beyond half the requested quantized angle (rounded upward to a whole pulse). At the speed limit its last PWM period repeats inside the same section. Then a deceleration section is built from the actual last-interval speed toward the supplied base speed. There is no separate cruise section. The initial and requested final kinematic speeds of this accelerated move are the base speed; the actual reported endpoint speed follows the interval-average definition above.

The completed angle is quantized to whole pulses using `pulsesForDeg()`: a remaining angle of half a pulse or more rounds up, symmetrically in reverse motion. Sub-pulse requests are rounded before deciding whether motion is needed. A single-pulse rest-to-rest move uses a symmetric within-pulse acceleration/deceleration interval, respecting the configured speed limit.

Timed deceleration is planned from the actual last interval-average speed; independent timer timing and rounding can make its pulse count differ from acceleration. `addAcceleratedSeries()` sets `minimumPulsesCount_` to the rounded target minus acceleration pulses. `getSeriesSequence()` checks the complete sequence for a remaining shortfall. If the algorithm ends early, the last actual PWM period repeats until the missing whole pulses have been generated. This completion tail stops exactly on its final pulse boundary; actual duration, speed and timestamps include it. `minimumPulsesCount_` survives reset/replay and is cleared by `limitWithDeg()`. No new acceleration is started. Existing timer overshoots are not removed.

With the default 90-degree / 5-ms settings, the result is 200 acceleration and 200 deceleration pulses, totaling 90 degrees.

CONSTANT_ACCELERATION retains exact per-step kinematics; LINEAR_INTERVAL_ACCELERATION retains the old linear interval formula and its existing limitations when an endpoint implies an infinite interval. Interval scheduling rounds to the nearest nanosecond, with a minimum of one nanosecond.

## Diagnostics and probes

`StepperMotorSeriesSequence::log(options, label)` prints Acceleration, Cruise or Deceleration headings according to the sign of accelerationDegPerSec2_ (speed magnitude, including reverse motion), followed by evaluated per-section data and aggregate duration, actual angle, actual and expected pulses, final speed, maximum speed, and maximum absolute acceleration. Acceleration uses differences of pulse-interval-average speeds divided by the separation of their interval midpoints; sequence boundaries are included. This is different from the old test's reconstructed kinematic endpoint acceleration.

`probe_sequences` defines `STEPPER_INTERVAL = 5 * MILLISECOND` in its source, calculates both timed and zero-delay sequences for the source-defined ROTATIONS list (currently 90 degrees), prints their statistics, and replays the timed pulses with software GPIO. The printed values describe simulated PWM, not measured hardware timing; scheduler jitter and GPIO high-pulse time are not included in those statistics. No hardware PWM driver is reconfigured by this change.

`probe_180_360_180` and `probe_infinite` do not call the changed APIs and retain their implementation. The original stepper_motor_test uses the zero-delay API and separate ideal endpoint metrics to retain its previous assertions. Additional GTest cases in the same CTest target cover pending period activation, repeated pulses, timer zero, reverse motion, halfway switching, callback replay, empty series, and repeated or older timestamps.

## GPIO execution

`move(sequence, pinStep, pinDir, pinEna, expecterInterval, options, pulseHigh)` is declared in stepper_motor.h and implemented in stepper_motor_action.cpp, linked into the stepper_motor library. All timing durations are nanoseconds. The caller initializes the shared Gpio backend before calling and terminates it after all motors have finished. Each motor can use its own three distinct BCM pins, options, timer interval and pulse-high duration. Callers must serialize access to the shared GPIO object; this API does not introduce concurrent motor scheduling.

The function configures only those three pins, drives active-low ENA to enable the driver, replays the evaluated pulse schedule, then drives STEP/DIR LOW and ENA HIGH. Cleanup attempts all applicable operations and preserves the first GPIO error. It returns zero on success or a negative GPIO code on failure, printed to stdout with ERROR: and the function context. Invalid pins, zero pulse width, invalid options and an error-bearing sequence are rejected before pin changes. An empty sequence performs no GPIO operations. Execution uses software sleeps and digital writes; it is not hardware PWM and does not measure physical pulse timing.

All explicit stepper error output uses stdout with an ERROR: prefix. Tests also cover remaining-angle rounding, correction replay, and independent pin sets using the desktop GPIO backend.
