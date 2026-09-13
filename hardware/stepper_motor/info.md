# Stepper motor planning, execution and probes

## Units and metrics

The public API is in `stepper_motor.h`. `moment` and `duration` are uint64_t nanoseconds from `lib/timelib.h`; angles are degrees. Simulations use a synthetic clock starting at zero. External execution callers provide monotonically increasing timestamps from one clock. `probeReal()` uses `std::chrono::steady_clock`, not the realtime clock in `now()`.

`expectedPulsesCount_` is the undelayed section plan, or the fitted pulse budget for a frozen braking profile; `intervalIndex_` tracks interval-law progress (elapsed pulses for frozen braking). `pulsesCount_` counts elapsed PWM periods in the execution model. `firstPulseAt_`, `lastPulseAt_`, `startedAt_` and `observedAt_` describe that execution. `totalSec()` includes the initial pulse interval and the final observation delay. Live sections also include their enable/direction setup delay. It excludes unobserved time inside the final GPIO calls. Before execution actual metrics are zero.

`finalSpeed()` is signed degrees divided by the last pulse interval. It stays nonzero after PWM is disabled: no mechanical settling speed or inertia is estimated. `idealFinalSpeed()` retains the analytical kinematic endpoint definition, which differs from the last interval-average speed. `idealIntervalSec()`, `idealTotalSec()` and `expectedRotationDeg()` describe the plan; the reserved terminal interval is included in ideal time. `scheduledIntervalSec()` additionally applies live PWM frequency quantization and its interval-law hold state.

`reset()` clears observations, callbacks, live setup delay and frequency-limit progress. Planned minimum counts, pulse budgets, terminal interval, live mode and frozen braking profiles survive reset. `limitWithDeg()` resets execution before changing the planned count and clearing the minimum count.

## Simulated scheduling

`intervalSec(at, options)` mutates execution state. Its first call starts the first interval at `at`; the first modeled pulse occurs when that interval completes. Duplicate or older observations do not advance execution. A zero return means completion or an unrepresentable interval.

In simulation, new periods selected during an active interval remain pending until its end. An exact-boundary selection applies to the following interval immediately. Recalculation becomes eligible after one complete interval at the selected period. Each eligible call advances the interval law once, but all repeated pulses contribute to displacement. Final timer calls count every elapsed pulse before stopping; normal completion does not retroactively stop at a nominal pulse boundary.

`evaluateSeries(series, expecterInterval, options, startedAt, stopAfterPulses, onPulse)` evaluates a reset copy without sleeping or accessing GPIO. Timer zero visits pulse boundaries; nonzero timers visit a grid anchored at zero across adjacent sections. An optional pulse threshold is checked at timer ticks and can overshoot. With a threshold, the final period can continue repeating after the interval law ends. The callback receives modeled pulse timestamps. Integer-angle completion tails using `minimumPulsesCount_` stop at their last pulse boundary in simulation.

CONSTANT_ACCELERATION uses per-step kinematics. Near-zero squared speeds tolerate floating-point cancellation relative to the initial speed squared. LINEAR_INTERVAL_ACCELERATION retains its historical limitations for infinite endpoint intervals. Simulation periods round to nanoseconds, with a minimum of one nanosecond.

## Accelerated sequences and finite final speed

`addAcceleratedSeries()` and `getSeriesSequence()` take `duration expecterInterval` before motor options and return evaluated sections. `getFastestSeries()` returns an unevaluated plan.

For an accelerated move with zero base speed, one pulse is reserved from the quantized requested displacement for a slow terminal interval. The preceding displacement uses the existing acceleration/deceleration construction. The terminal period is the natural single-pulse rest-to-rest period respecting speed/acceleration limits, rounded to nanoseconds. There is no fixed 100-ms minimum. No extra pulse is added to the requested angle, no infinite interval is requested, and no post-stop zero speed is reported. The terminal section is grouped with Deceleration in logs. Nonzero-base-speed sequences retain their endpoint behavior.

With timer zero, the preceding motion contains acceleration, optional cruise and deceleration. With a nonzero timer it accelerates until the first tick at or beyond half of the preceding quantized displacement, rounded upward to a pulse; the speed-limit period can repeat inside that section. A deceleration section is then built from its last interval-average speed. Optional cruise uses surplus displacement before braking. `stopAfterPulses_` records the simulated halfway threshold. `pairedTargetPulses_` retains the full acceleration/cruise/braking target before timer overshoot, excluding the reserved terminal pulse; REAL uses this full target rather than reconstructing it from the simulated halfway remainder.

