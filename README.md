# ue_irradiance_modeling

## CHANGELOG

**[08/28] Plugin IrradianceDXR – Primer pase funcional (UE 5.6.1)**
- Shader de prueba (`RayTracingIrradiance.usf`): RayGen + ClosestHit mínimos, salida rojo, miss por defecto  
- Pase RDG: textura UAV, PSO + SBT, `RayTraceDispatch`  
- GPU Readback: lectura 1x1, conversión a `FLinearColor`  
- Consola: `TestPyrano` (lanza pase), `TestPyranoRead` (lee resultado)  
- Modo de uso: abrir nivel → PIE → ejecutar comandos en consola  

**[08/15]** Añadido pseudo-código del shader del plugin.  
**[08/14]** Incluidos paths y dependencias necesarios para un funcionamiento básico del plugin.  
**[08/12]** Actualizado a UE 5.6 y solucionados problemas de compatibilidad.  
&emsp;Creada base del plugin para el cálculo de irradiancia por DXR.  
**[08/07]** Añadidos archivos base del proyecto de UE para el plugin del cálculo de irradiancia mediante DXR.  
**[06/30]** Añadido ejemplo de captura de iluminación en textura (bp_camera); contiene nivel, capturas de pantalla e instrucciones.  
**[06/26]** Creado repositorio. Añadidos planos provisionales de la planta y modelo 3D.
