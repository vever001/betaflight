/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <platform.h>

#include "build/build_config.h"
#include "build/debug.h"

#include "common/axis.h"
#include "common/maths.h"
#include "common/filter.h"

#include "config/config_reset.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "drivers/sound_beeper.h"
#include "drivers/time.h"

#include "fc/fc_core.h"
#include "fc/fc_rc.h"

#include "fc/rc_controls.h"
#include "fc/runtime_config.h"

#include "flight/pid.h"
#include "flight/imu.h"
#include "flight/mixer.h"

#include "io/gps.h"

#include "sensors/gyro.h"
#include "sensors/acceleration.h"


FAST_RAM uint32_t targetPidLooptime;
FAST_RAM pidAxisData_t pidData[XYZ_AXIS_COUNT];

static FAST_RAM bool pidStabilisationEnabled;

static FAST_RAM bool inCrashRecoveryMode = false;

static FAST_RAM float dT;

PG_REGISTER_WITH_RESET_TEMPLATE(pidConfig_t, pidConfig, PG_PID_CONFIG, 2);

#ifdef STM32F10X
#define PID_PROCESS_DENOM_DEFAULT       1
#elif defined(USE_GYRO_SPI_MPU6000) || defined(USE_GYRO_SPI_MPU6500)  || defined(USE_GYRO_SPI_ICM20689)
#define PID_PROCESS_DENOM_DEFAULT       4
#else
#define PID_PROCESS_DENOM_DEFAULT       2
#endif

#ifdef USE_RUNAWAY_TAKEOFF
PG_RESET_TEMPLATE(pidConfig_t, pidConfig,
    .pid_process_denom = PID_PROCESS_DENOM_DEFAULT,
    .runaway_takeoff_prevention = true,
    .runaway_takeoff_deactivate_throttle = 25,  // throttle level % needed to accumulate deactivation time
    .runaway_takeoff_deactivate_delay = 500     // Accumulated time (in milliseconds) before deactivation in successful takeoff
);
#else
PG_RESET_TEMPLATE(pidConfig_t, pidConfig,
    .pid_process_denom = PID_PROCESS_DENOM_DEFAULT
);
#endif

PG_REGISTER_ARRAY_WITH_RESET_FN(pidProfile_t, MAX_PROFILE_COUNT, pidProfiles, PG_PID_PROFILE, 2);

void resetPidProfile(pidProfile_t *pidProfile)
{
    RESET_CONFIG(pidProfile_t, pidProfile,
        .pid = {
            [PID_ROLL] =  { 40, 40, 30 },
            [PID_PITCH] = { 58, 50, 35 },
            [PID_YAW] =   { 70, 45, 20 },
            [PID_ALT] =   { 50, 0, 0 },
            [PID_POS] =   { 15, 0, 0 },     // POSHOLD_P * 100, POSHOLD_I * 100,
            [PID_POSR] =  { 34, 14, 53 },   // POSHOLD_RATE_P * 10, POSHOLD_RATE_I * 100, POSHOLD_RATE_D * 1000,
            [PID_NAVR] =  { 25, 33, 83 },   // NAV_P * 10, NAV_I * 100, NAV_D * 1000
            [PID_LEVEL] = { 50, 50, 75 },
            [PID_MAG] =   { 40, 0, 0 },
            [PID_VEL] =   { 55, 55, 75 }
        },

        .pidSumLimit = PIDSUM_LIMIT,
        .pidSumLimitYaw = PIDSUM_LIMIT_YAW,
        .yaw_lowpass_hz = 0,
        .dterm_lowpass_hz = 100,    // filtering ON by default
        .dterm_lowpass2_hz = 0,    // second Dterm LPF OFF by default
        .dterm_notch_hz = 260,
        .dterm_notch_cutoff = 160,
        .dterm_filter_type = FILTER_BIQUAD,
        .itermWindupPointPercent = 50,
        .vbatPidCompensation = 0,
        .pidAtMinThrottle = PID_STABILISATION_ON,
        .levelAngleLimit = 55,
        .setpointRelaxRatio = 100,
        .dtermSetpointWeight = 0,
        .yawRateAccelLimit = 100,
        .rateAccelLimit = 0,
        .itermThrottleThreshold = 350,
        .itermAcceleratorGain = 1000,
        .crash_time = 500,          // ms
        .crash_delay = 0,           // ms
        .crash_recovery_angle = 10, // degrees
        .crash_recovery_rate = 100, // degrees/second
        .crash_dthreshold = 50,     // degrees/second/second
        .crash_gthreshold = 400,    // degrees/second
        .crash_setpoint_threshold = 350, // degrees/second
        .crash_recovery = PID_CRASH_RECOVERY_OFF, // off by default
        .horizon_tilt_effect = 75,
        .horizon_tilt_expert_mode = false,
        .crash_limit_yaw = 200,
        .itermLimit = 150,
        .throttle_boost = 0,
        .throttle_boost_cutoff = 15,
        .iterm_rotation = false,
    );
}

