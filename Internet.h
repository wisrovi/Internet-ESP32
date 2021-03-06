/**
 * @file Internet.h
 * @date 27.08.2020
 * @author William Rodriguez
 * 
*/

#ifndef internet_library
#define internet_library

//#define internet_library_debug
#include "CONFIG_INTERNET.H"

#ifdef useWifi
bool thereConectionWifi = false;
#define EspacioMemoriaWifi 15000 //8500
#define time_check_keep_alive_wifi 300000
unsigned int localUdpPort = 3333;
#endif

#ifdef useEth
bool thereConectionEthernet = false;
#define EspacioMemoriaEthernet 1500
#endif

#ifdef useAP
bool thereConectionAP = false;
char passwordAP_char[25];
char nombreAP_char[25];
#define EspacioMemoriaWifiAP 16000 //3100
#define useCaptivePortal
#endif

#include <WiFi.h> //para obtener la MAC wifi

#ifdef useEth
#define internet_library_ethernet
#include <SPI.h>
#include <Ethernet.h>
#endif
typedef String (*fptr)(String);

/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *                                                                          *
                                 WIFI TASK
 *                                                                          *
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *****************************************************************************/

#ifdef useWifi
#define DEVICE_NAME "WISROVI"
#define WIFI_TIMEOUT 20000 // 20 seconds

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>

#include <WiFiUdp.h>
WiFiUDP Udp;
String getId_ESP_internet()
{
  uint64_t chipid = ESP.getEfuseMac();
  String id = String((uint32_t)chipid);

  String mac = WiFi.macAddress();
  id = mac.substring(mac.length() - 2, mac.length() - 1) + id + mac.substring(mac.length() - 1, mac.length());
  return id;
}
void ActivateUPD()
{
  Udp.begin(localUdpPort);
}
void LeerUDP()
{
  // receive incoming UDP packets
  char incomingPacket[255];
  int len = Udp.read(incomingPacket, 255);
  if (len > 0)
  {
    incomingPacket[len] = 0;
  }
  Serial.printf("UDP packet contents: %s\n", incomingPacket);
}
void RespondUDP()
{
  String id_esp = getId_ESP_internet();

  String data_response = "{ \"id_esp\" : ";
  data_response.concat(id_esp);
  data_response.concat(", \"name\" : \"ESP\" }");

  char data_response_char[100];
  data_response.toCharArray(data_response_char, 100);

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.print(data_response_char);
  Udp.endPacket();
}
void ReceivedUDP()
{
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    Serial.print("Escuchando a:");
    Serial.print(Udp.remoteIP().toString().c_str());
    Serial.print(" - en el puerto: ");
    Serial.println(Udp.remotePort());

    LeerUDP();

    RespondUDP();
  }
}

#include <Hash.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
AsyncWebServer server_OTA(80);

