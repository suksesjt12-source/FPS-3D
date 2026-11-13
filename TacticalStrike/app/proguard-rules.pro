-keep class com.tacticalstrike.game.NativeBridge { *; }
-keep class com.tacticalstrike.game.NativeBridge$* { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}
