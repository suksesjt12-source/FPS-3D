package com.tacticalstrike.game

import android.content.Context
import android.content.SharedPreferences

class SettingsManager(context: Context) {

    data class ControlPos(
        var xNorm: Float,
        var yNorm: Float,
        var radiusDp: Float
    )

    private val prefs: SharedPreferences =
        context.getSharedPreferences("tactical_strike_prefs", Context.MODE_PRIVATE)

    var touchSensitivity: Float
        get() = prefs.getFloat("touch_sens", 0.0032f)
        set(v) = prefs.edit().putFloat("touch_sens", v).apply()

    var adsMultiplier: Float
        get() = prefs.getFloat("ads_mult", 0.55f)
        set(v) = prefs.edit().putFloat("ads_mult", v).apply()

    var gyroEnabled: Boolean
        get() = prefs.getBoolean("gyro_enabled", true)
        set(v) = prefs.edit().putBoolean("gyro_enabled", v).apply()

    var gyroSensitivity: Float
        get() = prefs.getFloat("gyro_sens", 0.85f)
        set(v) = prefs.edit().putFloat("gyro_sens", v).apply()

    var invertY: Boolean
        get() = prefs.getBoolean("invert_y", false)
        set(v) = prefs.edit().putBoolean("invert_y", v).apply()

    var shadowsEnabled: Boolean
        get() = prefs.getBoolean("shadows", true)
        set(v) = prefs.edit().putBoolean("shadows", v).apply()

    var postProcessEnabled: Boolean
        get() = prefs.getBoolean("post_process", true)
        set(v) = prefs.edit().putBoolean("post_process", v).apply()

    var renderScale: Float
        get() = prefs.getFloat("render_scale", 1.0f)
        set(v) = prefs.edit().putFloat("render_scale", v).apply()

    var targetFps: Int
        get() = prefs.getInt("target_fps", 60)
        set(v) = prefs.edit().putInt("target_fps", v).apply()

    private val defaultPositions = mapOf(
        "joystick" to ControlPos(0.14f, 0.72f, 64f),
        "sprint"   to ControlPos(0.14f, 0.43f, 28f),
        "fire"     to ControlPos(0.84f, 0.68f, 44f),
        "aim"      to ControlPos(0.73f, 0.52f, 32f),
        "reload"   to ControlPos(0.72f, 0.78f, 28f),
        "jump"     to ControlPos(0.92f, 0.48f, 28f),
        "crouch"   to ControlPos(0.83f, 0.88f, 27f),
        "weapon"   to ControlPos(0.93f, 0.28f, 28f),
        "grenade"  to ControlPos(0.64f, 0.86f, 27f),
        "interact" to ControlPos(0.50f, 0.72f, 32f)
    )

    fun getControlPosition(key: String): ControlPos {
        val def = defaultPositions[key] ?: ControlPos(0.5f, 0.5f, 28f)
        return ControlPos(
            prefs.getFloat("hud_${key}_x", def.xNorm),
            prefs.getFloat("hud_${key}_y", def.yNorm),
            def.radiusDp
        )
    }

    fun setControlPosition(key: String, xNorm: Float, yNorm: Float) {
        prefs.edit()
            .putFloat("hud_${key}_x", xNorm.coerceIn(0.06f, 0.94f))
            .putFloat("hud_${key}_y", yNorm.coerceIn(0.10f, 0.92f))
            .apply()
    }

    fun resetHudLayout() {
        val ed = prefs.edit()
        for ((k, v) in defaultPositions) {
            ed.putFloat("hud_${k}_x", v.xNorm)
            ed.putFloat("hud_${k}_y", v.yNorm)
        }
        ed.apply()
    }

    fun applyToNative() {
        NativeBridge.nativeSetSensitivity(
            touchSensitivity,
            adsMultiplier,
            gyroSensitivity,
            gyroEnabled,
            invertY
        )
        NativeBridge.nativeSetGraphicsSettings(
            shadowsEnabled,
            postProcessEnabled,
            renderScale,
            targetFps
        )
    }
}