void pgResetFn_pidProfiles(pidProfile_t *pidProfiles)
{
    for (int i = 0; i < MAX_PROFILE_COUNT; i++) {
        resetPidProfile(&pidProfiles[i]);
    }
}

static void pidSetTargetLooptime(uint32_t pidLooptime)
{
    targetPidLooptime = pidLooptime;
    dT = (float)targetPidLooptime * 0.000001f;
}

void pidResetITerm(void)
{
    for (int axis = 0; axis < 3; axis++) {
        pidData[axis].I = 0.0f;
    }
}

static FAST_RAM_INITIALIZED float itermAccelerator = 1.0f;

void pidSetItermAccelerator(float newItermAccelerator)
{
    itermAccelerator = newItermAccelerator;
}

float pidItermAccelerator(void)
{
    return itermAccelerator;
}

void pidStabilisationState(pidStabilisationState_e pidControllerState)
{
    pidStabilisationEnabled = (pidControllerState == PID_STABILISATION_ON) ? true : false;
}

const angle_index_t rcAliasToAngleIndexMap[] = { AI_ROLL, AI_PITCH };

typedef union dtermLowpass_u {
    pt1Filter_t pt1Filter;
    biquadFilter_t biquadFilter;
#if defined(USE_FIR_FILTER_DENOISE)
    firFilterDenoise_t denoisingFilter;
#endif
} dtermLowpass_t;

static FAST_RAM filterApplyFnPtr dtermNotchApplyFn;
static FAST_RAM biquadFilter_t dtermNotch[2];
static FAST_RAM filterApplyFnPtr dtermLowpassApplyFn;
static FAST_RAM dtermLowpass_t dtermLowpass[2];
static FAST_RAM filterApplyFnPtr dtermLowpass2ApplyFn;
static FAST_RAM pt1Filter_t dtermLowpass2[2];
static FAST_RAM filterApplyFnPtr ptermYawLowpassApplyFn;
static FAST_RAM pt1Filter_t ptermYawLowpass;

