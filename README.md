para que la libreria funcione se debe modificar un archivo fuente:
C:\Users\<user>\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\cores\esp32\Server.h


poner la linea 28 según corresponda:

//antes:
virtual void begin(uint16_t port=0) =0;    //para wifi 

//despues:
virtual void begin() =0;	   // para ethernet




La libreria usa el modulo W5500 para la comunicacion Ethernet usando los pines que se desciben en la imagen: ConexionPines.jpeg