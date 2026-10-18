param(
    [string]$Sdk = 'C:\Users\G\AppData\Local\Android\Sdk',
    [string]$Java = 'C:\Program Files\Android\Android Studio\jbr',
    [string]$Platform = 'android-36.1',
    [string]$BuildTools = '36.1.0'
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$output = Join-Path $project 'build-android-preview'
$sdkTools = Join-Path $Sdk "build-tools\$BuildTools"
$androidJar = Join-Path $Sdk "platforms\$Platform\android.jar"
$javaBin = Join-Path $Java 'bin'
function Verify([string]$step) { if ($LASTEXITCODE -ne 0) { throw "$step failed ($LASTEXITCODE)" } }
New-Item -ItemType Directory -Force -Path $output,"$output\classes","$output\dex" | Out-Null
& "$sdkTools\aapt2.exe" compile --dir "$project\android\res" -o "$output\resources.zip"
Verify 'Compile resources'
& "$sdkTools\aapt2.exe" link -o "$output\unsigned.apk" --manifest "$project\android\AndroidManifest.xml" -I $androidJar "$output\resources.zip"
Verify 'Link resources'
& "$javaBin\javac.exe" -encoding UTF-8 --release 8 -classpath $androidJar -d "$output\classes" "$project\android\src\org\groupricochet\preview\MainActivity.java"
Verify 'Compile Android source'
& "$javaBin\jar.exe" cf "$output\classes.jar" -C "$output\classes" .
Verify 'Package classes'
$priorJava = $env:JAVA_HOME
try {
    $env:JAVA_HOME = $Java
    & "$sdkTools\d8.bat" --lib $androidJar --min-api 26 --output "$output\dex" "$output\classes.jar"
    Verify 'Dex'
    & "$javaBin\jar.exe" uf "$output\unsigned.apk" -C "$output\dex" classes.dex
    Verify 'Add dex'
    & "$sdkTools\zipalign.exe" -f 4 "$output\unsigned.apk" "$output\aligned.apk"
    Verify 'Align APK'
    $debugKey = Join-Path $output 'preview-debug.keystore'
    if (!(Test-Path -LiteralPath $debugKey)) {
        & "$javaBin\keytool.exe" -genkeypair -keystore $debugKey -storepass android -keypass android -alias androiddebugkey -dname 'CN=group-ricochet local debug' -keyalg RSA -keysize 2048 -validity 3650
        Verify 'Create local debug signing key'
    }
    New-Item -ItemType Directory -Force -Path "$project\dist\android" | Out-Null
    $apk = "$project\dist\android\group-ricochet-preview.apk"
    & "$sdkTools\apksigner.bat" sign --ks $debugKey --ks-pass pass:android --key-pass pass:android --out $apk "$output\aligned.apk"
    Verify 'Sign debug APK'
    & "$sdkTools\apksigner.bat" verify $apk
    Verify 'Verify signature'
    Write-Output $apk
} finally { $env:JAVA_HOME = $priorJava }
