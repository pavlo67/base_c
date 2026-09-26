# Stepper motors

Entry points: `stepper_motor_series.h` for planning/simulation, `stepper_motor.h`
for lifecycle/scheduling/probe, `smart/stepper_motor_smart.h` for accelerated GPIO
execution, and [Dumb](dumb/info.md) for fixed frequency. Configuration belongs to
`config/platform_config.h`. Motor objects own configuration and execution state,
are non-copyable and create no threads. The application owns shared
[GPIO lifecycle](../info.md) and must keep it alive through motor destruction.

## Units and observations

Angles are degrees; moment/duration are uint64 nanoseconds. External ticks must
increase monotonically in one clock domain; internal scheduling uses steady `Clock`.
Simulation starts at zero. `real` metrics count modeled PWM periods from commands
and observed call times, not encoder readings or measured edges. Frequency quantization,
startup edges, backend gaps and scheduler overshoot can differ physically; desktop
has no waveform. Free-running PWM is not an exact N-pulse counter.

`expectedPulsesCount_` is the plan/fitted braking budget; `pulsesCount_` is observed
model progress. Total time includes the first interval, setup guards and final
observation delay, but not unobserved final GPIO-call time. `finalSpeed()` is signed
angle per last interval, not mechanical settling speed; the analytical ideal endpoint
is different. `reset()` clears observations and frequency progress but retains
planned budgets, terminal intervals and frozen braking profiles. `limitWithDeg()`
also clears the minimum completion count before changing the planned count.

## Planning and simulation

`getFastestSeries()` creates an unevaluated plan; `getSeriesSequence()` and
`addAcceleratedSeries()` return evaluated sections. `evaluateSeries()` runs a reset
copy without GPIO/sleep. Timer zero visits pulse boundaries; nonzero timers use a
fixed grid shared across sections. Duplicate/older observations do not advance state.
A selected period takes effect at the current interval's end; a subsequent law
update requires a complete period. Every elapsed repeated pulse contributes to angle.
Threshold stopping is checked only on timer ticks and can overshoot; its final
period can repeat after the law ends. Minimum-count completion tails end on a pulse
boundary in simulation. Periods round to nanoseconds, minimum one nanosecond.

Zero-base-speed accelerated moves reserve one requested pulse for a finite slow
rest-to-rest terminal interval, respecting kinematic limits. There is no extra
pulse or fixed minimum duration; terminal slowing belongs to Deceleration in logs.
With nonzero timer, acceleration reaches the first tick at/beyond half the preceding
quantized displacement; braking starts from the last interval-average speed.
Optional cruise fills surplus; a last-period minimum count corrects shortfalls,
but timer overshoots remain. Nonzero-base-speed endpoints retain their behavior.
CONSTANT_ACCELERATION tolerates small cancellation in squared speeds;
LINEAR_INTERVAL_ACCELERATION retains limitations with infinite endpoint intervals.

## Incremental execution and ownership

`prepare(angle, usedPins)` validates and plans without GPIO; prepare every axis
before any `update()`. `update()` calls virtual `action(at)` when due, skips missed
ticks and enforces a per-motor watchdog; `nextWake()` supplies its next wake/deadline.
Zero angles complete without GPIO. Status is RUNNING (0), COMPLETE (1), or negative.
Completed/error states are sticky; old/duplicate action timestamps are ignored.

Initialization configures the three distinct pins, PWM OFF, DIR and active-low ENA,
then waits 500 us plus `pulseHigh`; direction changes wait `pulseHigh`.
STEP is 50% PWM at range 40000. `pulseHigh` is a minimum HIGH/LOW duration, not
manual pulse generation. Live frequencies are rounded down to integer 1..10000 Hz
within driver/speed/pulse-width limits. Unsupported channels or unrepresentable
period/frequency transitions fail rather than silently stalling.

Unchanged PWM commands retain model phase; changes start a new modeled period.
During acceleration, frequency changes respect interval-average acceleration;
a desired law index is retried until its frequency is reached. `stop()` attempts
PWM OFF, ENA HIGH and DIR LOW, preserving the first error and retry flags. Hardware
STEP remains in PWM mode at zero duty; software STEP goes LOW. Destruction retries
cleanup. Backend calls may block and cannot be interrupted by the watchdog.

`stepperMotorExecutionMutex()` serializes full probes with multi-axis actors.
Hold it through motor destruction; idle actors release it. Direct low-level callers
must also coordinate shared GPIO. The actor handoff itself is documented by its
consumer, [Machina move](../../../machina/move/info.md).

## Predictive braking