void pidInitFilters(const pidProfile_t *pidProfile)
{
    BUILD_BUG_ON(FD_YAW != 2); // only setting up Dterm filters on roll and pitch axes, so ensure yaw axis is 2

    if (targetPidLooptime == 0) {
        // no looptime set, so set all the filters to null
        dtermNotchApplyFn = nullFilterApply;
        dtermLowpassApplyFn = nullFilterApply;
        ptermYawLowpassApplyFn = nullFilterApply;
        return;
    }

    const uint32_t pidFrequencyNyquist = (1.0f / dT) / 2; // No rounding needed

    uint16_t dTermNotchHz;
    if (pidProfile->dterm_notch_hz <= pidFrequencyNyquist) {
        dTermNotchHz = pidProfile->dterm_notch_hz;
    } else {
        if (pidProfile->dterm_notch_cutoff < pidFrequencyNyquist) {
            dTermNotchHz = pidFrequencyNyquist;
        } else {
            dTermNotchHz = 0;
        }
    }

    if (dTermNotchHz != 0 && pidProfile->dterm_notch_cutoff != 0) {
        dtermNotchApplyFn = (filterApplyFnPtr)biquadFilterApply;
        const float notchQ = filterGetNotchQ(dTermNotchHz, pidProfile->dterm_notch_cutoff);
        for (int axis = FD_ROLL; axis <= FD_PITCH; axis++) {
            biquadFilterInit(&dtermNotch[axis], dTermNotchHz, targetPidLooptime, notchQ, FILTER_NOTCH);
        }
    } else {
        dtermNotchApplyFn = nullFilterApply;
    }

    //2nd Dterm Lowpass Filter
    if (pidProfile->dterm_lowpass2_hz == 0 || pidProfile->dterm_lowpass2_hz > pidFrequencyNyquist) {
    	dtermLowpass2ApplyFn = nullFilterApply;
    } else {
        dtermLowpass2ApplyFn = (filterApplyFnPtr)pt1FilterApply;
        for (int axis = FD_ROLL; axis <= FD_PITCH; axis++) {
            pt1FilterInit(&dtermLowpass2[axis], pt1FilterGain(pidProfile->dterm_lowpass2_hz, dT));
        }
    }

    if (pidProfile->dterm_lowpass_hz == 0 || pidProfile->dterm_lowpass_hz > pidFrequencyNyquist) {
        dtermLowpassApplyFn = nullFilterApply;
    } else {
        switch (pidProfile->dterm_filter_type) {
        default:
            dtermLowpassApplyFn = nullFilterApply;
            break;
        case FILTER_PT1:
            dtermLowpassApplyFn = (filterApplyFnPtr)pt1FilterApply;
            for (int axis = FD_ROLL; axis <= FD_PITCH; axis++) {
                pt1FilterInit(&dtermLowpass[axis].pt1Filter, pt1FilterGain(pidProfile->dterm_lowpass_hz, dT));
            }
            break;
        case FILTER_BIQUAD:
            dtermLowpassApplyFn = (filterApplyFnPtr)biquadFilterApply;
            for (int axis = FD_ROLL; axis <= FD_PITCH; axis++) {
                biquadFilterInitLPF(&dtermLowpass[axis].biquadFilter, pidProfile->dterm_lowpass_hz, targetPidLooptime);
            }
            break;
#if defined(USE_FIR_FILTER_DENOISE)
        case FILTER_FIR:
            dtermLowpassApplyFn = (filterApplyFnPtr)firFilterDenoiseUpdate;
            for (int axis = FD_ROLL; axis <= FD_PITCH; axis++) {
                firFilterDenoiseInit(&dtermLowpass[axis].denoisingFilter, pidProfile->dterm_lowpass_hz, targetPidLooptime);
            }
            break;
#endif
        }
    }

    if (pidProfile->yaw_lowpass_hz == 0 || pidProfile->yaw_lowpass_hz > pidFrequencyNyquist) {
        ptermYawLowpassApplyFn = nullFilterApply;
    } else {
        ptermYawLowpassApplyFn = (filterApplyFnPtr)pt1FilterApply;
        pt1FilterInit(&ptermYawLowpass, pt1FilterGain(pidProfile->yaw_lowpass_hz, dT));
    }

    pt1FilterInit(&throttleLpf, pt1FilterGain(pidProfile->throttle_boost_cutoff, dT));
}

typedef struct pidCoefficient_s {
    float Kp;
    float Ki;
    float Kd;
} pidCoefficient_t;

