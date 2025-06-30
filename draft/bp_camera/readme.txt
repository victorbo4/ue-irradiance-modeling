##############################################
CAPTURA DE ILUMINACIÓN - BP CAMERA - UE
##############################################


NOTAS
#####################

Esta carpeta contiene también capturas de pantalla del código Blueprint y los parámetros del material, para una visión más rápida.

INSTRUCCIONES 
#####################

- Abrir el proyecto (MyProject) y seleccionar "Checkpoint 2".
- Para ver el código de la cámara en Blueprint abrir "bp_camera/BP_Irradiance_Sensor" e ir a Event Graph (panel de la izquierda).
	-> Pulsando sobre los nodos de Blueprint se puede ver la equivalencia en C++, en el registro de salida. Si salta warning
	es porque el proyecto es en Blueprint y no en C++, por lo que no se han descargado las dependencias; pero en el warning se hace referencia
	a la clase o función.
- Los parámetros del material se pueden ver en "sensor_mat", en la carpeta principal.
- La textura resultante en la que se almacena la iluminación es "irr_mat". También tiene código dentro, se puede comprobar pulsando en ella.