REAL retains the full pre-terminal target, rather than a simulated halfway remainder.
At each newer acceleration call it estimates mean scheduling interval from elapsed
acceleration time/count, excluding setup and duplicate calls. It simulates one next
tick including the proposed frequency, then calls `canBrake()` on the resulting
speed and remaining displacement. If braking no longer fits, it rejects that update
and brakes from the currently active PWM command. Later delays may still overshoot.

Braking freezes a model whose pulse thresholds are fitted to the integer remaining
budget; the reserved terminal pulse stays separate. There is no slow completion
remainder. Actual elapsed pulses select the frozen command, potentially skipping
commands and exceeding the acceleration bound to catch up; speed/frequency/pulse-width
limits still apply. Exhausted pre-terminal budgets skip the braking tail.

`getBrakingModel()` builds this GPIO-free fixed-positive-timer profile; empty means
unrepresentable. Braking preserves direction without increasing speed. `canBrake()`
compares its full pulse count to a budget excluding the terminal pulse.
`getCruiseAndBraking()` is instead the simulation helper: reduce cruise for overshoot,
then fill remaining simulation shortfalls. REAL uses frozen profiles, not that tail.

## Blocking probes and statistics

`probe(angle, withEstimates, label, real)` validates and runs the clocked plan on a
steady timer, reports runtime/partial statistics and returns zero or negative error.
Optional `real` is reset on entry. Estimates additionally build/log the ideal plan
and log the clocked plan. Zero configured interval means 1-us live polling; missed
ticks are skipped. The watchdog returns TIME_LIMIT (-10008) after cleanup.

`StepperMotorSeriesSequence::log()` groups contiguous phases with time, rotation,
pulse count, last/max speed and max absolute acceleration. Cross-section acceleration
belongs to the following phase; verbose adds section details. `expecterInterval`
in reports is requested scheduler cadence, not measured call spacing or pulse period;
configured zero means 0 in ideal simulation but 0.001 ms for live polling.

## Platform configuration

`loadPlatformMotorConfig()` validates pan and tilt together, preserving the previous
pair on any failure. Required per-axis fields:

| Fields | Meaning and limits |
| --- | --- |
| pinStep, pinDir, pinEna | BCM0..27; all six pins distinct |
| freqMax, degPulse | Driver pulses/s; motor degrees/pulse before transmission |
| speedMaxDegSec | Platform degrees/s |
| gearRatio | Motor pulley / platform pulley tooth ratio |
| momentOfInertia, rotorInertia | Load and motor rotor inertia, kg m²; rotor may be zero |
| transmissionEfficiency | Forward-drive efficiency in (0,1] |
| torqueMaxNm | Positive motor torque for planning |
| expecterIntervalUs | Scheduler microseconds; zero uses 1-us polling |
| pulseHighUs | Minimum HIGH/LOW microseconds, 1..500000 |
| hardwarePwm | Dedicated hardware PWM selection |
| timeLimitMs | Positive movement watchdog |

Mechanics are double, finite and positive except allowed zero rotor inertia.
Timing must fit probe duration limits. Unsupported PWM STEP pins are rejected
before execution, but backend/routing errors can still occur at startup.
For r=gearRatio, eta=efficiency, Jr=rotor inertia, Jl=load inertia and T=torque:

    alpha_platform = T / (Jr/r + Jl*r/eta)  # radians/s²

Rotor inertia is upstream of losses; multiply by 180/pi for degrees/s².
`platformMotorOptions()` multiplies motor degPulse by r and derives acceleration,
preserving platform speed and driver frequency limits. This replaces YAML
accelMaxDegSec2; low-level APIs still accept acceleration-based options.

For a uniform disk about its central perpendicular axis, J=mR²/2; another axis
needs a different inertia. 1 g cm² = 1e-7 kg m². Example efficiency 0.95 is provisional,
not measured from belt dimensions. The model omits gravity, belt elasticity,
speed-dependent torque and mechanical settling; it is trajectory planning, not
current control or measured torque. Catch-up braking may exceed its acceleration.

`platform_probe` takes signed pan/tilt degree pairs, validates all inputs before GPIO,
starts each pair together and waits for both before the next. It loads relative
`machina.yaml`; `motorClass` selects Smart (default) or Dumb. Dumb additionally needs
dumbSpeedDegSec, dumbPauseMs and dumbRemainingPulsesTolerance per axis.
`pulses_probe` takes signed pulse counts on pan, using configured pulseHighUs for
HIGH/setup and STEP_LOW_US for LOW (11235 us at the recorded 20-degree/s setting),
with 10 ms between series. The platform probe holds motor execution ownership
through both-axis cleanup; both probes terminate GPIO after completion/failure.

`stepper_motor_test` covers planning/execution, `stepper_platform_test` configuration.
Desktop results establish the PWM model only; physical timing requires device checks.