static FAST_RAM pidCoefficient_t pidCoefficient[3];
static FAST_RAM float maxVelocity[3];
static FAST_RAM float relaxFactor;
static FAST_RAM float dtermSetpointWeight;
static FAST_RAM float levelGain, horizonGain, horizonTransition, horizonCutoffDegrees, horizonFactorRatio;
static FAST_RAM float ITermWindupPointInv;
static FAST_RAM uint8_t horizonTiltExpertMode;
static FAST_RAM timeDelta_t crashTimeLimitUs;
static FAST_RAM timeDelta_t crashTimeDelayUs;
static FAST_RAM int32_t crashRecoveryAngleDeciDegrees;
static FAST_RAM float crashRecoveryRate;
static FAST_RAM float crashDtermThreshold;
static FAST_RAM float crashGyroThreshold;
static FAST_RAM float crashSetpointThreshold;
static FAST_RAM float crashLimitYaw;
static FAST_RAM float itermLimit;
FAST_RAM float throttleBoost;
pt1Filter_t throttleLpf;
static FAST_RAM bool itermRotation;

void pidInitConfig(const pidProfile_t *pidProfile)
{
    for (int axis = FD_ROLL; axis <= FD_YAW; axis++) {
        pidCoefficient[axis].Kp = PTERM_SCALE * pidProfile->pid[axis].P;
        pidCoefficient[axis].Ki = ITERM_SCALE * pidProfile->pid[axis].I;
        pidCoefficient[axis].Kd = DTERM_SCALE * pidProfile->pid[axis].D;
    }

    dtermSetpointWeight = pidProfile->dtermSetpointWeight / 127.0f;
    if (pidProfile->setpointRelaxRatio == 0) {
        relaxFactor = 0;
    } else {
        relaxFactor = 100.0f / pidProfile->setpointRelaxRatio;
    }
    levelGain = pidProfile->pid[PID_LEVEL].P / 10.0f;
    horizonGain = pidProfile->pid[PID_LEVEL].I / 10.0f;
    horizonTransition = (float)pidProfile->pid[PID_LEVEL].D;
    horizonTiltExpertMode = pidProfile->horizon_tilt_expert_mode;
    horizonCutoffDegrees = (175 - pidProfile->horizon_tilt_effect) * 1.8f;
    horizonFactorRatio = (100 - pidProfile->horizon_tilt_effect) * 0.01f;
    maxVelocity[FD_ROLL] = maxVelocity[FD_PITCH] = pidProfile->rateAccelLimit * 100 * dT;
    maxVelocity[FD_YAW] = pidProfile->yawRateAccelLimit * 100 * dT;
    const float ITermWindupPoint = (float)pidProfile->itermWindupPointPercent / 100.0f;
    ITermWindupPointInv = 1.0f / (1.0f - ITermWindupPoint);
    crashTimeLimitUs = pidProfile->crash_time * 1000;
    crashTimeDelayUs = pidProfile->crash_delay * 1000;
    crashRecoveryAngleDeciDegrees = pidProfile->crash_recovery_angle * 10;
    crashRecoveryRate = pidProfile->crash_recovery_rate;
    crashGyroThreshold = pidProfile->crash_gthreshold;
    crashDtermThreshold = pidProfile->crash_dthreshold;
    crashSetpointThreshold = pidProfile->crash_setpoint_threshold;
    crashLimitYaw = pidProfile->crash_limit_yaw;
    itermLimit = pidProfile->itermLimit;
    throttleBoost = pidProfile->throttle_boost * 0.1f;
    itermRotation = pidProfile->iterm_rotation == 1;
}

void pidInit(const pidProfile_t *pidProfile)
{
    pidSetTargetLooptime(gyro.targetLooptime * pidConfig()->pid_process_denom); // Initialize pid looptime
    pidInitFilters(pidProfile);
    pidInitConfig(pidProfile);
}


void pidCopyProfile(uint8_t dstPidProfileIndex, uint8_t srcPidProfileIndex)
{
    if ((dstPidProfileIndex < MAX_PROFILE_COUNT-1 && srcPidProfileIndex < MAX_PROFILE_COUNT-1)
        && dstPidProfileIndex != srcPidProfileIndex
    ) {
        memcpy(pidProfilesMutable(dstPidProfileIndex), pidProfilesMutable(srcPidProfileIndex), sizeof(pidProfile_t));
    }
}

