package com.tacticalstrike.game

import android.content.Context
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class GameSurface(context: Context) : SurfaceView(context), SurfaceHolder.Callback {

    private val renderLock = ReentrantLock()
    private val condition = renderLock.newCondition()

    private var renderThread: RenderThread? = null
    private var pendingSurface: Surface? = null
    private var pendingWidth: Int = 0
    private var pendingHeight: Int = 0
    private var hasSurface: Boolean = false
    private var surfaceChangedFlag: Boolean = false
    private var paused: Boolean = false
    private var running: Boolean = false

    init {
        holder.addCallback(this)
        isFocusable = true
        isFocusableInTouchMode = true
        keepScreenOn = true
    }

    fun startLoop() {
        renderLock.withLock {
            if (renderThread == null) {
                running = true
                renderThread = RenderThread().apply { start() }
            }
        }
    }

    fun stopLoop() {
        val thread: RenderThread?
        renderLock.withLock {
            running = false
            thread = renderThread
            condition.signalAll()
        }
        try {
            thread?.join(1500)
        } catch (_: InterruptedException) {
        }
        renderLock.withLock {
            renderThread = null
        }
    }

    fun onHostPause() {
        renderLock.withLock {
            paused = true
            NativeBridge.nativeOnPause()
            condition.signalAll()
        }
    }

    fun onHostResume() {
        renderLock.withLock {
            paused = false
            NativeBridge.nativeOnResume()
            condition.signalAll()
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        renderLock.withLock {
            pendingSurface = holder.surface
            hasSurface = true
            condition.signalAll()
        }
        startLoop()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        renderLock.withLock {
            pendingWidth = width
            pendingHeight = height
            surfaceChangedFlag = true
            condition.signalAll()
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        renderLock.withLock {
            hasSurface = false
            pendingSurface = null
            condition.signalAll()
        }
    }

    private inner class RenderThread : Thread("TS-RenderThread") {
        override fun run() {
            var eglAttached = false
            while (true) {
                var surfaceToAttach: Surface? = null
                var doDestroy = false
                var resizeW = 0
                var resizeH = 0
                var doResize = false

                renderLock.withLock {
                    while (running && (!hasSurface || paused)) {
                        if (eglAttached && !hasSurface) {
                            doDestroy = true
                            break
                        }
                        condition.await()
                    }
                    if (!running) {
                        if (eglAttached) doDestroy = true
                    } else if (hasSurface && !eglAttached && pendingSurface != null) {
                        surfaceToAttach = pendingSurface
                    } else if (surfaceChangedFlag && eglAttached) {
                        resizeW = pendingWidth
                        resizeH = pendingHeight
                        doResize = true
                        surfaceChangedFlag = false
                    }
                }

                if (doDestroy) {
                    NativeBridge.nativeOnSurfaceDestroyed()
                    eglAttached = false
                }

                if (!running) break

                if (surfaceToAttach != null) {
                    NativeBridge.nativeOnSurfaceCreated(surfaceToAttach!!)
                    eglAttached = true
                    if (pendingWidth > 0 && pendingHeight > 0) {
                        NativeBridge.nativeOnSurfaceChanged(pendingWidth, pendingHeight)
                    }
                } else if (doResize) {
                    NativeBridge.nativeOnSurfaceChanged(resizeW, resizeH)
                }

                if (eglAttached && !paused) {
                    NativeBridge.nativeRenderFrame()
                }
            }
        }
    }
}
