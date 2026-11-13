package com.tacticalstrike.game

import android.content.res.AssetManager
import android.view.Surface

object NativeBridge {
    init {
        System.loadLibrary("tacticalstrike")
    }

    data class HudState(
        var gameState: Int = 0,          // 0=Menu, 1=Playing, 2=Paused, 3=Dead, 4=Complete
        var missionIndex: Int = 0,
        var unlockedMission: Int = 0,
        var health: Float = 100f,
        var armor: Float = 50f,
        var stamina: Float = 100f,
        var weaponSlot: Int = 0,
        var magAmmo: Int = 30,
        var reserveAmmo: Int = 150,
        var grenades: Int = 2,
        var reloading: Boolean = false,
        var reloadProgress: Float = 0f,
        var aimFactor: Float = 0f,
        var hitmarkerTimer: Float = 0f,
        var lastHitKill: Boolean = false,
        var damageFlash: Float = 0f,
        var damageDirAngle: Float = 0f,
        var enemiesAlive: Int = 0,
        var kills: Int = 0,
        var elapsedSec: Float = 0f,
        var nearInteractable: Boolean = false,
        var fps: Int = 60,
        var weaponSkin: Int = 0,
        var lastHitHeadshot: Boolean = false,
        var targetInfoTimer: Float = 0f,
        var targetEnemyType: Int = 0,
        var targetEnemyHpRatio: Float = 0f,
        var playerYaw: Float = 0f,
        var objRelX: Float = 9999f,
        var objRelZ: Float = 9999f,
        var radarCount: Int = 0,
        val radarEnemies: FloatArray = FloatArray(24),
        var sprinting: Boolean = false,
        var weaponName: String = "M4A1-TAC",
        var missionName: String = "OPERATION IRON CURTAIN",
        var objectiveTitle: String = ""
    )

    private val floatBuf = FloatArray(56)
    private val strBuf = Array(3) { "" }

    fun pollHudState(out: HudState) {
        nativePollHudState(floatBuf, strBuf)
        out.gameState = floatBuf[0].toInt()
        out.missionIndex = floatBuf[1].toInt()
        out.unlockedMission = floatBuf[2].toInt()
        out.health = floatBuf[3]
        out.armor = floatBuf[4]
        out.stamina = floatBuf[5]
        out.weaponSlot = floatBuf[6].toInt()
        out.magAmmo = floatBuf[7].toInt()
        out.reserveAmmo = floatBuf[8].toInt()
        out.grenades = floatBuf[9].toInt()
        out.reloading = floatBuf[10] > 0.5f
        out.reloadProgress = floatBuf[11]
        out.aimFactor = floatBuf[12]
        out.hitmarkerTimer = floatBuf[13]
        out.lastHitKill = floatBuf[14] > 0.5f
        out.damageFlash = floatBuf[15]
        out.damageDirAngle = floatBuf[16]
        out.enemiesAlive = floatBuf[17].toInt()
        out.kills = floatBuf[18].toInt()
        out.elapsedSec = floatBuf[19]
        out.nearInteractable = floatBuf[20] > 0.5f
        out.fps = floatBuf[21].toInt()
        out.weaponSkin = floatBuf[22].toInt()
        out.lastHitHeadshot = floatBuf[23] > 0.5f
        out.targetInfoTimer = floatBuf[24]
        out.targetEnemyType = floatBuf[25].toInt()
        out.targetEnemyHpRatio = floatBuf[26]
        out.playerYaw = floatBuf[27]
        out.objRelX = floatBuf[28]
        out.objRelZ = floatBuf[29]
        out.radarCount = floatBuf[30].toInt().coerceIn(0, 8)
        for (i in 0 until 24) {
            out.radarEnemies[i] = floatBuf[31 + i]
        }
        out.sprinting = floatBuf[55] > 0.5f
        out.weaponName = strBuf[0]
        out.missionName = strBuf[1]
        out.objectiveTitle = strBuf[2]
    }

    external fun nativeInit(assetManager: AssetManager, internalDataDir: String)
    external fun nativeOnSurfaceCreated(surface: Surface)
    external fun nativeOnSurfaceChanged(width: Int, height: Int)
    external fun nativeOnSurfaceDestroyed()
    external fun nativeRenderFrame()

    external fun nativeOnPause()
    external fun nativeOnResume()
    external fun nativeSetAudioMuted(muted: Boolean)

    external fun nativeStartMission(missionIndex: Int)
    external fun nativeRestartMission()
    external fun nativeRespawnCheckpoint()
    external fun nativeSetPaused(paused: Boolean)
    external fun nativeReturnToMenu()

    external fun nativeSetMoveAxis(x: Float, y: Float)
    external fun nativeAddLookDelta(dx: Float, dy: Float)
    external fun nativeAddGyroRate(wx: Float, wy: Float, dt: Float)
    external fun nativeSetSprint(active: Boolean)
    external fun nativeSetFire(active: Boolean)
    external fun nativeSetAim(active: Boolean)
    external fun nativeToggleCrouch()
    external fun nativeTriggerJump()
    external fun nativeTriggerReload()
    external fun nativeTriggerGrenade()
    external fun nativeTriggerInteract()
    external fun nativeSwitchWeapon(slot: Int)

    external fun nativeSetSensitivity(
        touchSens: Float,
        adsMult: Float,
        gyroSens: Float,
        gyroEnabled: Boolean,
        invertY: Boolean
    )

    external fun nativeSetGraphicsSettings(
        shadows: Boolean,
        postProcess: Boolean,
        renderScale: Float,
        targetFps: Int
    )

    external fun nativeSetWeaponSkin(skinIndex: Int)

    external fun nativePollHudState(outFloats: FloatArray, outStrings: Array<String>)
}