// calculates strength of horizon leveling; 0 = none, 1.0 = most leveling
static float calcHorizonLevelStrength(void)
{
    // start with 1.0 at center stick, 0.0 at max stick deflection:
    float horizonLevelStrength = 1.0f - MAX(getRcDeflectionAbs(FD_ROLL), getRcDeflectionAbs(FD_PITCH));

    // 0 at level, 90 at vertical, 180 at inverted (degrees):
    const float currentInclination = MAX(ABS(attitude.values.roll), ABS(attitude.values.pitch)) / 10.0f;

    // horizonTiltExpertMode:  0 = leveling always active when sticks centered,
    //                         1 = leveling can be totally off when inverted
    if (horizonTiltExpertMode) {
        if (horizonTransition > 0 && horizonCutoffDegrees > 0) {
                    // if d_level > 0 and horizonTiltEffect < 175
            // horizonCutoffDegrees: 0 to 125 => 270 to 90 (represents where leveling goes to zero)
            // inclinationLevelRatio (0.0 to 1.0) is smaller (less leveling)
            //  for larger inclinations; 0.0 at horizonCutoffDegrees value:
            const float inclinationLevelRatio = constrainf((horizonCutoffDegrees-currentInclination) / horizonCutoffDegrees, 0, 1);
            // apply configured horizon sensitivity:
                // when stick is near center (horizonLevelStrength ~= 1.0)
                //  H_sensitivity value has little effect,
                // when stick is deflected (horizonLevelStrength near 0.0)
                //  H_sensitivity value has more effect:
            horizonLevelStrength = (horizonLevelStrength - 1) * 100 / horizonTransition + 1;
            // apply inclination ratio, which may lower leveling
            //  to zero regardless of stick position:
            horizonLevelStrength *= inclinationLevelRatio;
        } else  { // d_level=0 or horizon_tilt_effect>=175 means no leveling
          horizonLevelStrength = 0;
        }
    } else { // horizon_tilt_expert_mode = 0 (leveling always active when sticks centered)
        float sensitFact;
        if (horizonFactorRatio < 1.01f) { // if horizonTiltEffect > 0
            // horizonFactorRatio: 1.0 to 0.0 (larger means more leveling)
            // inclinationLevelRatio (0.0 to 1.0) is smaller (less leveling)
            //  for larger inclinations, goes to 1.0 at inclination==level:
            const float inclinationLevelRatio = (180-currentInclination)/180 * (1.0f-horizonFactorRatio) + horizonFactorRatio;
            // apply ratio to configured horizon sensitivity:
            sensitFact = horizonTransition * inclinationLevelRatio;
        } else { // horizonTiltEffect=0 for "old" functionality
            sensitFact = horizonTransition;
        }

        if (sensitFact <= 0) {           // zero means no leveling
            horizonLevelStrength = 0;
        } else {
            // when stick is near center (horizonLevelStrength ~= 1.0)
            //  sensitFact value has little effect,
            // when stick is deflected (horizonLevelStrength near 0.0)
            //  sensitFact value has more effect:
            horizonLevelStrength = ((horizonLevelStrength - 1) * (100 / sensitFact)) + 1;
        }
    }
    return constrainf(horizonLevelStrength, 0, 1);
}

static float pidLevel(int axis, const pidProfile_t *pidProfile, const rollAndPitchTrims_t *angleTrim, float currentPidSetpoint) {
    // calculate error angle and limit the angle to the max inclination
    // rcDeflection is in range [-1.0, 1.0]
    float angle = pidProfile->levelAngleLimit * getRcDeflection(axis);
    angle = constrainf(angle, -pidProfile->levelAngleLimit, pidProfile->levelAngleLimit);
    const float errorAngle = angle - ((attitude.raw[axis] - angleTrim->raw[axis]) / 10.0f);
    if (FLIGHT_MODE(ANGLE_MODE)) {
        // ANGLE mode - control is angle based
        currentPidSetpoint = errorAngle * levelGain;
    } else {
        // HORIZON mode - mix of ANGLE and ACRO modes
        // mix in errorAngle to currentPidSetpoint to add a little auto-level feel
        const float horizonLevelStrength = calcHorizonLevelStrength();
        currentPidSetpoint = currentPidSetpoint + (errorAngle * horizonGain * horizonLevelStrength);
    }
    return currentPidSetpoint;
}

