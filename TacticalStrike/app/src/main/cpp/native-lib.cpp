#include "GameEngine.h"
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <memory>

namespace {
std::unique_ptr<ts::GameEngine> gEngine;
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeInit(JNIEnv* env,
                                                     jobject /*thiz*/,
                                                     jobject assetManager,
                                                     jstring internalDataDir) {
    if (!gEngine) {
        gEngine = std::make_unique<ts::GameEngine>();
    }
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    const char* dirChars = env->GetStringUTFChars(internalDataDir, nullptr);
    std::string dirStr = dirChars ? dirChars : "";
    if (dirChars) env->ReleaseStringUTFChars(internalDataDir, dirChars);
    gEngine->initApp(mgr, dirStr);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeOnSurfaceCreated(JNIEnv* env,
                                                                 jobject /*thiz*/,
                                                                 jobject surface) {
    if (!gEngine || !surface) return;
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    gEngine->onSurfaceCreated(window);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeOnSurfaceChanged(JNIEnv* /*env*/,
                                                                 jobject /*thiz*/,
                                                                 jint width,
                                                                 jint height) {
    if (gEngine) gEngine->onSurfaceChanged(width, height);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeOnSurfaceDestroyed(JNIEnv* /*env*/,
                                                                   jobject /*thiz*/) {
    if (gEngine) gEngine->onSurfaceDestroyed();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeRenderFrame(JNIEnv* /*env*/,
                                                            jobject /*thiz*/) {
    if (gEngine) gEngine->stepAndRender();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeOnPause(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->onPause();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeOnResume(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->onResume();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetAudioMuted(JNIEnv* /*env*/,
                                                              jobject /*thiz*/,
                                                              jboolean muted) {
    if (gEngine) gEngine->setAudioMuted(muted == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeStartMission(JNIEnv* /*env*/,
                                                             jobject /*thiz*/,
                                                             jint missionIndex) {
    if (gEngine) gEngine->startMission(missionIndex);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeRestartMission(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->restartMission();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeRespawnCheckpoint(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->respawnAtCheckpoint();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetPaused(JNIEnv* /*env*/,
                                                          jobject /*thiz*/,
                                                          jboolean paused) {
    if (gEngine) gEngine->setPaused(paused == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeReturnToMenu(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->returnToMenu();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetMoveAxis(JNIEnv* /*env*/,
                                                            jobject /*thiz*/,
                                                            jfloat x,
                                                            jfloat y) {
    if (gEngine) gEngine->getInputManager().setMoveAxis(x, y);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeAddLookDelta(JNIEnv* /*env*/,
                                                             jobject /*thiz*/,
                                                             jfloat dx,
                                                             jfloat dy) {
    if (gEngine) gEngine->getInputManager().addLookDelta(dx, dy);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeAddGyroRate(JNIEnv* /*env*/,
                                                            jobject /*thiz*/,
                                                            jfloat wx,
                                                            jfloat wy,
                                                            jfloat dt) {
    if (gEngine) gEngine->getInputManager().addGyroRate(wx, wy, dt);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetSprint(JNIEnv* /*env*/,
                                                          jobject /*thiz*/,
                                                          jboolean active) {
    if (gEngine) gEngine->getInputManager().setSprint(active == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetFire(JNIEnv* /*env*/,
                                                        jobject /*thiz*/,
                                                        jboolean active) {
    if (gEngine) gEngine->getInputManager().setFire(active == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetAim(JNIEnv* /*env*/,
                                                       jobject /*thiz*/,
                                                       jboolean active) {
    if (gEngine) gEngine->getInputManager().setAim(active == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeToggleCrouch(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->getInputManager().toggleCrouch();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeTriggerJump(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->getInputManager().triggerJump();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeTriggerReload(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->getInputManager().triggerReload();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeTriggerGrenade(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->getInputManager().triggerGrenade();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeTriggerInteract(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine) gEngine->getInputManager().triggerInteract();
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSwitchWeapon(JNIEnv* /*env*/,
                                                             jobject /*thiz*/,
                                                             jint slot) {
    if (gEngine) gEngine->getInputManager().requestWeaponSwitch(slot);
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetSensitivity(JNIEnv* /*env*/,
                                                               jobject /*thiz*/,
                                                               jfloat touchSens,
                                                               jfloat adsMult,
                                                               jfloat gyroSens,
                                                               jboolean gyroEnabled,
                                                               jboolean invertY) {
    if (gEngine) {
        gEngine->setSensitivity(touchSens, adsMult, gyroSens, gyroEnabled == JNI_TRUE, invertY == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativeSetGraphicsSettings(JNIEnv* /*env*/,
                                                                    jobject /*thiz*/,
                                                                    jboolean shadows,
                                                                    jboolean postProcess,
                                                                    jfloat renderScale,
                                                                    jint targetFps) {
    if (gEngine) {
        gEngine->setGraphicsSettings(shadows == JNI_TRUE, postProcess == JNI_TRUE, renderScale, targetFps);
    }
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativePollHudState(JNIEnv* env,
                                                             jobject /*thiz*/,
                                                             jfloatArray outFloats,
                                                             jobjectArray outStrings) {
    if (!gEngine || !outFloats || !outStrings) return;
    ts::HudSnapshot s = gEngine->getHudSnapshot();

    float f[22] = {
        static_cast<float>(s.gameState),
        static_cast<float>(s.missionIndex),
        static_cast<float>(s.unlockedMission),
        s.health,
        s.armor,
        s.stamina,
        static_cast<float>(s.weaponSlot),
        static_cast<float>(s.magAmmo),
        static_cast<float>(s.reserveAmmo),
        static_cast<float>(s.grenades),
        s.reloading ? 1.0f : 0.0f,
        s.reloadProgress,
        s.aimFactor,
        s.hitmarkerTimer,
        s.lastHitKill ? 1.0f : 0.0f,
        s.damageFlash,
        s.damageDirAngle,
        static_cast<float>(s.enemiesAlive),
        static_cast<float>(s.kills),
        s.elapsedSec,
        s.nearInteractable ? 1.0f : 0.0f,
        static_cast<float>(s.fps)
    };
    env->SetFloatArrayRegion(outFloats, 0, 22, f);

    jstring jWeapon = env->NewStringUTF(s.weaponName.c_str());
    jstring jMission = env->NewStringUTF(s.missionName.c_str());
    jstring jObj = env->NewStringUTF(s.objectiveTitle.c_str());
    env->SetObjectArrayElement(outStrings, 0, jWeapon);
    env->SetObjectArrayElement(outStrings, 1, jMission);
    env->SetObjectArrayElement(outStrings, 2, jObj);
    env->DeleteLocalRef(jWeapon);
    env->DeleteLocalRef(jMission);
    env->DeleteLocalRef(jObj);
}

} // extern "C"