#include <ArduinoJson.h>
bool IniciarServidorWifi = false;
bool servidorWifi_iniciado = false;
bool usarWifiManager = true;
DynamicJsonDocument rutasUrl(1024);
typedef std::function<void(AsyncWebServerRequest *request)> THandlerFunction;
bool RegistrarNuevoServicioGet(String url, THandlerFunction fn)
{
  bool servicioCreado = false;

  bool estaActivoUsoWii = false;
#ifdef useAP
  estaActivoUsoWii = thereConectionAP;
#endif

  if (estaActivoUsoWii || thereConectionWifi)
  {
    char url_char[url.length() + 1];
    url.toCharArray(url_char, url.length() + 1);
    bool existeRuta = rutasUrl[url];
    ;
    if (existeRuta == false)
    {
      server_OTA.on(url_char, HTTP_GET, fn);
      rutasUrl[url] = true;
      servicioCreado = true;
    }
    else
    {
      Serial.print("wisrovi@internet: [/]:  [WIFI]: El servicio '");
      Serial.print(url);
      Serial.println("' no se puede crear, ya existe o esta reservado.");
    }
    /*String allStationsConected;
		serializeJson(rutasUrl, allStationsConected);
		Serial.print("[AP]: ");
		Serial.println(allStationsConected);*/
  }
  return servicioCreado;
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

TaskHandle_t nucleo1WifiKeepAlive_Handle = NULL;
void keepWiFiAlive(void *pvParameters);

bool useCredentialsConfigured = false;
bool actived_use_wifi = false;
#define tiempoIntentoConexionMaxima 20 //segundos

bool setIniciarServidorWifi()
{
  if (thereConectionWifi)
  {
    IniciarServidorWifi = true;
  }
  return IniciarServidorWifi;
}

#ifndef useAP
char nombreAP_char[25];
char passwordAP_char[25];
bool thereConectionAP = false;
#endif

bool ConfigRedWifiConection(String nombreAP, String passwordAP)
{
  bool is_AP = false;
#ifdef useAP
  is_AP = thereConectionAP;
#endif

  if (is_AP == false)
  {
    nombreAP.toCharArray(nombreAP_char, 25);
    passwordAP.toCharArray(passwordAP_char, 25);
    useCredentialsConfigured = true;
  }
  return useCredentialsConfigured;
}

void ConfigWifiKeepAlive()
{
  //Esta tarea se ejecuta en el nucleo 1, y se encargar de mantener la conexión wifi activa,
  //si pierde conexión, la reestablece nuevamente con las credenciales guardadas
  //siempre que haya conexion, esta libreria monta su actualización por OTA
  //cuando se pierde la conexión, la libreria reinicia los servicios asociados a la libreria para evitar que un servicio no vaya a funcionar

  xTaskCreatePinnedToCore(
      keepWiFiAlive,                // Function that should be called
      "keepWiFiAlive",              // Name of the task (for debugging)
      EspacioMemoriaWifi,           // Stack size (bytes),                             EL USADO Y FUNCIONAL: 8000
      NULL,                         // Parameter to pass
      1,                            // Task priority
      &nucleo1WifiKeepAlive_Handle, // Task handle
      1                             // Core you want to run the task on (0 or 1)
  );
}

void deshabilitarWifimanager()
{
  usarWifiManager = false;
}

void keepWiFiAlive(void *parameter)
{
  Serial.print("wisrovi@internet: [/]:  [WIFI]: [Core 1]: ");
  Serial.println(xPortGetCoreID());

  rutasUrl["/"] = true;
  rutasUrl["/update"] = true;

  int retardoConexionEstablecida = 0;
  bool debugWifiManager = true;
  bool haveIpOutFor = false;
  thereConectionWifi = false;

  WiFiManager wifiManager;

  int conteoIntentos = 0;
  bool heObtenidoIpConCredenciales = false;
  while (useCredentialsConfigured == true && heObtenidoIpConCredenciales == false)
  {
    Serial.println("\n wisrovi@internet: [/]:  [WIFI]: Usando credenciales para buscar ip");
    WiFi.begin(nombreAP_char, passwordAP_char);
    byte contadorTiempoConexion = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      //Serial.print(".");
      contadorTiempoConexion++;
      if (contadorTiempoConexion >= (tiempoIntentoConexionMaxima * 2))
      {
        contadorTiempoConexion = 0;
        break;
      }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      heObtenidoIpConCredenciales = true;
    }
    else
    {
      conteoIntentos++;
      if (conteoIntentos >= 3)
      {
        useCredentialsConfigured = false;
      }
    }
  }

  if (useCredentialsConfigured == false && usarWifiManager == true)
  {
    Serial.println("wisrovi@internet: [/]:  [WIFI]: Las credenciales dadas no son validas, se ha activado un AP para que desde el movil pueda configurar la Wifi, favor ingresar a: 192.168.4.1 en un navegador.");
    //wifiManager.setTimeout(300);
    wifiManager.setDebugOutput(debugWifiManager);
    wifiManager.autoConnect(DEVICE_NAME);
    Serial.println("\n wisrovi@internet: [/]:  [WIFI]: Las nuevas credenciales han sido almacenadas.");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    //Serial.println("[WIFI]: Iniciando AP para configuración...");
  }

  //Serial.println(wifiManager.getSSID());
  //Serial.println(wifiManager.getPassword());

  if (WiFi.status() == WL_CONNECTED)
  {
    haveIpOutFor = true;
    thereConectionWifi = true;
  }
  else
  {
    if (usarWifiManager == false)
    {
      Serial.println("\nwisrovi@internet: [/]:  [WIFI]: Cerrando la tarea pues con las credenciales dadas no es posible conectarse a la red y se ha desactivado el WifiManager.");
      vTaskDelete(NULL); //borrar esta tarea
    }
  }
  String ssid = WiFi.SSID();
  String PSK = WiFi.psk();

  int conteoIntentosConexion = 0;
  unsigned long startAttemptTime = millis();
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED && haveIpOutFor == false)
    {
      bool thereConection = true;
      while (thereConection && servidorWifi_iniciado)
      {
        retardoConexionEstablecida++;
        if (retardoConexionEstablecida > 1000)
        {
          retardoConexionEstablecida = 0;
          thereConection = false;
        }
        vTaskDelay(15 / portTICK_PERIOD_MS);
        AsyncElegantOTA.loop();
        ReceivedUDP();
      }
      conteoIntentosConexion = 0;
      thereConectionWifi = true;
      if (servidorWifi_iniciado == true)
      {
        if (millis() - startAttemptTime > time_check_keep_alive_wifi)
        {
          Serial.print("wisrovi@internet: [/]:  [WIFI]: [Core 1]: ");
          Serial.println(xPortGetCoreID());
        }
        continue;
      }
    }
    else
    {
      thereConectionWifi = false;
      servidorWifi_iniciado = false;
    }

    if (thereConectionWifi == false)
    {
      Serial.println("wisrovi@internet: [/]:  [WIFI] Connecting...");
      if (conteoIntentosConexion <= 2)
      {
        char ssid_char[ssid.length() + 1];
        ssid.toCharArray(ssid_char, ssid.length() + 1);

        char psk_char[PSK.length() + 1];
        PSK.toCharArray(psk_char, PSK.length() + 1);

        WiFi.mode(WIFI_STA);
        WiFi.setHostname(DEVICE_NAME);
        WiFi.begin(ssid_char, psk_char);
      }
      else
      {
        while (useCredentialsConfigured && WiFi.status() != WL_CONNECTED)
        {
          WiFi.begin(nombreAP_char, passwordAP_char);
          byte contadorTiempoConexion = 0;
          Serial.println("wisrovi@internet: [/]:  [WIFI]: Usando credenciales para buscar ip 2");
          while (WiFi.status() != WL_CONNECTED)
          {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            //Serial.print(".");
            contadorTiempoConexion++;
            if (contadorTiempoConexion >= (tiempoIntentoConexionMaxima * 2))
            {
              contadorTiempoConexion = 0;
              break;
            }
          }

          if (WiFi.status() == WL_CONNECTED)
          {
            break;
          }
          else
          {
            conteoIntentos++;
            if (conteoIntentos >= 3)
            {
              useCredentialsConfigured = false;
            }
          }
        }

        if (useCredentialsConfigured == false && usarWifiManager == true)
        {
          wifiManager.setTimeout(300);
          wifiManager.setDebugOutput(debugWifiManager);
          //wifiManager.autoConnect(DEVICE_NAME);
          wifiManager.startConfigPortal(DEVICE_NAME);
        }
      }
    }

    unsigned long startAttemptTime = millis();

    // Keep looping while we're not connected and haven't reached the timeout
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT)
    {
    }

    // If we couldn't connect within the timeout period, retry in 30 seconds.
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("wisrovi@internet: [/]:  [WIFI]: FAILED");
      conteoIntentosConexion++;
      servidorWifi_iniciado = false;
      vTaskDelay(30000 / portTICK_PERIOD_MS);
      continue;
    }
    else
    {
      conteoIntentosConexion = 0;
      thereConectionWifi = true;
    }

    if (IniciarServidorWifi == true)
    {
      Serial.print("wisrovi@internet: [/]:  [WIFI]: Connected: ");
      Serial.println(WiFi.localIP());
      thereConectionWifi = true;

      server_OTA.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Version Libreria Internet: 1.0.");
      });

      AsyncElegantOTA.begin(&server_OTA); // Start ElegantOTA
      server_OTA.onNotFound(notFound);
      server_OTA.begin();
      Serial.println("wisrovi@internet: [/OTA/]:  [WIFI]: Activando OTA.");

      ActivateUPD();
      Serial.print("wisrovi@internet: [/UDP/]:  [WIFI]: Activando UDP in puerto ");
      Serial.print(localUdpPort);
      Serial.println(".");
      servidorWifi_iniciado = true;
    }

    haveIpOutFor = false;
  }
}