static float accelerationLimit(int axis, float currentPidSetpoint)
{
    static float previousSetpoint[3];
    const float currentVelocity = currentPidSetpoint - previousSetpoint[axis];

    if (ABS(currentVelocity) > maxVelocity[axis]) {
        currentPidSetpoint = (currentVelocity > 0) ? previousSetpoint[axis] + maxVelocity[axis] : previousSetpoint[axis] - maxVelocity[axis];
    }

    previousSetpoint[axis] = currentPidSetpoint;
    return currentPidSetpoint;
}

static timeUs_t crashDetectedAtUs;

static void handleCrashRecovery(
    const pidCrashRecovery_e crash_recovery, const rollAndPitchTrims_t *angleTrim,
    const int axis, const timeUs_t currentTimeUs, const float gyroRate, float *currentPidSetpoint, float *errorRate)
{
    if (inCrashRecoveryMode && cmpTimeUs(currentTimeUs, crashDetectedAtUs) > crashTimeDelayUs) {
        if (crash_recovery == PID_CRASH_RECOVERY_BEEP) {
            BEEP_ON;
        }
        if (axis == FD_YAW) {
            *errorRate = constrainf(*errorRate, -crashLimitYaw, crashLimitYaw);
        } else {
            // on roll and pitch axes calculate currentPidSetpoint and errorRate to level the aircraft to recover from crash
            if (sensors(SENSOR_ACC)) {
                // errorAngle is deviation from horizontal
                const float errorAngle =  -(attitude.raw[axis] - angleTrim->raw[axis]) / 10.0f;
                *currentPidSetpoint = errorAngle * levelGain;
                *errorRate = *currentPidSetpoint - gyroRate;
            }
        }
        // reset ITerm, since accumulated error before crash is now meaningless
        // and ITerm windup during crash recovery can be extreme, especially on yaw axis
        pidData[axis].I = 0.0f;
        if (cmpTimeUs(currentTimeUs, crashDetectedAtUs) > crashTimeLimitUs
            || (getMotorMixRange() < 1.0f
                   && ABS(gyro.gyroADCf[FD_ROLL]) < crashRecoveryRate
                   && ABS(gyro.gyroADCf[FD_PITCH]) < crashRecoveryRate
                   && ABS(gyro.gyroADCf[FD_YAW]) < crashRecoveryRate)) {
            if (sensors(SENSOR_ACC)) {
                // check aircraft nearly level
                if (ABS(attitude.raw[FD_ROLL] - angleTrim->raw[FD_ROLL]) < crashRecoveryAngleDeciDegrees
                   && ABS(attitude.raw[FD_PITCH] - angleTrim->raw[FD_PITCH]) < crashRecoveryAngleDeciDegrees) {
                    inCrashRecoveryMode = false;
                    BEEP_OFF;
                }
            } else {
                inCrashRecoveryMode = false;
                BEEP_OFF;
            }
        }
    }
}

static void detectAndSetCrashRecovery(
    const pidCrashRecovery_e crash_recovery, const int axis,
    const timeUs_t currentTimeUs, const float delta, const float errorRate)
{
    // if crash recovery is on and accelerometer enabled and there is no gyro overflow, then check for a crash
    // no point in trying to recover if the crash is so severe that the gyro overflows
    if (crash_recovery && !gyroOverflowDetected()) {
        if (ARMING_FLAG(ARMED)) {
            if (getMotorMixRange() >= 1.0f && !inCrashRecoveryMode
                && ABS(delta) > crashDtermThreshold
                && ABS(errorRate) > crashGyroThreshold
                && ABS(getSetpointRate(axis)) < crashSetpointThreshold) {
                inCrashRecoveryMode = true;
                crashDetectedAtUs = currentTimeUs;
            }
            if (inCrashRecoveryMode && cmpTimeUs(currentTimeUs, crashDetectedAtUs) < crashTimeDelayUs && (ABS(errorRate) < crashGyroThreshold
                || ABS(getSetpointRate(axis)) > crashSetpointThreshold)) {
                inCrashRecoveryMode = false;
                BEEP_OFF;
            }
        } else if (inCrashRecoveryMode) {
            inCrashRecoveryMode = false;
            BEEP_OFF;
        }
    }
}

