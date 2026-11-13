import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';

const ROOT = path.resolve('TacticalStrike');
const MAIN = path.join(ROOT, 'app/src/main');
const CPP = path.join(MAIN, 'cpp');
const ASSETS = path.join(MAIN, 'assets');
const KT_DIR = path.join(MAIN, 'java/com/tacticalstrike/game');

test('All required project structure files exist and are non-empty', () => {
  const required = [
    'settings.gradle.kts',
    'build.gradle.kts',
    'gradle.properties',
    'gradlew',
    'gradle/wrapper/gradle-wrapper.properties',
    '.github/workflows/android.yml',
    'app/build.gradle.kts',
    'app/proguard-rules.pro',
    'app/src/main/AndroidManifest.xml',
    'app/src/main/java/com/tacticalstrike/game/MainActivity.kt',
    'app/src/main/java/com/tacticalstrike/game/GameSurface.kt',
    'app/src/main/java/com/tacticalstrike/game/TouchController.kt',
    'app/src/main/java/com/tacticalstrike/game/SettingsManager.kt',
    'app/src/main/java/com/tacticalstrike/game/NativeBridge.kt',
    'app/src/main/cpp/CMakeLists.txt',
    'app/src/main/cpp/native-lib.cpp',
    'app/src/main/cpp/GameEngine.cpp',
    'app/src/main/cpp/Renderer.cpp',
    'app/src/main/cpp/ShaderManager.cpp',
    'app/src/main/cpp/TextureManager.cpp',
    'app/src/main/cpp/ModelLoader.cpp',
    'app/src/main/cpp/Camera.cpp',
    'app/src/main/cpp/Player.cpp',
    'app/src/main/cpp/WeaponSystem.cpp',
    'app/src/main/cpp/EnemySystem.cpp',
    'app/src/main/cpp/EnemyAI.cpp',
    'app/src/main/cpp/Physics.cpp',
    'app/src/main/cpp/Collision.cpp',
    'app/src/main/cpp/Animation.cpp',
    'app/src/main/cpp/AudioSystem.cpp',
    'app/src/main/cpp/ParticleSystem.cpp',
    'app/src/main/cpp/LightingSystem.cpp',
    'app/src/main/cpp/LevelManager.cpp',
    'app/src/main/cpp/MissionManager.cpp',
    'app/src/main/cpp/SaveManager.cpp',
    'app/src/main/cpp/InputManager.cpp',
  ];
  for (const rel of required) {
    const p = path.join(ROOT, rel);
    assert.ok(fs.existsSync(p), `Missing required file: ${rel}`);
    assert.ok(fs.statSync(p).size > 20, `File unexpectedly small/empty: ${rel}`);
  }
});

test('TacticalStrike contains zero WebView/HTML/CSS/JS/Three.js artifacts', () => {
  const walk = (dir) => {
    for (const ent of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, ent.name);
      if (ent.isDirectory()) {
        walk(full);
      } else {
        assert.ok(!/\.(html|css|js|ts|tsx|jsx)$/i.test(ent.name), `Forbidden web file in TacticalStrike: ${full}`);
        if (/\.(kt|cpp|h|xml|kts)$/i.test(ent.name)) {
          const content = fs.readFileSync(full, 'utf8');
          assert.ok(!/WebView|Three\.js|Phaser|Capacitor/i.test(content), `Forbidden tech reference in ${full}`);
        }
      }
    }
  };
  walk(ROOT);
});

test('Every C++ source file is registered in CMakeLists.txt', () => {
  const cmake = fs.readFileSync(path.join(CPP, 'CMakeLists.txt'), 'utf8');
  const cppFiles = fs.readdirSync(CPP).filter((f) => f.endsWith('.cpp'));
  for (const f of cppFiles) {
    assert.ok(cmake.includes(f), `${f} is not listed in CMakeLists.txt`);
  }
});

test('Every NativeBridge.kt external fun matches native-lib.cpp JNI export', () => {
  const kt = fs.readFileSync(path.join(KT_DIR, 'NativeBridge.kt'), 'utf8');
  const cpp = fs.readFileSync(path.join(CPP, 'native-lib.cpp'), 'utf8');
  const matches = [...kt.matchAll(/external\s+fun\s+([A-Za-z0-9_]+)/g)];
  assert.ok(matches.length >= 20, 'Expected at least 20 JNI functions');
  for (const m of matches) {
    const fnName = m[1];
    const jniSymbol = `Java_com_tacticalstrike_game_NativeBridge_${fnName}`;
    assert.ok(cpp.includes(jniSymbol), `Missing JNI symbol in native-lib.cpp: ${jniSymbol}`);
  }
});