bool getStatusConectionwifi()
{
  if (thereConectionAP == true)
  {
    //Serial.println("[WIFI]: no AP activo.");
    return true;
  }
  else
  {
    return thereConectionWifi;
  }
}

bool isActiveUseWifi()
{
  return actived_use_wifi;
}

#endif

/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *                                                                          *
                                ETHERNET TASK
 *                                                                          *
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *****************************************************************************/

#ifdef useEth
#define ETH_TIMEOUT 20000 // 20 seconds

#ifndef useAP
#ifndef useWifi
bool thereConectionAP = false;
#endif
#endif

//#include <esp_wifi.h>    //aveces se requiere para obtener el ChipID

#define RESET_P 26 // Tie the Wiz820io/W5500 reset pin to ESP32 GPIO26 pin.

bool actived_use_eth = false;

EthernetServer serverETH(80);
bool servidor_eth_activo = false;
struct ServiciosGetEthernet
{
  String servicio;
  String respuesta;
  fptr funcion_ejecutar;
};

struct DataResponse
{
  bool encontrada;
  byte id;
};

#define numero_total_servicios_get_montar_ethernet 50
ServiciosGetEthernet serviciosEthernet[numero_total_servicios_get_montar_ethernet];
byte idServicioAutoincremental = 0;

TaskHandle_t nucleo1EthKeepAlive_Handle = NULL;
void keepEthAlive(void *pvParameters);

int ConvertChar(char c)
{
  switch (c)
  {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  case 'A':
    return 10;
  case 'B':
    return 11;
  case 'C':
    return 12;
  case 'D':
    return 13;
  case 'E':
    return 14;
  case 'F':
    return 15;
  }
  return -1;
}

uint8_t ConvertChartoMac(char h1, char h0)
{
  h1 = ConvertChar(h1);
  h0 = ConvertChar(h0);
  return int(h1) * 16 + h0;
}

void ConfigEthernetKeepAlive()
{
  xTaskCreatePinnedToCore(
      keepEthAlive,                // Function that should be called
      "keepEthAlive",              // Name of the task (for debugging)
      EspacioMemoriaEthernet,      // Stack size (bytes)
      NULL,                        // Parameter to pass
      1,                           // Task priority
      &nucleo1EthKeepAlive_Handle, // Task handle
      1                            // Core you want to run the task on (0 or 1)
  );
}

