package com.tacticalstrike.game

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.util.Locale

class MainActivity : AppCompatActivity(), SensorEventListener {

    private lateinit var settings: SettingsManager
    private lateinit var gameSurface: GameSurface
    private lateinit var touchController: TouchController
    private lateinit var overlayContainer: FrameLayout

    private lateinit var sensorManager: SensorManager
    private var gyroSensor: Sensor? = null
    private var lastGyroTimestampNs: Long = 0L

    private lateinit var audioManager: AudioManager
    private var audioFocusRequest: AudioFocusRequest? = null

    private val hudHandler = Handler(Looper.getMainLooper())
    private val hudState = NativeBridge.HudState()
    private var lastShownOverlayState = -1
    private var settingsVisible = false

    private val hudPollRunnable = object : Runnable {
        override fun run() {
            NativeBridge.pollHudState(hudState)
            touchController.updateHudSnapshot(hudState)
            if (!settingsVisible && !touchController.isEditingLayout && hudState.gameState != lastShownOverlayState) {
                lastShownOverlayState = hudState.gameState
                rebuildOverlayForState(hudState.gameState)
            }
            hudHandler.postDelayed(this, 33L)
        }
    }

    private val audioFocusListener = AudioManager.OnAudioFocusChangeListener { focusChange ->
        val mute = focusChange == AudioManager.AUDIOFOCUS_LOSS ||
            focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT
        NativeBridge.nativeSetAudioMuted(mute)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        applyImmersiveMode()

        settings = SettingsManager(this)
        NativeBridge.nativeInit(assets, filesDir.absolutePath)
        settings.applyToNative()

        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        gyroSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager

        val root = FrameLayout(this)
        gameSurface = GameSurface(this)
        touchController = TouchController(
            context = this,
            settings = settings,
            onPauseClicked = { NativeBridge.nativeSetPaused(true) },
            onLayoutSaved = {
                lastShownOverlayState = -1
                rebuildOverlayForState(hudState.gameState)
            }
        )
        overlayContainer = FrameLayout(this)

        root.addView(gameSurface, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        root.addView(touchController, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        root.addView(overlayContainer, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))
        setContentView(root)

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (touchController.isEditingLayout) {
                    touchController.isEditingLayout = false
                    lastShownOverlayState = -1
                    rebuildOverlayForState(hudState.gameState)
                    return
                }
                if (settingsVisible) {
                    settingsVisible = false
                    lastShownOverlayState = -1
                    rebuildOverlayForState(hudState.gameState)
                    return
                }
                when (hudState.gameState) {
                    1 -> NativeBridge.nativeSetPaused(true)
                    2 -> NativeBridge.nativeSetPaused(false)
                    3, 4 -> NativeBridge.nativeReturnToMenu()
                    else -> finish()
                }
            }
        })
    }

    override fun onResume() {
        super.onResume()
        applyImmersiveMode()
        requestAudioFocus()
        gameSurface.onHostResume()
        gyroSensor?.let {
            lastGyroTimestampNs = 0L
            sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME)
        }
        hudHandler.post(hudPollRunnable)
    }

    override fun onPause() {
        super.onPause()
        hudHandler.removeCallbacks(hudPollRunnable)
        sensorManager.unregisterListener(this)
        gameSurface.onHostPause()
        abandonAudioFocus()
    }

    override fun onDestroy() {
        super.onDestroy()
        gameSurface.stopLoop()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) applyImmersiveMode()
    }

    override fun onSensorChanged(event: SensorEvent) {
        if (event.sensor.type != Sensor.TYPE_GYROSCOPE) return
        if (lastGyroTimestampNs != 0L && hudState.gameState == 1) {
            val dt = (event.timestamp - lastGyroTimestampNs) * 1e-9f
            if (dt in 0.0005f..0.1f) {
                NativeBridge.nativeAddGyroRate(event.values[0], event.values[1], dt)
            }
        }
        lastGyroTimestampNs = event.timestamp
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    private fun applyImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    private fun requestAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val req = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build()
                )
                .setOnAudioFocusChangeListener(audioFocusListener)
                .build()
            audioFocusRequest = req
            audioManager.requestAudioFocus(req)
        } else {
            @Suppress("DEPRECATION")
            audioManager.requestAudioFocus(audioFocusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
        }
    }

    private fun abandonAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            audioFocusRequest?.let { audioManager.abandonAudioFocusRequest(it) }
        } else {
            @Suppress("DEPRECATION")
            audioManager.abandonAudioFocus(audioFocusListener)
        }
    }

    private fun rebuildOverlayForState(state: Int) {
        overlayContainer.removeAllViews()
        if (settingsVisible) {
            showSettingsOverlay()
            return
        }
        when (state) {
            0 -> showMainMenu()
            2 -> showPauseMenu()
            3 -> showDeathMenu()
            4 -> showVictoryMenu()
            else -> Unit // Playing -> TouchController renders HUD
        }
    }

    private fun makeCardContainer(): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setBackgroundColor(Color.argb(215, 13, 17, 23))
            val pad = (24 * resources.displayMetrics.density).toInt()
            setPadding(pad, pad, pad, pad)
        }
    }

    private fun makeButton(label: String, bgColor: Int, onClick: () -> Unit): Button {
        val d = resources.displayMetrics.density
        return Button(this).apply {
            text = label
            setTextColor(Color.WHITE)
            setBackgroundColor(bgColor)
            typeface = Typeface.DEFAULT_BOLD
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(
                (260 * d).toInt(),
                (44 * d).toInt()
            ).apply { topMargin = (8 * d).toInt() }
            setOnClickListener { onClick() }
        }
    }

    private fun makeTitle(title: String, color: Int = Color.rgb(245, 158, 11)): TextView {
        return TextView(this).apply {
            text = title
            setTextColor(color)
            typeface = Typeface.DEFAULT_BOLD
            textSize = 22f
            gravity = Gravity.CENTER
        }
    }

    private fun showMainMenu() {
        val d = resources.displayMetrics.density
        val bg = FrameLayout(this).apply {
            setBackgroundColor(Color.argb(145, 8, 12, 18))
        }
        val card = makeCardContainer()
        card.addView(makeTitle("TACTICAL STRIKE: ZERO HOUR"))
        card.addView(TextView(this).apply {
            text = "NATIVE C++17 / OPENGL ES 3.0 CAMPAIGN"
            setTextColor(Color.rgb(148, 163, 184))
            textSize = 11f
            gravity = Gravity.CENTER
            setPadding(0, (4 * d).toInt(), 0, (12 * d).toInt())
        })

        val missions = arrayOf(
            "MISSION 1: OPERATION IRON CURTAIN",
            "MISSION 2: REACTOR LOCKDOWN",
            "MISSION 3: ZERO HOUR EXTRACTION"
        )
        for (i in missions.indices) {
            val unlocked = i <= hudState.unlockedMission
            val label = if (unlocked) missions[i] else "${missions[i]} [LOCKED]"
            val col = if (unlocked) Color.rgb(217, 119, 6) else Color.rgb(51, 65, 85)
            card.addView(makeButton(label, col) {
                if (unlocked) {
                    NativeBridge.nativeStartMission(i)
                }
            }.apply { isEnabled = unlocked })
        }

        card.addView(makeButton("SETTINGS & SENSITIVITY", Color.rgb(30, 41, 59)) {
            settingsVisible = true
            rebuildOverlayForState(0)
        })
        card.addView(makeButton("CUSTOMIZE HUD LAYOUT", Color.rgb(30, 41, 59)) {
            overlayContainer.removeAllViews()
            touchController.isEditingLayout = true
        })

        val lp = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.CENTER
        )
        bg.addView(card, lp)
        overlayContainer.addView(bg)
    }

    private fun showPauseMenu() {
        val bg = FrameLayout(this).apply { setBackgroundColor(Color.argb(175, 8, 12, 18)) }
        val card = makeCardContainer()
        card.addView(makeTitle("MISSION PAUSED"))
        card.addView(makeButton("RESUME MISSION", Color.rgb(22, 163, 74)) {
            NativeBridge.nativeSetPaused(false)
        })
        card.addView(makeButton("RESTART MISSION", Color.rgb(217, 119, 6)) {
            NativeBridge.nativeRestartMission()
        })
        card.addView(makeButton("SETTINGS", Color.rgb(30, 41, 59)) {
            settingsVisible = true
            rebuildOverlayForState(2)
        })
        card.addView(makeButton("CUSTOMIZE HUD LAYOUT", Color.rgb(30, 41, 59)) {
            overlayContainer.removeAllViews()
            touchController.isEditingLayout = true
        })
        card.addView(makeButton("RETURN TO MAIN MENU", Color.rgb(127, 29, 29)) {
            NativeBridge.nativeReturnToMenu()
        })
        bg.addView(card, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER
        ))
        overlayContainer.addView(bg)
    }

    private fun showDeathMenu() {
        val bg = FrameLayout(this).apply { setBackgroundColor(Color.argb(190, 45, 10, 10)) }
        val card = makeCardContainer()
        card.addView(makeTitle("OPERATOR DOWN", Color.rgb(239, 68, 68)))
        card.addView(makeButton("RESPAWN AT CHECKPOINT", Color.rgb(217, 119, 6)) {
            NativeBridge.nativeRespawnCheckpoint()
        })
        card.addView(makeButton("RESTART MISSION", Color.rgb(51, 65, 85)) {
            NativeBridge.nativeRestartMission()
        })
        card.addView(makeButton("MAIN MENU", Color.rgb(30, 41, 59)) {
            NativeBridge.nativeReturnToMenu()
        })
        bg.addView(card, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER
        ))
        overlayContainer.addView(bg)
    }

    private fun showVictoryMenu() {
        val bg = FrameLayout(this).apply { setBackgroundColor(Color.argb(185, 8, 22, 16)) }
        val card = makeCardContainer()
        card.addView(makeTitle("MISSION ACCOMPLISHED", Color.rgb(34, 197, 94)))
        val mins = (hudState.elapsedSec / 60f).toInt()
        val secs = (hudState.elapsedSec % 60f).toInt()
        card.addView(TextView(this).apply {
            text = String.format(Locale.US, "TIME: %02d:%02d   |   HOSTILES ELIMINATED: %d", mins, secs, hudState.kills)
            setTextColor(Color.WHITE)
            textSize = 13f
            gravity = Gravity.CENTER
            setPadding(0, 12, 0, 16)
        })
        if (hudState.missionIndex < 2) {
            card.addView(makeButton("NEXT MISSION", Color.rgb(22, 163, 74)) {
                NativeBridge.nativeStartMission(hudState.missionIndex + 1)
            })
        }
        card.addView(makeButton("REPLAY MISSION", Color.rgb(217, 119, 6)) {
            NativeBridge.nativeRestartMission()
        })
        card.addView(makeButton("MAIN MENU", Color.rgb(30, 41, 59)) {
            NativeBridge.nativeReturnToMenu()
        })
        bg.addView(card, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER
        ))
        overlayContainer.addView(bg)
    }

    private fun showSettingsOverlay() {
        val d = resources.displayMetrics.density
        val bg = FrameLayout(this).apply { setBackgroundColor(Color.argb(200, 8, 12, 18)) }
        val card = makeCardContainer()
        card.addView(makeTitle("TACTICAL SETTINGS"))

        fun addSlider(label: String, progress: Int, max: Int, onChange: (Int) -> Unit) {
            val tv = TextView(this).apply {
                text = label
                setTextColor(Color.rgb(226, 232, 240))
                textSize = 12f
                setPadding(0, (8 * d).toInt(), 0, 0)
            }
            val sb = SeekBar(this).apply {
                this.max = max
                this.progress = progress.coerceIn(0, max)
                layoutParams = LinearLayout.LayoutParams((280 * d).toInt(), LinearLayout.LayoutParams.WRAP_CONTENT)
                setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                    override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) {
                        if (fromUser) onChange(p)
                    }
                    override fun onStartTrackingTouch(s: SeekBar?) {}
                    override fun onStopTrackingTouch(s: SeekBar?) {}
                })
            }
            card.addView(tv)
            card.addView(sb)
        }

        fun addToggle(label: String, checked: Boolean, onToggle: (Boolean) -> Unit) {
            val sw = SwitchCompat(this).apply {
                text = label
                setTextColor(Color.WHITE)
                isChecked = checked
                layoutParams = LinearLayout.LayoutParams((280 * d).toInt(), LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                    topMargin = (6 * d).toInt()
                }
                setOnCheckedChangeListener { _, isChecked ->
                    onToggle(isChecked)
                    settings.applyToNative()
                }
            }
            card.addView(sw)
        }

        addSlider("Touch Look Sensitivity", ((settings.touchSensitivity - 0.001f) / 0.0001f).toInt(), 90) { p ->
            settings.touchSensitivity = 0.001f + p * 0.0001f
            settings.applyToNative()
        }
        addSlider("Gyroscope Sensitivity", (settings.gyroSensitivity * 50f).toInt(), 100) { p ->
            settings.gyroSensitivity = p / 50f
            settings.applyToNative()
        }

        addToggle("Gyroscope Aiming", settings.gyroEnabled) { settings.gyroEnabled = it }
        addToggle("Invert Look Y-Axis", settings.invertY) { settings.invertY = it }
        addToggle("Real-Time Shadow Mapping", settings.shadowsEnabled) { settings.shadowsEnabled = it }
        addToggle("Post-Processing (ACES & Vignette)", settings.postProcessEnabled) { settings.postProcessEnabled = it }
        addToggle("60 FPS Target (Off = 30 FPS Battery Saver)", settings.targetFps >= 60) {
            settings.targetFps = if (it) 60 else 30
            settings.renderScale = if (it) 1.0f else 0.85f
        }

        val skinNames = arrayOf("CARBON OPS", "DESERT TIGER", "ARCTIC WOLF", "CRIMSON HAZARD")
        lateinit var skinBtn: Button
        skinBtn = makeButton("WEAPON SKIN: ${skinNames[settings.weaponSkin]}", Color.rgb(51, 65, 85)) {
            settings.weaponSkin = (settings.weaponSkin + 1) % skinNames.size
            settings.applyToNative()
            skinBtn.text = "WEAPON SKIN: ${skinNames[settings.weaponSkin]}"
        }
        card.addView(skinBtn)

        card.addView(makeButton("CLOSE SETTINGS", Color.rgb(217, 119, 6)) {
            settingsVisible = false
            lastShownOverlayState = -1
            rebuildOverlayForState(hudState.gameState)
        })

        val scroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            addView(card)
        }
        bg.addView(scroll, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER
        ))
        overlayContainer.addView(bg)
    }
}
