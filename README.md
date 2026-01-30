# Guía del Anexo de software y datos

Este repositorio contiene los recursos técnicos utilizados en el desarrollo del Trabajo Fin de Grado (TFG), organizados según el flujo de trabajo lógico del proyecto. El contenido se estructura en dos bloques fundamentales: el ecosistema de simulación en Unreal Engine y el flujo de procesamiento y aprendizaje automático en Jupyter Notebooks.

El mismo contenido se proporciona también en formato comprimido en la sección **Releases** del repositorio, con el fin de facilitar su descarga y consulta.

> **Nota sobre el complemento de software:**  
> El complemento de Unreal Engine desarrollado en este trabajo se incluye en este repositorio como parte del proyecto de simulación completo.  
> Adicionalmente, el código del complemento se encuentra disponible de forma independiente en un repositorio específico, concebido como componente reutilizable y desacoplado del experimento concreto:
>  
> https://github.com/victorbo4/Pyrano
## 1. Complemento de software en Unreal Engine

Se adjunta el proyecto de prueba `PyranoDemo`, el cual incluye la escena virtual de la azotea de la ETSIDI. El núcleo del desarrollo se encuentra en la carpeta `Plugins`, organizada de la siguiente forma:

* **`Shaders/`**: Contiene el shader de cómputo `IrradianceIntegrate.usf` y las funciones auxiliares en `IrradianceCommon.ush`.
* **`Source/`**: Código fuente C++ desglosado en sus módulos `Pyrano` (módulo *Runtime*) y `PyranoEditor` (módulo Editor).
* **`Docs/`**: Documentación técnica generada con **Doxygen**. Para consultarla, abrir el archivo `index.html` en la carpeta `Docs/html/`.

> **Nota de ejecución:** En caso de querer ejecutar el proyecto para poner a prueba el complemento de software desarrollado, siga las indicaciones del archivo `INSTRUCCIONES_DE_USO.pdf` situado en la raíz de la carpeta del proyecto.

## 2. Modelo de aprendizaje automático y normalización

Esta sección contiene la inteligencia de datos del proyecto. Para una navegación detallada entre scripts, consulte el **README** específico dentro de este directorio. Se estructura en:

* **`normalization/`**: Script encargado del ajuste de coeficientes para la normalización radiométrica.
* **`notebooks/`**: Cuadernos de Jupyter utilizados para el entrenamiento, optimización de hiperparámetros y validación final del modelo.

* **`data/`**: Conjuntos de datos final en formato CSV (*dataset master*), utilizado para el entrenamiento y evaluación del modelo.