void keepEthAlive(void *parameter)
{
  //Serial.print("[ETHERNET]: [Core 1]: "); Serial.println(xPortGetCoreID());

  thereConectionEthernet = false;

  //Para el servidor ETH
  bool servidor_eth_activo_inicializado = false;
  bool thereIsRequestForData = false;

  //Para setear el pin CS del hardware
  Ethernet.init(5); // GPIO5 on the ESP32.   // Use Ethernet.init(pin) to configure the CS pin.

  { //WizReset
/**
        Reset Ethernet: Wiz W5500 reset function.
    **/
#ifdef internet_library_debug
    Serial.print("wisrovi@internet: [/]:  [ETHERNET]: Resetting Wiz W5500 Ethernet Board...  ");
#endif
    pinMode(RESET_P, OUTPUT);
    digitalWrite(RESET_P, HIGH);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    digitalWrite(RESET_P, LOW);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(RESET_P, HIGH);
    vTaskDelay(350 / portTICK_PERIOD_MS);
    //Serial.println("Done.");
  }
  /****************************************/

  uint8_t eth_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  { //Asignar MAC para el modulo Ethernet
    bool GenerarMacAutomatica = true;
    /*
      Para asignar la MAC que se usará en el modulo Ethernet, tomo los primeros 3 hexetos de la mac wifi,
      estos tres primeros hexetos me definen el fabricante del producto, por lo cual uso el mismo fabricante del ESP para generar la MAC Ethernet

      Los siguientes tres hexetos finales los defino usando el chipID del ESP, este ID es un codigo unico donde está el serial del dispositivo, contiene entre 7 u 8 indices
      para lo cual divido en dos caracteres iniciando por las unidades, es decir,
      decenas y unidades             corresponden a el hexeto 6
      milesimas y centenas           corresponden a el hexeto 5
      centenar_de_mil y diezmilesima corresponden a el hexeto 4

      por ejemplo:
        MAC    = AA:BB:CC:DD:EE:FF
        ChipID = 76543210

        Quedaría:
        MAC_Ethernet = AA:BB:CC:54:32:10
    */
    if (GenerarMacAutomatica)
    {
      String mac_string = WiFi.macAddress();

      //String chipID_string = String(ESP.getChipId());
      String chipID_string = String((uint32_t)ESP.getEfuseMac());
      byte sizeChipID = chipID_string.length();
      char chipID_char[sizeChipID + 1];
      chipID_string.toCharArray(chipID_char, sizeChipID + 1);

      char mac_char[mac_string.length() + 1];
      mac_string.toCharArray(mac_char, mac_string.length() + 1);

      eth_MAC[0] = ConvertChartoMac(mac_char[0], mac_char[1]);
      eth_MAC[1] = ConvertChartoMac(mac_char[3], mac_char[4]);
      eth_MAC[2] = ConvertChartoMac(mac_char[6], mac_char[7]);
      //eth_MAC[3] = ConvertChartoMac(mac_char[9], mac_char[10]);
      //eth_MAC[4] = ConvertChartoMac(mac_char[12], mac_char[13]);
      //eth_MAC[5] = ConvertChartoMac(mac_char[15], mac_char[16]);

      eth_MAC[3] = ConvertChartoMac(chipID_char[sizeChipID - 5], chipID_char[sizeChipID - 4]);
      eth_MAC[4] = ConvertChartoMac(chipID_char[sizeChipID - 3], chipID_char[sizeChipID - 2]);
      eth_MAC[5] = ConvertChartoMac(chipID_char[sizeChipID - 1], chipID_char[sizeChipID]);
    }
  }

  { //Imprimir MAC
#ifdef internet_library_debug
    Serial.print("wisrovi@internet: [/]:  [ETHERNET]: MAC: ");
    for (byte i = 0; i < 6; i++)
    {
      Serial.print(eth_MAC[i], HEX);
      if (i < 5)
      {
        Serial.print(":");
      }
    }
    Serial.println();
#endif
  }

  bool thereConection = false;
  { //Buscar IP
    while (!thereConection)
    {
#ifdef internet_library_debug
      Serial.println("wisrovi@internet: [/]:  [ETHERNET]: Connecting...");
#endif
      if (Ethernet.begin(eth_MAC) == 0)
      {
#ifdef internet_library_debug
        //Serial.println("[ETHERNET]: Failed to configure Ethernet using DHCP");
#endif
        if (Ethernet.hardwareStatus() == EthernetNoHardware)
        {
#ifdef internet_library_debug
          //Serial.println("[ETHERNET]: Ethernet shield was not found.  Sorry, can't run without hardware. :(");
#endif
        }
        if (Ethernet.linkStatus() == LinkOFF)
        {
#ifdef internet_library_debug
          //Serial.println("[ETHERNET]: Ethernet cable is not connected.");
#endif
        }
        vTaskDelay(30000 / portTICK_PERIOD_MS);
      }
      else
      {
        thereConection = true;
        thereConectionEthernet = true;
      }
    }
  }
  Serial.print("wisrovi@internet: [/]:  [ETHERNET]: DHCP assigned IP: ");
  Serial.println(Ethernet.localIP());

  unsigned long startAttemptTime = millis();

  for (;;)
  {
    if (millis() - startAttemptTime > ETH_TIMEOUT)
    {
      startAttemptTime = millis();
#ifdef internet_library_debug
      Serial.print("wisrovi@internet: [/]:  [ETHERNET]: [Core 1]: ");
      Serial.println(xPortGetCoreID());
#endif
    }

    { //Buscar fallos en el ethernet relacionados con el cambio de ip por dhcp para corregirlo
      switch (Ethernet.maintain())
      {
      case 1:
//renewed fail
#ifdef internet_library_debug
        //Serial.println("wisrovi@internet: [/]:  [ETHERNET]: Error: renewed fail");
#endif
        thereConectionEthernet = false;
        break;
      case 2:
//renewed success
#ifdef internet_library_debug
        //Serial.println("wisrovi@internet: [/]:  [ETHERNET]: Renewed success");
        //print your local IP address:
        /*Serial.print("My IP address: ");
              Serial.println(Ethernet.localIP());*/
#endif
        break;
      case 3:
//rebind fail
#ifdef internet_library_debug
        //Serial.println("[ETHERNET]: Error: rebind fail");
#endif
        thereConectionEthernet = false;
        break;
      case 4:
//rebind success
#ifdef internet_library_debug
        //Serial.println("[ETHERNET]: Rebind success");
        //print your local IP address:
        /*Serial.print("My IP address: ");
              Serial.println(Ethernet.localIP());*/
#endif
        break;
      default:
        //nothing happened
        break;
      }
    }

    {
      { //levantar servidor si ha solicitado que se IniciarServidorEth
        if (servidor_eth_activo == true)
        {
          if (servidor_eth_activo_inicializado == false)
          {
            servidor_eth_activo_inicializado = true;
            serverETH.begin();
            Serial.println("wisrovi@internet: [/]:  [ETHERNET]: Servidor iniciado.");
          }
          else
          {
            //Esta rutina solo funciona si el servidor ETH ha sido activo e inicializado
            EthernetClient clientETH = serverETH.available();
            if (clientETH)
            {
#ifdef internet_library_debug
              Serial.println("wisrovi@internet: [/]:  [ETHERNET]: Nuevo cliente.");
#endif
              bool currentLineIsBlank = true;
              String response = "";
              while (clientETH.connected())
              { //leer peticion
                if (clientETH.available())
                {
                  char c = clientETH.read();
                  //Serial.write(c);
                  response.concat(c);
                  // if you've gotten to the end of the line (received a newline
                  // character) and the line is blank, the http request has ended,
                  // so you can send a reply
                  if (c == '\n' && currentLineIsBlank)
                  {
                    // send a standard http response header

                    clientETH.println("HTTP/1.1 200 OK");
                    clientETH.println("Content-Type: text/plain");
                    clientETH.println("Connection: close"); // the connection will be closed after completion of the response
                    clientETH.println();

                    DataResponse solicitud;
                    solicitud.encontrada = false;

                    {
#ifdef internet_library_debug
                      Serial.print("wisrovi@internet: [/]:  [ETHERNET]: data request: ");
#endif
                      int index_start = response.indexOf('GET');
                      response = response.substring(index_start + 2);
                      int index_stop = response.indexOf('\n');
                      String peticion = "";
                      if (index_stop > 9)
                      {
                        peticion = response.substring(0, index_stop - 10);
                      }

#ifdef internet_library_debug
                      Serial.print("peticion: <");
                      Serial.print(peticion);
                      Serial.println(">");
#endif

                      int index_buscar_variables = peticion.indexOf('?');
                      String servicio_recibido_ethernet = peticion.substring(0, index_buscar_variables);
                      String variables_recibidas_ethernet = peticion.substring(index_buscar_variables + 1);

#ifdef internet_library_debug
                      Serial.print("servicio: <");
                      Serial.print(servicio_recibido_ethernet);
                      Serial.print("> variables: <");
                      Serial.print(variables_recibidas_ethernet);
                      Serial.println(">");
#endif

                      for (byte i = 0; i < numero_total_servicios_get_montar_ethernet; i++)
                      {
                        if (serviciosEthernet[i].servicio.equals(servicio_recibido_ethernet))
                        {
#ifdef internet_library_debug
                          Serial.println("Servicio encontrado, ejecutando funcion y enviado las variables leidas, y entregando una respuesta conforme a la peticion.");
#endif
                          String value = serviciosEthernet[i].funcion_ejecutar(variables_recibidas_ethernet);
                          serviciosEthernet[i].respuesta = value;
                          solicitud.encontrada = true;
                          solicitud.id = i;
                          break;
                        }
                      }
                    }
                    if (solicitud.encontrada)
                    {
                      clientETH.print(serviciosEthernet[solicitud.id].respuesta);
                    }
                    else
                    {
                      clientETH.print("BAD REQUEST");
                    }

                    break;
                  }
                  if (c == '\n')
                  {
                    // you're starting a new line
                    currentLineIsBlank = true;
                  }
                  else if (c != '\r')
                  {
                    // you've gotten a character on the current line
                    currentLineIsBlank = false;
                  }
                }
              }

              clientETH.stop();
#ifdef internet_library_debug
              Serial.println("wisrovi@internet: [/]:  [ETHERNET]: cliente desconectado");
#endif
            }
          }
        }
      }
    }

    vTaskDelay(15 / portTICK_PERIOD_MS);
  }
}