test('Every asset path referenced in C++ exists in app/src/main/assets', () => {
  const cppFiles = fs.readdirSync(CPP).filter((f) => f.endsWith('.cpp'));
  const assetRegex = /"((?:shaders|models|textures|audio|levels)\/[A-Za-z0-9_.-]+)"/g;
  for (const f of cppFiles) {
    const text = fs.readFileSync(path.join(CPP, f), 'utf8');
    for (const m of text.matchAll(assetRegex)) {
      const relAsset = m[1];
      const fullPath = path.join(ASSETS, relAsset);
      assert.ok(fs.existsSync(fullPath), `Missing asset '${relAsset}' referenced in ${f}`);
    }
  }
});

test('All glTF 2.0 models and binary buffers are structurally valid', () => {
  const modelsDir = path.join(ASSETS, 'models');
  const gltfs = fs.readdirSync(modelsDir).filter((f) => f.endsWith('.gltf'));
  assert.ok(gltfs.length >= 8, 'Expected 8 glTF models');

  for (const f of gltfs) {
    const gltf = JSON.parse(fs.readFileSync(path.join(modelsDir, f), 'utf8'));
    assert.equal(gltf.asset.version, '2.0');
    assert.ok(gltf.buffers.length >= 1);
    const binPath = path.join(modelsDir, gltf.buffers[0].uri);
    assert.ok(fs.existsSync(binPath), `Missing .bin for ${f}`);
    const binSize = fs.statSync(binPath).size;
    assert.equal(binSize, gltf.buffers[0].byteLength, `Buffer byteLength mismatch in ${f}`);

    for (const bv of gltf.bufferViews) {
      assert.ok(bv.byteOffset + bv.byteLength <= binSize, `BufferView out of bounds in ${f}`);
    }
    for (const acc of gltf.accessors) {
      assert.ok(acc.bufferView >= 0 && acc.bufferView < gltf.bufferViews.length, `Invalid accessor bufferView in ${f}`);
    }
    if (f === 'enemy_trooper.gltf') {
      assert.ok(gltf.skins.length === 1 && gltf.skins[0].joints.length === 8);
      assert.ok(gltf.animations.length === 5, 'Expected 5 skeletal animation clips');
    }
  }
});

test('All KTX1 ETC2 textures have valid headers and mipmap sizes', () => {
  const texDir = path.join(ASSETS, 'textures');
  const ktxs = fs.readdirSync(texDir).filter((f) => f.endsWith('.ktx'));
  assert.ok(ktxs.length >= 13);
  const magic = Buffer.from([0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A]);

  for (const f of ktxs) {
    const buf = fs.readFileSync(path.join(texDir, f));
    assert.ok(buf.subarray(0, 12).equals(magic), `Invalid KTX1 magic in ${f}`);
    const fmt = buf.readUInt32LE(28);
    assert.ok(fmt === 0x9274 || fmt === 0x9278, `Expected ETC2 format in ${f}, got 0x${fmt.toString(16)}`);
    const mips = buf.readUInt32LE(56);
    assert.ok(mips >= 4, `Expected mipmap chain in ${f}`);
  }
});

test('All WAV audio assets have valid RIFF/WAVE PCM headers', () => {
  const audioDir = path.join(ASSETS, 'audio');
  const wavs = fs.readdirSync(audioDir).filter((f) => f.endsWith('.wav'));
  assert.equal(wavs.length, 9);
  for (const f of wavs) {
    const buf = fs.readFileSync(path.join(audioDir, f));
    assert.equal(buf.subarray(0, 4).toString('ascii'), 'RIFF');
    assert.equal(buf.subarray(8, 12).toString('ascii'), 'WAVE');
    assert.equal(buf.readUInt16LE(20), 1); // PCM
    assert.equal(buf.readUInt32LE(24), 22050);
  }
});