static void handleItermRotation()
{
    // rotate old I to the new coordinate system
    const float gyroToAngle = dT * RAD;
    for (int i = FD_ROLL; i <= FD_YAW; i++) {
        int i_1 = (i + 1) % 3;
        int i_2 = (i + 2) % 3;
        float angle = gyro.gyroADCf[i] * gyroToAngle;
        float newPID_I_i_1 = pidData[i_1].I + pidData[i_2].I * angle;
        pidData[i_2].I -= pidData[i_1].I * angle;
        pidData[i_1].I = newPID_I_i_1;
    }
}

// Betaflight pid controller, which will be maintained in the future with additional features specialised for current (mini) multirotor usage.
// Based on 2DOF reference design (matlab)
void pidController(const pidProfile_t *pidProfile, const rollAndPitchTrims_t *angleTrim, timeUs_t currentTimeUs)
{
    static float previousGyroRateDterm[2];
    static float previousPidSetpoint[2];

    const float tpaFactor = getThrottlePIDAttenuation();
    const float motorMixRange = getMotorMixRange();

#ifdef USE_YAW_SPIN_RECOVERY
    const bool yawSpinActive = gyroYawSpinDetected();
#endif

    // Dynamic i component,
    // gradually scale back integration when above windup point
    const float dynCi = MIN((1.0f - motorMixRange) * ITermWindupPointInv, 1.0f) * dT * itermAccelerator;

    // Dynamic d component, enable 2-DOF PID controller only for rate mode
    const float dynCd = flightModeFlags ? 0.0f : dtermSetpointWeight;

    // Precalculate gyro deta for D-term here, this allows loop unrolling
    float gyroRateDterm[2];
    for (int axis = FD_ROLL; axis < FD_YAW; ++axis) {
        gyroRateDterm[axis] = dtermNotchApplyFn((filter_t *) &dtermNotch[axis], gyro.gyroADCf[axis]);
        gyroRateDterm[axis] = dtermLowpassApplyFn((filter_t *) &dtermLowpass[axis], gyroRateDterm[axis]);
        gyroRateDterm[axis] = dtermLowpass2ApplyFn((filter_t *) &dtermLowpass2[axis], gyroRateDterm[axis]);
    }

    if (itermRotation) {
        handleItermRotation();
    }

    // ----------PID controller----------
    for (int axis = FD_ROLL; axis <= FD_YAW; ++axis) {
        float currentPidSetpoint = getSetpointRate(axis);
        if (maxVelocity[axis]) {
            currentPidSetpoint = accelerationLimit(axis, currentPidSetpoint);
        }
        // Yaw control is GYRO based, direct sticks control is applied to rate PID
        if ((FLIGHT_MODE(ANGLE_MODE) || FLIGHT_MODE(HORIZON_MODE)) && axis != YAW) {
            currentPidSetpoint = pidLevel(axis, pidProfile, angleTrim, currentPidSetpoint);
        }

        // Handle yaw spin recovery - zero the setpoint on yaw to aid in recovery
        // It's not necessary to zero the set points for R/P because the PIDs will be zeroed below
#ifdef USE_YAW_SPIN_RECOVERY
        if ((axis == FD_YAW) && yawSpinActive) {
            currentPidSetpoint = 0.0f;
        }
#endif // USE_YAW_SPIN_RECOVERY

        // -----calculate error rate
        const float gyroRate = gyro.gyroADCf[axis]; // Process variable from gyro output in deg/sec
        float errorRate = currentPidSetpoint - gyroRate; // r - y

        handleCrashRecovery(
            pidProfile->crash_recovery, angleTrim, axis, currentTimeUs, gyroRate,
            &currentPidSetpoint, &errorRate);

        // --------low-level gyro-based PID based on 2DOF PID controller. ----------
        // 2-DOF PID controller with optional filter on derivative term.
        // b = 1 and only c (dtermSetpointWeight) can be tuned (amount derivative on measurement or error).

        // -----calculate P component and add Dynamic Part based on stick input
        pidData[axis].P = pidCoefficient[axis].Kp * errorRate * tpaFactor;
        if (axis == FD_YAW) {
            pidData[axis].P = ptermYawLowpassApplyFn((filter_t *) &ptermYawLowpass, pidData[axis].P);
        }

        // -----calculate I component
        const float ITerm = pidData[axis].I;
        const float ITermNew = constrainf(ITerm + pidCoefficient[axis].Ki * errorRate * dynCi, -itermLimit, itermLimit);
        const bool outputSaturated = mixerIsOutputSaturated(axis, errorRate);
        if (outputSaturated == false || ABS(ITermNew) < ABS(ITerm)) {
            // Only increase ITerm if output is not saturated
            pidData[axis].I = ITermNew;
        }

        // -----calculate D component
        if (axis != FD_YAW) {
            // no transition if relaxFactor == 0
            float transition = relaxFactor > 0 ? MIN(1.f, getRcDeflectionAbs(axis) * relaxFactor) : 1;

            // Divide rate change by dT to get differential (ie dr/dt).
            // dT is fixed and calculated from the target PID loop time
            // This is done to avoid DTerm spikes that occur with dynamically
            // calculated deltaT whenever another task causes the PID
            // loop execution to be delayed.
            const float delta = (
                dynCd * transition * (currentPidSetpoint - previousPidSetpoint[axis]) -
                (gyroRateDterm[axis] - previousGyroRateDterm[axis])) / dT;

            previousPidSetpoint[axis] = currentPidSetpoint;
            previousGyroRateDterm[axis] = gyroRateDterm[axis];

            detectAndSetCrashRecovery(pidProfile->crash_recovery, axis, currentTimeUs, delta, errorRate);

            pidData[axis].D = pidCoefficient[axis].Kd * delta * tpaFactor;

#ifdef USE_YAW_SPIN_RECOVERY
            if (yawSpinActive)  {
                // zero PIDs on pitch and roll leaving yaw P to correct spin 
                pidData[axis].P = 0;
                pidData[axis].I = 0;
                pidData[axis].D = 0;
            }
#endif // USE_YAW_SPIN_RECOVERY
        }
    }

    // calculating the PID sum
    pidData[FD_ROLL].Sum = pidData[FD_ROLL].P + pidData[FD_ROLL].I + pidData[FD_ROLL].D;
    pidData[FD_PITCH].Sum = pidData[FD_PITCH].P + pidData[FD_PITCH].I + pidData[FD_PITCH].D;

#ifdef USE_YAW_SPIN_RECOVERY
    if (yawSpinActive) {
    // yaw P alone to correct spin 
        pidData[FD_YAW].I = 0;
    }
#endif // USE_YAW_SPIN_RECOVERY

    // YAW has no D
    pidData[FD_YAW].Sum = pidData[FD_YAW].P + pidData[FD_YAW].I;

    // Disable PID control if at zero throttle or if gyro overflow detected
    // This may look very innefficient, but it is done on purpose to always show real CPU usage as in flight
    if (!pidStabilisationEnabled || gyroOverflowDetected()) {
        for (int axis = FD_ROLL; axis <= FD_YAW; ++axis) {
            pidData[axis].P = 0;
            pidData[axis].I = 0;
            pidData[axis].D = 0;

            pidData[axis].Sum = 0;
        }
    }
}

bool crashRecoveryModeActive(void)
{
    return inCrashRecoveryMode;
}