bool getStatusConectionEthernet()
{
  if (thereConectionAP)
  {
    return true;
  }
  else
  {
    return thereConectionEthernet;
  }
}

bool isActiveUseEthernet()
{
  return actived_use_eth;
}

bool RegistrarNuevoServicioGet_ETH(String url, fptr callback)
{ //}, THandlerFunction fn){
  servidor_eth_activo = true;
  serviciosEthernet[idServicioAutoincremental].servicio = url;
  serviciosEthernet[idServicioAutoincremental].funcion_ejecutar = callback;

#ifdef internet_library_debug
  Serial.print("[ETHERNET]: Registrando nuevo servicio Get.");
#endif
  idServicioAutoincremental++;
}

#else

bool isActiveUseEthernet()
{
  return false;
}

bool getStatusConectionEthernet()
{
  return false;
}

void RegistrarNuevoServicioGet_ETH(String url, fptr callback)
{
}

#endif

/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *                                                                          *
                                AP TASK
 *                                                                          *
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *****************************************************************************/

#ifdef useAP

#define email_portal_cautivo "wisrovi.rodriguez@gmail.com"
#define website_portal_cautivo "http://www.wisrovi.com/"
#define linkedin_portal_cautivo "https://www.linkedin.com/in/wisrovi-rodriguez"
//http://conectivitycheck.gstatic.com/

#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoJson.h>
DynamicJsonDocument clientsStations(2048);
#include "esp_wifi.h"

TaskHandle_t nucleo1WifiAP_Handle = NULL;
void WifiAP(void *pvParameters);

bool AP_started = false;
bool actived_use_AP = false;

#include <DNSServer.h>
#include <AsyncTCP.h>
#ifndef useWifi
DynamicJsonDocument rutasUrl(1024);
#include <Hash.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
AsyncWebServer server_OTA(80);
typedef std::function<void(AsyncWebServerRequest *request)> THandlerFunction;
bool RegistrarNuevoServicioGet(String url, THandlerFunction fn)
{
  bool servicioCreado = false;

  bool estaActivoUsoWii = false;
#ifdef useWifi
  estaActivoUsoWii = thereConectionWifi;
#endif

  if (thereConectionAP || estaActivoUsoWii)
  {
    char url_char[url.length() + 1];
    url.toCharArray(url_char, url.length() + 1);
    bool existeRuta = rutasUrl[url];
    ;
    if (existeRuta == false)
    {
      server_OTA.on(url_char, HTTP_GET, fn);
      rutasUrl[url] = true;
      servicioCreado = true;
    }
    else
    {
      Serial.print("wisrovi@internet: [/]:  [AP]: El servicio '");
      Serial.print(url);
      Serial.println("' no se puede crear, ya existe o esta reservado.");
    }
    /*String allStationsConected;
			serializeJson(rutasUrl, allStationsConected);
			Serial.print("[AP]: ");
			Serial.println(allStationsConected);*/
  }
  return servicioCreado;
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}
#endif

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;

void linkedin(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", linkedin_portal_cautivo);
}

void Website(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", website_portal_cautivo);
}

void email(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", email_portal_cautivo);
}

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    //request->addInterestingHeader("ANY");
    // redirect if not in wifi client mode (through filter)
    // and request for different host (due to DNS * response)
    if (request->host() != WiFi.softAPIP().toString())
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html>");
    response->print("<html>");
    response->print("<head>");
    //response->print("<meta http-equiv='refresh' content='5'/>");
    response->print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\"/>");
    response->print("<title>Captive Portal</title>");
    response->print("<style> body {  background-color: rgb(43, 42, 42);  text-align: center;  color: white; }</style>");
    response->print("</head>");
    response->print("<body>");
    response->print("<p>&nbsp;</p>");
    response->print("<h1 style=\"text-align: center;\">Portal Cautivo</h1>");
    response->print("<h3 style=\"text-align: center;\">P&aacute;gina de Informaci&oacute;n</h3>");
    response->print("<p style=\"text-align: center; \"><strong>AVISO:  </strong><em><span style=\"color: #ff0000; font-size: large;\">Para garantizar que el m&oacute;vil no se desconecte del AP, se recomienda:</span>");
    response->print("<br/><br/>  <input type=\"checkbox\" value=\"1\" />&nbsp;&nbsp;<span style=\"color: #ff0000; font-size: large;\">Apagar los datos m&oacute;viles  </span></input> ");
    response->print("<br/><br/><input type=\"checkbox\" value=\"3\" />&nbsp;&nbsp;<span style=\"color: #ff0000; font-size: large;\">Apagar VPN (si tiene alguna)</span></input> ");
    response->print("<br/><br/>  <input type=\"checkbox\" value=\"2\" />&nbsp;&nbsp;<span style=\"color: #ff0000; font-size: large;\">Quitar el proxy (si tiene alguno)</span></input> </em></p><p>&nbsp;</p>");
    //response->print("");

    response->print("<p style=\"text-align: center;\"><em>Para poder realizar las diferentes solicitudes al AP se recomienda usar la app proporcionada por el fabricante.</em></p>");
    response->print("<p>&nbsp;</p>");

    response->print("<p style=\"text-align: center;\">Creado por: Wisrovi Rodriguez.</p>");
    response->print("<p style=\"text-align: center; \">");
    response->printf("&nbsp;      &nbsp;<a title=\"Email author\" href=\"http://%s%s\" target=\"_blank\" rel=\"noopener\">email</a>", request->host().c_str(), "/Website");
    response->printf("&nbsp;      &nbsp;<a title=\"Linkedin autor\" href=\"http://%s%s\" target=\"_blank\" rel=\"noopener\">linkedin</a>", request->host().c_str(), "/Website");
    response->printf("&nbsp;      &nbsp;<a title=\"Curriculum vitae autor\" href=\"http://%s%s\" target=\"_blank\" rel=\"noopener\">Website</a>", request->host().c_str(), "/Website");
    //response->printf("<p style=\"text-align: center;color: greenyellow !important;\"><a title=\"Curriculum vitae autor\" href=\"http://%s%s\" target=\"_blank\" rel=\"noopener\">Website</a></p>", request->host().c_str(), "/Website");
    response->print("</body>");
    response->print("</html>");
    request->send(response);
  }
};

