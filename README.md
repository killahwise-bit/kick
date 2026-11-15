# Nokta Pulse

Plugin de **sidechain / volume shaper** con editor de curva, hecho con [JUCE](https://juce.com) (C++).
Formatos: **VST3** (Windows, macOS, Linux), **AU** (macOS) y **Standalone**.

La curva controla la ganancia a lo largo de un ciclo: `arriba = volumen completo`, `abajo = silencio`.
No necesita que otra pista “alimente” el efecto: puede funcionar sincronizado al tempo, con notas MIDI o escuchando un sidechain de audio.

## Controles

| Control | Qué hace |
|---|---|
| **IN / OUT** | Ganancia de entrada y salida (dB). Doble clic = valor por defecto |
| **TIME** | Duración del ciclo: 1/1, 1/2, 1/4, 1/8, 1/16 y tresillos (1/4T, 1/8T, 1/16T) |
| **OFFSET** | Desplaza la curva dentro del ciclo (en Sync) o retrasa el disparo (en MIDI/Audio) |
| **TRIGGER** | `Sync`: el ciclo corre con el tempo del host · `MIDI`: cada nota reinicia la curva · `Audio`: el ciclo se reinicia cuando la señal supera **THRESH** |
| **THRESH** | Umbral de disparo (solo en modo Audio) |
| **REDUCTION** | Medidor de reducción de ganancia |
| **Presets (abajo)** | 7 formas de curva de partida |

**Modo Audio:** usa la entrada *sidechain* si tu DAW la tiene activada; si no, detecta sobre la propia señal de entrada.

### Editor de curva

- **Arrastrar un nodo**: mover (con `Shift` ajusta a la rejilla)
- **Doble clic en vacío**: añadir nodo · **doble clic sobre un nodo**: borrarlo
- **Clic derecho o `Alt` + arrastrar sobre un tramo**: cambiar su curvatura
- Dos nodos con la misma posición horizontal crean un salto (escalón / puerta)
- La forma de onda cian es la entrada del último ciclo; la línea cian vertical es el cursor de reproducción

## Compilar

Necesitas CMake ≥ 3.22 y un compilador C++17 (Visual Studio 2022, Xcode o GCC/Clang). JUCE se descarga solo.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Los plugins quedan en `build/NoktaPulse_artefacts/Release/` (`VST3/`, `AU/`, `Standalone/`).
Para copiarlos automáticamente a las carpetas del sistema tras compilar, cambia `COPY_PLUGIN_AFTER_BUILD` a `TRUE` en `CMakeLists.txt`.

En Linux instala antes: `libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev`.

## Subir a GitHub

```bash
cd nokta-pulse
git init
git add .
git commit -m "Nokta Pulse: primera versión"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/nokta-pulse.git
git push -u origin main
```

El workflow `.github/workflows/build.yml` compila en Windows, macOS y Linux en cada push y deja los plugins
como *artifacts* descargables en la pestaña **Actions**.

## Estructura

```
Source/
  CurveModel.h       curva (puntos, interpolación, presets)
  PluginProcessor.*  audio, parámetros, estado, disparo Sync/MIDI/Audio
  PluginEditor.*     interfaz (escala con la ventana)
  Components.h       editor de curva, barra de tiempo, medidores, presets
  LookAndFeel.h      estética (perillas, selectores)
```

## Ideas pendientes

- Varias curvas guardables / banco de presets de usuario
- Mix seco/húmedo y filtro en el camino de la señal
- Medidor de entrada y tipos de nodo (suave / recto)
- Firmar y empaquetar instaladores

## Licencia

Elige la que prefieras para tu código. JUCE se distribuye bajo AGPLv3 o licencia comercial: revisa
[juce.com/juce-8-licence](https://juce.com/juce-8-licence) antes de publicar binarios.
