# Modelización de la irradiancia solar con simulación en Unreal Engine y Machine Learning  

Este repositorio contiene el código y los datos asociados al TFG **Modelización de la irradiancia solar en sistemas fotovoltaicos con motores de videojuegos**.  

## Estructura del repositorio  

- `data/`: dataset utilizado para el entrenamiento del modelo, obtenido a partir de datos atmosféricos del servicio de Solcast, datos simulados mediante el plugin en UE y datos reales de los piranómetros de la universidad.  
- `notebooks/`: cuadernos Jupyter empleados:  
    - generación del dataset,  
    - modelos baseline,  
    - modelo final,  
    - comparación de los resultados.  
- `artifacts/`: resultados, predicciones exportadas y modelo final exportado. También contiene estructuras almacenadas de 10 de las 37 iteraciones realizadas para la obtención del modelo final, y un cuaderno para el análisis de las iteraciones.  
- `normalization/`: scripts utilizados para la normalización radiométrica interna de Pyrano.

## Flujo de ejecución recomendado  

IMPORTANTE: En caso de desear ejecutar el código de nuevo, no es conveniente volver a ejecutar el cuaderno `01_generate_dataset.ipynb`, ya que se adjunta directamente el dataset final generado y no los datos en bruto empleados para su construcción.