package com.tacticalstrike.game

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin
import java.util.Locale

class TouchController(
    context: Context,
    private val settings: SettingsManager,
    private val onPauseClicked: () -> Unit,
    private val onLayoutSaved: () -> Unit
) : View(context) {

    data class ControlButton(
        val key: String,
        val label: String,
        var cx: Float = 0f,
        var cy: Float = 0f,
        var radius: Float = 60f,
        var pointerId: Int = -1,
        var active: Boolean = false
    )

    private val density = resources.displayMetrics.density
    private val hud = NativeBridge.HudState()

    var isEditingLayout = false
        set(value) {
            field = value
            editingDragKey = null
            editingDragPointer = -1
            invalidate()
        }

    private var editingDragKey: String? = null
    private var editingDragPointer: Int = -1

    private val buttons = listOf(
        ControlButton("sprint", "RUN"),
        ControlButton("fire", "FIRE"),
        ControlButton("aim", "ADS"),
        ControlButton("reload", "RLD"),
        ControlButton("jump", "JMP"),
        ControlButton("crouch", "CRH"),
        ControlButton("weapon", "WPN"),
        ControlButton("grenade", "GRN"),
        ControlButton("interact", "HACK")
    )

    private var joyBaseX = 0f
    private var joyBaseY = 0f
    private var joyRadius = 110f
    private var joyKnobX = 0f
    private var joyKnobY = 0f
    private var joyPointerId = -1

    private var lookPointerId = -1
    private var lastLookX = 0f
    private var lastLookY = 0f

    private val pauseRect = RectF()
    private val saveLayoutRect = RectF()
    private val resetLayoutRect = RectF()

    // Paints
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val leftTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.LEFT
        isFakeBoldText = true
    }
    private val rightTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.RIGHT
        isFakeBoldText = true
    }

    fun updateHudSnapshot(snapshot: NativeBridge.HudState) {
        hud.gameState = snapshot.gameState
        hud.missionIndex = snapshot.missionIndex
        hud.health = snapshot.health
        hud.armor = snapshot.armor
        hud.stamina = snapshot.stamina
        hud.weaponSlot = snapshot.weaponSlot
        hud.magAmmo = snapshot.magAmmo
        hud.reserveAmmo = snapshot.reserveAmmo
        hud.grenades = snapshot.grenades
        hud.reloading = snapshot.reloading
        hud.reloadProgress = snapshot.reloadProgress
        hud.aimFactor = snapshot.aimFactor
        hud.hitmarkerTimer = snapshot.hitmarkerTimer
        hud.lastHitKill = snapshot.lastHitKill
        hud.damageFlash = snapshot.damageFlash
        hud.damageDirAngle = snapshot.damageDirAngle
        hud.enemiesAlive = snapshot.enemiesAlive
        hud.kills = snapshot.kills
        hud.elapsedSec = snapshot.elapsedSec
        hud.nearInteractable = snapshot.nearInteractable
        hud.fps = snapshot.fps
        hud.weaponName = snapshot.weaponName
        hud.missionName = snapshot.missionName
        hud.objectiveTitle = snapshot.objectiveTitle
        invalidate()
    }

    fun refreshControlPositions() {
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return

        val jPos = settings.getControlPosition("joystick")
        joyBaseX = jPos.xNorm * w
        joyBaseY = jPos.yNorm * h
        joyRadius = jPos.radiusDp * density
        if (joyPointerId == -1) {
            joyKnobX = joyBaseX
            joyKnobY = joyBaseY
        }

        for (b in buttons) {
            val p = settings.getControlPosition(b.key)
            b.cx = p.xNorm * w
            b.cy = p.yNorm * h
            b.radius = p.radiusDp * density
        }

        pauseRect.set(w - 64f * density, 14f * density, w - 16f * density, 52f * density)
        saveLayoutRect.set(w * 0.5f - 145f * density, 18f * density, w * 0.5f - 10f * density, 56f * density)
        resetLayoutRect.set(w * 0.5f + 10f * density, 18f * density, w * 0.5f + 145f * density, 56f * density)
        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        refreshControlPositions()
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (isEditingLayout) {
            return handleLayoutEditTouch(event)
        }
        if (hud.gameState != 1) {
            return false
        }

        val action = event.actionMasked
        val idx = event.actionIndex
        val pointerId = event.getPointerId(idx)
        val x = event.getX(idx)
        val y = event.getY(idx)

        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                if (pauseRect.contains(x, y)) {
                    onPauseClicked()
                    return true
                }
                val hitBtn = findHitButton(x, y)
                if (hitBtn != null) {
                    hitBtn.pointerId = pointerId
                    hitBtn.active = true
                    onButtonStateChanged(hitBtn, true)
                    invalidate()
                    return true
                }
                // Left half -> virtual joystick if free
                if (x < width * 0.45f && joyPointerId == -1) {
                    joyPointerId = pointerId
                    updateJoystick(x, y)
                    invalidate()
                    return true
                }
                // Right half (or fallback) -> camera look
                if (lookPointerId == -1) {
                    lookPointerId = pointerId
                    lastLookX = x
                    lastLookY = y
                    return true
                }
            }

            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    val pId = event.getPointerId(i)
                    val px = event.getX(i)
                    val py = event.getY(i)
                    if (pId == joyPointerId) {
                        updateJoystick(px, py)
                    } else if (pId == lookPointerId) {
                        val dx = px - lastLookX
                        val dy = py - lastLookY
                        lastLookX = px
                        lastLookY = py
                        NativeBridge.nativeAddLookDelta(dx, dy)
                    }
                }
                invalidate()
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                if (action == MotionEvent.ACTION_CANCEL) {
                    resetAllTouches()
                    return true
                }
                if (pointerId == joyPointerId) {
                    joyPointerId = -1
                    joyKnobX = joyBaseX
                    joyKnobY = joyBaseY
                    NativeBridge.nativeSetMoveAxis(0f, 0f)
                    NativeBridge.nativeSetSprint(isButtonHeld("sprint"))
                    invalidate()
                } else if (pointerId == lookPointerId) {
                    lookPointerId = -1
                } else {
                    for (b in buttons) {
                        if (b.pointerId == pointerId) {
                            b.pointerId = -1
                            b.active = false
                            onButtonStateChanged(b, false)
                            invalidate()
                        }
                    }
                }
            }
        }
        return true
    }

    private fun handleLayoutEditTouch(event: MotionEvent): Boolean {
        val action = event.actionMasked
        val idx = event.actionIndex
        val pId = event.getPointerId(idx)
        val x = event.getX(idx)
        val y = event.getY(idx)

        when (action) {
            MotionEvent.ACTION_DOWN -> {
                if (saveLayoutRect.contains(x, y)) {
                    isEditingLayout = false
                    onLayoutSaved()
                    return true
                }
                if (resetLayoutRect.contains(x, y)) {
                    settings.resetHudLayout()
                    refreshControlPositions()
                    return true
                }
                if (hypot(x - joyBaseX, y - joyBaseY) <= joyRadius * 1.15f) {
                    editingDragKey = "joystick"
                    editingDragPointer = pId
                    return true
                }
                val b = findHitButton(x, y)
                if (b != null) {
                    editingDragKey = b.key
                    editingDragPointer = pId
                    return true
                }
            }
            MotionEvent.ACTION_MOVE -> {
                val key = editingDragKey ?: return true
                for (i in 0 until event.pointerCount) {
                    if (event.getPointerId(i) == editingDragPointer) {
                        val nx = event.getX(i) / width.toFloat()
                        val ny = event.getY(i) / height.toFloat()
                        settings.setControlPosition(key, nx, ny)
                        refreshControlPositions()
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                editingDragKey = null
                editingDragPointer = -1
            }
        }
        return true
    }

    private fun findHitButton(x: Float, y: Float): ControlButton? {
        for (b in buttons) {
            if (b.key == "interact" && !hud.nearInteractable && !isEditingLayout) continue
            val dist = hypot(x - b.cx, y - b.cy)
            if (dist <= b.radius * 1.15f) return b
        }
        return null
    }

    private fun isButtonHeld(key: String): Boolean {
        return buttons.any { it.key == key && it.active }
    }

    private fun updateJoystick(px: Float, py: Float) {
        val dx = px - joyBaseX
        val dy = py - joyBaseY
        val dist = hypot(dx, dy)
        val maxR = joyRadius
        val clamped = min(dist, maxR)
        val angle = atan2(dy, dx)
        joyKnobX = joyBaseX + cos(angle) * clamped
        joyKnobY = joyBaseY + sin(angle) * clamped

        val nx = (joyKnobX - joyBaseX) / maxR
        val ny = -(joyKnobY - joyBaseY) / maxR
        NativeBridge.nativeSetMoveAxis(nx, ny)
        // Auto-sprint if pushed strongly forward or sprint button held
        val autoSprint = ny > 0.88f || isButtonHeld("sprint")
        NativeBridge.nativeSetSprint(autoSprint)
    }

    private fun onButtonStateChanged(b: ControlButton, down: Boolean) {
        when (b.key) {
            "fire" -> NativeBridge.nativeSetFire(down)
            "aim" -> NativeBridge.nativeSetAim(down)
            "sprint" -> NativeBridge.nativeSetSprint(down)
            "crouch" -> if (down) NativeBridge.nativeToggleCrouch()
            "jump" -> if (down) NativeBridge.nativeTriggerJump()
            "reload" -> if (down) NativeBridge.nativeTriggerReload()
            "grenade" -> if (down) NativeBridge.nativeTriggerGrenade()
            "interact" -> if (down) NativeBridge.nativeTriggerInteract()
            "weapon" -> if (down) NativeBridge.nativeSwitchWeapon(99)
        }
    }

    private fun resetAllTouches() {
        joyPointerId = -1
        lookPointerId = -1
        joyKnobX = joyBaseX
        joyKnobY = joyBaseY
        NativeBridge.nativeSetMoveAxis(0f, 0f)
        NativeBridge.nativeSetSprint(false)
        NativeBridge.nativeSetFire(false)
        NativeBridge.nativeSetAim(false)
        for (b in buttons) {
            b.pointerId = -1
            b.active = false
        }
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (isEditingLayout) {
            drawLayoutEditor(canvas)
            return
        }
        if (hud.gameState != 1) return

        val w = width.toFloat()
        val h = height.toFloat()

        // 1. Directional Damage Arc & Screen Pulse
        if (hud.damageFlash > 0.02f) {
            val alpha = (hud.damageFlash * 110f).toInt().coerceIn(0, 140)
            strokePaint.color = Color.argb(alpha, 239, 68, 68)
            strokePaint.strokeWidth = 10f * density
            val arcR = min(w, h) * 0.30f
            val deg = Math.toDegrees(hud.damageDirAngle.toDouble()).toFloat() - 90f
            val arcRect = RectF(w * 0.5f - arcR, h * 0.5f - arcR, w * 0.5f + arcR, h * 0.5f + arcR)
            canvas.drawArc(arcRect, deg - 22f, 44f, false, strokePaint)
        }

        // 2. Dynamic Crosshair & Hitmarker
        drawCrosshair(canvas, w * 0.5f, h * 0.5f)

        // 3. Top Tactical Bar: Mission, Objective, Hostiles, Timer, Pause
        drawTopHud(canvas, w)

        // 4. Bottom-Left Player Vitals (HP, Armor, Stamina)
        drawVitals(canvas, h)

        // 5. Bottom-Right Weapon & Ammo
        drawWeaponHud(canvas, w, h)

        // 6. Virtual Movement Joystick & Action Buttons
        drawTouchControls(canvas)
    }

    private fun drawCrosshair(canvas: Canvas, cx: Float, cy: Float) {
        val spreadPx = (10f - hud.aimFactor * 6f) * density
        val lineLen = (7f - hud.aimFactor * 2f) * density
        strokePaint.color = Color.argb(215, 245, 248, 250)
        strokePaint.strokeWidth = 2.0f * density

        if (hud.aimFactor < 0.75f) {
            canvas.drawLine(cx - spreadPx - lineLen, cy, cx - spreadPx, cy, strokePaint)
            canvas.drawLine(cx + spreadPx, cy, cx + spreadPx + lineLen, cy, strokePaint)
            canvas.drawLine(cx, cy - spreadPx - lineLen, cx, cy - spreadPx, strokePaint)
            canvas.drawLine(cx, cy + spreadPx, cx, cy + spreadPx + lineLen, strokePaint)
        }
        fillPaint.color = if (hud.aimFactor > 0.5f) Color.rgb(239, 68, 68) else Color.WHITE
        canvas.drawCircle(cx, cy, 2.2f * density, fillPaint)

        // Hitmarker
        if (hud.hitmarkerTimer > 0.005f) {
            strokePaint.color = if (hud.lastHitKill) Color.rgb(239, 68, 68) else Color.WHITE
            strokePaint.strokeWidth = 2.6f * density
            val r1 = 6f * density
            val r2 = 15f * density
            canvas.drawLine(cx - r2, cy - r2, cx - r1, cy - r1, strokePaint)
            canvas.drawLine(cx + r1, cy - r1, cx + r2, cy - r2, strokePaint)
            canvas.drawLine(cx - r2, cy + r2, cx - r1, cy + r1, strokePaint)
            canvas.drawLine(cx + r1, cy + r1, cx + r2, cy + r2, strokePaint)
        }

        // Reload Progress Ring
        if (hud.reloading) {
            val rr = 22f * density
            strokePaint.color = Color.argb(180, 245, 158, 11)
            strokePaint.strokeWidth = 3.2f * density
            val rect = RectF(cx - rr, cy - rr, cx + rr, cy + rr)
            canvas.drawArc(rect, -90f, hud.reloadProgress * 360f, false, strokePaint)
        }
    }

    private fun drawTopHud(canvas: Canvas, w: Float) {
        // Objective Panel (Top-Left)
        fillPaint.color = Color.argb(145, 12, 16, 22)
        val panel = RectF(12f * density, 10f * density, 246f * density, 58f * density)
        canvas.drawRoundRect(panel, 6f * density, 6f * density, fillPaint)
        strokePaint.color = Color.argb(160, 245, 158, 11)
        strokePaint.strokeWidth = 1.2f * density
        canvas.drawLine(12f * density, 12f * density, 12f * density, 56f * density, strokePaint)

        leftTextPaint.color = Color.rgb(245, 158, 11)
        leftTextPaint.textSize = 10.5f * density
        canvas.drawText(hud.missionName, 20f * density, 26f * density, leftTextPaint)

        leftTextPaint.color = Color.WHITE
        leftTextPaint.textSize = 12f * density
        canvas.drawText(hud.objectiveTitle, 20f * density, 46f * density, leftTextPaint)

        // Status Pill-Free Tactical Strip (Top-Center)
        val mins = (hud.elapsedSec / 60f).toInt()
        val secs = (hud.elapsedSec % 60f).toInt()
        textPaint.color = Color.argb(230, 226, 232, 240)
        textPaint.textSize = 12.5f * density
        val stripCenter = (254f * density + (w - 72f * density)) * 0.5f
        canvas.drawText(
            String.format(Locale.US, "HOSTILES: %d   |   KILLS: %d   |   %02d:%02d   |   %d FPS", hud.enemiesAlive, hud.kills, mins, secs, hud.fps),
            stripCenter,
            29f * density,
            textPaint
        )

        // Pause Button (Top-Right)
        fillPaint.color = Color.argb(165, 15, 23, 42)
        canvas.drawRoundRect(pauseRect, 6f * density, 6f * density, fillPaint)
        strokePaint.color = Color.argb(180, 148, 163, 184)
        strokePaint.strokeWidth = 1.5f * density
        canvas.drawRoundRect(pauseRect, 6f * density, 6f * density, strokePaint)
        textPaint.color = Color.WHITE
        textPaint.textSize = 14f * density
        canvas.drawText("II", pauseRect.centerX(), pauseRect.centerY() + 5f * density, textPaint)
    }

    private fun drawVitals(canvas: Canvas, h: Float) {
        val bx = 16f * density
        val by = h - 46f * density
        val bw = 164f * density

        fillPaint.color = Color.argb(145, 12, 16, 22)
        canvas.drawRoundRect(RectF(bx - 6f * density, by - 20f * density, bx + bw + 8f * density, by + 35f * density), 6f * density, 6f * density, fillPaint)

        leftTextPaint.color = if (hud.health <= 30f) Color.rgb(239, 68, 68) else Color.WHITE
        leftTextPaint.textSize = 13.5f * density
        canvas.drawText("HP ${hud.health.toInt()}", bx, by - 3f * density, leftTextPaint)

        rightTextPaint.color = Color.rgb(56, 189, 248)
        rightTextPaint.textSize = 12.5f * density
        canvas.drawText("ARM ${hud.armor.toInt()}", bx + bw, by - 3f * density, rightTextPaint)

        // Health bar
        fillPaint.color = Color.argb(180, 30, 41, 59)
        canvas.drawRect(bx, by + 3f * density, bx + bw, by + 11f * density, fillPaint)
        fillPaint.color = if (hud.health <= 30f) Color.rgb(239, 68, 68) else Color.rgb(34, 197, 94)
        canvas.drawRect(bx, by + 3f * density, bx + bw * (hud.health / 100f).coerceIn(0f, 1f), by + 11f * density, fillPaint)

        // Armor bar
        fillPaint.color = Color.argb(180, 30, 41, 59)
        canvas.drawRect(bx, by + 15f * density, bx + bw, by + 22f * density, fillPaint)
        fillPaint.color = Color.rgb(56, 189, 248)
        canvas.drawRect(bx, by + 15f * density, bx + bw * (hud.armor / 100f).coerceIn(0f, 1f), by + 22f * density, fillPaint)

        // Stamina strip
        fillPaint.color = Color.rgb(245, 158, 11)
        canvas.drawRect(bx, by + 26f * density, bx + bw * (hud.stamina / 100f).coerceIn(0f, 1f), by + 30f * density, fillPaint)
    }

    private fun drawWeaponHud(canvas: Canvas, w: Float, h: Float) {
        val rx = w - 16f * density
        val by = h - 14f * density

        fillPaint.color = Color.argb(145, 12, 16, 22)
        canvas.drawRoundRect(RectF(rx - 168f * density, by - 55f * density, rx + 4f * density, by + 4f * density), 6f * density, 6f * density, fillPaint)

        rightTextPaint.color = Color.rgb(245, 158, 11)
        rightTextPaint.textSize = 11.5f * density
        canvas.drawText(hud.weaponName, rx - 6f * density, by - 36f * density, rightTextPaint)

        rightTextPaint.color = if (hud.magAmmo <= 5) Color.rgb(239, 68, 68) else Color.WHITE
        rightTextPaint.textSize = 23f * density
        canvas.drawText("${hud.magAmmo} / ${hud.reserveAmmo}", rx - 6f * density, by - 11f * density, rightTextPaint)

        leftTextPaint.color = Color.rgb(203, 213, 225)
        leftTextPaint.textSize = 11f * density
        canvas.drawText("FRAG: ${hud.grenades}", rx - 160f * density, by - 11f * density, leftTextPaint)
    }

    private fun drawTouchControls(canvas: Canvas) {
        // Joystick base
        fillPaint.color = Color.argb(65, 15, 23, 42)
        canvas.drawCircle(joyBaseX, joyBaseY, joyRadius, fillPaint)
        strokePaint.color = Color.argb(150, 203, 213, 225)
        strokePaint.strokeWidth = 2f * density
        canvas.drawCircle(joyBaseX, joyBaseY, joyRadius, strokePaint)

        // Joystick knob
        fillPaint.color = Color.argb(175, 245, 158, 11)
        canvas.drawCircle(joyKnobX, joyKnobY, joyRadius * 0.38f, fillPaint)

        for (b in buttons) {
            if (b.key == "interact" && !hud.nearInteractable) continue
            val isFire = (b.key == "fire")
            val isInteract = (b.key == "interact")
            fillPaint.color = when {
                b.active -> Color.argb(200, 245, 158, 11)
                isFire -> Color.argb(135, 220, 38, 38)
                isInteract -> Color.argb(165, 14, 165, 233)
                else -> Color.argb(105, 15, 23, 42)
            }
            canvas.drawCircle(b.cx, b.cy, b.radius, fillPaint)
            strokePaint.color = Color.argb(190, 226, 232, 240)
            strokePaint.strokeWidth = 1.8f * density
            canvas.drawCircle(b.cx, b.cy, b.radius, strokePaint)

            textPaint.color = Color.WHITE
            textPaint.textSize = 12f * density
            val label = if (b.key == "grenade") "GRN(${hud.grenades})" else b.label
            canvas.drawText(label, b.cx, b.cy + 4f * density, textPaint)
        }
    }

    private fun drawLayoutEditor(canvas: Canvas) {
        fillPaint.color = Color.argb(160, 8, 12, 18)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), fillPaint)

        textPaint.color = Color.rgb(245, 158, 11)
        textPaint.textSize = 15f * density
        canvas.drawText("DRAG CONTROLS TO CUSTOMIZE HUD LAYOUT", width * 0.5f, 84f * density, textPaint)

        fillPaint.color = Color.rgb(22, 163, 74)
        canvas.drawRoundRect(saveLayoutRect, 6f * density, 6f * density, fillPaint)
        textPaint.color = Color.WHITE
        textPaint.textSize = 13f * density
        canvas.drawText("SAVE LAYOUT", saveLayoutRect.centerX(), saveLayoutRect.centerY() + 4f * density, textPaint)

        fillPaint.color = Color.rgb(71, 85, 105)
        canvas.drawRoundRect(resetLayoutRect, 6f * density, 6f * density, fillPaint)
        canvas.drawText("RESET DEFAULT", resetLayoutRect.centerX(), resetLayoutRect.centerY() + 4f * density, textPaint)

        // Draw all controls including interact
        fillPaint.color = Color.argb(110, 245, 158, 11)
        canvas.drawCircle(joyBaseX, joyBaseY, joyRadius, fillPaint)
        textPaint.textSize = 12f * density
        canvas.drawText("MOVE", joyBaseX, joyBaseY + 4f * density, textPaint)

        for (b in buttons) {
            fillPaint.color = Color.argb(140, 56, 189, 248)
            canvas.drawCircle(b.cx, b.cy, b.radius, fillPaint)
            strokePaint.color = Color.WHITE
            strokePaint.strokeWidth = 2f * density
            canvas.drawCircle(b.cx, b.cy, b.radius, strokePaint)
            canvas.drawText(b.label, b.cx, b.cy + 4f * density, textPaint)
        }
    }
}
