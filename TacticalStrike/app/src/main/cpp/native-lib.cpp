#include "GameEngine.h"
#include <algorithm>
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
Java_com_tacticalstrike_game_NativeBridge_nativeSetWeaponSkin(JNIEnv* /*env*/,
                                                               jobject /*thiz*/,
                                                               jint skinIndex) {
    if (gEngine) {
        gEngine->setWeaponSkin(skinIndex);
    }
}

JNIEXPORT void JNICALL
Java_com_tacticalstrike_game_NativeBridge_nativePollHudState(JNIEnv* env,
                                                             jobject /*thiz*/,
                                                             jfloatArray outFloats,
                                                             jobjectArray outStrings) {
    if (!gEngine || !outFloats || !outStrings) return;
    ts::HudSnapshot s = gEngine->getHudSnapshot();

    float f[56]{};
    f[0] = static_cast<float>(s.gameState);
    f[1] = static_cast<float>(s.missionIndex);
    f[2] = static_cast<float>(s.unlockedMission);
    f[3] = s.health;
    f[4] = s.armor;
    f[5] = s.stamina;
    f[6] = static_cast<float>(s.weaponSlot);
    f[7] = static_cast<float>(s.magAmmo);
    f[8] = static_cast<float>(s.reserveAmmo);
    f[9] = static_cast<float>(s.grenades);
    f[10] = s.reloading ? 1.0f : 0.0f;
    f[11] = s.reloadProgress;
    f[12] = s.aimFactor;
    f[13] = s.hitmarkerTimer;
    f[14] = s.lastHitKill ? 1.0f : 0.0f;
    f[15] = s.damageFlash;
    f[16] = s.damageDirAngle;
    f[17] = static_cast<float>(s.enemiesAlive);
    f[18] = static_cast<float>(s.kills);
    f[19] = s.elapsedSec;
    f[20] = s.nearInteractable ? 1.0f : 0.0f;
    f[21] = static_cast<float>(s.fps);
    f[22] = static_cast<float>(s.weaponSkin);
    f[23] = s.lastHitHeadshot ? 1.0f : 0.0f;
    f[24] = s.targetInfoTimer;
    f[25] = static_cast<float>(s.targetEnemyType);
    f[26] = s.targetEnemyHpRatio;
    f[27] = s.playerYaw;
    f[28] = s.objRelX;
    f[29] = s.objRelZ;
    f[30] = static_cast<float>(s.radarCount);
    for (int i = 0; i < 24; ++i) {
        f[31 + i] = s.radarEnemies[i];
    }
    f[55] = s.sprinting ? 1.0f : 0.0f;

    jsize cap = env->GetArrayLength(outFloats);
    env->SetFloatArrayRegion(outFloats, 0, std::min<jsize>(cap, 56), f);

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