Timed deceleration uses `minimumPulsesCount_` to fill any remaining integer angle at its last period. `getSeriesSequence()` also corrects overall shortfalls. Coarse-timer overshoots are retained. Reserving the terminal pulse does not guarantee exactly N physical edges from a free-running PWM generator.

For default 90-degree / 5-ms settings, clocked simulation has 200 acceleration pulses, 199 pulses distributed across cruise/main braking and one reserved terminal pulse: 400 pulses / 90 degrees. The terminal period and last-interval speed now depend on the configured kinematic limits; live PWM additionally quantizes frequency. REAL phase counts and timing come from predictive execution and actual call spacing.

## Incremental GPIO execution

`StepperMotorAction(sequence, pinStep, pinDir, pinEna, options, pulseHigh, hardwarePwm=false)` owns a reset execution copy. Callers initialize the shared GPIO backend before constructing/executing motors, serialize shared access and terminate it after motor objects have been stopped/destroyed. The class is non-copyable and does not create threads.

`action(moment at)` makes one state-machine update and does not sleep or replay future pulses. Return values are `RUNNING` (0), `COMPLETE` (1), or a negative error. Repeated/older timestamps are ignored; completed/error states are sticky. The initial update configures only the three supplied distinct BCM pins, starts with PWM disabled, sets DIR and active-low ENA, then schedules a 500-us enable guard plus `pulseHigh`. Subsequent calls start/update PWM when the guard expires. Direction changes receive a `pulseHigh` guard; all waiting belongs to the external scheduler.

STEP uses the existing GPIO PWM setters, with range 40000 and 50% duty. `pulseHigh` is the minimum HIGH and LOW duration, not a manually generated pulse width. Hardware mode uses `GpioMode::hardwarePwm`; OFF uses `GpioMode::output` with the backend's software/DMA PWM. No manual per-pulse sleeps or writes are used. Invalid pins/options, unsupported hardware channels, periods below the GPIO API's 1-Hz minimum and pulse-width/frequency conflicts return errors.

Live periods use integer frequencies within 1..10000 Hz and motor speed/frequency limits, rounded down. The execution model uses these commanded periods and actual `action()` timestamps. Unchanged commands preserve the modeled pulse phase; changed commands start a new modeled period at that update. Outside frozen REAL braking profiles, the interval law waits for a full commanded period and frequency steps are reduced when necessary to respect the pulse-interval-average acceleration bound; the same law index is retried until its desired frequency is reached. A required step that cannot be represented even by a 1-Hz change returns an error rather than silently stalling.

For paired acceleration/braking moves, REAL replaces the simulated halfway rule with predictive control. This also applies when its input is a zero-timer ideal plan. Acceleration can continue to the speed limit and hold that speed inside the same section. At each strictly newer call after acceleration starts, the model interval is elapsed acceleration time divided by the number of observed call intervals. Enable/direction setup time and duplicate/older calls are excluded. The first observation after acceleration starts provides the first mean; no maximum-interval estimate is used.

The executor advances a copy through the next model tick, including the current proposed command, next speed update, frequency limits and intervening pulses. It calls `canBrake()` for that next speed and the remaining pulse budget after the lookahead. When the check fails, it starts braking at the current call using the PWM command actually active before the proposed acceleration update. Thus the rejected command is never sent to GPIO. Future delays can still consume more displacement than predicted.

At that transition, the planned optional cruise and braking are replaced by a frozen braking profile. Its pulse thresholds are proportionally fitted to the integer remaining displacement; there is no slow last-period completion remainder. The separately reserved terminal pulse stays separate. If acceleration has already exhausted the pre-terminal budget, the executor skips the exhausted braking tail. During braking, each actual call counts every elapsed modeled PWM pulse and selects the frozen command for that count. It may skip intermediate commands and exceed the acceleration limit to catch up. Speed/frequency and minimum pulse-width limits remain enforced. The model interval and profile do not change after braking starts.

Following sections carry the preceding pulse interval into their acceleration statistics. Live completion counts all periods elapsed up to the stopping call, including overshoot. A speed-limit hold remains grouped with Acceleration in REAL output; it is not a separate runtime Cruise section.

`stop()` attempts PWM OFF, ENA HIGH and DIR LOW, preserving the first error; the destructor also attempts cleanup. Hardware STEP remains in PWM mode with zero duty, avoiding unsupported digital writes or pinmux changes. Ordinary software PWM OFF drives STEP LOW. Cleanup failures retain the corresponding cleanup flags for a later retry. GPIO lifecycle remains the caller's responsibility.

## Braking and cruise helpers