test('All level JSON files have unobstructed player spawns and valid objectives', () => {
  const levelsDir = path.join(ASSETS, 'levels');
  const levels = fs.readdirSync(levelsDir).filter((f) => f.endsWith('.json'));
  assert.equal(levels.length, 3);

  for (const f of levels) {
    const lvl = JSON.parse(fs.readFileSync(path.join(levelsDir, f), 'utf8'));
    const [px, py, pz] = lvl.playerSpawn;
    assert.ok(lvl.objectives.length >= 3);
    assert.ok(lvl.enemies.length >= 6);
    assert.ok(lvl.checkpoints.length >= 2);

    // Check player spawn is clear of solid wall blocks (ignoring ground floor at y < 0.01)
    for (const blk of lvl.blocks) {
      if (!blk.solid) continue;
      const [cx, cy, cz] = blk.center;
      const [hx, hy, hz] = blk.half;
      if (cy + hy <= 0.05) continue; // floor
      const inside =
        px >= cx - hx - 0.5 && px <= cx + hx + 0.5 &&
        pz >= cz - hz - 0.5 && pz <= cz + hz + 0.5 &&
        py >= cy - hy && py <= cy + hy;
      assert.ok(!inside, `Player spawn overlaps block in ${f}`);
    }
  }
});

test('All GLSL ES 3.0 shaders have valid version, precision, and main()', () => {
  const shaderDir = path.join(ASSETS, 'shaders');
  const files = fs.readdirSync(shaderDir);
  assert.equal(files.length, 12);
  for (const f of files) {
    const src = fs.readFileSync(path.join(shaderDir, f), 'utf8');
    assert.ok(src.startsWith('#version 300 es'), `Missing #version 300 es in ${f}`);
    assert.ok(/precision\s+(highp|mediump|lowp)\s+float\s*;/.test(src), `Missing float precision in ${f}`);
    assert.ok(/void\s+main\s*\(\s*\)/.test(src), `Missing main() in ${f}`);
  }
});

test('Android resources are minSdk-24 safe (styles-v28 & mipmap PNG fallbacks)', () => {
  const baseStyles = fs.readFileSync(path.join(MAIN, 'res/values/styles.xml'), 'utf8');
  assert.ok(!baseStyles.includes('windowLayoutInDisplayCutoutMode'),
    'Base values/styles.xml must not contain API 28 windowLayoutInDisplayCutoutMode');

  const v28StylesPath = path.join(MAIN, 'res/values-v28/styles.xml');
  assert.ok(fs.existsSync(v28StylesPath), 'Missing values-v28/styles.xml');
  const v28Styles = fs.readFileSync(v28StylesPath, 'utf8');
  assert.ok(v28Styles.includes('windowLayoutInDisplayCutoutMode'));

  const pngMagic = Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]);
  for (const bucket of ['mipmap-mdpi', 'mipmap-hdpi', 'mipmap-xhdpi', 'mipmap-xxhdpi', 'mipmap-xxxhdpi']) {
    for (const icon of ['ic_launcher.png', 'ic_launcher_round.png']) {
      const p = path.join(MAIN, 'res', bucket, icon);
      assert.ok(fs.existsSync(p), `Missing fallback icon ${bucket}/${icon}`);
      const buf = fs.readFileSync(p);
      assert.ok(buf.subarray(0, 8).equals(pngMagic), `Invalid PNG signature in ${bucket}/${icon}`);
    }
  }
});

test('Both GitHub Actions APK workflows accept SDK licenses and build with Gradle 8.7 directly', () => {
  const workflows = [
    path.resolve('.github/workflows/android.yml'),
    path.join(ROOT, '.github/workflows/android.yml'),
  ];
  for (const wf of workflows) {
    assert.ok(fs.existsSync(wf), `Missing workflow: ${wf}`);
    const yaml = fs.readFileSync(wf, 'utf8');
    assert.ok(yaml.includes('sdkmanager --licenses'), `Missing sdkmanager --licenses in ${wf}`);
    assert.ok(yaml.includes('set +o pipefail'), `Missing pipefail guard for yes | sdkmanager in ${wf}`);
    assert.ok(!/yes\s*\|\s*sdkmanager\s+--install/.test(yaml), `Unsafe yes | sdkmanager --install in ${wf}`);
    assert.ok(yaml.includes('ndk;26.3.11579264'), `Missing NDK version in ${wf}`);
    assert.ok(yaml.includes('cmake;3.22.1'), `Missing CMake version in ${wf}`);
    assert.ok(yaml.includes('gradle assembleDebug'), `Missing gradle assembleDebug in ${wf}`);
    assert.ok(yaml.includes('gradle assembleRelease'), `Missing gradle assembleRelease in ${wf}`);
    assert.ok(!yaml.includes('./gradlew'), `Should invoke gradle directly instead of missing wrapper jar in ${wf}`);
  }
});
