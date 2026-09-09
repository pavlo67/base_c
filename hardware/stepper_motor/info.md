# Stepper motor planning, execution and probes

## Units and metrics

The public API is in `stepper_motor.h`. `moment` and `duration` are uint64_t nanoseconds from `lib/timelib.h`; angles are degrees. Simulations use a synthetic clock starting at zero. External execution callers provide monotonically increasing timestamps from one clock. `run()` uses `std::chrono::steady_clock`, not the realtime clock in `now()`.

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

For an accelerated move with zero base speed, one pulse is reserved from the quantized requested displacement for a slow terminal interval. The preceding displacement uses the existing acceleration/deceleration construction. The terminal period is the greater of 100 ms and the natural single-pulse rest-to-rest period respecting speed/acceleration limits. Thus the extra slowing is bounded at 100 ms; naturally slower moves are not accelerated. No extra pulse is added to the requested angle, no infinite interval is requested, and no post-stop zero speed is reported. The terminal section is grouped with Deceleration in logs. Nonzero-base-speed sequences retain their endpoint behavior.

With timer zero, the preceding motion contains acceleration, optional cruise and deceleration. With a nonzero timer it accelerates until the first tick at or beyond half of the preceding quantized displacement, rounded upward to a pulse; the speed-limit period can repeat inside that section. A deceleration section is then built from its last interval-average speed. Optional cruise uses surplus displacement before braking. `stopAfterPulses_` records the simulated halfway threshold. `pairedTargetPulses_` retains the full acceleration/cruise/braking target before timer overshoot, excluding the reserved terminal pulse; REAL uses this full target rather than reconstructing it from the simulated halfway remainder.

Timed deceleration uses `minimumPulsesCount_` to fill any remaining integer angle at its last period. `getSeriesSequence()` also corrects overall shortfalls. Coarse-timer overshoots are retained. Reserving the terminal pulse does not guarantee exactly N physical edges from a free-running PWM generator.

For default 90-degree / 5-ms settings, clocked simulation has 200 acceleration pulses, 199 pulses distributed across cruise/main braking and one reserved terminal pulse: 400 pulses / 90 degrees. Last-interval speed is 2.25 deg/s, versus approximately 9 deg/s before terminal slowing. REAL phase counts and timing come from predictive execution and actual call spacing.

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

`StepperMotorAction::run(expecterInterval, timeLimit=60*SECOND)` supplies a steady-clock timer to `action()` until completion. It never inspects section parameters or counts. Timer zero selects a 1-us polling cadence; it does not turn live GPIO execution into an ideal simulation. Missed ticks are skipped rather than replayed. An independent deadline bounds sleeps and returns `TIME_LIMIT` (-10008) after stopping PWM; it cannot interrupt a blocking backend call. Invalid timer limits are errors.

Blocking callers construct `StepperMotorAction` and call `motor.run(expecterInterval, timeLimit)`, then read `motor.result()` for runtime statistics, including partial observations on failure. `stepperMotorRun()` performs this directly and destroys the motor before GPIO termination.

`probe_sequences` keeps its rotation list, 5-ms timer, 60-second watchdog and pins as source constants. It prints `Target`, ideal zero-timer statistics, clocked simulation statistics, and `real` statistics collected during `run()`.

The configuration files in this checkout are `_base_defines.h` and `_base_defines_example.h` at the repository root (there is no `_base_defines/` directory). Both define:

- `STEPPER_MOTOR_PROBE_VERBOSE`, default false: print per-section details when enabled. When disabled, only Target, phase headings, phase block totals and TOTAL rows remain, apart from warnings/errors.
- `STEPPER_MOTOR_PROBE_HARDWARE_PWM`, default true: select dedicated hardware PWM. Desktop prints a Warning and uses OFF. Other platforms return actual hardware capability/configuration errors instead of silently falling back.