`getBrakingModel(speedDegPerSec, finalSpeedDegPerSec, modelInterval, options, algorithm=CONSTANT_ACCELERATION)` runs a fresh live-PWM braking calculation on fixed model intervals. Speeds are signed degrees/s, timing is nanoseconds, and options use the normal motor units. It records each commanded pulse interval with its cumulative braking pulse threshold in `brakingModel_`. `expectedPulsesCount_` becomes the model's full braking displacement, `modelInterval_` stores its timer, and `brakingFinalSpeed_` preserves the requested kinematic endpoint. An empty profile means the requested model cannot be represented; the timer must be positive and braking must preserve direction without increasing speed. `reset()` clears execution observations while retaining this profile.

`canBrake(speedDegPerSec, finalSpeedDegPerSec, modelInterval, remainingPulses, options, algorithm=CONSTANT_ACCELERATION)` performs that model calculation and returns true only when it is representable and its full pulse count fits the remaining budget. `remainingPulses` excludes any separately reserved terminal pulse. Both helpers are independent of GPIO and do not sleep. They model commanded PWM periods and scheduling, not measured edges.

`getCruiseAndBraking(speedDegPerSec, finalSpeedDegPerSec, remainingPulses, timer, options, algorithm, startedAt=0, livePwm=false)` remains the simulation helper for evaluated optional cruise and braking. It evaluates braking without a completion minimum, assigns surplus displacement to cruise, then reduces cruise when timer overshoot and the shifted braking start exceed the remaining budget. Residual simulation shortfalls use `minimumPulsesCount_`. Cruise uses `stopAfterPulses_` for displacement and `pairedTargetPulses_` for the remaining combined budget; `cruiseFrequency_` preserves a reached integer-Hz live command across reset. REAL predictive execution uses the frozen profile instead of this cruise-tail helper.

## Blocking run and probe configuration

`StepperMotorAction::probeReal(expecterInterval, timeLimit=60*SECOND)` supplies a steady-clock timer to `action()` until completion. It never inspects section parameters or counts. Timer zero selects a 1-us polling cadence; it does not turn live GPIO execution into an ideal simulation. Missed ticks are skipped rather than replayed. An independent deadline bounds sleeps and returns `TIME_LIMIT` (-10008) after stopping PWM; it cannot interrupt a blocking backend call. Invalid timer limits are errors.

Blocking callers construct `StepperMotorAction` and call `motor.probeReal(expecterInterval, timeLimit)`, then read `motor.result()` for runtime statistics, including partial observations on failure. The complete probe wrapper performs this directly. Applications initialize GPIO once at startup and terminate it after all motor objects have stopped and been destroyed.

`probe_sequences` keeps its rotation list, 5-ms timer, 60-second watchdog and pins as source constants. It prints `Target`, ideal zero-timer statistics, clocked simulation statistics, and `real` statistics collected during `probeReal()`.

The configuration files in this checkout are `_base_defines.h` and `_base_defines_example.h` at the repository root (there is no `_base_defines/` directory). Both define:

- `STEPPER_MOTOR_PROBE_VERBOSE`, default false: print per-section details when enabled. When disabled, only Target, phase headings, phase block totals and TOTAL rows remain, apart from warnings/errors.
- `STEPPER_MOTOR_PROBE_HARDWARE_PWM`, default true: select dedicated hardware PWM. Desktop prints a Warning and uses OFF. Other platforms return actual hardware capability/configuration errors instead of silently falling back.

Hardware PWM in this probe uses the explicit `HARDWARE_PWM_PIN_STEP = 18` constant: STEP must be wired/routed to BCM18. OFF retains `PIN_STEP` (currently BCM17); DIR and ENA retain the shared hardware constants. Hardware routing, overlays and resource constraints are documented in `hardware/gpio/pwm/PWM.md`.

## Shared complete-move runner

`StepperMotorAction::probeAll(const StepperMotorRunConfig&, float rotationDeg, const std::string& label = "motor", StepperMotorSeriesSequence* real = nullptr)` in `stepper_motor_action.cpp` owns the full blocking, rest-to-rest move. It validates options/angle/pins/timing, calculates constant-acceleration ideal and configured-timer series, logs both, constructs the motor, executes its timer loop, logs runtime statistics even on failure and destroys the motor. GPIO initialization and termination belong to the application. The return value is zero on success or a negative error code. Optional `real` is reset on entry and receives runtime/partial results and failure diagnostics. Zero angles return without touching GPIO. Invalid inputs are rejected before planning and GPIO access.