bool ConfigWifiAP(String nombreAP, String passwordAP)
{
  if (thereConectionAP)
  {
    nombreAP.toCharArray(nombreAP_char, 25);
    passwordAP.toCharArray(passwordAP_char, 25);

    xTaskCreatePinnedToCore(
        WifiAP, "WifiAP" // A name just for humans
        ,
        EspacioMemoriaWifiAP // // Stack size (bytes),
        ,
        NULL // Parameter to pass
        ,
        1 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
        ,
        &nucleo1WifiAP_Handle, 1);
    return true;
  }
  else
  {
    return false;
  }
}

bool IniciarServidorAP = false;
bool servidorAP_iniciado = false;

bool setIniciarServidorAP()
{
  if (thereConectionAP)
  {
    IniciarServidorAP = true;
  }
  return IniciarServidorAP;
}

void ProcessClient(String ipCliente)
{
  if (thereConectionAP)
  {
    bool encontrada = false;
    for (byte i = 0; i < clientsStations.size(); i++)
    {
      DynamicJsonDocument doc(128);
      doc[String(i)] = clientsStations[String(i)];
      String ip_save = doc[String(i)]["ip"];
      if (ipCliente.equals(ip_save))
      {
        doc[String(i)]["active"] = true;
        doc[String(i)]["ip"] = ipCliente;
        String msg;
        String msg;
        serializeJson(doc, msg);
        if (msg.length() > 3)
        {
          clientsStations[String(i)] = doc[String(i)];
        }
        encontrada = true;
        break;
      }
    }

    if (encontrada == false)
    {
      //no se econtró la ip en la lista de clientes registrados
    }
    /*String allStationsConected;
    serializeJson(clientsStations, allStationsConected);
    Serial.print("wisrovi@internet: [/]:  [AP]: ");
    Serial.println(allStationsConected);*/
  }
}

void ProcesarClienteRequest(AsyncWebServerRequest *request)
{
  String ipCliente = request->client()->remoteIP().toString().c_str();
  ProcessClient(ipCliente);
}