Hardware PWM in this probe uses the explicit `HARDWARE_PWM_PIN_STEP = 18` constant: STEP must be wired/routed to BCM18. OFF retains `PIN_STEP` (currently BCM17); DIR and ENA retain the shared hardware constants. Hardware routing, overlays and resource constraints are documented in `hardware/gpio/pwm/PWM.md`.

## Shared complete-move runner

`stepperMotorRun(const StepperMotorRunConfig&, float rotationDeg, const std::string& label = "motor", StepperMotorSeriesSequence* real = nullptr)` in `stepper_motor_action.cpp` owns the full blocking, rest-to-rest move. It validates options/angle/pins/timing, calculates constant-acceleration ideal and configured-timer series, logs both, initializes GPIO, constructs the motor, executes its timer loop, logs runtime statistics even on failure, destroys the motor and terminates GPIO. The return value is zero on success or a negative error code; cleanup failure is returned when execution succeeded. Optional `real` is reset on entry and receives runtime/partial results and failure diagnostics. Zero angles return without touching GPIO. Invalid inputs are rejected before planning and GPIO access.

`StepperMotorRunConfig` contains `pinStep_`, `pinDir_`, `pinEna_` (BCM), `options_`, `expecterInterval_`, `pulseHigh_`, `timeLimit_` (nanoseconds), `hardwarePwm_` and `verbose_`. Rotation is signed degrees; application wire units must be decoded by the caller. The helper owns the shared GPIO lifecycle, so callers must serialize whole calls and must not use it while another GPIO consumer is active. Logging is outside the timed execution loop. Runtime values are PWM estimates, not encoder observations; pulse quantization and timer overshoot still apply.

`probe_sequences` chooses its source configuration/platform PWM preference and iterates angles through `stepperMotorRun()`. Machina uses the same helper with axis-specific configurations and labels. Low-level callers with an existing sequence and caller-owned GPIO use `StepperMotorAction::run()` and `result()` directly; there is no free `run()` or `executeSequence()` API. Cleanup happens before GPIO termination. Diagnostics use `[function()] ERROR: details`, with qualified context names where needed.

## Statistics, limitations and tests

`StepperMotorSeriesSequence::log(options, label, verbose=false)` groups contiguous sections of the same phase. Each Acceleration/Cruise/Deceleration block ends with the same time, rotation, actual pulse counts, last-interval speed, maximum speed and maximum absolute acceleration fields as TOTAL. Cross-section acceleration uses interval midpoint separation and belongs to the following block. The terminal interval belongs to Deceleration. Summary rows omit the `expected=` field. Verbose mode adds per-section details, including the explicitly named `expectedPulsesCount` planning field, without removing summaries.

`real` means runtime observations and commanded PWM estimates, not encoder readings or measured GPIO edges. The GPIO API does not expose waveform phase, native frequency quantization or edge counts. Startup edges, backend reconfiguration gaps and physical timing can differ from the model; desktop has no physical waveform at all. PWM is not a hardware N-pulse counter, and scheduler delays can overshoot the target. Mechanical inertia/settling is not estimated.

The `stepper_motor_test` GTest/CTest target covers existing simulation behavior, both directions, pulse quantization/replay, finite terminal slowing, grouped/verbose output, external ticks, repeated/older timestamps, runtime PWM settings, independent pins, hardware mode through the desktop contract, errors, timeout and cleanup. Raspberry Pi waveform behavior requires on-device validation. The older `probe_180_360_180` and `probe_infinite` retain their APIs and implementation.

Additional GTest/CTest cases cover braking prediction against fixed-timer replay (including nonzero final speed), the exact fits/does-not-fit pulse boundary, skipping commands after delayed observations, signed 33/90/180/720-degree moves, full target retention after simulated halfway overshoot, startup exclusion and mean timing, duplicate/older timestamps, frozen profiles during later delays, zero-timer input plans, speed/acceleration limits during acceleration, and permitted stronger braking. Runtime timing comparisons are deterministic desktop PWM-model results; Raspberry Pi waveforms and mechanical behavior require on-device validation.