`StepperMotorRunConfig` contains `pinStep_`, `pinDir_`, `pinEna_` (BCM), `options_`, `expecterInterval_`, `pulseHigh_`, `timeLimit_` (nanoseconds), `hardwarePwm_` and `verbose_`. Rotation is signed degrees; application wire units must be decoded by the caller. The application owns the shared GPIO lifecycle; probe calls preserve other GPIO consumers. Serialize GPIO access and use distinct motor pins. Logging is outside the timed execution loop. Runtime values are PWM estimates, not encoder observations; pulse quantization and timer overshoot still apply.

`probe_sequences` chooses its source configuration/platform PWM preference and iterates angles through `StepperMotorAction::probeAll()`. Machina's HTTP action dispatcher uses the complete probe wrapper; its separate real motor worker uses the incremental two-axis actor and does not publish commands yet. Low-level callers with an existing sequence and caller-owned GPIO use `StepperMotorAction::probeReal()` and `result()` directly; there is no free `probeReal()` or `executeSequence()` API. Cleanup happens before GPIO termination. Diagnostics use `[function()] ERROR: details`, with qualified context names where needed.

## Statistics, limitations and tests

`StepperMotorSeriesSequence::log(options, label, verbose=false)` groups contiguous sections of the same phase. Each Acceleration/Cruise/Deceleration block ends with the same time, rotation, actual pulse counts, last-interval speed, maximum speed and maximum absolute acceleration fields as TOTAL. Cross-section acceleration uses interval midpoint separation and belongs to the following block. The terminal interval belongs to Deceleration. Summary rows omit the `expected=` field. Verbose mode adds per-section details, including the explicitly named `expectedPulsesCount` planning field, without removing summaries.

`real` means runtime observations and commanded PWM estimates, not encoder readings or measured GPIO edges. The GPIO API does not expose waveform phase, native frequency quantization or edge counts. Startup edges, backend reconfiguration gaps and physical timing can differ from the model; desktop has no physical waveform at all. PWM is not a hardware N-pulse counter, and scheduler delays can overshoot the target. Mechanical inertia/settling is not estimated.

The `stepper_motor_test` GTest/CTest target covers existing simulation behavior, both directions, pulse quantization/replay, finite terminal slowing, grouped/verbose output, external ticks, repeated/older timestamps, runtime PWM settings, independent pins, hardware mode through the desktop contract, errors, timeout and cleanup. Natural terminal-period tests also cover one/two-pulse moves, signed 0.25/1/2-degree platform moves, slow single-pulse motion, timer rounding and live braking after delayed calls. Raspberry Pi waveform behavior requires on-device validation. The older `probe_180_360_180` and `probe_infinite` retain their APIs and implementation.

Additional GTest/CTest cases cover braking prediction against fixed-timer replay (including nonzero final speed), the exact fits/does-not-fit pulse boundary, skipping commands after delayed observations, signed 33/90/180/720-degree moves, full target retention after simulated halfway overshoot, startup exclusion and mean timing, duplicate/older timestamps, frozen profiles during later delays, zero-timer input plans, speed/acceleration limits during acceleration, and permitted stronger braking. Runtime timing comparisons are deterministic desktop PWM-model results; Raspberry Pi waveforms and mechanical behavior require on-device validation.

## Platform configuration

`platform_config.h` provides `PlatformMechanics`,
`platformMotorOptions()` and `loadPlatformMotorConfig()`. The latter loads pan and
tilt together and preserves both previous configurations on any error. Machina
and `probe_platform` share this loader. `stepper_platform` links YAML configuration
support separately from the low-level `stepper_motor` library.

YAML fields for each axis:

- `gearRatio`: positive double, motor pulley teeth / platform pulley teeth.
  YAML stores a decimal number, not an expression: pan 0.5555555555555556 (20/36), tilt 0.5 (18/36).
- `momentOfInertia`: positive platform/load inertia in kg*m^2, excluding the rotor.
  Uniform disks about their central axis perpendicular to the disk plane use J=m*R^2/2:
  pan 1.5 kg / 0.15 m diameter gives 0.00421875; tilt 1 kg / 0.075 m gives 0.000703125.
  Rotation about a diameter or an offset axis requires different values.
- `rotorInertia`: nonnegative motor rotor inertia in kg*m^2.
  1 g*cm^2 = 1e-7 kg*m^2: pan 200 gives 0.00002; tilt 54 gives 0.0000054.