bool firstList = false;
TaskHandle_t ListClientIp_Handle = NULL;
unsigned long timeRefreshClientesDataIp;
void ListClientIp(void *pvParameters)
{
  (void *)pvParameters;
  Serial.print("wisrovi@internet: [/]:  [ListClientIp]: Nucleo: ");
  Serial.println(xPortGetCoreID());

  timeRefreshClientesDataIp = millis();
  for (;;)
  {
    if (millis() - timeRefreshClientesDataIp > 30000)
    {
      timeRefreshClientesDataIp = millis();
      {
        tcpip_adapter_sta_list_t adapter_sta_list;
        { //extraigo la lista de estaciones
          wifi_sta_list_t wifi_sta_list;
          memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
          memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
          esp_wifi_ap_get_sta_list(&wifi_sta_list);
          tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
        }

        byte conteoErrores = 0;
        for (int idStation = 0; idStation < adapter_sta_list.num; idStation++)
        {
          tcpip_adapter_sta_info_t station = adapter_sta_list.sta[idStation];
          String mac = "";
          String ip = "";
          { //leo la MAC y la IP
            for (int i = 0; i < 6; i++)
            {
              mac.concat(String(station.mac[i], HEX));
              if (i < 5)
              {
                mac.concat(":");
              }
            }
            ip = ip4addr_ntoa(&(station.ip));
          }

          bool mac_and_ip_ok = false;
          bool ya_esta_creada = false;
          if (!mac.equals("0:0:0:0:0:0")) // || !ip.equals("0.0.0.0"))  //confirmo que los datos de MAC son datos_validos y que no se encuentren creados
          {
            mac_and_ip_ok = true;
            { //buscar y si encuentro una coincidencia, en este caso de la MAC
              for (byte i = 0; i < clientsStations.size(); i++)
              {
                DynamicJsonDocument doc(128);
                doc[String(i)] = clientsStations[String(i)];
                String mac_save = doc[String(i)]["mac"];
                if (mac.equals(mac_save))
                {
                  ya_esta_creada = true;
                  //doc[String(i)]["active"] = true;
                  String msg;
                  serializeJson(doc, msg);
                  if (msg.length() > 3)
                  {
                    clientsStations[String(i)] = doc[String(i)];
                  }

                  //Serial.println("wisrovi@internet: [/]:  [AP]: la MAC ya se encontraba creada como cliente en esta sesion del AP.");
                  break;
                }
              }
            }
          }
          else
          {
#ifdef internet_library_debug
            Serial.print("wisrovi@internet: [/]:  [ListClientIp]: Error detectar mac o ip en cliente -> ");
            Serial.print("[ MAC: ");
            Serial.print(mac);
            Serial.print(" - IP: ");
            Serial.print(ip);
            Serial.println(" ]");
#endif
            conteoErrores++;
            if (conteoErrores < adapter_sta_list.num)
            {
              idStation--;
            }
          }

          if (mac_and_ip_ok && !ya_esta_creada) //inserto en el array los datos de mac e ip, siempre que no existan
          {
            if (clientsStations.size() == 0)
            {
              //Serial.println("[AP]: Creando Json estaciones conectadas.");
            }
            DynamicJsonDocument thisStation(128);
            if (!mac.equals("0:0:0:0:0:0")) //se filtra que la mac no sea 0:0:0:0:0:0
            {
              thisStation["mac"] = mac;
            }
            else
            {
#ifdef internet_library_debug
              Serial.print("wisrovi@internet: [/]:  [ListClientIp]: Error detectar mac, la ip es: ");
              Serial.println(ip);
#endif
            }

            if (!ip.equals("0.0.0.0")) //se filtra que la ip no sea 0.0.0.0
            {
              thisStation["ip"] = ip;
            }
            else
            {
#ifdef internet_library_debug
              Serial.print("wisrovi@internet: [/]:  [ListClientIp]: Error detectar ip, la mac es: ");
              Serial.print(mac);
              Serial.print(" con ip: ");
              Serial.println(ip);
#endif
            }
            thisStation["id"] = idStation;
            thisStation["active"] = false;
            clientsStations[String(idStation)] = thisStation;
          }

          if (mac_and_ip_ok && ya_esta_creada)
          {
            if (!ip.equals("0.0.0.0"))
            {
              if (clientsStations.size() > 0)
              {
                for (byte i = 0; i < clientsStations.size(); i++)
                {
                  DynamicJsonDocument doc(128);
                  doc[String(i)] = clientsStations[String(i)];
                  String mac_guardada = doc[String(i)]["mac"];
                  if (mac.equals(mac_guardada))
                  {
                    String ip_guardada = clientsStations[String(i)]["ip"];
                    if (!ip_guardada.equals(ip))
                    {
                      doc[String(i)]["ip"] = ip;

                      String msg;
                      serializeJson(doc, msg);
                      if (msg.length() > 3)
                      {
                        clientsStations[String(i)] = doc[String(i)];
                      }

#ifdef internet_library_debug
                      Serial.print("wisrovi@internet: [/]:  [ListClientIp]: Refresh ip: ");
                      Serial.print(mac);
                      Serial.print(" con ip: ");
                      Serial.println(ip);
#endif
                    }
                    break;
                  }
                }
              }
            }
          }
        }

        firstList = true;
      }
      //Serial.println("wisrovi@internet: [/]:  [ListClientIp]: Refresh ip finished. ");
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void StartListClientIp()
{
  xTaskCreatePinnedToCore(
      ListClientIp,         // Function that should be called
      "ListClientIp",       // Name of the task (for debugging)
      2048,                 // Stack size (bytes)
      NULL,                 // Parameter to pass
      1,                    // Task priority
      &ListClientIp_Handle, // Task handle
      1                     // Core you want to run the task on (0 or 1)
  );
}

void WifiAP(void *pvParameters)
{
  (void *)pvParameters;
  Serial.print("wisrovi@internet: [/]:  [WifiAP]: Nucleo: ");
  Serial.println(xPortGetCoreID());

  //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(nombreAP_char, passwordAP_char);

#ifdef useCaptivePortal
  //dnsServer.setTTL(60*5);  // default is 60 seconds, modify TTL associated  with the domain name (in seconds)
  //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
#endif

  server_OTA.on("/email", HTTP_GET, email);
  server_OTA.on("/linkedin", HTTP_GET, linkedin);
  server_OTA.on("/Website", HTTP_GET, Website);
  rutasUrl["/email"] = true;
  rutasUrl["/linkedin"] = true;
  rutasUrl["/Website"] = true;
  server_OTA.onNotFound(notFound);

#ifdef useCaptivePortal
  server_OTA.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); //only when requested from AP
#endif

  Serial.println();
  Serial.print("wisrovi@internet: [/]:  [AP]: SSID: ");
  Serial.println(nombreAP_char);
  Serial.print("wisrovi@internet: [/]:  [AP]: PWD: ");
  Serial.print(passwordAP_char);
  Serial.print("wisrovi@internet: [/]:  [AP]: Gate: ");
  Serial.println(WiFi.softAPIP());

  vTaskDelay(2500 / portTICK_PERIOD_MS);

  AP_started = true;

  byte numStationsConected = 0;
  unsigned long timeGetCheckNumberStationsConected = millis();
  for (;;)
  {
    if (IniciarServidorAP == true && servidorAP_iniciado == false)
    {
      servidorAP_iniciado = true;

      AsyncElegantOTA.begin(&server_OTA); // Start ElegantOTA
      server_OTA.begin();
      Serial.println("wisrovi@internet: [/]:  [AP]: Iniciando Servidor");

      StartListClientIp();
    }

    if (servidorAP_iniciado)
    {
#ifdef useCaptivePortal
      dnsServer.processNextRequest();
#endif
      AsyncElegantOTA.loop();
    }
    else
    {
      //Serial.println("No se ha iniado el servidor.");
    }

    if (millis() - timeGetCheckNumberStationsConected > 1000)
    {
      timeGetCheckNumberStationsConected = millis();
      byte cantidadEstacionesConectadas = WiFi.softAPgetStationNum(); //https://esp8266-arduino-spanish.readthedocs.io/es/latest/esp8266wifi/soft-access-point-class.html
      if (numStationsConected != cantidadEstacionesConectadas)
      {
        if (numStationsConected > cantidadEstacionesConectadas)
        {
          Serial.println("wisrovi@internet: [/]:  [AP]: cliente desconectado");
        }
        else
        {
          Serial.println("\n");
          Serial.println("wisrovi@internet: [/]:  [AP]: Nuevo cliente detectado");
          timeRefreshClientesDataIp = millis();
        }
        numStationsConected = cantidadEstacionesConectadas;
      }
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    //vTaskDelete( NULL );*/
  }
}

String getClientes()
{
  if (firstList)
  {
    String output;
    serializeJson(clientsStations, output);
    return output;
  }
  else
  {
    return "wait 30 seconds while refresh list";
  }
}

bool getAPStart()
{
  if (thereConectionAP)
  {
    return AP_started;
  }
  else
  {
    return true;
  }
}

bool isActiveUseAP()
{
  return actived_use_AP;
}

#endif

/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *                                                                          *
                       SEND AND RESPONSE TASK
 *                                                                          *
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *****************************************************************************/

String _ip;
int _port;
String _urlSolicitud;
bool _enviar_get = false;

#include <RestClient.h>
RestClient client = RestClient("192.168.1.5");
String respuestaGet = "";

#include <HTTPClient.h>

TaskHandle_t nucleo1InternetResponse_Handle = NULL;
void internetResponse(void *pvParameters);
#define EspacioMemoriaInternetResponse 1800

bool ConfigInternet(bool usarWifi, bool usarEth, bool isAP)
{
  bool existConfigRed = false;

  if (isAP)
  {
#ifdef useAP
    actived_use_AP = isAP;
    existConfigRed = true;
    thereConectionAP = true;
#endif
  }
  else
  {
#ifdef useWifi
    actived_use_wifi = usarWifi;
    if (usarWifi)
    {
      existConfigRed = true;
      thereConectionAP = false;
      ConfigWifiKeepAlive();
    }
#endif

#ifdef useEth
    actived_use_eth = usarEth;
    if (usarEth)
    {
      thereConectionEthernet = true;
      existConfigRed = true;
      ConfigEthernetKeepAlive();

      client.setHeader("Host: www.wisrovi.com");
      client.setHeader("Connection: close");
    }
#endif

    if (existConfigRed)
    {
      xTaskCreatePinnedToCore(
          internetResponse,                // Function that should be called
          "internetResponse",              // Name of the task (for debugging)
          EspacioMemoriaInternetResponse,  // Stack size (bytes)
          NULL,                            // Parameter to pass
          1,                               // Task priority
          &nucleo1InternetResponse_Handle, // Task handle
          1                                // Core you want to run the task on (0 or 1)
      );
    }
  }
  return existConfigRed;
}

bool thereInternet()
{
  bool hayUsoEthernet = false;
  bool hayUsoWifi = false;
#ifdef useEth
  hayUsoEthernet = thereConectionEthernet;
#endif
#ifdef useWifi
  hayUsoWifi = thereConectionWifi;
#endif
  return hayUsoEthernet || hayUsoWifi;
}

void internetResponse(void *parameter)
{
  respuestaGet = "";
  _enviar_get = false;

  for (;;)
  {
    vTaskDelay(15 / portTICK_PERIOD_MS);
    if (_enviar_get)
    {
      _enviar_get = false;

      bool thereInternet = false;
      respuestaGet = "";

#ifdef useEth
      if (thereConectionEthernet)
      {
        bool checkHardwareEthernet = true;
        if (Ethernet.hardwareStatus() == EthernetNoHardware)
        {
#ifdef internet_library_debug
          //Serial.println("[ETHERNET]: Ethernet shield was not found.  Sorry, can't run without hardware. :(");
#endif
          checkHardwareEthernet = false;
        }
        if (Ethernet.linkStatus() == LinkOFF)
        {
#ifdef internet_library_debug
          //Serial.println("[ETHERNET]: Ethernet cable is not connected.");
#endif
          checkHardwareEthernet = false;
        }

        if (checkHardwareEthernet)
        {
          thereInternet = true;
          char ip_char[_ip.length() + 1];
          _ip.toCharArray(ip_char, _ip.length() + 1);

          char urlSolicitud_char[_urlSolicitud.length() + 1];
          _urlSolicitud.toCharArray(urlSolicitud_char, _urlSolicitud.length() + 1);

          int statusCode = client.get(ip_char, _port, urlSolicitud_char, &respuestaGet);
          if (statusCode == 200)
          {
#ifdef internet_library_debug
            //Serial.print("[ETHERNET]: Rta get: ");
            //Serial.println(respuestaGet);
#endif
          }
          else
          {
            Serial.print("wisrovi@internet: [/]:  [ETHERNET]: Status get: ");
            Serial.println(statusCode);
          }
        }
      }
#endif

#ifdef useWifi
      if (!thereInternet)
      {
        if (thereConectionWifi)
        {
          thereInternet = true;

          String urlSend = "http://";
          urlSend.concat(_ip);
          urlSend.concat(":");
          urlSend.concat(String(_port));
          urlSend.concat(_urlSolicitud);
          //Serial.println(urlSend);

          HTTPClient http;
          http.begin(urlSend);       // configure traged server and url
          int httpCode = http.GET(); // start connection and send HTTP header
          if (httpCode > 0)
          { // httpCode will be negative on error
            // HTTP header has been send and Server response header has been handled
            if (httpCode == HTTP_CODE_OK)
            { // file found at server
              respuestaGet = http.getString();
              //Serial.print("[WIFI]: Rta get: ");
              //Serial.println(respuestaGet);
            }
          }
          else
          {
            Serial.print("wisrovi@internet: [/]:  [WIFI]: Error get: ");
            Serial.println(http.errorToString(httpCode).c_str());
          }
          http.end();
        }
      }
#endif

      if (!thereInternet)
      {
        Serial.println("wisrovi@internet: [/]:  No hay internet, no se puede enviar el mensaje.");
      }
    }
  }
  //vTaskDelete(NULL); //borrar esta tarea
}

void SendGet(String ip, int port, String urlSolicitud)
{
  _ip = ip;
  _port = port;
  _urlSolicitud = urlSolicitud;
  _enviar_get = true;
}

String getResponse()
{
  return respuestaGet;
}

/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *                                                                          *
                                  EXAMPLE
 *                                                                          *
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *****************************************************************************/

/**
Configurar que deseo activar de la libreria: ethernet, wifi o ambos
esto va en el setup


bool usarWifi = true;
  bool usarEth = true;
  ConfigInternet(usarWifi, usarEth);

*/

/***


    Para enviar un mensaje se debe:
    SendGet("ip", puerto, "rutaGet");

*/

/**

    Para recibir la respuesta se debe esperar al menos 2 segundos, luego se captura la respuesta con:
    getResponse()

*/

#endif