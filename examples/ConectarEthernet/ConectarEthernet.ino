/**
 * @file Internet.h
 * @date 27.08.2020
 * @author William Rodriguez
 * 
*/







#include <Internet.h>

//task demo
TaskHandle_t nucleo0Handle = NULL;
void Nucleo0(void *pvParameters) { // This is a task.
  (void) pvParameters;
  Serial.print("Nucleo: ");
  Serial.println(xPortGetCoreID());

  pinMode(2, OUTPUT);

  unsigned long startAttemptTime = millis();
  for (;;) { // A Task shall never return or exit.
    if (millis() - startAttemptTime > 5000) {
      startAttemptTime = millis();
      //Serial.print("[Blink]: [Core 0]: "); Serial.println(xPortGetCoreID());
    }

    digitalWrite(2, !digitalRead(2));   // turn the LED on (HIGH is the voltage level)
    vTaskDelay(500 / portTICK_PERIOD_MS); // one tick delay (15ms) in between reads for stability
  }
  vTaskDelete(NULL); //borrar esta tarea
}






// Servicios get del esp

String prueba(String variables){
  Serial.print("Variables recibidas: ");
  Serial.println(variables);
  return "hola mundo!.";
}

String saludar(String variables){
  Serial.print("Variables recibidas: ");
  Serial.println(variables);
  return "Bienvenido al mundo";
}

String despedir(String variables){
  Serial.print("Variables recibidas: ");
  Serial.println(variables);
  return "Que tengas un lindo dia";
}





void setup() {
  Serial.begin(115200);
  bool usarWifi = false;
  bool usarEth = true;
  bool isAP = false;  
  ConfigInternet(usarWifi, usarEth, isAP);  

  if(isActiveUseEthernet()){    
    while (!( getStatusConectionEthernet() )) {
      //esperando a que se conecte a una red ethernet
      delay(100);
      Serial.print(".");
    }
  }

  Serial.println("Conexion Ethernet completa.");

  {   //aca va la declaracion de los servicios de internet que se vayan a montar por el puerto ethernet
    //Estas instancias de peticion y respuesta se definen fuera de un Task y en el SETUP
    RegistrarNuevoServicioGet_ETH("/prueba", prueba);
    RegistrarNuevoServicioGet_ETH("/saludar", saludar);
    RegistrarNuevoServicioGet_ETH("/despedir", despedir);
    
  }
    
  xTaskCreatePinnedToCore(
      Nucleo0
      ,  "Nucleo0"   // A name just for humans
      ,  1024  // This stack size can be checked & adjusted by reading the Stack Highwater
      ,  NULL
      ,  3  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
      ,  &nucleo0Handle
      ,  0);
}

void loop() {

}