- `transmissionEfficiency`: double in (0,1], initially 0.95 for both axes (5% loss).
  This is a provisional estimate for pan HTD-3M 225 x 14 mm and tilt HTD-3M 195 x 15 mm,
  not a measured efficiency or a calculation from belt dimensions. Tension, bearings,
  pulley geometry, speed and loading affect actual losses. Gates reports up to 98%
  for another synchronous belt family, not a specification of these assemblies:
  https://www.gates.com/content/dam/documents-library/catalogs/poly-chain-gt-carbon-drive-design-manual-en.pdf
- `torqueMaxNm`: positive motor torque for planning, initially 0.5 Nm.
  Replaces the YAML `accelMaxDegSec2` field; legacy acceleration alone is insufficient.
- `degPulse`: motor degrees/pulse before the transmission.
- `speedMaxDegSec`: platform degrees/s. `freqMax` remains motor pulses/s.

All mechanics are parsed as double. With r=gearRatio, eta=efficiency, Jr=rotor inertia,
Jl=load inertia and T=motor torque, acceleration in platform radians/s^2 is:

    alpha = T / (Jr/r + Jl*r/eta)

This follows T=Jr*alpha_motor + Jl*alpha_platform*r/eta and alpha_platform=r*alpha_motor.
The rotor is upstream of transmission losses. Conversion to degrees/s^2 uses 180/pi.
`platformMotorOptions()` replaces degPulse by motor degPulse*r and fills the internal
acceleration limit. It preserves the platform speed and driver frequency limits.
Invalid/nonfinite/unrepresentable derived options are rejected without changing output.
Low-level APIs retain their existing acceleration-based options; configured callers use
platform units consistently for angles, speed, acceleration and printed statistics.

The model uses constant forward-drive efficiency for the planned acceleration/deceleration
magnitude. Existing predictive braking can exceed that magnitude when timer delays require
catch-up, as explicitly allowed. This is a trajectory parameter, not driver current control
or measured torque. Gravitational torque, elastic belt dynamics and speed-dependent motor
torque are not modeled by these parameters.

`probe_platform` in `_probe/probe_platform.cpp` accepts a complete list of signed relative
platform angles in degrees, including decimal/scientific notation. All arguments are parsed
before motion; nonfinite/out-of-range values and trailing garbage are rejected. Configuration
path `_env/machina.yaml` and axis `AXIS` (0 pan, 1 tilt) are source constants. Run from the
repository root, or base root for a standalone base build. Each angle goes through `StepperMotorAction::probeAll()`; execution stops on the first error.
GPIO is initialized once before the rotation list and terminated after the list, including motion failure.
Example from repository root: `_bin/probe_platform +90 -45 180`. Zero is a no-op.
The existing `probe_sequences` keeps its original low-level source-constant configuration.

`stepper_platform_test` (GTest/CTest) covers torque balance, both ratios and directions,
invalid mechanics, signed argument parsing, YAML disk parameters and atomic reload rejection.
Desktop tests/probes use the GPIO stub and do not validate physical motion.

## Scheduler interval in statistics

`StepperMotorSeriesSequence::log()` accepts an optional fourth argument,
`expecterInterval` (nanoseconds). When supplied, every phase block total and TOTAL
row includes `expecterInterval=… ms` with six decimal places. `StepperMotorAction::probeAll()`
supplies the configured simulation interval for `clocked` and the requested polling
cadence for `real`. With configured zero, these are respectively 0 ms (ideal
simulation) and 0.001 ms (the 1-us polling fallback). This is the requested
scheduler cadence, not the measured spacing between calls or the STEP pulse period.

## Real motor scheduling and execution ownership

`stepperMotorExecutionMutex()` returns the process-wide mutex shared by complete
probes and Machina's real actor. A probe owns it for its full call; the actor owns
it from command preparation through destruction of both motors. Idle polling does
not own it. Low-level direct callers still need to coordinate GPIO access.

`StepperMotorRunner` in `stepper_motor_runner.h/.cpp` handles one real motor without
sleeping or probe statistics. `prepare(config, angle, usedPins)` validates options,
timing and pins against a shared pin-use array, prepares the plan and constructs
the action without accessing GPIO. Prepare every axis before starting any update.
`update()` uses current steady-clock time, directly calls `action(at)` when due,
skips missed ticks and stops PWM on its independent watchdog. `nextWake()` gives
the next timer/deadline wake time after the first update. Zero-angle preparation is
already complete. The return status is RUNNING, COMPLETE or a negative error.

Keep execution ownership through runner destruction; the contained motor stops
before the owning actor releases the shared mutex. GPIO initialization/termination
remain at application scope.

`StepperMotorRunner` uses the shared `Clock` from `lib/timelib.h` for its timer
and watchdog time points; there is no runner-owned clock alias.
