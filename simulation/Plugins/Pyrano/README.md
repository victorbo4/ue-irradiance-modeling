# Plugin para la estimación de irradiancia y simulación de piranómetros virtuales en Unreal Engine 5.6.1

Plugin de Unreal Engine desarrollado en el marco del TFG para **simular y exportar irradiancia** en sensores virtuales en entornos urbanos, combinando captura HDR del entorno y cálculo híbrido (GPU/CPU).

> **Nota sobre el alcance del repositorio:**  
> Este repositorio contiene **únicamente el plugin** (componente reutilizable).  
> El proyecto Unreal completo (escena, assets y configuración experimental), así como el pipeline de aprendizaje automático, se encuentran disponibles en el repositorio asociado al TFG:  
> **https://github.com/victorbo4/ue-irradiance-modeling**


## Arquitectura general y características

- **Sensores virtuales** (`UPyranometerComponent`) gestionables desde el Editor.
- **Control desde el Editor** (UMG/Blueprint ↔ C++) mediante `UPyranoEditorSubsystem`.
- **Planificación de capturas** por sensores e instantes temporales (`UIrradianceScheduler`).
- **Captura HDR lineal** del búfer `SceneColor` mediante `FIrradianceViewExtension` (Replacing Tonemapper).
- **Cálculo híbrido de irradiancia**:
  - componente ambiental integrada en **GPU** (RDG + compute shaders),
  - componente solar geométrica en **CPU** (ray tracing / line trace).
- **Exportación a CSV** con métricas y descriptores (p. ej., oclusión, SVF, clear-sky baseline).



## Requerimientos

- Unreal Engine 5.6.1
- Proyecto configurado para ejecutar el plugin en PIE (Play In Editor).
- Plugin Sun Position nativo instalado, con actor SunSky activo.



## Instalación

1. Copiar la carpeta del plugin en:
   - `<TuProyecto>/Plugins/Pyrano/`
2. Abrir el proyecto en Unreal y habilita el plugin si es necesario.
3. Compilar el proyecto.
4. Iniciar el complemento desde el menú del Editor: `Herramientas → Pyrano Capture Planner`.
5. El botón `Help` del complemento ofrece información adicional y referencias de uso.



## Uso general

1. Añadir uno o varios sensores (`UPyranometerComponent`) en la escena.
2. Configurar y lanzar la simulación/capturas desde la interfaz del Editor.
3. Obtener los resultados exportados (CSV) en la ruta configurada.

> La configuración detallada del escenario experimental y los assets se encuentran en el material suplementario del TFG.



## Alcance del repositorio

Este repositorio **no incluye**:
- el proyecto Unreal completo,
- assets del escenario (p. ej., modelo de azotea),
- Cesium/configuración geoespacial,
- código de entrenamiento/inferencia ML.

---
